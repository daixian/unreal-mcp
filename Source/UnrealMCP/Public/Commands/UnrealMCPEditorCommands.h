#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class AActor;
class UWorld;

/**
 * @brief 编辑器相关 MCP 命令处理器。
 * @note 覆盖关卡管理、Actor 操作、视口控制等常用编辑器动作。
 */
class UNREALMCP_API FUnrealMCPEditorCommands
{
public:
    /**
     * @brief 构造函数。
     */
    FUnrealMCPEditorCommands();

    /**
     * @brief 分发并执行编辑器相关命令。
     * @param [in] CommandType 命令类型字符串。
     * @param [in] Params 命令参数 JSON 对象。
     * @return TSharedPtr<FJsonObject> 执行结果或错误信息。
     */
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    /**
     * @brief 在内容浏览器中创建目录。
     * @param [in] Params 目录创建参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleMakeDirectory(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 复制资产到新路径。
     * @param [in] Params 资产复制参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 加载指定关卡。
     * @param [in] Params 关卡路径参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleLoadLevel(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 保存当前关卡。
     * @param [in] Params 预留参数（当前未使用）。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleSaveCurrentLevel(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 启动 PIE（Play In Editor）会话。
     * @param [in] Params 启动参数（预留扩展）。
     * @return TSharedPtr<FJsonObject> 启动结果。
     */
    TSharedPtr<FJsonObject> HandleStartPIE(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 停止当前 PIE 会话。
     * @param [in] Params 停止参数（当前未使用）。
     * @return TSharedPtr<FJsonObject> 停止结果。
     */
    TSharedPtr<FJsonObject> HandleStopPIE(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 查询当前是否处于 PIE 运行状态。
     * @param [in] Params 查询参数（当前未使用）。
     * @return TSharedPtr<FJsonObject> 运行状态结果。
     */
    TSharedPtr<FJsonObject> HandleGetPlayState(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 获取当前关卡中的全部 Actor。
     * @param [in] Params 查询参数（支持 include_components/detailed_components/world_type）。
     * @return TSharedPtr<FJsonObject> Actor 列表结果。
     */
    TSharedPtr<FJsonObject> HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 按名称模式查找 Actor。
     * @param [in] Params 查询参数（包含 pattern，可选 world_type）。
     * @return TSharedPtr<FJsonObject> 匹配结果。
     */
    TSharedPtr<FJsonObject> HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 在编辑器世界中生成 Actor。
     * @param [in] Params 生成参数（类型、名称、变换等）。
     * @return TSharedPtr<FJsonObject> 生成结果。
     */
    TSharedPtr<FJsonObject> HandleSpawnActor(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 删除指定 Actor。
     * @param [in] Params 删除参数（包含名称）。
     * @return TSharedPtr<FJsonObject> 删除结果。
     */
    TSharedPtr<FJsonObject> HandleDeleteActor(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置 Actor 的位置/旋转/缩放。
     * @param [in] Params 变换参数。
     * @return TSharedPtr<FJsonObject> 更新结果。
     */
    TSharedPtr<FJsonObject> HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 查询 Actor 属性。
     * @param [in] Params 查询参数（支持 actor_path/name/world_type）。
     * @return TSharedPtr<FJsonObject> 属性结果。
     */
    TSharedPtr<FJsonObject> HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 查询单个 Actor 的全部组件信息。
     * @param [in] Params 查询参数（支持 actor_path/name/world_type）。
     * @return TSharedPtr<FJsonObject> 组件结果。
     */
    TSharedPtr<FJsonObject> HandleGetActorComponents(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 查询场景中所有 Actor 的组件信息。
     * @param [in] Params 查询参数（可选 pattern/world_type）。
     * @return TSharedPtr<FJsonObject> 场景组件结果。
     */
    TSharedPtr<FJsonObject> HandleGetSceneComponents(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置 Actor 指定属性值。
     * @param [in] Params 属性设置参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 通过 Blueprint 生成 Actor。
     * @param [in] Params 蓝图生成参数。
     * @return TSharedPtr<FJsonObject> 生成结果。
     */
    TSharedPtr<FJsonObject> HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 聚焦编辑器视口到目标 Actor 或指定位置。
     * @param [in] Params 视口控制参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleFocusViewport(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 对当前视口截图并写入文件。
     * @param [in] Params 截图参数（包含输出路径）。
     * @return TSharedPtr<FJsonObject> 截图结果。
     */
    TSharedPtr<FJsonObject> HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 根据参数查找 Actor。
     * @param [in] Params 查询参数（支持 actor_path/name/world_type）。
     * @param [out] OutErrorMessage 查找失败时的错误信息。
     * @param [out] OutResolvedWorld 可选输出，返回实际使用的 World 指针。
     * @param [out] OutResolvedWorldType 可选输出，返回实际解析出的 world_type。
     * @return AActor* 找到的 Actor，失败返回 nullptr。
     */
    AActor* ResolveActorByParams(
        const TSharedPtr<FJsonObject>& Params,
        FString& OutErrorMessage,
        UWorld** OutResolvedWorld = nullptr,
        FString* OutResolvedWorldType = nullptr
    );
};
