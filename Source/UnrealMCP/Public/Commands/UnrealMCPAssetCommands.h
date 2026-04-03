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
     * @brief 保存资产。
     * @param [in] Params 查询参数。
     * @return TSharedPtr<FJsonObject> 保存结果。
     */
    TSharedPtr<FJsonObject> HandleSaveAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 从外部文件导入资产。
     * @param [in] Params 导入参数。
     * @return TSharedPtr<FJsonObject> 导入结果。
     */
    TSharedPtr<FJsonObject> HandleImportAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 导出资产到外部目录。
     * @param [in] Params 导出参数。
     * @return TSharedPtr<FJsonObject> 导出结果。
     */
    TSharedPtr<FJsonObject> HandleExportAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 重新导入已有资产。
     * @param [in] Params 重导入参数。
     * @return TSharedPtr<FJsonObject> 重导入结果。
     */
    TSharedPtr<FJsonObject> HandleReimportAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 修复一个或多个路径下的重定向器引用。
     * @param [in] Params 修复参数。
     * @return TSharedPtr<FJsonObject> 修复结果。
     */
    TSharedPtr<FJsonObject> HandleFixupRedirectors(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 重命名资产。
     * @param [in] Params 重命名参数。
     * @return TSharedPtr<FJsonObject> 重命名结果。
     */
    TSharedPtr<FJsonObject> HandleRenameAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 移动资产到新路径。
     * @param [in] Params 移动参数。
     * @return TSharedPtr<FJsonObject> 移动结果。
     */
    TSharedPtr<FJsonObject> HandleMoveAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 删除资产。
     * @param [in] Params 删除参数。
     * @return TSharedPtr<FJsonObject> 删除结果。
     */
    TSharedPtr<FJsonObject> HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 获取当前 Content Browser 选中的资产。
     * @param [in] Params 查询参数。
     * @return TSharedPtr<FJsonObject> 选中资产结果。
     */
    TSharedPtr<FJsonObject> HandleGetSelectedAssets(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 将 Content Browser 同步到指定资产列表。
     * @param [in] Params 同步参数。
     * @return TSharedPtr<FJsonObject> 同步结果。
     */
    TSharedPtr<FJsonObject> HandleSyncContentBrowserToAssets(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 保存全部脏资产和脏地图。
     * @param [in] Params 保存参数。
     * @return TSharedPtr<FJsonObject> 保存结果。
     */
    TSharedPtr<FJsonObject> HandleSaveAllDirtyAssets(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 获取 Blueprint 资产摘要。
     * @param [in] Params 查询参数。
     * @return TSharedPtr<FJsonObject> Blueprint 摘要。
     */
    TSharedPtr<FJsonObject> HandleGetBlueprintSummary(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 创建基础材质资源。
     * @param [in] Params 创建参数（包含 name，可选 path）。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 创建材质实例资源。
     * @param [in] Params 创建参数（包含 name、parent_material，可选 path）。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 获取材质或材质实例的参数摘要。
     * @param [in] Params 查询参数。
     * @return TSharedPtr<FJsonObject> 参数结果。
     */
    TSharedPtr<FJsonObject> HandleGetMaterialParameters(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置材质实例的 Scalar 参数值。
     * @param [in] Params 设置参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetMaterialInstanceScalarParameter(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置材质实例的 Vector 参数值。
     * @param [in] Params 设置参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetMaterialInstanceVectorParameter(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置材质实例的 Texture 参数值。
     * @param [in] Params 设置参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetMaterialInstanceTextureParameter(const TSharedPtr<FJsonObject>& Params);
};
