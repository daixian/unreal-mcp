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
     * @brief 使用通用工厂创建任意资产。
     * @param [in] Params 创建参数。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleCreateAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 保存资产。
     * @param [in] Params 查询参数。
     * @return TSharedPtr<FJsonObject> 保存结果。
     */
    TSharedPtr<FJsonObject> HandleSaveAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 签出资产。
     * @param [in] Params 查询参数。
     * @return TSharedPtr<FJsonObject> 签出结果。
     */
    TSharedPtr<FJsonObject> HandleCheckoutAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 提交资产到源码控制。
     * @param [in] Params 提交参数。
     * @return TSharedPtr<FJsonObject> 提交结果。
     */
    TSharedPtr<FJsonObject> HandleSubmitAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 还原资产在源码控制中的改动。
     * @param [in] Params 还原参数。
     * @return TSharedPtr<FJsonObject> 还原结果。
     */
    TSharedPtr<FJsonObject> HandleRevertAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 查询资产的源码控制状态。
     * @param [in] Params 查询参数。
     * @return TSharedPtr<FJsonObject> 源码控制状态结果。
     */
    TSharedPtr<FJsonObject> HandleGetSourceControlStatus(const TSharedPtr<FJsonObject>& Params);

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
     * @brief 批量重命名资产。
     * @param [in] Params 批量改名参数。
     * @return TSharedPtr<FJsonObject> 批量改名结果。
     */
    TSharedPtr<FJsonObject> HandleBatchRenameAssets(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 批量移动资产到同一目录。
     * @param [in] Params 批量移动参数。
     * @return TSharedPtr<FJsonObject> 批量移动结果。
     */
    TSharedPtr<FJsonObject> HandleBatchMoveAssets(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置资产元数据标签。
     * @param [in] Params 写入参数。
     * @return TSharedPtr<FJsonObject> 写入结果。
     */
    TSharedPtr<FJsonObject> HandleSetAssetMetadata(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 将多个重复资产合并到目标资产。
     * @param [in] Params 合并参数。
     * @return TSharedPtr<FJsonObject> 合并结果。
     */
    TSharedPtr<FJsonObject> HandleConsolidateAssets(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 使用 UE 的 Replace References 工作流批量替换引用。
     * @param [in] Params 替换参数。
     * @return TSharedPtr<FJsonObject> 替换结果。
     */
    TSharedPtr<FJsonObject> HandleReplaceAssetReferences(const TSharedPtr<FJsonObject>& Params);

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
     * @brief 创建 DataAsset 资源实例。
     * @param [in] Params 创建参数（包含 data_asset_name/data_asset_class，可选 path）。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleCreateDataAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 创建 PrimaryDataAsset 资源实例。
     * @param [in] Params 创建参数（包含 data_asset_name/data_asset_class，可选 path）。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleCreatePrimaryDataAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 创建曲线资源。
     * @param [in] Params 创建参数（包含 curve_name/curve_type，可选 path）。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleCreateCurve(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 创建 DataTable 资源。
     * @param [in] Params 创建参数（包含 table_name/row_struct，可选 path）。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleCreateDataTable(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 读取 DataTable 的一行或全部行。
     * @param [in] Params 查询参数（资产定位字段，和可选 row_name）。
     * @return TSharedPtr<FJsonObject> 读取结果。
     */
    TSharedPtr<FJsonObject> HandleGetDataTableRows(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 从 CSV 或 JSON 文件导入 DataTable 内容。
     * @param [in] Params 导入参数（资产定位字段、source_file，可选 format/save_asset）。
     * @return TSharedPtr<FJsonObject> 导入结果。
     */
    TSharedPtr<FJsonObject> HandleImportDataTable(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 新增或覆盖 DataTable 的单行数据。
     * @param [in] Params 写入参数（资产定位字段、row_name、row_data）。
     * @return TSharedPtr<FJsonObject> 写入结果。
     */
    TSharedPtr<FJsonObject> HandleSetDataTableRow(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 把 DataTable 内容导出为 CSV 或 JSON 文件。
     * @param [in] Params 导出参数（资产定位字段、export_file，可选 format）。
     * @return TSharedPtr<FJsonObject> 导出结果。
     */
    TSharedPtr<FJsonObject> HandleExportDataTable(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 删除 DataTable 的单行数据。
     * @param [in] Params 删除参数（资产定位字段和 row_name）。
     * @return TSharedPtr<FJsonObject> 删除结果。
     */
    TSharedPtr<FJsonObject> HandleRemoveDataTableRow(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 创建基础材质资源。
     * @param [in] Params 创建参数（包含 name，可选 path）。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 创建材质函数资源。
     * @param [in] Params 创建参数（包含 name，可选 path、function_class）。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleCreateMaterialFunction(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 创建渲染目标资源。
     * @param [in] Params 创建参数（包含 name，可选 path、width、height、format、clear_color）。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleCreateRenderTarget(const TSharedPtr<FJsonObject>& Params);

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

    /**
     * @brief 给 Actor 上全部可写材质槽的 MeshComponent 赋材质。
     * @param [in] Params 设置参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleAssignMaterialToActor(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 给 Actor 的指定 MeshComponent 赋材质。
     * @param [in] Params 设置参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleAssignMaterialToComponent(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 替换 Actor 或指定组件上的某个材质槽。
     * @param [in] Params 替换参数。
     * @return TSharedPtr<FJsonObject> 替换结果。
     */
    TSharedPtr<FJsonObject> HandleReplaceMaterialSlot(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向材质图中添加一个材质表达式节点。
     * @param [in] Params 创建参数。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleAddMaterialExpression(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 连接两个材质表达式，或把表达式输出连接到材质属性。
     * @param [in] Params 连接参数。
     * @return TSharedPtr<FJsonObject> 连接结果。
     */
    TSharedPtr<FJsonObject> HandleConnectMaterialExpressions(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 自动整理材质图中表达式节点布局。
     * @param [in] Params 整理参数。
     * @return TSharedPtr<FJsonObject> 整理结果。
     */
    TSharedPtr<FJsonObject> HandleLayoutMaterialGraph(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 触发材质重新编译并保存。
     * @param [in] Params 编译参数。
     * @return TSharedPtr<FJsonObject> 编译结果。
     */
    TSharedPtr<FJsonObject> HandleCompileMaterial(const TSharedPtr<FJsonObject>& Params);
};
