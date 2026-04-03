/**
 * @file MCPServerRunnable.cpp
 * @brief MCP socket 服务线程实现，负责连接管理与消息收发。
 */
#include "MCPServerRunnable.h"
#include "UnrealMCPBridge.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "JsonObjectConverter.h"
#include "Misc/ScopeLock.h"
#include "HAL/PlatformTime.h"

/** @brief 接收缓冲区大小（字节）。 */
constexpr int32 MCPReceiveBufferSize = 8192;
/** @brief 单次等待客户端可读事件的时长。 */
constexpr double MCPClientReadWaitMilliseconds = 100.0;
/** @brief 空闲客户端最大保活时长，超过后主动断开。 */
constexpr double MCPClientIdleTimeoutSeconds = 2.0;

class FUnrealMCPServerSocketDestroyer
{
public:
    void operator()(FSocket* Socket) const
    {
        if (!Socket)
        {
            return;
        }

        if (ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
        {
            SocketSubsystem->DestroySocket(Socket);
            return;
        }

        delete Socket;
    }
};

enum class EMCPJsonScanResult : uint8
{
    NeedMoreData,
    FoundMessage,
    InvalidData,
};

static TSharedPtr<FSocket> MakeUnrealMCPServerSocketShareable(FSocket* Socket)
{
    if (!Socket)
    {
        return TSharedPtr<FSocket>();
    }

    return MakeShareable(Socket, FUnrealMCPServerSocketDestroyer());
}

static void CloseSocketSafely(const TSharedPtr<FSocket>& Socket)
{
    if (!Socket.IsValid())
    {
        return;
    }

    Socket->Shutdown(ESocketShutdownMode::ReadWrite);
    Socket->Close();
}

static FString SerializeJsonObjectToString(const TSharedPtr<FJsonObject>& JsonObject)
{
    if (!JsonObject.IsValid())
    {
        return TEXT("{\"status\":\"error\",\"error\":\"响应对象无效\"}");
    }

    FString ResultString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
    return ResultString;
}

static FString BuildErrorResponseString(const FString& ErrorMessage)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetStringField(TEXT("status"), TEXT("error"));
    ResponseObject->SetStringField(TEXT("error"), ErrorMessage);
    return SerializeJsonObjectToString(ResponseObject);
}

static bool SendUtf8Message(const TSharedPtr<FSocket>& Socket, const FString& Message);

static bool SendErrorResponse(const TSharedPtr<FSocket>& Socket, const FString& ErrorMessage)
{
    return SendUtf8Message(Socket, BuildErrorResponseString(ErrorMessage));
}

static bool SendUtf8Message(const TSharedPtr<FSocket>& Socket, const FString& Message)
{
    if (!Socket.IsValid())
    {
        return false;
    }

    FTCHARToUTF8 MessageUtf8(*Message);
    const uint8* Data = reinterpret_cast<const uint8*>(MessageUtf8.Get());
    const int32 TotalBytes = MessageUtf8.Length();
    int32 TotalSent = 0;

    while (TotalSent < TotalBytes)
    {
        int32 BytesSent = 0;
        if (!Socket->Send(Data + TotalSent, TotalBytes - TotalSent, BytesSent) || BytesSent <= 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: 发送响应失败，已发送 %d/%d 字节"), TotalSent, TotalBytes);
            return false;
        }

        TotalSent += BytesSent;
    }

    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: 响应发送完成，字节数=%d"), TotalSent);
    return true;
}

static EMCPJsonScanResult TryFindNextJsonMessageEnd(
    const TArray<uint8>& Buffer,
    int32& OutMessageEndExclusive,
    uint8& OutInvalidStartByte)
{
    OutMessageEndExclusive = INDEX_NONE;
    OutInvalidStartByte = 0;

    bool bStarted = false;
    bool bInsideString = false;
    bool bEscaping = false;
    int32 ObjectDepth = 0;

    for (int32 Index = 0; Index < Buffer.Num(); ++Index)
    {
        const uint8 Byte = Buffer[Index];

        if (!bStarted)
        {
            if (Byte == '{')
            {
                bStarted = true;
                ObjectDepth = 1;
            }
            else if (Byte == ' ' || Byte == '\t' || Byte == '\r' || Byte == '\n')
            {
                continue;
            }
            else
            {
                OutInvalidStartByte = Byte;
                return EMCPJsonScanResult::InvalidData;
            }

            continue;
        }

        if (bInsideString)
        {
            if (bEscaping)
            {
                bEscaping = false;
                continue;
            }

            if (Byte == '\\')
            {
                bEscaping = true;
            }
            else if (Byte == '"')
            {
                bInsideString = false;
            }

            continue;
        }

        if (Byte == '"')
        {
            bInsideString = true;
            continue;
        }

        if (Byte == '{')
        {
            ++ObjectDepth;
            continue;
        }

        if (Byte == '}')
        {
            --ObjectDepth;
            if (ObjectDepth == 0)
            {
                OutMessageEndExclusive = Index + 1;
                return EMCPJsonScanResult::FoundMessage;
            }
        }
    }

    return EMCPJsonScanResult::NeedMoreData;
}

static FString Utf8BytesToString(const TArray<uint8>& Bytes)
{
    if (Bytes.Num() == 0)
    {
        return FString();
    }

    FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
    return FString(Converter.Length(), Converter.Get());
}

/**
 * @brief 构造函数。
 * @param [in] InBridge 命令执行桥接对象。
 * @param [in] InListenerSocket 已创建的监听套接字。
 */
FMCPServerRunnable::FMCPServerRunnable(UUnrealMCPBridge* InBridge, TSharedPtr<FSocket> InListenerSocket)
    : Bridge(InBridge)
    , ListenerSocket(InListenerSocket)
    , bRunning(true)
{
    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Created server runnable"));
}

/**
 * @brief 析构函数。
 */
FMCPServerRunnable::~FMCPServerRunnable()
{
    // Note: We don't delete the sockets here as they're owned by the bridge
}

/**
 * @brief 线程初始化函数。
 * @return bool 始终返回 true。
 */
bool FMCPServerRunnable::Init()
{
    return true;
}

/**
 * @brief 线程主循环：接收连接，并交由统一的字节流处理逻辑解析 JSON 命令。
 * @return uint32 线程退出码。
 */
uint32 FMCPServerRunnable::Run()
{
    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: 服务线程已启动"));
    
    while (bRunning)
    {
        bool bPending = false;
        if (!ListenerSocket.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: 监听 socket 已失效，结束服务线程"));
            break;
        }

        if (ListenerSocket->HasPendingConnection(bPending) && bPending)
        {
            UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: 检测到待处理客户端连接，开始 Accept"));
            
            ClientSocket = MakeUnrealMCPServerSocketShareable(ListenerSocket->Accept(TEXT("MCPClient")));
            if (ClientSocket.IsValid())
            {
                UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: 客户端连接已接受"));
                
                ClientSocket->SetNoDelay(true);
                ClientSocket->SetNonBlocking(false);
                int32 SocketBufferSize = 65536;  // 64KB buffer
                ClientSocket->SetSendBufferSize(SocketBufferSize, SocketBufferSize);
                ClientSocket->SetReceiveBufferSize(SocketBufferSize, SocketBufferSize);
                HandleClientConnection(ClientSocket);
                CloseSocketSafely(ClientSocket);
                ClientSocket.Reset();
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Accept 客户端连接失败"));
            }
        }
        
        FPlatformProcess::Sleep(0.1f);
    }
    
    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: 服务线程即将退出"));
    return 0;
}

/**
 * @brief 请求停止线程主循环。
 */
void FMCPServerRunnable::Stop()
{
    bRunning = false;
    CloseSocketSafely(ClientSocket);
    CloseSocketSafely(ListenerSocket);
}

/**
 * @brief 线程退出回调。
 */
void FMCPServerRunnable::Exit()
{
}

/**
 * @brief 处理单个客户端连接。
 * @param [in] InClientSocket 客户端连接套接字。
 */
void FMCPServerRunnable::HandleClientConnection(TSharedPtr<FSocket> InClientSocket)
{
    if (!InClientSocket.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("MCPServerRunnable: HandleClientConnection 收到无效客户端 socket"));
        return;
    }

    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: 开始处理客户端连接"));

    uint8 Buffer[MCPReceiveBufferSize];
    TArray<uint8> PendingBytes;
    PendingBytes.Reserve(MCPReceiveBufferSize);
    const FTimespan ReadWaitTime = FTimespan::FromMilliseconds(MCPClientReadWaitMilliseconds);
    double LastReceiveTimeSeconds = FPlatformTime::Seconds();

    while (bRunning && InClientSocket.IsValid())
    {
        if (!InClientSocket->Wait(ESocketWaitConditions::WaitForRead, ReadWaitTime))
        {
            const ESocketConnectionState ConnectionState = InClientSocket->GetConnectionState();
            if (ConnectionState != SCS_Connected)
            {
                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT("MCPServerRunnable: 客户端连接状态异常，结束处理。ConnectionState=%d"),
                    static_cast<int32>(ConnectionState));
                break;
            }

            const double IdleDurationSeconds = FPlatformTime::Seconds() - LastReceiveTimeSeconds;
            if (IdleDurationSeconds >= MCPClientIdleTimeoutSeconds)
            {
                if (PendingBytes.Num() > 0)
                {
                    UE_LOG(
                        LogTemp,
                        Warning,
                        TEXT("MCPServerRunnable: 客户端在 %.2f 秒内没有补齐完整 JSON，返回错误并断开。缓冲区字节数=%d"),
                        IdleDurationSeconds,
                        PendingBytes.Num());
                    SendErrorResponse(InClientSocket, TEXT("MCP 消息在超时前没有组成完整 JSON 对象"));
                }
                else
                {
                    UE_LOG(
                        LogTemp,
                        Warning,
                        TEXT("MCPServerRunnable: 客户端在 %.2f 秒内没有发送任何数据，主动断开空闲连接"),
                        IdleDurationSeconds);
                }
                break;
            }

            continue;
        }

        int32 BytesRead = 0;
        const bool bReadSuccess = InClientSocket->Recv(Buffer, MCPReceiveBufferSize, BytesRead, ESocketReceiveFlags::None);

        if (BytesRead > 0)
        {
            LastReceiveTimeSeconds = FPlatformTime::Seconds();
            PendingBytes.Append(Buffer, BytesRead);
            UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: 本次收到 %d 字节，累计缓冲区 %d 字节"), BytesRead, PendingBytes.Num());

            while (PendingBytes.Num() > 0)
            {
                int32 MessageEndExclusive = 0;
                uint8 InvalidStartByte = 0;
                const EMCPJsonScanResult ScanResult = TryFindNextJsonMessageEnd(
                    PendingBytes,
                    MessageEndExclusive,
                    InvalidStartByte);

                if (ScanResult == EMCPJsonScanResult::NeedMoreData)
                {
                    break;
                }

                if (ScanResult == EMCPJsonScanResult::InvalidData)
                {
                    UE_LOG(
                        LogTemp,
                        Warning,
                        TEXT("MCPServerRunnable: 收到非法 JSON 起始字节 0x%02X，返回协议错误并断开客户端"),
                        InvalidStartByte);
                    SendErrorResponse(InClientSocket, TEXT("收到的消息不是合法的 JSON 对象"));
                    PendingBytes.Reset();
                    break;
                }

                TArray<uint8> MessageBytes;
                MessageBytes.Append(PendingBytes.GetData(), MessageEndExclusive);
                const FString MessageText = Utf8BytesToString(MessageBytes).TrimStartAndEnd();
                PendingBytes.RemoveAt(0, MessageEndExclusive, EAllowShrinking::No);

                if (MessageText.IsEmpty())
                {
                    UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: 解析到空消息，已忽略"));
                    continue;
                }

                ProcessMessage(InClientSocket, MessageText);
            }
        }
        else if (!bReadSuccess)
        {
            const int32 LastError = (int32)ISocketSubsystem::Get()->GetLastErrorCode();
            UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: 客户端连接结束或读取失败，错误码: %d"), LastError);
            break;
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: 客户端连接已关闭，没有收到新数据"));
            break;
        }
    }

    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: 客户端连接处理结束"));
}

/**
 * @brief 解析并执行单条消息。
 * @param [in] Client 客户端套接字。
 * @param [in] Message 原始 JSON 文本。
 */
void FMCPServerRunnable::ProcessMessage(TSharedPtr<FSocket> Client, const FString& Message)
{
    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: 开始解析消息，字符数=%d"), Message.Len());

    if (!Bridge)
    {
        UE_LOG(LogTemp, Error, TEXT("MCPServerRunnable: Bridge 无效，无法执行命令"));
        SendErrorResponse(Client, TEXT("MCP Bridge 无效，无法执行命令"));
        return;
    }
    
    // Parse message as JSON
    TSharedPtr<FJsonObject> JsonMessage;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonMessage) || !JsonMessage.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: 消息不是合法 JSON 对象"));
        SendErrorResponse(Client, TEXT("MCP 消息不是合法 JSON 对象"));
        return;
    }
    
    // Extract command type and parameters using MCP protocol format
    FString CommandType;
    TSharedPtr<FJsonObject> Params = MakeShareable(new FJsonObject());
    
    if (!JsonMessage->TryGetStringField(TEXT("type"), CommandType))
    {
        UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: 消息缺少 'type' 字段"));
        SendErrorResponse(Client, TEXT("MCP 消息缺少 type 字段"));
        return;
    }
    
    // Parameters are optional in MCP protocol
    if (JsonMessage->HasField(TEXT("params")))
    {
        TSharedPtr<FJsonValue> ParamsValue = JsonMessage->TryGetField(TEXT("params"));
        if (ParamsValue.IsValid() && ParamsValue->Type == EJson::Object)
        {
            Params = ParamsValue->AsObject();
        }
    }
    
    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: 开始执行命令 %s"), *CommandType);
    
    // Execute command
    FString Response = Bridge->ExecuteCommand(CommandType, Params);
    if (Response.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("MCPServerRunnable: 命令 %s 返回了空响应，已改写为错误响应"), *CommandType);
        Response = BuildErrorResponseString(FString::Printf(TEXT("命令 %s 没有返回有效响应"), *CommandType));
    }
    
    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: 准备发送命令 %s 的响应，字符数=%d"), *CommandType, Response.Len());
    
    if (!SendUtf8Message(Client, Response))
    {
        UE_LOG(LogTemp, Error, TEXT("MCPServerRunnable: 发送命令 %s 的响应失败"), *CommandType);
    }
} 
