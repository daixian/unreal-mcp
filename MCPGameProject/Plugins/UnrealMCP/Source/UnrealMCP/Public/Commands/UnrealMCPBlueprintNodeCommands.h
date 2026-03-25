#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * @brief Blueprint 节点图相关 MCP 命令处理器。
 */
class UNREALMCP_API FUnrealMCPBlueprintNodeCommands
{
public:
    /**
     * @brief 构造函数。
     */
    FUnrealMCPBlueprintNodeCommands();

    /**
     * @brief 分发并执行 Blueprint 节点命令。
     * @param [in] CommandType 命令类型字符串。
     * @param [in] Params 命令参数 JSON 对象。
     * @return TSharedPtr<FJsonObject> 执行结果或错误信息。
     */
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    /**
     * @brief 连接 Blueprint 图节点。
     * @param [in] Params 节点连接参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleConnectBlueprintNodes(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 添加当前 Blueprint 的组件引用节点。
     * @param [in] Params 组件引用参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleAddBlueprintGetSelfComponentReference(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 添加 Blueprint 事件节点。
     * @param [in] Params 事件节点参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleAddBlueprintEvent(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 添加 Blueprint 函数调用节点。
     * @param [in] Params 函数调用参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleAddBlueprintFunctionCall(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 添加 Blueprint 变量定义。
     * @param [in] Params 变量参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleAddBlueprintVariable(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 添加输入动作节点。
     * @param [in] Params 输入动作参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleAddBlueprintInputActionNode(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 添加 Self 引用节点。
     * @param [in] Params Self 节点参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleAddBlueprintSelfReference(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 查找 Blueprint 图中的节点。
     * @param [in] Params 查询参数。
     * @return TSharedPtr<FJsonObject> 查询结果。
     */
    TSharedPtr<FJsonObject> HandleFindBlueprintNodes(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 配置 ZSpace 最小交互节点链路。
     * @param [in] Params 配置参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleSetupZSpaceMinimalInteraction(const TSharedPtr<FJsonObject>& Params);
};
