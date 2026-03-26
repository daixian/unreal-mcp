#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * @brief 处理资产查询与摘要相关 MCP 命令的处理器。
 */
class UNREALMCP_API FUnrealMCPAssetCommands
{
public:
    /**
     * @brief 构造函数。
     */
    FUnrealMCPAssetCommands();

    /**
     * @brief 分发并执行资产相关命令。
     * @param [in] CommandType 命令类型字符串。
     * @param [in] Params 命令参数 JSON 对象。
     * @return TSharedPtr<FJsonObject> 执行结果或错误信息。
     */
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    /**
     * @brief 搜索资产。
     * @param [in] Params 搜索参数。
     * @return TSharedPtr<FJsonObject> 搜索结果。
     */
    TSharedPtr<FJsonObject> HandleSearchAssets(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 获取资产元数据。
     * @param [in] Params 查询参数。
     * @return TSharedPtr<FJsonObject> 资产元数据。
     */
    TSharedPtr<FJsonObject> HandleGetAssetMetadata(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 获取资产依赖。
     * @param [in] Params 查询参数。
     * @return TSharedPtr<FJsonObject> 依赖结果。
     */
    TSharedPtr<FJsonObject> HandleGetAssetDependencies(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 获取资产被引用者。
     * @param [in] Params 查询参数。
     * @return TSharedPtr<FJsonObject> 引用者结果。
     */
    TSharedPtr<FJsonObject> HandleGetAssetReferencers(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 获取通用资产摘要。
     * @param [in] Params 查询参数。
     * @return TSharedPtr<FJsonObject> 资产摘要。
     */
    TSharedPtr<FJsonObject> HandleGetAssetSummary(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 获取 Blueprint 资产摘要。
     * @param [in] Params 查询参数。
     * @return TSharedPtr<FJsonObject> Blueprint 摘要。
     */
    TSharedPtr<FJsonObject> HandleGetBlueprintSummary(const TSharedPtr<FJsonObject>& Params);
};
