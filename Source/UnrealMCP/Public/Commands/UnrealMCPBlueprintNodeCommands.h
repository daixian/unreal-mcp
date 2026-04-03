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
     * @brief 断开 Blueprint 图节点引脚连接。
     * @param [in] Params 断开连接参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleDisconnectBlueprintNodes(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 移动 Blueprint 图中的单个节点。
     * @param [in] Params 节点移动参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleMoveBlueprintNode(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 批量布局 Blueprint 图节点。
     * @param [in] Params 布局参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleLayoutBlueprintNodes(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 添加 Blueprint 注释节点。
     * @param [in] Params 注释节点参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleAddBlueprintCommentNode(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 添加 Blueprint Reroute 节点。
     * @param [in] Params Reroute 节点参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleAddBlueprintRerouteNode(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 新建 Blueprint 图。
     * @param [in] Params 图创建参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleCreateBlueprintGraph(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 删除 Blueprint 图。
     * @param [in] Params 图删除参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleDeleteBlueprintGraph(const TSharedPtr<FJsonObject>& Params);

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
     * @brief 以通用方式生成 Blueprint 图节点。
     * @param [in] Params 节点生成参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleSpawnBlueprintNode(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 描述 Blueprint 图中的单个节点及其引脚。
     * @param [in] Params 节点查询参数。
     * @return TSharedPtr<FJsonObject> 查询结果。
     */
    TSharedPtr<FJsonObject> HandleDescribeBlueprintNode(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置 Blueprint 节点引脚默认值。
     * @param [in] Params 默认值参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleSetBlueprintPinDefault(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 删除 Blueprint 图中的单个节点。
     * @param [in] Params 节点删除参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleDeleteBlueprintNode(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 复制 Blueprint 节点子图。
     * @param [in] Params 节点复制参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleDuplicateBlueprintSubgraph(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 将选中节点折叠为函数图。
     * @param [in] Params 节点折叠参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleCollapseNodesToFunction(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 将选中节点折叠为宏图。
     * @param [in] Params 节点折叠参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleCollapseNodesToMacro(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 校验 Blueprint 图是否可编译。
     * @param [in] Params 图校验参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleValidateBlueprintGraph(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 查找 Blueprint 图中的节点。
     * @param [in] Params 查询参数。
     * @return TSharedPtr<FJsonObject> 查询结果。
     */
    TSharedPtr<FJsonObject> HandleFindBlueprintNodes(const TSharedPtr<FJsonObject>& Params);

};
