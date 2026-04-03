#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * @brief 处理项目级 MCP 命令的处理器。
 */
class UNREALMCP_API FUnrealMCPProjectCommands
{
public:
    /**
     * @brief 构造函数。
     */
    FUnrealMCPProjectCommands();

    /**
     * @brief 分发并执行项目级命令。
     * @param [in] CommandType 命令类型字符串。
     * @param [in] Params 命令参数 JSON 对象。
     * @return TSharedPtr<FJsonObject> 执行结果或错误信息。
     */
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    /**
     * @brief 创建输入映射（Action/Axis）配置。
     * @param [in] Params 输入映射参数。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleCreateInputMapping(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 列出当前项目中的输入映射。
     * @param [in] Params 查询过滤参数。
     * @return TSharedPtr<FJsonObject> 输入映射列表。
     */
    TSharedPtr<FJsonObject> HandleListInputMappings(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 删除输入映射（Action/Axis）。
     * @param [in] Params 删除过滤参数。
     * @return TSharedPtr<FJsonObject> 删除结果。
     */
    TSharedPtr<FJsonObject> HandleRemoveInputMapping(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 创建 Enhanced Input Action 资源。
     * @param [in] Params 创建参数。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleCreateInputActionAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 创建 Input Mapping Context 资源。
     * @param [in] Params 创建参数。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleCreateInputMappingContext(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 往 Input Mapping Context 中添加按键映射。
     * @param [in] Params 添加参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddMappingToContext(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 在运行中的本地玩家上分配 Mapping Context。
     * @param [in] Params 分配参数。
     * @return TSharedPtr<FJsonObject> 分配结果。
     */
    TSharedPtr<FJsonObject> HandleAssignMappingContext(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 读取项目设置对象上的单个属性。
     * @param [in] Params 查询参数。
     * @return TSharedPtr<FJsonObject> 设置值结果。
     */
    TSharedPtr<FJsonObject> HandleGetProjectSetting(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 写入项目设置对象上的单个属性。
     * @param [in] Params 写入参数。
     * @return TSharedPtr<FJsonObject> 写入结果。
     */
    TSharedPtr<FJsonObject> HandleSetProjectSetting(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置项目默认地图相关配置。
     * @param [in] Params 默认地图参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetDefaultMaps(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置项目级 Game Framework 默认类。
     * @param [in] Params Game Framework 参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetGameFrameworkDefaults(const TSharedPtr<FJsonObject>& Params);
};
