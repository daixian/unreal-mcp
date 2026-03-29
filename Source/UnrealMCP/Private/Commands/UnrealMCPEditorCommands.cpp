/**
 * @file UnrealMCPEditorCommands.cpp
 * @brief 编辑器命令处理实现，覆盖关卡、Actor 与视口操作。
 */
#include "Commands/UnrealMCPEditorCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorSubsystem.h"
#include "LevelEditorViewport.h"
#include "ImageUtils.h"
#include "HighResScreenshot.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "Misc/EngineVersionComparison.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "PlayInEditorDataTypes.h"
#include "EditorAssetLibrary.h"

#if UE_VERSION_OLDER_THAN(5, 5, 0)
#include "EditorLevelLibrary.h"
#endif

namespace UnrealMCPEditorCommandsPrivate
{
    /**
     * @brief 将 EWorldType 枚举转换为字符串。
     * @param [in] WorldType 世界类型枚举值。
     * @return FString 便于序列化的世界类型字符串。
     */
    FString WorldTypeToString(EWorldType::Type WorldType)
    {
        switch (WorldType)
        {
            case EWorldType::Editor:
                return TEXT("Editor");
            case EWorldType::EditorPreview:
                return TEXT("EditorPreview");
            case EWorldType::PIE:
                return TEXT("PIE");
            case EWorldType::Game:
                return TEXT("Game");
            case EWorldType::GamePreview:
                return TEXT("GamePreview");
            case EWorldType::Inactive:
                return TEXT("Inactive");
            case EWorldType::None:
            default:
                return TEXT("None");
        }
    }

    /**
     * @brief 解析命令参数中的世界选择偏好。
     * @param [in] Params 命令参数。
     * @return FString 归一化后的世界类型（auto/editor/pie）。
     * @note 兼容旧参数 use_pie_world；若存在则优先使用该布尔开关。
     */
    FString GetRequestedWorldType(const TSharedPtr<FJsonObject>& Params)
    {
        bool bUsePIEWorld = false;
        if (Params.IsValid() && Params->TryGetBoolField(TEXT("use_pie_world"), bUsePIEWorld))
        {
            return bUsePIEWorld ? TEXT("pie") : TEXT("editor");
        }

        FString RequestedWorldType = TEXT("auto");
        if (Params.IsValid())
        {
            Params->TryGetStringField(TEXT("world_type"), RequestedWorldType);
        }

        RequestedWorldType = RequestedWorldType.TrimStartAndEnd().ToLower();
        if (RequestedWorldType.IsEmpty())
        {
            RequestedWorldType = TEXT("auto");
        }
        return RequestedWorldType;
    }

    /**
     * @brief 获取编辑器世界。
     * @return UWorld* 编辑器世界；不存在时返回 nullptr。
     */
    UWorld* GetEditorWorld()
    {
        return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    }

    /**
     * @brief 获取当前 PIE 运行时世界。
     * @return UWorld* PIE 世界；未运行 PIE 时返回 nullptr。
     */
    UWorld* GetPIEWorld()
    {
        if (GEditor && GEditor->PlayWorld)
        {
            return GEditor->PlayWorld;
        }

        if (!GEngine)
        {
            return nullptr;
        }

        for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
        {
            if ((WorldContext.WorldType == EWorldType::PIE || WorldContext.WorldType == EWorldType::GamePreview) &&
                WorldContext.World() != nullptr)
            {
                return WorldContext.World();
            }
        }
        return nullptr;
    }

    /**
     * @brief 按参数选择目标世界（支持 auto/editor/pie）。
     * @param [in] Params 命令参数。
     * @param [out] OutResolvedWorldType 实际解析得到的 world_type（auto 可能落到 editor/pie）。
     * @param [out] OutErrorMessage 失败时的错误描述。
     * @return UWorld* 解析出的目标世界；失败返回 nullptr。
     */
    UWorld* ResolveWorldByParams(
        const TSharedPtr<FJsonObject>& Params,
        FString& OutResolvedWorldType,
        FString& OutErrorMessage
    )
    {
        const FString RequestedWorldType = GetRequestedWorldType(Params);

        if (RequestedWorldType == TEXT("editor"))
        {
            UWorld* EditorWorld = GetEditorWorld();
            if (!EditorWorld)
            {
                OutErrorMessage = TEXT("Failed to get editor world");
                return nullptr;
            }
            OutResolvedWorldType = TEXT("editor");
            return EditorWorld;
        }

        if (RequestedWorldType == TEXT("pie"))
        {
            UWorld* PIEWorld = GetPIEWorld();
            if (!PIEWorld)
            {
                OutErrorMessage = TEXT("PIE world is not running. Start Play-In-Editor first.");
                return nullptr;
            }
            OutResolvedWorldType = TEXT("pie");
            return PIEWorld;
        }

        if (RequestedWorldType == TEXT("auto"))
        {
            if (UWorld* PIEWorld = GetPIEWorld())
            {
                OutResolvedWorldType = TEXT("pie");
                return PIEWorld;
            }

            if (UWorld* EditorWorld = GetEditorWorld())
            {
                OutResolvedWorldType = TEXT("editor");
                return EditorWorld;
            }

            OutErrorMessage = TEXT("Failed to resolve world in auto mode (both PIE and Editor worlds are unavailable)");
            return nullptr;
        }

        OutErrorMessage = FString::Printf(
            TEXT("Invalid 'world_type': %s. Valid values are: auto, editor, pie"),
            *RequestedWorldType
        );
        return nullptr;
    }

    /**
     * @brief 向响应对象写入世界元信息，便于客户端确认查询来源。
     * @param [in,out] ResultObj 响应 JSON 对象。
     * @param [in] World 实际使用的世界。
     * @param [in] ResolvedWorldType 解析后的 world_type（editor/pie）。
     */
    void AppendWorldInfo(const TSharedPtr<FJsonObject>& ResultObj, UWorld* World, const FString& ResolvedWorldType)
    {
        if (!ResultObj.IsValid() || !World)
        {
            return;
        }

        ResultObj->SetStringField(TEXT("resolved_world_type"), ResolvedWorldType);
        ResultObj->SetStringField(TEXT("world_type"), WorldTypeToString(World->WorldType));
        ResultObj->SetStringField(TEXT("world_name"), World->GetName());
        ResultObj->SetStringField(TEXT("world_path"), World->GetPathName());
    }

    /**
     * @brief 使用兼容版本的 API 加载关卡。
     * @param [in] LevelPath 关卡资源路径。
     * @return bool 加载成功返回 true，否则返回 false。
     */
    bool LoadLevelCompat(const FString& LevelPath)
    {
#if UE_VERSION_OLDER_THAN(5, 5, 0)
        return UEditorLevelLibrary::LoadLevel(LevelPath);
#else
        ULevelEditorSubsystem* LevelEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<ULevelEditorSubsystem>() : nullptr;
        return LevelEditorSubsystem ? LevelEditorSubsystem->LoadLevel(LevelPath) : false;
#endif
    }

    /**
     * @brief 使用兼容版本的 API 保存当前关卡。
     * @return bool 保存成功返回 true，否则返回 false。
     */
    bool SaveCurrentLevelCompat()
    {
#if UE_VERSION_OLDER_THAN(5, 5, 0)
        return UEditorLevelLibrary::SaveCurrentLevel();
#else
        ULevelEditorSubsystem* LevelEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<ULevelEditorSubsystem>() : nullptr;
        return LevelEditorSubsystem ? LevelEditorSubsystem->SaveCurrentLevel() : false;
#endif
    }

    /**
     * @brief 使用兼容版本的 API 进行 PNG 压缩。
     * @param [in] Width 图像宽度。
     * @param [in] Height 图像高度。
     * @param [in] Bitmap 原始像素。
     * @param [out] OutCompressedBitmap 压缩后的 PNG 数据。
     */
    void CompressImageAsPngCompat(
        int32 Width,
        int32 Height,
        const TArray<FColor>& Bitmap,
        TArray64<uint8>& OutCompressedBitmap
    )
    {
#if UE_VERSION_OLDER_THAN(5, 5, 0)
        TArray<uint8> LegacyCompressedBitmap;
        FImageUtils::CompressImageArray(Width, Height, Bitmap, LegacyCompressedBitmap);
        OutCompressedBitmap.Reset();
        OutCompressedBitmap.Append(LegacyCompressedBitmap.GetData(), LegacyCompressedBitmap.Num());
#else
        FImageUtils::PNGCompressImageArray(
            Width,
            Height,
            TArrayView64<const FColor>(Bitmap.GetData(), Bitmap.Num()),
            OutCompressedBitmap
        );
#endif
    }
}

/**
 * @brief 构造函数。
 */
FUnrealMCPEditorCommands::FUnrealMCPEditorCommands()
{
}

/**
 * @brief 分发编辑器命令到对应处理函数。
 * @param [in] CommandType 命令类型。
 * @param [in] Params 命令参数。
 * @return TSharedPtr<FJsonObject> 执行结果或错误信息。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    // Content and level management commands
    if (CommandType == TEXT("make_directory"))
    {
        return HandleMakeDirectory(Params);
    }
    else if (CommandType == TEXT("duplicate_asset"))
    {
        return HandleDuplicateAsset(Params);
    }
    else if (CommandType == TEXT("load_level"))
    {
        return HandleLoadLevel(Params);
    }
    else if (CommandType == TEXT("save_current_level"))
    {
        return HandleSaveCurrentLevel(Params);
    }
    else if (CommandType == TEXT("start_pie"))
    {
        return HandleStartPIE(Params);
    }
    else if (CommandType == TEXT("stop_pie"))
    {
        return HandleStopPIE(Params);
    }
    else if (CommandType == TEXT("get_play_state"))
    {
        return HandleGetPlayState(Params);
    }

    // Actor manipulation commands
    if (CommandType == TEXT("get_actors_in_level"))
    {
        return HandleGetActorsInLevel(Params);
    }
    else if (CommandType == TEXT("find_actors_by_name"))
    {
        return HandleFindActorsByName(Params);
    }
    else if (CommandType == TEXT("spawn_actor") || CommandType == TEXT("create_actor"))
    {
        if (CommandType == TEXT("create_actor"))
        {
            UE_LOG(LogTemp, Warning, TEXT("'create_actor' command is deprecated and will be removed in a future version. Please use 'spawn_actor' instead."));
        }
        return HandleSpawnActor(Params);
    }
    else if (CommandType == TEXT("delete_actor"))
    {
        return HandleDeleteActor(Params);
    }
    else if (CommandType == TEXT("set_actor_transform"))
    {
        return HandleSetActorTransform(Params);
    }
    else if (CommandType == TEXT("get_actor_properties"))
    {
        return HandleGetActorProperties(Params);
    }
    else if (CommandType == TEXT("get_actor_components"))
    {
        return HandleGetActorComponents(Params);
    }
    else if (CommandType == TEXT("get_scene_components"))
    {
        return HandleGetSceneComponents(Params);
    }
    else if (CommandType == TEXT("set_actor_property"))
    {
        return HandleSetActorProperty(Params);
    }
    // Blueprint actor spawning
    else if (CommandType == TEXT("spawn_blueprint_actor"))
    {
        return HandleSpawnBlueprintActor(Params);
    }
    // Editor viewport commands
    else if (CommandType == TEXT("focus_viewport"))
    {
        return HandleFocusViewport(Params);
    }
    else if (CommandType == TEXT("take_screenshot"))
    {
        return HandleTakeScreenshot(Params);
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown editor command: %s"), *CommandType));
}

/**
 * @brief 在内容浏览器创建目录。
 * @param [in] Params 目录参数。
 * @return TSharedPtr<FJsonObject> 创建结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleMakeDirectory(const TSharedPtr<FJsonObject>& Params)
{
    FString DirectoryPath;
    if (!Params->TryGetStringField(TEXT("directory_path"), DirectoryPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'directory_path' parameter"));
    }

    if (!DirectoryPath.StartsWith(TEXT("/Game")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'directory_path' must start with /Game"));
    }

    const bool bOk = UEditorAssetLibrary::MakeDirectory(DirectoryPath);
    if (!bOk)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to create directory: %s"), *DirectoryPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("directory_path"), DirectoryPath);
    return ResultObj;
}

/**
 * @brief 复制资产到目标路径。
 * @param [in] Params 复制参数。
 * @return TSharedPtr<FJsonObject> 复制结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString SourceAssetPath;
    FString DestinationAssetPath;
    bool bOverwrite = false;

    if (!Params->TryGetStringField(TEXT("source_asset_path"), SourceAssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_asset_path' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("destination_asset_path"), DestinationAssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'destination_asset_path' parameter"));
    }
    Params->TryGetBoolField(TEXT("overwrite"), bOverwrite);

    if (!UEditorAssetLibrary::DoesAssetExist(SourceAssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Source asset does not exist: %s"), *SourceAssetPath));
    }

    if (UEditorAssetLibrary::DoesAssetExist(DestinationAssetPath))
    {
        if (!bOverwrite)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Destination asset already exists: %s"), *DestinationAssetPath));
        }

        if (!UEditorAssetLibrary::DeleteAsset(DestinationAssetPath))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Failed to remove existing destination asset: %s"), *DestinationAssetPath));
        }
    }

    UObject* DuplicatedAsset = UEditorAssetLibrary::DuplicateAsset(SourceAssetPath, DestinationAssetPath);
    if (DuplicatedAsset == nullptr)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to duplicate asset from %s to %s"), *SourceAssetPath, *DestinationAssetPath));
    }

    UEditorAssetLibrary::SaveAsset(DestinationAssetPath, false);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("source_asset_path"), SourceAssetPath);
    ResultObj->SetStringField(TEXT("destination_asset_path"), DestinationAssetPath);
    return ResultObj;
}

/**
 * @brief 加载指定关卡。
 * @param [in] Params 关卡参数。
 * @return TSharedPtr<FJsonObject> 加载结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleLoadLevel(const TSharedPtr<FJsonObject>& Params)
{
    FString LevelPath;
    if (!Params->TryGetStringField(TEXT("level_path"), LevelPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'level_path' parameter"));
    }

    const bool bOk = UnrealMCPEditorCommandsPrivate::LoadLevelCompat(LevelPath);
    if (!bOk)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to load level: %s"), *LevelPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("level_path"), LevelPath);
    return ResultObj;
}

/**
 * @brief 保存当前关卡。
 * @param [in] Params 预留参数（当前未使用）。
 * @return TSharedPtr<FJsonObject> 保存结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSaveCurrentLevel(const TSharedPtr<FJsonObject>& Params)
{
    const bool bOk = UnrealMCPEditorCommandsPrivate::SaveCurrentLevelCompat();
    if (!bOk)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to save current level"));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    const FString CurrentLevel = World ? World->GetOutermost()->GetName() : TEXT("");

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("level_path"), CurrentLevel);
    return ResultObj;
}

/**
 * @brief 启动 PIE（Play In Editor）会话。
 * @param [in] Params 启动参数（可选 simulate）。
 * @return TSharedPtr<FJsonObject> 启动结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleStartPIE(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor instance is not available"));
    }

    if (GEditor->PlayWorld)
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), true);
        ResultObj->SetBoolField(TEXT("already_playing"), true);
        ResultObj->SetStringField(TEXT("message"), TEXT("PIE is already running"));
        UnrealMCPEditorCommandsPrivate::AppendWorldInfo(ResultObj, GEditor->PlayWorld, TEXT("pie"));
        return ResultObj;
    }

    FRequestPlaySessionParams PlaySessionParams;

    bool bSimulateInEditor = false;
    if (Params.IsValid() && Params->TryGetBoolField(TEXT("simulate"), bSimulateInEditor) && bSimulateInEditor)
    {
        PlaySessionParams.WorldType = EPlaySessionWorldType::SimulateInEditor;
    }

    GEditor->RequestPlaySession(PlaySessionParams);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("already_playing"), false);
    ResultObj->SetBoolField(TEXT("play_world_available"), GEditor->PlayWorld != nullptr);
    ResultObj->SetStringField(TEXT("message"), TEXT("PIE start requested"));
    return ResultObj;
}

/**
 * @brief 停止当前 PIE 会话。
 * @param [in] Params 停止参数（当前未使用）。
 * @return TSharedPtr<FJsonObject> 停止结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleStopPIE(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor instance is not available"));
    }

    if (!GEditor->PlayWorld)
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), true);
        ResultObj->SetBoolField(TEXT("already_stopped"), true);
        ResultObj->SetStringField(TEXT("message"), TEXT("PIE is not running"));
        return ResultObj;
    }

    GEditor->RequestEndPlayMap();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("already_stopped"), false);
    ResultObj->SetStringField(TEXT("message"), TEXT("PIE stop requested"));
    return ResultObj;
}

/**
 * @brief 查询当前是否处于 PIE 运行状态。
 * @param [in] Params 查询参数（当前未使用）。
 * @return TSharedPtr<FJsonObject> 运行状态结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetPlayState(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor instance is not available"));
    }

    const bool bIsPlaying = (GEditor->PlayWorld != nullptr);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("is_playing"), bIsPlaying);
    if (bIsPlaying)
    {
        UnrealMCPEditorCommandsPrivate::AppendWorldInfo(ResultObj, GEditor->PlayWorld, TEXT("pie"));
    }

    return ResultObj;
}

/**
 * @brief 获取当前关卡的全部 Actor。
 * @param [in] Params 查询参数（支持 include_components/detailed_components/world_type）。
 * @return TSharedPtr<FJsonObject> Actor 列表。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
{
    bool bIncludeComponents = false;
    bool bDetailedComponents = true;
    Params->TryGetBoolField(TEXT("include_components"), bIncludeComponents);
    Params->TryGetBoolField(TEXT("detailed_components"), bDetailedComponents);

    FString ErrorMessage;
    FString ResolvedWorldType;
    UWorld* World = UnrealMCPEditorCommandsPrivate::ResolveWorldByParams(Params, ResolvedWorldType, ErrorMessage);
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> ActorArray;
    for (AActor* Actor : AllActors)
    {
        if (Actor)
        {
            ActorArray.Add(FUnrealMCPCommonUtils::ActorToJson(Actor, bIncludeComponents, bDetailedComponents));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), ActorArray);
    UnrealMCPEditorCommandsPrivate::AppendWorldInfo(ResultObj, World, ResolvedWorldType);
    
    return ResultObj;
}

/**
 * @brief 按名称模式查找 Actor。
 * @param [in] Params 查询参数（包含 pattern，可选 world_type）。
 * @return TSharedPtr<FJsonObject> 匹配 Actor 列表。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
{
    FString Pattern;
    if (!Params->TryGetStringField(TEXT("pattern"), Pattern))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pattern' parameter"));
    }

    bool bIncludeComponents = false;
    bool bDetailedComponents = true;
    Params->TryGetBoolField(TEXT("include_components"), bIncludeComponents);
    Params->TryGetBoolField(TEXT("detailed_components"), bDetailedComponents);

    FString ErrorMessage;
    FString ResolvedWorldType;
    UWorld* World = UnrealMCPEditorCommandsPrivate::ResolveWorldByParams(Params, ResolvedWorldType, ErrorMessage);
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }
    
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> MatchingActors;
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName().Contains(Pattern))
        {
            MatchingActors.Add(FUnrealMCPCommonUtils::ActorToJson(Actor, bIncludeComponents, bDetailedComponents));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), MatchingActors);
    UnrealMCPEditorCommandsPrivate::AppendWorldInfo(ResultObj, World, ResolvedWorldType);
    
    return ResultObj;
}

/**
 * @brief 在编辑器世界中生成指定类型 Actor。
 * @param [in] Params 生成参数。
 * @return TSharedPtr<FJsonObject> 生成结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActorType;
    if (!Params->TryGetStringField(TEXT("type"), ActorType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    // Get actor name (required parameter)
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Get optional transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Create the actor based on type
    AActor* NewActor = nullptr;
    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Check if an actor with this name already exists
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    if (ActorType == TEXT("StaticMeshActor"))
    {
        NewActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
        if (NewActor && Params->HasField(TEXT("static_mesh")))
        {
            const FString MeshPath = Params->GetStringField(TEXT("static_mesh"));
            UStaticMesh* MeshAsset = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
            if (MeshAsset)
            {
                AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(NewActor);
                if (MeshActor && MeshActor->GetStaticMeshComponent())
                {
                    MeshActor->GetStaticMeshComponent()->SetStaticMesh(MeshAsset);
                }
            }
            else
            {
                return FUnrealMCPCommonUtils::CreateErrorResponse(
                    FString::Printf(TEXT("Failed to load static mesh asset: %s"), *MeshPath));
            }
        }
    }
    else if (ActorType == TEXT("PointLight"))
    {
        NewActor = World->SpawnActor<APointLight>(APointLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("SpotLight"))
    {
        NewActor = World->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("DirectionalLight"))
    {
        NewActor = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("CameraActor"))
    {
        NewActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, SpawnParams);
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown actor type: %s"), *ActorType));
    }

    if (NewActor)
    {
        // Set scale (since SpawnActor only takes location and rotation)
        FTransform Transform = NewActor->GetTransform();
        Transform.SetScale3D(Scale);
        NewActor->SetActorTransform(Transform);

        // Return the created actor's details
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create actor"));
}

/**
 * @brief 删除指定名称 Actor。
 * @param [in] Params 删除参数。
 * @return TSharedPtr<FJsonObject> 删除结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            // Store actor info before deletion for the response
            TSharedPtr<FJsonObject> ActorInfo = FUnrealMCPCommonUtils::ActorToJsonObject(Actor);
            
            // Delete the actor
            Actor->Destroy();
            
            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetObjectField(TEXT("deleted_actor"), ActorInfo);
            return ResultObj;
        }
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

/**
 * @brief 设置 Actor 变换（位置/旋转/缩放）。
 * @param [in] Params 变换参数。
 * @return TSharedPtr<FJsonObject> 更新后的 Actor 信息。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get transform parameters
    FTransform NewTransform = TargetActor->GetTransform();

    if (Params->HasField(TEXT("location")))
    {
        NewTransform.SetLocation(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        NewTransform.SetRotation(FQuat(FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"))));
    }
    if (Params->HasField(TEXT("scale")))
    {
        NewTransform.SetScale3D(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
    }

    // Set the new transform
    TargetActor->SetActorTransform(NewTransform);

    // Return updated actor info
    return FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

/**
 * @brief 根据参数查找 Actor。
 * @param [in] Params 查询参数（支持 actor_path/name/world_type）。
 * @param [out] OutErrorMessage 查找失败时错误信息。
 * @param [out] OutResolvedWorld 可选输出，返回实际使用的 World 指针。
 * @param [out] OutResolvedWorldType 可选输出，返回实际解析出的 world_type。
 * @return AActor* 找到的 Actor，失败返回 nullptr。
 */
AActor* FUnrealMCPEditorCommands::ResolveActorByParams(
    const TSharedPtr<FJsonObject>& Params,
    FString& OutErrorMessage,
    UWorld** OutResolvedWorld,
    FString* OutResolvedWorldType
)
{
    FString ResolvedWorldType;
    UWorld* World = UnrealMCPEditorCommandsPrivate::ResolveWorldByParams(Params, ResolvedWorldType, OutErrorMessage);
    if (!World)
    {
        return nullptr;
    }

    if (OutResolvedWorld)
    {
        *OutResolvedWorld = World;
    }
    if (OutResolvedWorldType)
    {
        *OutResolvedWorldType = ResolvedWorldType;
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

    FString ActorPath;
    if (Params->TryGetStringField(TEXT("actor_path"), ActorPath) && !ActorPath.IsEmpty())
    {
        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->GetPathName() == ActorPath)
            {
                return Actor;
            }
        }

        OutErrorMessage = FString::Printf(TEXT("Actor not found by actor_path: %s"), *ActorPath);
        return nullptr;
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName) || ActorName.IsEmpty())
    {
        OutErrorMessage = TEXT("Missing 'name' or 'actor_path' parameter");
        return nullptr;
    }

    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return Actor;
        }
    }

    OutErrorMessage = FString::Printf(TEXT("Actor not found: %s"), *ActorName);
    return nullptr;
}

/**
 * @brief 查询 Actor 全量属性。
 * @param [in] Params 查询参数（支持 actor_path/name/world_type）。
 * @return TSharedPtr<FJsonObject> 属性结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params)
{
    bool bIncludeComponents = true;
    bool bDetailedComponents = true;
    Params->TryGetBoolField(TEXT("include_components"), bIncludeComponents);
    Params->TryGetBoolField(TEXT("detailed_components"), bDetailedComponents);

    FString ErrorMessage;
    UWorld* ResolvedWorld = nullptr;
    FString ResolvedWorldType;
    AActor* TargetActor = ResolveActorByParams(Params, ErrorMessage, &ResolvedWorld, &ResolvedWorldType);
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    TSharedPtr<FJsonObject> ResultObj = FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true, bIncludeComponents, bDetailedComponents);
    UnrealMCPEditorCommandsPrivate::AppendWorldInfo(ResultObj, ResolvedWorld, ResolvedWorldType);
    return ResultObj;
}

/**
 * @brief 查询单个 Actor 的全部组件信息。
 * @param [in] Params 查询参数（支持 actor_path/name/world_type）。
 * @return TSharedPtr<FJsonObject> 组件结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorComponents(const TSharedPtr<FJsonObject>& Params)
{
    bool bDetailedComponents = true;
    Params->TryGetBoolField(TEXT("detailed_components"), bDetailedComponents);

    FString ErrorMessage;
    UWorld* ResolvedWorld = nullptr;
    FString ResolvedWorldType;
    AActor* TargetActor = ResolveActorByParams(Params, ErrorMessage, &ResolvedWorld, &ResolvedWorldType);
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> ActorObject = FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true, true, bDetailedComponents);

    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetObjectField(TEXT("actor"), ActorObject);

    const TArray<TSharedPtr<FJsonValue>>* ComponentArray = nullptr;
    if (ActorObject->TryGetArrayField(TEXT("components"), ComponentArray))
    {
        ResultObj->SetArrayField(TEXT("components"), *ComponentArray);
        ResultObj->SetNumberField(TEXT("component_count"), ComponentArray->Num());
    }
    else
    {
        TArray<TSharedPtr<FJsonValue>> EmptyComponents;
        ResultObj->SetArrayField(TEXT("components"), EmptyComponents);
        ResultObj->SetNumberField(TEXT("component_count"), 0);
    }
    UnrealMCPEditorCommandsPrivate::AppendWorldInfo(ResultObj, ResolvedWorld, ResolvedWorldType);

    return ResultObj;
}

/**
 * @brief 查询场景中所有 Actor 的组件信息。
 * @param [in] Params 查询参数（可选 pattern/world_type）。
 * @return TSharedPtr<FJsonObject> 场景组件结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetSceneComponents(const TSharedPtr<FJsonObject>& Params)
{
    bool bDetailedComponents = true;
    Params->TryGetBoolField(TEXT("detailed_components"), bDetailedComponents);

    FString Pattern;
    Params->TryGetStringField(TEXT("pattern"), Pattern);

    FString ErrorMessage;
    FString ResolvedWorldType;
    UWorld* World = UnrealMCPEditorCommandsPrivate::ResolveWorldByParams(Params, ResolvedWorldType, ErrorMessage);
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

    TArray<TSharedPtr<FJsonValue>> ActorArray;
    int32 TotalComponents = 0;

    for (AActor* Actor : AllActors)
    {
        if (!Actor)
        {
            continue;
        }

        if (!Pattern.IsEmpty() && !Actor->GetName().Contains(Pattern))
        {
            continue;
        }

        TSharedPtr<FJsonObject> ActorObject = FUnrealMCPCommonUtils::ActorToJsonObject(Actor, true, true, bDetailedComponents);
        if (!ActorObject.IsValid())
        {
            continue;
        }

        const TArray<TSharedPtr<FJsonValue>>* ComponentArray = nullptr;
        if (ActorObject->TryGetArrayField(TEXT("components"), ComponentArray))
        {
            TotalComponents += ComponentArray->Num();
        }

        ActorArray.Add(MakeShared<FJsonValueObject>(ActorObject));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetArrayField(TEXT("actors"), ActorArray);
    ResultObj->SetNumberField(TEXT("actor_count"), ActorArray.Num());
    ResultObj->SetNumberField(TEXT("component_count"), TotalComponents);
    UnrealMCPEditorCommandsPrivate::AppendWorldInfo(ResultObj, World, ResolvedWorldType);
    return ResultObj;
}

/**
 * @brief 设置 Actor 指定属性。
 * @param [in] Params 属性设置参数。
 * @return TSharedPtr<FJsonObject> 设置结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get property name
    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }

    // Get property value
    if (!Params->HasField(TEXT("property_value")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));
    }
    
    TSharedPtr<FJsonValue> PropertyValue = Params->Values.FindRef(TEXT("property_value"));
    
    // Set the property using our utility function
    FString ErrorMessage;
    if (FUnrealMCPCommonUtils::SetObjectProperty(TargetActor, PropertyName, PropertyValue, ErrorMessage))
    {
        // Property set successfully
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("actor"), ActorName);
        ResultObj->SetStringField(TEXT("property"), PropertyName);
        ResultObj->SetBoolField(TEXT("success"), true);
        
        // Also include the full actor details
        ResultObj->SetObjectField(TEXT("actor_details"), FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true));
        return ResultObj;
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }
}

/**
 * @brief 根据 Blueprint 资产生成 Actor。
 * @param [in] Params 蓝图生成参数。
 * @return TSharedPtr<FJsonObject> 生成结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    // Find the blueprint
    if (BlueprintName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint name is empty"));
    }

    FString Root      = TEXT("/Game/Blueprints/");
    FString AssetPath = Root + BlueprintName;

    if (!FPackageName::DoesPackageExist(AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found – it must reside under /Game/Blueprints"), *BlueprintName));
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Spawn the actor
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    FTransform SpawnTransform;
    SpawnTransform.SetLocation(Location);
    SpawnTransform.SetRotation(FQuat(Rotation));
    SpawnTransform.SetScale3D(Scale);

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;

    AActor* NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform, SpawnParams);
    if (NewActor)
    {
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn blueprint actor"));
}

/**
 * @brief 聚焦视口到目标 Actor 或坐标位置。
 * @param [in] Params 聚焦参数。
 * @return TSharedPtr<FJsonObject> 执行结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleFocusViewport(const TSharedPtr<FJsonObject>& Params)
{
    // Get target actor name if provided
    FString TargetActorName;
    bool HasTargetActor = Params->TryGetStringField(TEXT("target"), TargetActorName);

    // Get location if provided
    FVector Location(0.0f, 0.0f, 0.0f);
    bool HasLocation = false;
    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
        HasLocation = true;
    }

    // Get distance
    float Distance = 1000.0f;
    if (Params->HasField(TEXT("distance")))
    {
        Distance = Params->GetNumberField(TEXT("distance"));
    }

    // Get orientation if provided
    FRotator Orientation(0.0f, 0.0f, 0.0f);
    bool HasOrientation = false;
    if (Params->HasField(TEXT("orientation")))
    {
        Orientation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("orientation"));
        HasOrientation = true;
    }

    // Get the active viewport
    FLevelEditorViewportClient* ViewportClient = (FLevelEditorViewportClient*)GEditor->GetActiveViewport()->GetClient();
    if (!ViewportClient)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get active viewport"));
    }

    // If we have a target actor, focus on it
    if (HasTargetActor)
    {
        // Find the actor
        AActor* TargetActor = nullptr;
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
        
        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->GetName() == TargetActorName)
            {
                TargetActor = Actor;
                break;
            }
        }

        if (!TargetActor)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *TargetActorName));
        }

        // Focus on the actor
        ViewportClient->SetViewLocation(TargetActor->GetActorLocation() - FVector(Distance, 0.0f, 0.0f));
    }
    // Otherwise use the provided location
    else if (HasLocation)
    {
        ViewportClient->SetViewLocation(Location - FVector(Distance, 0.0f, 0.0f));
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Either 'target' or 'location' must be provided"));
    }

    // Set orientation if provided
    if (HasOrientation)
    {
        ViewportClient->SetViewRotation(Orientation);
    }

    // Force viewport to redraw
    ViewportClient->Invalidate();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

/**
 * @brief 对当前激活视口进行截图。
 * @param [in] Params 截图参数（含输出路径）。
 * @return TSharedPtr<FJsonObject> 截图结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    // Get file path parameter
    FString FilePath;
    if (!Params->TryGetStringField(TEXT("filepath"), FilePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'filepath' parameter"));
    }
    
    // Ensure the file path has a proper extension
    if (!FilePath.EndsWith(TEXT(".png")))
    {
        FilePath += TEXT(".png");
    }

    // Get the active viewport
    if (GEditor && GEditor->GetActiveViewport())
    {
        FViewport* Viewport = GEditor->GetActiveViewport();
        TArray<FColor> Bitmap;
        FIntRect ViewportRect(0, 0, Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y);
        
        if (Viewport->ReadPixels(Bitmap, FReadSurfaceDataFlags(), ViewportRect))
        {
            TArray64<uint8> CompressedBitmap;
            UnrealMCPEditorCommandsPrivate::CompressImageAsPngCompat(
                Viewport->GetSizeXY().X,
                Viewport->GetSizeXY().Y,
                Bitmap,
                CompressedBitmap
            );
            
            if (FFileHelper::SaveArrayToFile(CompressedBitmap, *FilePath))
            {
                TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
                ResultObj->SetStringField(TEXT("filepath"), FilePath);
                return ResultObj;
            }
        }
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to take screenshot"));
} 
