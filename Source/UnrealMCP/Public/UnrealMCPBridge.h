#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Http.h"
#include "Json.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Commands/UnrealMCPEditorCommands.h"
#include "Commands/UnrealMCPAssetCommands.h"
#include "Commands/UnrealMCPBlueprintCommands.h"
#include "Commands/UnrealMCPBlueprintNodeCommands.h"
#include "Commands/UnrealMCPProjectCommands.h"
#include "Commands/UnrealMCPUMGCommands.h"
#include "UnrealMCPBridge.generated.h"

class FMCPServerRunnable;

/**
 * @brief Unreal 编辑器中的 MCP 桥接子系统。
 * @note 该子系统通过 TCP 接收 JSON 命令，并将命令路由到对应的命令处理器。
 */
UCLASS()
class UNREALMCP_API UUnrealMCPBridge : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * @brief 构造函数，初始化命令处理器对象。
	 */
	UUnrealMCPBridge();

	/**
	 * @brief 析构函数，释放命令处理器对象。
	 */
	virtual ~UUnrealMCPBridge();

	/**
	 * @brief 子系统初始化入口。
	 * @param [in] Collection 子系统集合，用于注册依赖关系。
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * @brief 子系统反初始化入口。
	 */
	virtual void Deinitialize() override;

	/**
	 * @brief 启动 MCP TCP 监听服务。
	 */
	void StartServer();

	/**
	 * @brief 停止 MCP TCP 监听服务。
	 */
	void StopServer();

	/**
	 * @brief 查询服务器运行状态。
	 * @return bool 正在运行返回 true，否则返回 false。
	 */
	bool IsRunning() const { return bIsRunning; }

	/**
	 * @brief 执行单条 MCP 命令并返回 JSON 字符串结果。
	 * @param [in] CommandType 命令类型字符串。
	 * @param [in] Params 命令参数 JSON 对象。
	 * @return FString 序列化后的 JSON 响应。
	 */
	FString ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	/** @brief TCP 服务是否处于运行状态。 */
	bool bIsRunning;
	/** @brief 监听客户端连接的服务端套接字。 */
	TSharedPtr<FSocket> ListenerSocket;
	/** @brief 当前连接套接字（保留字段，后续可用于连接级控制）。 */
	TSharedPtr<FSocket> ConnectionSocket;
	/** @brief MCP 服务线程执行体。 */
	FMCPServerRunnable* ServerRunnable;
	/** @brief 承载 socket 收发循环的后台线程。 */
	FRunnableThread* ServerThread;

	/** @brief 服务器绑定的 IPv4 地址。 */
	FIPv4Address ServerAddress;
	/** @brief 服务器监听端口。 */
	uint16 Port;

	/** @brief 编辑器命令处理器实例。 */
	TSharedPtr<FUnrealMCPEditorCommands> EditorCommands;
	/** @brief 资产命令处理器实例。 */
	TSharedPtr<FUnrealMCPAssetCommands> AssetCommands;
	/** @brief Blueprint 资产命令处理器实例。 */
	TSharedPtr<FUnrealMCPBlueprintCommands> BlueprintCommands;
	/** @brief Blueprint 节点图命令处理器实例。 */
	TSharedPtr<FUnrealMCPBlueprintNodeCommands> BlueprintNodeCommands;
	/** @brief 项目级命令处理器实例。 */
	TSharedPtr<FUnrealMCPProjectCommands> ProjectCommands;
	/** @brief UMG 命令处理器实例。 */
	TSharedPtr<FUnrealMCPUMGCommands> UMGCommands;
};
