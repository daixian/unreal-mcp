#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * @brief Blueprint 资产与组件相关 MCP 命令处理器。
 */
class UNREALMCP_API FUnrealMCPBlueprintCommands
{
public:
    /**
     * @brief 构造函数。
     */
    FUnrealMCPBlueprintCommands();

    /**
     * @brief 分发并执行 Blueprint 相关命令。
     * @param [in] CommandType 命令类型字符串。
     * @param [in] Params 命令参数 JSON 对象。
     * @return TSharedPtr<FJsonObject> 执行结果或错误信息。
     */
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    /**
     * @brief 创建 Blueprint 资产。
     * @param [in] Params 创建参数。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 基于现有 Blueprint 创建子 Blueprint 资产。
     * @param [in] Params 创建参数。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleCreateChildBlueprint(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 Blueprint 添加组件。
     * @param [in] Params 组件参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddComponentToBlueprint(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 从 Blueprint 中删除组件。
     * @param [in] Params 删除参数。
     * @return TSharedPtr<FJsonObject> 删除结果。
     */
    TSharedPtr<FJsonObject> HandleRemoveComponentFromBlueprint(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 调整 Blueprint 组件的父子关系。
     * @param [in] Params 挂接参数。
     * @return TSharedPtr<FJsonObject> 挂接结果。
     */
    TSharedPtr<FJsonObject> HandleAttachComponentInBlueprint(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置 Blueprint 中组件属性。
     * @param [in] Params 属性参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetComponentProperty(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置 Blueprint 组件物理属性。
     * @param [in] Params 物理参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetPhysicsProperties(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 编译 Blueprint。
     * @param [in] Params 编译参数。
     * @return TSharedPtr<FJsonObject> 编译结果。
     */
    TSharedPtr<FJsonObject> HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 批量编译多个 Blueprint。
     * @param [in] Params 批量编译参数。
     * @return TSharedPtr<FJsonObject> 编译结果。
     */
    TSharedPtr<FJsonObject> HandleCompileBlueprints(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 清理 Blueprint 重设父类后残留的组件与成员节点。
     * @param [in] Params 清理参数。
     * @return TSharedPtr<FJsonObject> 清理结果。
     */
    TSharedPtr<FJsonObject> HandleCleanupBlueprintForReparent(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置 Blueprint 默认对象属性。
     * @param [in] Params 属性参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetBlueprintProperty(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置静态网格组件资源属性。
     * @param [in] Params 静态网格参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetStaticMeshProperties(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置 Pawn 相关默认属性。
     * @param [in] Params Pawn 属性参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetPawnProperties(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置 GameMode 的默认 Pawn 类。
     * @param [in] Params GameMode 配置参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetGameModeDefaultPawn(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 添加 Blueprint 成员变量。
     * @param [in] Params 变量参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddBlueprintVariable(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 删除 Blueprint 成员变量。
     * @param [in] Params 删除参数。
     * @return TSharedPtr<FJsonObject> 删除结果。
     */
    TSharedPtr<FJsonObject> HandleDeleteBlueprintVariable(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 清理 Blueprint 中未被引用的成员变量。
     * @param [in] Params 清理参数。
     * @return TSharedPtr<FJsonObject> 清理结果。
     */
    TSharedPtr<FJsonObject> HandleRemoveUnusedBlueprintVariables(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 为 Blueprint 添加接口实现。
     * @param [in] Params 接口参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddBlueprintInterface(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置 Blueprint 成员变量默认值。
     * @param [in] Params 默认值参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetBlueprintVariableDefault(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 为 Blueprint 新增函数图。
     * @param [in] Params 函数参数。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleAddBlueprintFunction(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 删除 Blueprint 中指定函数图。
     * @param [in] Params 函数参数。
     * @return TSharedPtr<FJsonObject> 删除结果。
     */
    TSharedPtr<FJsonObject> HandleDeleteBlueprintFunction(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 获取 Blueprint 最新一次编译的错误与警告摘要。
     * @param [in] Params 查询参数。
     * @return TSharedPtr<FJsonObject> 编译结果与消息列表。
     */
    TSharedPtr<FJsonObject> HandleGetBlueprintCompileErrors(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 重命名 Blueprint 成员。
     * @param [in] Params 重命名参数。
     * @return TSharedPtr<FJsonObject> 重命名结果。
     */
    TSharedPtr<FJsonObject> HandleRenameBlueprintMember(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 保存 Blueprint 资产。
     * @param [in] Params 保存参数。
     * @return TSharedPtr<FJsonObject> 保存结果。
     */
    TSharedPtr<FJsonObject> HandleSaveBlueprint(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 打开 Blueprint 编辑器。
     * @param [in] Params 打开参数。
     * @return TSharedPtr<FJsonObject> 打开结果。
     */
    TSharedPtr<FJsonObject> HandleOpenBlueprintEditor(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 修改 Blueprint 的父类。
     * @param [in] Params 改父类参数。
     * @return TSharedPtr<FJsonObject> 改父类结果。
     */
    TSharedPtr<FJsonObject> HandleReparentBlueprint(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief Blueprint 组件添加辅助函数。
     * @param [in] BlueprintName Blueprint 名称。
     * @param [in] ComponentType 组件类型。
     * @param [in] ComponentName 组件名称。
     * @param [in] MeshType 网格资源类型或路径。
     * @param [in] Location 组件位置数组（XYZ）。
     * @param [in] Rotation 组件旋转数组（Pitch/Yaw/Roll）。
     * @param [in] Scale 组件缩放数组（XYZ）。
     * @param [in] ComponentProperties 额外组件属性。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> AddComponentToBlueprint(const FString& BlueprintName, const FString& ComponentType, 
                                                   const FString& ComponentName, const FString& MeshType,
                                                   const TArray<float>& Location, const TArray<float>& Rotation,
                                                   const TArray<float>& Scale, const TSharedPtr<FJsonObject>& ComponentProperties);
}; 
