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
     * @brief 启动 VR Preview 会话。
     * @param [in] Params 启动参数（当前预留扩展）。
     * @return TSharedPtr<FJsonObject> 启动结果。
     */
    TSharedPtr<FJsonObject> HandleStartVRPreview(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 启动 Standalone Game（新进程）会话。
     * @param [in] Params 启动参数（支持 map_override/additional_command_line_parameters）。
     * @return TSharedPtr<FJsonObject> 启动结果。
     */
    TSharedPtr<FJsonObject> HandleStartStandaloneGame(const TSharedPtr<FJsonObject>& Params);

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
     * @brief 启用当前编辑器会话的 Live Coding。
     * @param [in] Params 启动参数（支持 show_console）。
     * @return TSharedPtr<FJsonObject> 启动结果。
     */
    TSharedPtr<FJsonObject> HandleStartLiveCoding(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 触发一次 Live Coding 编译。
     * @param [in] Params 编译参数（支持 wait_for_completion/show_console）。
     * @return TSharedPtr<FJsonObject> 编译请求结果。
     */
    TSharedPtr<FJsonObject> HandleCompileLiveCoding(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 查询 Live Coding 的当前状态。
     * @param [in] Params 查询参数（当前未使用）。
     * @return TSharedPtr<FJsonObject> 状态结果。
     */
    TSharedPtr<FJsonObject> HandleGetLiveCodingState(const TSharedPtr<FJsonObject>& Params);

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
     * @brief 按多种过滤条件查找 Actor。
     * @param [in] Params 查询参数（支持 class/tag/folder/path/data_layer 等条件）。
     * @return TSharedPtr<FJsonObject> 匹配结果。
     */
    TSharedPtr<FJsonObject> HandleFindActors(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 在编辑器世界中生成 Actor。
     * @param [in] Params 生成参数（类型、名称、变换等）。
     * @return TSharedPtr<FJsonObject> 生成结果。
     */
    TSharedPtr<FJsonObject> HandleSpawnActor(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 按类路径在关卡中生成 Actor。
     * @param [in] Params 生成参数（包含 class_path、actor_name、变换等）。
     * @return TSharedPtr<FJsonObject> 生成结果。
     */
    TSharedPtr<FJsonObject> HandleSpawnActorFromClass(const TSharedPtr<FJsonObject>& Params);

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
     * @brief 读取当前 World 的 WorldSettings 常用属性。
     * @param [in] Params 查询参数（支持 world_type、property_names）。
     * @return TSharedPtr<FJsonObject> WorldSettings 属性结果。
     */
    TSharedPtr<FJsonObject> HandleGetWorldSettings(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 写入当前 World 的 WorldSettings 属性。
     * @param [in] Params 更新参数（支持 world_type、settings）。
     * @return TSharedPtr<FJsonObject> WorldSettings 更新结果。
     */
    TSharedPtr<FJsonObject> HandleSetWorldSettings(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置 Actor 指定属性值。
     * @param [in] Params 属性设置参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置 Actor 的标签列表。
     * @param [in] Params 标签设置参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetActorTags(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置 Actor 的文件夹路径。
     * @param [in] Params 文件夹路径设置参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetActorFolderPath(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置 Actor 的可见性状态。
     * @param [in] Params 可见性设置参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetActorVisibility(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置 Actor 根组件的 Mobility。
     * @param [in] Params Mobility 设置参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetActorMobility(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 通过 Blueprint 生成 Actor。
     * @param [in] Params 蓝图生成参数。
     * @return TSharedPtr<FJsonObject> 生成结果。
     */
    TSharedPtr<FJsonObject> HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 复制指定 Actor。
     * @param [in] Params 复制参数。
     * @return TSharedPtr<FJsonObject> 复制结果。
     */
    TSharedPtr<FJsonObject> HandleDuplicateActor(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 选中指定 Actor。
     * @param [in] Params 选中参数。
     * @return TSharedPtr<FJsonObject> 选中结果。
     */
    TSharedPtr<FJsonObject> HandleSelectActor(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 获取当前选中的 Actor 列表。
     * @param [in] Params 查询参数。
     * @return TSharedPtr<FJsonObject> 选中 Actor 结果。
     */
    TSharedPtr<FJsonObject> HandleGetSelectedActors(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 获取编辑器当前选中的 Actor 与资产。
     * @param [in] Params 查询参数（支持 include_components/detailed_components/include_tags）。
     * @return TSharedPtr<FJsonObject> 聚合后的选中结果。
     */
    TSharedPtr<FJsonObject> HandleGetEditorSelection(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 创建一个灯光 Actor，并可在创建时写入常用灯光属性。
     * @param [in] Params 创建参数（包含 actor_name、light_type，可选位置、旋转和灯光属性）。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleCreateLight(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 修改已有灯光 Actor 的常用灯光属性。
     * @param [in] Params 更新参数（支持 name/actor_path/world_type 与一组可选灯光属性）。
     * @return TSharedPtr<FJsonObject> 更新结果。
     */
    TSharedPtr<FJsonObject> HandleSetLightProperties(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 将场景捕捉到指定 RenderTarget。
     * @param [in] Params 捕捉参数（包含 render_target_asset_path，可选 SceneCapture2D 与捕捉配置）。
     * @return TSharedPtr<FJsonObject> 捕捉结果。
     */
    TSharedPtr<FJsonObject> HandleCaptureSceneToRenderTarget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 修改 PostProcessVolume 的常用参数与 PostProcessSettings。
     * @param [in] Params 更新参数（支持 name/actor_path/world_type、Volume 属性与 settings 字典）。
     * @return TSharedPtr<FJsonObject> 更新结果。
     */
    TSharedPtr<FJsonObject> HandleSetPostProcessSettings(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 将子 Actor 挂接到父 Actor。
     * @param [in] Params 挂接参数。
     * @return TSharedPtr<FJsonObject> 挂接结果。
     */
    TSharedPtr<FJsonObject> HandleAttachActor(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 将 Actor 从父 Actor 分离。
     * @param [in] Params 分离参数。
     * @return TSharedPtr<FJsonObject> 分离结果。
     */
    TSharedPtr<FJsonObject> HandleDetachActor(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 批量设置多个 Actor 的 Transform。
     * @param [in] Params 批量变换参数。
     * @return TSharedPtr<FJsonObject> 更新结果。
     */
    TSharedPtr<FJsonObject> HandleSetActorsTransform(const TSharedPtr<FJsonObject>& Params);

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
     * @brief 请求一张高分辨率视口截图。
     * @param [in] Params 截图参数（包含输出路径、分辨率等）。
     * @return TSharedPtr<FJsonObject> 请求结果。
     */
    TSharedPtr<FJsonObject> HandleTakeHighResScreenshot(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 以连续帧的形式捕获当前视口。
     * @param [in] Params 序列捕获参数（包含输出目录、帧数、间隔等）。
     * @return TSharedPtr<FJsonObject> 请求结果。
     */
    TSharedPtr<FJsonObject> HandleCaptureViewportSequence(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 打开指定资产的编辑器。
     * @param [in] Params 打开参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleOpenAssetEditor(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 关闭指定资产的编辑器标签。
     * @param [in] Params 关闭参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleCloseAssetEditor(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 执行 Unreal 控制台命令。
     * @param [in] Params 执行参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleExecuteConsoleCommand(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 执行 Unreal Python 命令或脚本。
     * @param [in] Params 执行参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleExecuteUnrealPython(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 运行 Editor Utility Widget 并打开对应标签页。
     * @param [in] Params 运行参数（包含 asset_path，可选 tab_id）。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleRunEditorUtilityWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 运行 Editor Utility Blueprint 的 Run 入口。
     * @param [in] Params 运行参数（包含 asset_path）。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleRunEditorUtilityBlueprint(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置关卡视口显示模式。
     * @param [in] Params 设置参数（包含 view_mode，可选 apply_to_all）。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleSetViewportMode(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 获取当前活动关卡视口的摄像机信息。
     * @param [in] Params 查询参数（当前预留 apply_to_all 等扩展）。
     * @return TSharedPtr<FJsonObject> 视口摄像机结果。
     */
    TSharedPtr<FJsonObject> HandleGetViewportCamera(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 读取插件内部捕获的输出日志。
     * @param [in] Params 查询参数。
     * @return TSharedPtr<FJsonObject> 日志结果。
     */
    TSharedPtr<FJsonObject> HandleGetOutputLog(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 清空插件内部捕获的输出日志缓冲。
     * @param [in] Params 清空参数（当前未使用）。
     * @return TSharedPtr<FJsonObject> 清空结果。
     */
    TSharedPtr<FJsonObject> HandleClearOutputLog(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 读取 Message Log 指定列表中的消息。
     * @param [in] Params 查询参数。
     * @return TSharedPtr<FJsonObject> 日志结果。
     */
    TSharedPtr<FJsonObject> HandleGetMessageLog(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 发送一条编辑器通知。
     * @param [in] Params 通知参数。
     * @return TSharedPtr<FJsonObject> 执行结果。
     */
    TSharedPtr<FJsonObject> HandleShowEditorNotification(const TSharedPtr<FJsonObject>& Params);

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
