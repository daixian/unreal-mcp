/**
 * @file MCPServerRunnable.cpp
 * @brief MCP socket 服务线程实现，负责连接管理与消息收发。
 */
#include "MCPServerRunnable.h"
#include "UnrealMCPBridge.h"
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

    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: 响应发送完成，字节数: %d"), TotalSent);
    return true;
}

static bool TryFindNextJsonMessageEnd(const TArray<uint8>& Buffer, int32& OutMessageEndExclusive)
{
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
                UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: 收到非法 JSON 起始字节: 0x%02X"), Byte);
                return false;
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
                return true;
            }
        }
    }

    return false;
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
    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Server thread starting..."));
    
    while (bRunning)
    {
        // UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Waiting for client connection..."));
        
        bool bPending = false;
        if (ListenerSocket->HasPendingConnection(bPending) && bPending)
        {
            UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Client connection pending, accepting..."));
            
            ClientSocket = MakeShareable(ListenerSocket->Accept(TEXT("MCPClient")));
            if (ClientSocket.IsValid())
            {
                UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Client connection accepted"));
                
                // Set socket options to improve connection stability
                ClientSocket->SetNoDelay(true);
                ClientSocket->SetNonBlocking(false);
                int32 SocketBufferSize = 65536;  // 64KB buffer
                ClientSocket->SetSendBufferSize(SocketBufferSize, SocketBufferSize);
                ClientSocket->SetReceiveBufferSize(SocketBufferSize, SocketBufferSize);
                HandleClientConnection(ClientSocket);
                ClientSocket->Close();
                ClientSocket.Reset();
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Failed to accept client connection"));
            }
        }
        
        // Small sleep to prevent tight loop
        FPlatformProcess::Sleep(0.1f);
    }
    
    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Server thread stopping"));
    return 0;
}

/**
 * @brief 请求停止线程主循环。
 */
void FMCPServerRunnable::Stop()
{
    bRunning = false;
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
        UE_LOG(LogTemp, Error, TEXT("MCPServerRunnable: Invalid client socket passed to HandleClientConnection"));
        return;
    }

    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: 开始处理客户端连接"));

    uint8 Buffer[MCPReceiveBufferSize];
    TArray<uint8> PendingBytes;
    PendingBytes.Reserve(MCPReceiveBufferSize);

    while (bRunning && InClientSocket.IsValid())
    {
        int32 BytesRead = 0;
        const bool bReadSuccess = InClientSocket->Recv(Buffer, MCPReceiveBufferSize, BytesRead, ESocketReceiveFlags::None);

        if (BytesRead > 0)
        {
            PendingBytes.Append(Buffer, BytesRead);
            UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: 本次收到 %d 字节，累计缓冲区 %d 字节"), BytesRead, PendingBytes.Num());

            int32 MessageEndExclusive = 0;
            while (TryFindNextJsonMessageEnd(PendingBytes, MessageEndExclusive))
            {
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
    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Processing message: %s"), *Message);
    
    // Parse message as JSON
    TSharedPtr<FJsonObject> JsonMessage;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonMessage) || !JsonMessage.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: Failed to parse message as JSON"));
        return;
    }
    
    // Extract command type and parameters using MCP protocol format
    FString CommandType;
    TSharedPtr<FJsonObject> Params = MakeShareable(new FJsonObject());
    
    if (!JsonMessage->TryGetStringField(TEXT("type"), CommandType))
    {
        UE_LOG(LogTemp, Warning, TEXT("MCPServerRunnable: 消息缺少 'type' 字段"));
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
    
    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Executing command: %s"), *CommandType);
    
    // Execute command
    FString Response = Bridge->ExecuteCommand(CommandType, Params);
    
    UE_LOG(LogTemp, Display, TEXT("MCPServerRunnable: Sending response: %s"), *Response);
    
    if (!SendUtf8Message(Client, Response))
    {
        UE_LOG(LogTemp, Error, TEXT("MCPServerRunnable: Failed to send response"));
    }
} 
