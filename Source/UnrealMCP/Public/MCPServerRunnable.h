#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Sockets.h"
#include "Interfaces/IPv4/IPv4Address.h"

class UUnrealMCPBridge;

/**
 * @brief MCP socket 服务线程执行体。
 * @note 负责接收客户端 JSON 消息、调用桥接层执行命令并回包。
 */
class FMCPServerRunnable : public FRunnable
{
public:
	/**
	 * @brief 构造函数。
	 * @param [in] InBridge 命令执行桥接对象。
	 * @param [in] InListenerSocket 已创建并监听中的套接字。
	 */
	FMCPServerRunnable(UUnrealMCPBridge* InBridge, TSharedPtr<FSocket> InListenerSocket);

	/**
	 * @brief 析构函数。
	 */
	virtual ~FMCPServerRunnable();

	/**
	 * @brief 线程初始化回调。
	 * @return bool 初始化成功返回 true。
	 */
	virtual bool Init() override;

	/**
	 * @brief 线程主循环，处理连接与消息收发。
	 * @return uint32 线程退出码。
	 */
	virtual uint32 Run() override;

	/**
	 * @brief 请求线程停止。
	 */
	virtual void Stop() override;

	/**
	 * @brief 线程退出回调。
	 */
	virtual void Exit() override;

protected:
	/**
	 * @brief 处理单个客户端连接生命周期。
	 * @param [in] ClientSocket 客户端连接套接字。
	 */
	void HandleClientConnection(TSharedPtr<FSocket> ClientSocket);

	/**
	 * @brief 解析并处理单条 JSON 消息。
	 * @param [in] Client 客户端连接套接字。
	 * @param [in] Message 收到的 JSON 文本。
	 */
	void ProcessMessage(TSharedPtr<FSocket> Client, const FString& Message);

private:
	/** @brief 命令执行桥接对象（不拥有生命周期）。 */
	UUnrealMCPBridge* Bridge;
	/** @brief 服务监听套接字。 */
	TSharedPtr<FSocket> ListenerSocket;
	/** @brief 当前活跃客户端套接字。 */
	TSharedPtr<FSocket> ClientSocket;
	/** @brief 线程运行标记，false 时退出循环。 */
	bool bRunning;
};
