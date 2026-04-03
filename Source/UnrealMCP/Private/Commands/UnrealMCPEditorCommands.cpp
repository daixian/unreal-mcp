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
#include "Engine/EngineBaseTypes.h"
#include "PlayInEditorDataTypes.h"
#include "EditorAssetLibrary.h"
#include "EditorUtilityLibrary.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityBlueprint.h"
#include "EditorUtilityWidget.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "Modules/ModuleManager.h"
#include "Misc/OutputDevice.h"
#include "Misc/ScopeLock.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "MessageLogModule.h"
#include "IMessageLogListing.h"
#include "Logging/TokenizedMessage.h"
#include "IPythonScriptPlugin.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Containers/Ticker.h"
#include "UnrealClient.h"

#if PLATFORM_WINDOWS
#include "ILiveCodingModule.h"
#endif

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
     * @brief 将 Play 会话目标枚举转为字符串。
     * @param [in] Destination Play 会话目标。
     * @return FString 序列化后的目标字符串。
     */
    FString PlaySessionDestinationToString(EPlaySessionDestinationType Destination)
    {
        switch (Destination)
        {
            case EPlaySessionDestinationType::InProcess:
                return TEXT("in_process");
            case EPlaySessionDestinationType::NewProcess:
                return TEXT("new_process");
            case EPlaySessionDestinationType::Launcher:
                return TEXT("launcher");
            default:
                return TEXT("unknown");
        }
    }

    /**
     * @brief 为结果对象写入当前 Play 会话目标与 Standalone 标识。
     * @param [in,out] ResultObj 输出结果。
     * @param [in] RequestParams 会话请求参数。
     */
    void AppendPlaySessionDestinationInfo(
        const TSharedPtr<FJsonObject>& ResultObj,
        const FRequestPlaySessionParams& RequestParams
    )
    {
        if (!ResultObj.IsValid())
        {
            return;
        }

        const bool bIsStandaloneGame = RequestParams.SessionDestination == EPlaySessionDestinationType::NewProcess &&
            RequestParams.SessionPreviewTypeOverride.Get(EPlaySessionPreviewType::NoPreview) != EPlaySessionPreviewType::MobilePreview;

        ResultObj->SetStringField(
            TEXT("play_session_destination"),
            PlaySessionDestinationToString(RequestParams.SessionDestination)
        );
        ResultObj->SetBoolField(TEXT("is_standalone_game"), bIsStandaloneGame);
    }

    /**
     * @brief 构造资产身份信息对象。
     * @param [in] AssetData 资产数据。
     * @return TSharedPtr<FJsonObject> 资产身份信息。
     */
    TSharedPtr<FJsonObject> CreateAssetIdentityObject(const FAssetData& AssetData)
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("asset_name"), AssetData.AssetName.ToString());
        Result->SetStringField(TEXT("asset_class"), AssetData.AssetClassPath.GetAssetName().ToString());
        Result->SetStringField(TEXT("asset_class_path"), AssetData.AssetClassPath.ToString());
        Result->SetStringField(TEXT("asset_path"), AssetData.PackageName.ToString());
        Result->SetStringField(TEXT("package_name"), AssetData.PackageName.ToString());
        Result->SetStringField(TEXT("package_path"), AssetData.PackagePath.ToString());
        Result->SetStringField(TEXT("object_path"), AssetData.GetObjectPathString());
        return Result;
    }

    /**
     * @brief 把 FAssetData 的标签复制到 JSON 对象。
     * @param [in] AssetData 资产数据。
     * @param [in,out] Result 目标 JSON 对象。
     * @param [in] MaxTagCount 最多复制的标签数量。
     */
    void AppendAssetTagsToObject(const FAssetData& AssetData, const TSharedPtr<FJsonObject>& Result, int32 MaxTagCount)
    {
        if (!Result.IsValid())
        {
            return;
        }

        TSharedPtr<FJsonObject> Tags = MakeShared<FJsonObject>();
        int32 Count = 0;
        for (const TPair<FName, FAssetTagValueRef>& TagValue : AssetData.TagsAndValues)
        {
            if (Count >= MaxTagCount)
            {
                break;
            }

            Tags->SetStringField(TagValue.Key.ToString(), TagValue.Value.AsString());
            ++Count;
        }

        Result->SetObjectField(TEXT("tags"), Tags);
        Result->SetNumberField(TEXT("tag_count"), Count);
    }

#if PLATFORM_WINDOWS
    /**
     * @brief 将 Live Coding 编译结果枚举转换为字符串。
     * @param [in] CompileResult Live Coding 编译结果枚举。
     * @return FString 便于序列化的结果字符串。
     */
    FString LiveCodingCompileResultToString(ELiveCodingCompileResult CompileResult)
    {
        switch (CompileResult)
        {
            case ELiveCodingCompileResult::Success:
                return TEXT("Success");
            case ELiveCodingCompileResult::NoChanges:
                return TEXT("NoChanges");
            case ELiveCodingCompileResult::InProgress:
                return TEXT("InProgress");
            case ELiveCodingCompileResult::CompileStillActive:
                return TEXT("CompileStillActive");
            case ELiveCodingCompileResult::NotStarted:
                return TEXT("NotStarted");
            case ELiveCodingCompileResult::Failure:
                return TEXT("Failure");
            case ELiveCodingCompileResult::Cancelled:
                return TEXT("Cancelled");
            default:
                return TEXT("Unknown");
        }
    }

    /**
     * @brief 读取 Live Coding 模块的公共状态并写入响应对象。
     * @param [in,out] ResultObj 结果 JSON 对象。
     * @param [in] LiveCodingModule Live Coding 模块实例。
     */
    void AppendLiveCodingState(const TSharedPtr<FJsonObject>& ResultObj, ILiveCodingModule& LiveCodingModule)
    {
        if (!ResultObj.IsValid())
        {
            return;
        }

        ResultObj->SetBoolField(TEXT("has_started"), LiveCodingModule.HasStarted());
        ResultObj->SetBoolField(TEXT("is_enabled_for_session"), LiveCodingModule.IsEnabledForSession());
        ResultObj->SetBoolField(TEXT("can_enable_for_session"), LiveCodingModule.CanEnableForSession());
        ResultObj->SetBoolField(TEXT("is_enabled_by_default"), LiveCodingModule.IsEnabledByDefault());
        ResultObj->SetBoolField(TEXT("is_compiling"), LiveCodingModule.IsCompiling());
        ResultObj->SetBoolField(TEXT("automatically_compile_new_classes"), LiveCodingModule.AutomaticallyCompileNewClasses());
        ResultObj->SetStringField(TEXT("enable_error"), LiveCodingModule.GetEnableErrorText().ToString());
    }

    /**
     * @brief 加载 Live Coding 模块。
     * @param [out] OutErrorMessage 加载失败时的错误信息。
     * @return ILiveCodingModule* 成功时返回模块指针，否则返回 nullptr。
     */
    ILiveCodingModule* LoadLiveCodingModule(FString& OutErrorMessage)
    {
        if (!FModuleManager::Get().ModuleExists(TEXT(LIVE_CODING_MODULE_NAME)))
        {
            OutErrorMessage = TEXT("当前编辑器构建未提供 Live Coding 模块");
            return nullptr;
        }

        ILiveCodingModule* LiveCodingModule = FModuleManager::LoadModulePtr<ILiveCodingModule>(TEXT(LIVE_CODING_MODULE_NAME));
        if (!LiveCodingModule)
        {
            OutErrorMessage = TEXT("加载 Live Coding 模块失败");
            return nullptr;
        }

        return LiveCodingModule;
    }
#endif

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

    /**
     * @brief 当前视口序列捕获的运行状态。
     */
    struct FViewportSequenceCaptureState
    {
        FString SequenceId;
        FString OutputDirectory;
        FString BaseFilename;
        int32 FrameCount = 0;
        int32 CapturedFrameCount = 0;
        double IntervalSeconds = 0.0;
        double NextCaptureTimeSeconds = 0.0;
        bool bUseHighRes = false;
        bool bShowUI = false;
        bool bCaptureHDR = false;
        bool bCaptureInFlight = false;
        FIntPoint RequestedResolution = FIntPoint::ZeroValue;
        float ResolutionMultiplier = 1.0f;
        FString LastRequestedFilePath;
        FString LastErrorMessage;
        FIntPoint LastCapturedResolution = FIntPoint::ZeroValue;
        TArray<FString> PlannedFilePaths;
        FTSTicker::FDelegateHandle TickHandle;
        FDelegateHandle ScreenshotDelegateHandle;
    };

    TSharedPtr<FViewportSequenceCaptureState> GViewportSequenceCaptureState;

    /**
     * @brief 规范化输出文件路径，并在缺少扩展名时补默认扩展。
     * @param [in] InFilePath 原始输入路径。
     * @param [in] DefaultExtension 默认扩展名（包含点号）。
     * @return FString 规范化后的绝对路径。
     */
    FString NormalizeCaptureFilePath(const FString& InFilePath, const FString& DefaultExtension)
    {
        FString FilePath = InFilePath.TrimStartAndEnd();
        if (FilePath.IsEmpty())
        {
            return FilePath;
        }

        if (FPaths::GetExtension(FilePath, true).IsEmpty())
        {
            FilePath += DefaultExtension;
        }

        return FPaths::ConvertRelativePathToFull(FilePath);
    }

    /**
     * @brief 确保文件所在目录存在。
     * @param [in] FilePath 目标文件路径。
     * @param [out] OutErrorMessage 失败时的错误信息。
     * @return bool 成功返回 true。
     */
    bool EnsureCaptureDirectoryExists(const FString& FilePath, FString& OutErrorMessage)
    {
        const FString DirectoryPath = FPaths::GetPath(FilePath);
        if (DirectoryPath.IsEmpty())
        {
            OutErrorMessage = TEXT("输出路径缺少目录部分");
            return false;
        }

        if (IFileManager::Get().DirectoryExists(*DirectoryPath))
        {
            return true;
        }

        if (!IFileManager::Get().MakeDirectory(*DirectoryPath, true))
        {
            OutErrorMessage = FString::Printf(TEXT("创建输出目录失败: %s"), *DirectoryPath);
            return false;
        }

        return true;
    }

    /**
     * @brief 把字符串数组写入 JSON 结果字段。
     * @param [in,out] Result 结果对象。
     * @param [in] FieldName 字段名。
     * @param [in] Values 字符串数组。
     */
    void SetStringArrayField(
        const TSharedPtr<FJsonObject>& Result,
        const FString& FieldName,
        const TArray<FString>& Values
    )
    {
        TArray<TSharedPtr<FJsonValue>> JsonArray;
        JsonArray.Reserve(Values.Num());
        for (const FString& Value : Values)
        {
            JsonArray.Add(MakeShared<FJsonValueString>(Value));
        }
        Result->SetArrayField(FieldName, JsonArray);
    }

    /**
     * @brief 从 JSON 中读取二维整数数组。
     * @param [in] Params 参数对象。
     * @param [in] FieldName 字段名。
     * @param [out] OutValue 解析出的尺寸。
     * @param [out] OutErrorMessage 失败时的错误信息。
     * @return bool 成功返回 true。
     */
    bool TryGetIntPointField(
        const TSharedPtr<FJsonObject>& Params,
        const FString& FieldName,
        FIntPoint& OutValue,
        FString& OutErrorMessage
    )
    {
        const TArray<TSharedPtr<FJsonValue>>* ResolutionArray = nullptr;
        if (!Params.IsValid() || !Params->TryGetArrayField(FieldName, ResolutionArray))
        {
            OutErrorMessage = FString::Printf(TEXT("缺少 '%s' 参数"), *FieldName);
            return false;
        }

        if (!ResolutionArray || ResolutionArray->Num() != 2)
        {
            OutErrorMessage = FString::Printf(TEXT("参数 '%s' 必须是 2 个数字组成的数组"), *FieldName);
            return false;
        }

        double ResolutionX = 0.0;
        double ResolutionY = 0.0;
        if (!(*ResolutionArray)[0]->TryGetNumber(ResolutionX) || !(*ResolutionArray)[1]->TryGetNumber(ResolutionY))
        {
            OutErrorMessage = FString::Printf(TEXT("参数 '%s' 必须只包含数字"), *FieldName);
            return false;
        }

        OutValue = FIntPoint(FMath::RoundToInt(ResolutionX), FMath::RoundToInt(ResolutionY));
        return true;
    }

    /**
     * @brief 请求当前活动视口执行一次截图。
     * @param [in] FilePath 输出文件路径。
     * @param [in] bUseHighRes 是否使用高分辨率截图流程。
     * @param [in] RequestedResolution 期望分辨率，零值表示使用倍率。
     * @param [in] ResolutionMultiplier 分辨率倍率。
     * @param [in] bShowUI 普通截图时是否包含 UI。
     * @param [in] bCaptureHDR 是否捕获 HDR。
     * @param [out] OutErrorMessage 失败时的错误信息。
     * @return bool 请求成功返回 true。
     */
    bool RequestViewportScreenshot(
        const FString& FilePath,
        bool bUseHighRes,
        const FIntPoint& RequestedResolution,
        float ResolutionMultiplier,
        bool bShowUI,
        bool bCaptureHDR,
        FString& OutErrorMessage
    )
    {
        if (!GEditor)
        {
            OutErrorMessage = TEXT("编辑器实例不可用");
            return false;
        }

        FViewport* ActiveViewport = GEditor->GetActiveViewport();
        if (!ActiveViewport)
        {
            OutErrorMessage = TEXT("未找到活动视口");
            return false;
        }

        if (!EnsureCaptureDirectoryExists(FilePath, OutErrorMessage))
        {
            return false;
        }

        if (bUseHighRes && !bCaptureHDR)
        {
            TArray<FColor> Bitmap;
            const FIntPoint ViewportSize = ActiveViewport->GetSizeXY();
            const FIntRect ViewRect(0, 0, ViewportSize.X, ViewportSize.Y);
            if (!ActiveViewport->ReadPixels(Bitmap, FReadSurfaceDataFlags(), ViewRect))
            {
                OutErrorMessage = TEXT("读取视口像素失败");
                return false;
            }

            const float ClampedMultiplier = FMath::Clamp(
                ResolutionMultiplier,
                FHighResScreenshotConfig::MinResolutionMultipler,
                FHighResScreenshotConfig::MaxResolutionMultipler
            );
            const FIntPoint TargetResolution = (RequestedResolution.X > 0 && RequestedResolution.Y > 0)
                ? RequestedResolution
                : FIntPoint(
                    FMath::Max(1, FMath::RoundToInt(ViewportSize.X * ClampedMultiplier)),
                    FMath::Max(1, FMath::RoundToInt(ViewportSize.Y * ClampedMultiplier))
                );

            const TArray<FColor>* OutputBitmap = &Bitmap;
            TArray<FColor> ResizedBitmap;
            if (TargetResolution != ViewportSize)
            {
                FImageUtils::ImageResize(
                    ViewportSize.X,
                    ViewportSize.Y,
                    Bitmap,
                    TargetResolution.X,
                    TargetResolution.Y,
                    ResizedBitmap,
                    true,
                    false
                );
                OutputBitmap = &ResizedBitmap;
            }

            TArray64<uint8> CompressedBitmap;
            CompressImageAsPngCompat(
                TargetResolution.X,
                TargetResolution.Y,
                *OutputBitmap,
                CompressedBitmap
            );

            if (!FFileHelper::SaveArrayToFile(CompressedBitmap, *FilePath))
            {
                OutErrorMessage = FString::Printf(TEXT("保存截图文件失败: %s"), *FilePath);
                return false;
            }

            return true;
        }

        if (bUseHighRes)
        {
            FHighResScreenshotConfig& HighResScreenshotConfig = GetHighResScreenshotConfig();
            if (RequestedResolution.X > 0 && RequestedResolution.Y > 0)
            {
                HighResScreenshotConfig.SetResolution(RequestedResolution.X, RequestedResolution.Y);
            }
            else
            {
                const float ClampedMultiplier = FMath::Clamp(
                    ResolutionMultiplier,
                    FHighResScreenshotConfig::MinResolutionMultipler,
                    FHighResScreenshotConfig::MaxResolutionMultipler
                );
                const FIntPoint ViewportSize = ActiveViewport->GetSizeXY();
                HighResScreenshotConfig.SetResolution(ViewportSize.X, ViewportSize.Y, ClampedMultiplier);
            }

            HighResScreenshotConfig.SetFilename(FilePath);
            HighResScreenshotConfig.SetMaskEnabled(false);
            HighResScreenshotConfig.SetHDRCapture(bCaptureHDR);

            if (!ActiveViewport->TakeHighResScreenShot())
            {
                OutErrorMessage = TEXT("高分辨率截图请求提交失败");
                return false;
            }

            return true;
        }

        FScreenshotRequest::RequestScreenshot(FilePath, bShowUI, false, bCaptureHDR);
        return true;
    }

    /**
     * @brief 构造序列帧输出文件路径。
     * @param [in] OutputDirectory 输出目录。
     * @param [in] BaseFilename 文件名前缀。
     * @param [in] FrameIndex 当前帧序号。
     * @param [in] bCaptureHDR 是否捕获 HDR。
     * @return FString 单帧文件路径。
     */
    FString BuildViewportSequenceFramePath(
        const FString& OutputDirectory,
        const FString& BaseFilename,
        int32 FrameIndex,
        bool bCaptureHDR
    )
    {
        const FString Extension = bCaptureHDR ? TEXT(".exr") : TEXT(".png");
        return NormalizeCaptureFilePath(
            FPaths::Combine(OutputDirectory, FString::Printf(TEXT("%s_%05d"), *BaseFilename, FrameIndex)),
            Extension
        );
    }

    /**
     * @brief 清理当前序列捕获状态并注销相关回调。
     */
    void StopViewportSequenceCapture()
    {
        if (!GViewportSequenceCaptureState.IsValid())
        {
            return;
        }

        if (GViewportSequenceCaptureState->TickHandle.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(GViewportSequenceCaptureState->TickHandle);
            GViewportSequenceCaptureState->TickHandle.Reset();
        }

        if (GViewportSequenceCaptureState->ScreenshotDelegateHandle.IsValid())
        {
            FScreenshotRequest::OnScreenshotCaptured().Remove(GViewportSequenceCaptureState->ScreenshotDelegateHandle);
            GViewportSequenceCaptureState->ScreenshotDelegateHandle.Reset();
        }

        GViewportSequenceCaptureState.Reset();
    }

    /**
     * @brief 处理序列捕获的截图完成回调。
     * @param [in] Width 截图宽度。
     * @param [in] Height 截图高度。
     * @param [in] Colors 颜色数据（当前仅用于确认回调到达）。
     */
    void HandleViewportSequenceScreenshotCaptured(int32 Width, int32 Height, const TArray<FColor>& Colors)
    {
        if (!GViewportSequenceCaptureState.IsValid())
        {
            return;
        }

        FViewportSequenceCaptureState& State = *GViewportSequenceCaptureState;
        State.bCaptureInFlight = false;
        State.LastCapturedResolution = FIntPoint(Width, Height);
        ++State.CapturedFrameCount;
        State.NextCaptureTimeSeconds = FPlatformTime::Seconds() + State.IntervalSeconds;

        UE_LOG(
            LogTemp,
            Display,
            TEXT("视口序列捕获完成一帧：SequenceId=%s Frame=%d/%d File=%s Colors=%d"),
            *State.SequenceId,
            State.CapturedFrameCount,
            State.FrameCount,
            *State.LastRequestedFilePath,
            Colors.Num()
        );

        if (State.CapturedFrameCount >= State.FrameCount)
        {
            UE_LOG(LogTemp, Display, TEXT("视口序列捕获全部完成：SequenceId=%s"), *State.SequenceId);
            StopViewportSequenceCapture();
        }
    }

    /**
     * @brief 驱动序列捕获逐帧发起截图请求。
     * @param [in] DeltaTime 本次 Tick 的时间增量。
     * @return bool 返回 true 表示继续保留 Ticker。
     */
    bool TickViewportSequenceCapture(float DeltaTime)
    {
        if (!GViewportSequenceCaptureState.IsValid())
        {
            return false;
        }

        FViewportSequenceCaptureState& State = *GViewportSequenceCaptureState;
        if (State.CapturedFrameCount >= State.FrameCount)
        {
            StopViewportSequenceCapture();
            return false;
        }

        if (State.bCaptureInFlight || FPlatformTime::Seconds() < State.NextCaptureTimeSeconds)
        {
            return true;
        }

        const int32 FrameIndex = State.CapturedFrameCount + 1;
        const FString FramePath = State.PlannedFilePaths.IsValidIndex(State.CapturedFrameCount)
            ? State.PlannedFilePaths[State.CapturedFrameCount]
            : BuildViewportSequenceFramePath(State.OutputDirectory, State.BaseFilename, FrameIndex, State.bCaptureHDR);

        FString ErrorMessage;
        if (!RequestViewportScreenshot(
                FramePath,
                State.bUseHighRes,
                State.RequestedResolution,
                State.ResolutionMultiplier,
                State.bShowUI,
                State.bCaptureHDR,
                ErrorMessage))
        {
            State.LastErrorMessage = ErrorMessage;
            UE_LOG(
                LogTemp,
                Error,
                TEXT("视口序列捕获失败：SequenceId=%s Frame=%d Error=%s"),
                *State.SequenceId,
                FrameIndex,
                *ErrorMessage
            );
            StopViewportSequenceCapture();
            return false;
        }

        State.LastRequestedFilePath = FramePath;
        if (State.bUseHighRes && !State.bCaptureHDR)
        {
            State.LastCapturedResolution = (State.RequestedResolution.X > 0 && State.RequestedResolution.Y > 0)
                ? State.RequestedResolution
                : FIntPoint::ZeroValue;
            ++State.CapturedFrameCount;
            State.NextCaptureTimeSeconds = FPlatformTime::Seconds() + State.IntervalSeconds;

            UE_LOG(
                LogTemp,
                Display,
                TEXT("视口序列捕获完成一帧：SequenceId=%s Frame=%d/%d File=%s"),
                *State.SequenceId,
                State.CapturedFrameCount,
                State.FrameCount,
                *State.LastRequestedFilePath
            );

            if (State.CapturedFrameCount >= State.FrameCount)
            {
                UE_LOG(LogTemp, Display, TEXT("视口序列捕获全部完成：SequenceId=%s"), *State.SequenceId);
                StopViewportSequenceCapture();
                return false;
            }

            return true;
        }

        State.bCaptureInFlight = true;
        return true;
    }

    /**
     * @brief 将日志冗长级别转换为字符串。
     * @param [in] Verbosity 日志冗长级别。
     * @return FString 序列化后的冗长级别文本。
     */
    FString LogVerbosityToString(ELogVerbosity::Type Verbosity)
    {
        switch (Verbosity & ELogVerbosity::VerbosityMask)
        {
            case ELogVerbosity::Fatal:
                return TEXT("Fatal");
            case ELogVerbosity::Error:
                return TEXT("Error");
            case ELogVerbosity::Warning:
                return TEXT("Warning");
            case ELogVerbosity::Display:
                return TEXT("Display");
            case ELogVerbosity::Log:
                return TEXT("Log");
            case ELogVerbosity::Verbose:
                return TEXT("Verbose");
            case ELogVerbosity::VeryVerbose:
                return TEXT("VeryVerbose");
            default:
                return TEXT("Unknown");
        }
    }

    /**
     * @brief 将 Message Log 严重级别转换为字符串。
     * @param [in] Severity Message Log 严重级别。
     * @return FString 序列化后的严重级别文本。
     */
    FString MessageSeverityToString(EMessageSeverity::Type Severity)
    {
        switch (Severity)
        {
            case EMessageSeverity::Error:
                return TEXT("Error");
            case EMessageSeverity::PerformanceWarning:
                return TEXT("PerformanceWarning");
            case EMessageSeverity::Warning:
                return TEXT("Warning");
            case EMessageSeverity::Info:
                return TEXT("Info");
            default:
                return TEXT("Unknown");
        }
    }

    /**
     * @brief 解析 Python 执行模式字符串。
     * @param [in] ExecutionModeText 输入文本。
     * @param [out] OutExecutionMode 解析结果。
     * @return bool 解析成功返回 true。
     */
    bool TryParsePythonExecutionMode(
        const FString& ExecutionModeText,
        EPythonCommandExecutionMode& OutExecutionMode
    )
    {
        const FString Normalized = ExecutionModeText.TrimStartAndEnd();
        if (Normalized.IsEmpty())
        {
            OutExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
            return true;
        }

        return LexTryParseString(OutExecutionMode, *Normalized);
    }

    /**
     * @brief 判断 Python 输入是否包含多行脚本内容。
     * @param [in] CommandText 原始命令文本。
     * @return bool 包含换行时返回 true。
     */
    bool IsPythonCommandMultiLine(const FString& CommandText)
    {
        return CommandText.Contains(TEXT("\n")) || CommandText.Contains(TEXT("\r"));
    }

    /**
     * @brief 粗略判断 Python 输入是否更像是脚本文件命令。
     * @param [in] CommandText 原始命令文本。
     * @return bool 看起来像 .py 文件路径时返回 true。
     */
    bool LooksLikePythonFileCommand(const FString& CommandText)
    {
        const FString TrimmedCommand = CommandText.TrimStartAndEnd();
        if (TrimmedCommand.IsEmpty())
        {
            return false;
        }

        const int32 ExtensionIndex = TrimmedCommand.Find(TEXT(".py"), ESearchCase::IgnoreCase, ESearchDir::FromStart);
        if (ExtensionIndex == INDEX_NONE)
        {
            return false;
        }

        const int32 NextCharacterIndex = ExtensionIndex + 3;
        if (NextCharacterIndex >= TrimmedCommand.Len())
        {
            return true;
        }

        const TCHAR NextCharacter = TrimmedCommand[NextCharacterIndex];
        return FChar::IsWhitespace(NextCharacter) || NextCharacter == TCHAR('"');
    }

    /**
     * @brief 解析 Python 执行模式，支持 Auto 自动选择。
     * @param [in] RequestedExecutionModeText 请求的执行模式文本。
     * @param [in] CommandText 原始命令文本。
     * @param [out] OutExecutionMode 最终执行模式。
     * @param [out] bOutAutoSelected 是否使用了自动选择。
     * @return bool 解析成功返回 true。
     */
    bool TryResolvePythonExecutionMode(
        const FString& RequestedExecutionModeText,
        const FString& CommandText,
        EPythonCommandExecutionMode& OutExecutionMode,
        bool& bOutAutoSelected
    )
    {
        const FString Normalized = RequestedExecutionModeText.TrimStartAndEnd();
        if (Normalized.IsEmpty() || Normalized.Equals(TEXT("Auto"), ESearchCase::IgnoreCase))
        {
            bOutAutoSelected = true;
            OutExecutionMode = (IsPythonCommandMultiLine(CommandText) || LooksLikePythonFileCommand(CommandText))
                ? EPythonCommandExecutionMode::ExecuteFile
                : EPythonCommandExecutionMode::ExecuteStatement;
            return true;
        }

        bOutAutoSelected = false;
        return TryParsePythonExecutionMode(Normalized, OutExecutionMode);
    }

    /**
     * @brief 解析 Python 文件执行作用域字符串。
     * @param [in] ScopeText 输入文本。
     * @param [out] OutScope 解析结果。
     * @return bool 解析成功返回 true。
     */
    bool TryParsePythonFileExecutionScope(
        const FString& ScopeText,
        EPythonFileExecutionScope& OutScope
    )
    {
        const FString Normalized = ScopeText.TrimStartAndEnd().ToLower();
        if (Normalized.IsEmpty() || Normalized == TEXT("private"))
        {
            OutScope = EPythonFileExecutionScope::Private;
            return true;
        }

        if (Normalized == TEXT("public"))
        {
            OutScope = EPythonFileExecutionScope::Public;
            return true;
        }

        return false;
    }

    /**
     * @brief 将 Python 日志文本数组写入 JSON 字段。
     * @param [in,out] Result 输出对象。
     * @param [in] FieldName 字段名。
     * @param [in] Lines 文本行数组。
     */
    void AppendPythonTextArrayField(
        const TSharedPtr<FJsonObject>& Result,
        const FString& FieldName,
        const TArray<FString>& Lines
    )
    {
        TArray<TSharedPtr<FJsonValue>> JsonValues;
        JsonValues.Reserve(Lines.Num());
        for (const FString& Line : Lines)
        {
            JsonValues.Add(MakeShared<FJsonValueString>(Line));
        }
        Result->SetArrayField(FieldName, JsonValues);
    }

    /**
     * @brief 获取当前活动的关卡视口客户端。
     * @param [out] OutErrorMessage 获取失败时的错误信息。
     * @return FLevelEditorViewportClient* 成功时返回视口客户端，否则返回 nullptr。
     */
    FLevelEditorViewportClient* GetActiveLevelViewportClient(FString& OutErrorMessage)
    {
        if (!GEditor)
        {
            OutErrorMessage = TEXT("Editor instance is not available");
            return nullptr;
        }

        if (FViewport* ActiveViewport = GEditor->GetActiveViewport())
        {
            for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
            {
                if (ViewportClient && ViewportClient->Viewport == ActiveViewport)
                {
                    return ViewportClient;
                }
            }
        }

        for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
        {
            if (ViewportClient)
            {
                return ViewportClient;
            }
        }

        OutErrorMessage = TEXT("Failed to resolve active level viewport");
        return nullptr;
    }

    /**
     * @brief 归一化视口模式字符串，便于兼容空格、下划线和大小写差异。
     * @param [in] Input 原始输入文本。
     * @return FString 归一化后的比较键。
     */
    FString NormalizeViewModeToken(const FString& Input)
    {
        FString Normalized;
        Normalized.Reserve(Input.Len());
        for (const TCHAR Character : Input.TrimStartAndEnd())
        {
            if (FChar::IsAlnum(Character))
            {
                Normalized.AppendChar(FChar::ToLower(Character));
            }
        }
        return Normalized;
    }

    /**
     * @brief 将视图模式枚举转换为稳定字符串。
     * @param [in] ViewModeIndex 视图模式枚举。
     * @return FString 视图模式文本。
     */
    FString ViewModeIndexToString(EViewModeIndex ViewModeIndex)
    {
        switch (ViewModeIndex)
        {
            case VMI_BrushWireframe:
                return TEXT("BrushWireframe");
            case VMI_Wireframe:
                return TEXT("Wireframe");
            case VMI_Unlit:
                return TEXT("Unlit");
            case VMI_Lit:
                return TEXT("Lit");
            case VMI_Lit_DetailLighting:
                return TEXT("DetailLighting");
            case VMI_LightingOnly:
                return TEXT("LightingOnly");
            case VMI_LightComplexity:
                return TEXT("LightComplexity");
            case VMI_ShaderComplexity:
                return TEXT("ShaderComplexity");
            case VMI_LightmapDensity:
                return TEXT("LightmapDensity");
            case VMI_LitLightmapDensity:
                return TEXT("LitLightmapDensity");
            case VMI_ReflectionOverride:
                return TEXT("ReflectionOverride");
            case VMI_VisualizeBuffer:
                return TEXT("VisualizeBuffer");
            case VMI_StationaryLightOverlap:
                return TEXT("StationaryLightOverlap");
            case VMI_CollisionPawn:
                return TEXT("CollisionPawn");
            case VMI_CollisionVisibility:
                return TEXT("CollisionVisibility");
            case VMI_LODColoration:
                return TEXT("LODColoration");
            case VMI_QuadOverdraw:
                return TEXT("QuadOverdraw");
            case VMI_ShaderComplexityWithQuadOverdraw:
                return TEXT("ShaderComplexityWithQuadOverdraw");
            case VMI_PathTracing:
                return TEXT("PathTracing");
            case VMI_VisualizeNanite:
                return TEXT("VisualizeNanite");
            case VMI_VisualizeLumen:
                return TEXT("VisualizeLumen");
            case VMI_VisualizeVirtualShadowMap:
                return TEXT("VisualizeVirtualShadowMap");
            case VMI_Lit_Wireframe:
                return TEXT("LitWireframe");
            case VMI_Clay:
                return TEXT("Clay");
            case VMI_Zebra:
                return TEXT("Zebra");
            default:
                return FString::Printf(TEXT("%d"), static_cast<int32>(ViewModeIndex));
        }
    }

    /**
     * @brief 解析视口模式文本或数字索引。
     * @param [in] RequestedViewMode 用户输入的视口模式。
     * @param [out] OutViewModeIndex 解析成功后的视口模式枚举。
     * @return bool 解析成功返回 true。
     */
    bool TryResolveViewModeIndex(const FString& RequestedViewMode, EViewModeIndex& OutViewModeIndex)
    {
        const FString TrimmedViewMode = RequestedViewMode.TrimStartAndEnd();
        if (TrimmedViewMode.IsEmpty())
        {
            return false;
        }

        if (TrimmedViewMode.IsNumeric())
        {
            const int32 ViewModeValue = FCString::Atoi(*TrimmedViewMode);
            if (ViewModeValue >= 0 && ViewModeValue < static_cast<int32>(VMI_Max))
            {
                OutViewModeIndex = static_cast<EViewModeIndex>(ViewModeValue);
                return true;
            }
        }

        const FString NormalizedRequestedViewMode = NormalizeViewModeToken(TrimmedViewMode);
        for (int32 ViewModeIndex = 0; ViewModeIndex < static_cast<int32>(VMI_Max); ++ViewModeIndex)
        {
            const EViewModeIndex CandidateViewMode = static_cast<EViewModeIndex>(ViewModeIndex);
            if (NormalizedRequestedViewMode == NormalizeViewModeToken(ViewModeIndexToString(CandidateViewMode)))
            {
                OutViewModeIndex = CandidateViewMode;
                return true;
            }
        }

        if (NormalizedRequestedViewMode == TEXT("detaillighting"))
        {
            OutViewModeIndex = VMI_Lit_DetailLighting;
            return true;
        }
        if (NormalizedRequestedViewMode == TEXT("lightingonly"))
        {
            OutViewModeIndex = VMI_LightingOnly;
            return true;
        }
        if (NormalizedRequestedViewMode == TEXT("litwireframe"))
        {
            OutViewModeIndex = VMI_Lit_Wireframe;
            return true;
        }
        if (NormalizedRequestedViewMode == TEXT("collisionpawn") || NormalizedRequestedViewMode == TEXT("playercollision"))
        {
            OutViewModeIndex = VMI_CollisionPawn;
            return true;
        }
        if (NormalizedRequestedViewMode == TEXT("collisionvisibility") ||
            NormalizedRequestedViewMode == TEXT("visibilitycollision"))
        {
            OutViewModeIndex = VMI_CollisionVisibility;
            return true;
        }
        if (NormalizedRequestedViewMode == TEXT("reflections"))
        {
            OutViewModeIndex = VMI_ReflectionOverride;
            return true;
        }
        if (NormalizedRequestedViewMode == TEXT("nanite"))
        {
            OutViewModeIndex = VMI_VisualizeNanite;
            return true;
        }
        if (NormalizedRequestedViewMode == TEXT("lumen"))
        {
            OutViewModeIndex = VMI_VisualizeLumen;
            return true;
        }
        if (NormalizedRequestedViewMode == TEXT("virtualshadowmap"))
        {
            OutViewModeIndex = VMI_VisualizeVirtualShadowMap;
            return true;
        }

        return false;
    }

    /**
     * @brief 将视口类型枚举转换为可序列化文本。
     * @param [in] ViewportType 视口类型枚举。
     * @return FString 视口类型文本。
     */
    FString ViewportTypeToString(ELevelViewportType ViewportType)
    {
        switch (ViewportType)
        {
            case LVT_OrthoXY:
                return TEXT("ortho_xy");
            case LVT_OrthoXZ:
                return TEXT("ortho_xz");
            case LVT_OrthoYZ:
                return TEXT("ortho_yz");
            case LVT_OrthoNegativeXY:
                return TEXT("ortho_negative_xy");
            case LVT_OrthoNegativeXZ:
                return TEXT("ortho_negative_xz");
            case LVT_OrthoNegativeYZ:
                return TEXT("ortho_negative_yz");
            case LVT_Perspective:
                return TEXT("perspective");
            case LVT_OrthoFreelook:
                return TEXT("ortho_freelook");
            default:
                return TEXT("unknown");
        }
    }

    /**
     * @brief 向结果对象写入 FVector 字段。
     * @param [in,out] Result 结果 JSON 对象。
     * @param [in] FieldName 字段名。
     * @param [in] Value 向量值。
     */
    void SetVectorField(const TSharedPtr<FJsonObject>& Result, const FString& FieldName, const FVector& Value)
    {
        TArray<TSharedPtr<FJsonValue>> JsonArray;
        JsonArray.Add(MakeShared<FJsonValueNumber>(Value.X));
        JsonArray.Add(MakeShared<FJsonValueNumber>(Value.Y));
        JsonArray.Add(MakeShared<FJsonValueNumber>(Value.Z));
        Result->SetArrayField(FieldName, JsonArray);
    }

    /**
     * @brief 向结果对象写入 FRotator 字段。
     * @param [in,out] Result 结果 JSON 对象。
     * @param [in] FieldName 字段名。
     * @param [in] Value 旋转值。
     */
    void SetRotatorField(const TSharedPtr<FJsonObject>& Result, const FString& FieldName, const FRotator& Value)
    {
        TArray<TSharedPtr<FJsonValue>> JsonArray;
        JsonArray.Add(MakeShared<FJsonValueNumber>(Value.Pitch));
        JsonArray.Add(MakeShared<FJsonValueNumber>(Value.Yaw));
        JsonArray.Add(MakeShared<FJsonValueNumber>(Value.Roll));
        Result->SetArrayField(FieldName, JsonArray);
    }

    /**
     * @brief 向结果对象写入二维整型数组字段。
     * @param [in,out] Result 结果 JSON 对象。
     * @param [in] FieldName 字段名。
     * @param [in] Value 二维尺寸。
     */
    void SetIntPointField(const TSharedPtr<FJsonObject>& Result, const FString& FieldName, const FIntPoint& Value)
    {
        TArray<TSharedPtr<FJsonValue>> JsonArray;
        JsonArray.Add(MakeShared<FJsonValueNumber>(Value.X));
        JsonArray.Add(MakeShared<FJsonValueNumber>(Value.Y));
        Result->SetArrayField(FieldName, JsonArray);
    }

    /**
     * @brief 输出日志条目快照。
     */
    struct FOutputLogEntrySnapshot
    {
        FString Timestamp;
        FString Category;
        FString Verbosity;
        FString Message;
    };

    /**
     * @brief 捕获输出日志，供 MCP 查询。
     */
    class FUnrealMCPOutputLogCapture : public FOutputDevice
    {
    public:
        FUnrealMCPOutputLogCapture()
        {
            if (GLog)
            {
                GLog->AddOutputDevice(this);
            }
        }

        virtual ~FUnrealMCPOutputLogCapture() override
        {
            if (GLog)
            {
                GLog->RemoveOutputDevice(this);
            }
        }

        virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
        {
            const FScopeLock Lock(&EntriesCriticalSection);

            FOutputLogEntrySnapshot Entry;
            Entry.Timestamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));
            Entry.Category = Category.ToString();
            Entry.Verbosity = LogVerbosityToString(Verbosity);
            Entry.Message = V ? FString(V) : FString();
            Entries.Add(MoveTemp(Entry));

            if (Entries.Num() > MaxEntries)
            {
                Entries.RemoveAt(0, Entries.Num() - MaxEntries, EAllowShrinking::No);
            }
        }

        virtual bool CanBeUsedOnAnyThread() const override
        {
            return true;
        }

        int32 ClearEntries()
        {
            const FScopeLock Lock(&EntriesCriticalSection);
            const int32 ClearedCount = Entries.Num();
            Entries.Reset();
            return ClearedCount;
        }

        TArray<FOutputLogEntrySnapshot> QueryEntries(
            int32 Limit,
            const FString& CategoryFilter,
            const FString& VerbosityFilter,
            const FString& ContainsFilter,
            int32& OutFilteredCount
        ) const
        {
            const FScopeLock Lock(&EntriesCriticalSection);

            const FString NormalizedCategory = CategoryFilter.TrimStartAndEnd();
            const FString NormalizedVerbosity = VerbosityFilter.TrimStartAndEnd();
            const FString NormalizedContains = ContainsFilter.TrimStartAndEnd();

            TArray<FOutputLogEntrySnapshot> FilteredEntries;
            for (const FOutputLogEntrySnapshot& Entry : Entries)
            {
                if (!NormalizedCategory.IsEmpty() && !Entry.Category.Equals(NormalizedCategory, ESearchCase::IgnoreCase))
                {
                    continue;
                }

                if (!NormalizedVerbosity.IsEmpty() && !Entry.Verbosity.Equals(NormalizedVerbosity, ESearchCase::IgnoreCase))
                {
                    continue;
                }

                if (!NormalizedContains.IsEmpty() && !Entry.Message.Contains(NormalizedContains, ESearchCase::IgnoreCase))
                {
                    continue;
                }

                FilteredEntries.Add(Entry);
            }

            OutFilteredCount = FilteredEntries.Num();
            const int32 EffectiveLimit = Limit > 0 ? Limit : FilteredEntries.Num();
            const int32 StartIndex = FMath::Max(0, FilteredEntries.Num() - EffectiveLimit);

            TArray<FOutputLogEntrySnapshot> ResultEntries;
            for (int32 Index = FilteredEntries.Num() - 1; Index >= StartIndex; --Index)
            {
                ResultEntries.Add(FilteredEntries[Index]);
            }
            return ResultEntries;
        }

    private:
        mutable FCriticalSection EntriesCriticalSection;
        TArray<FOutputLogEntrySnapshot> Entries;
        int32 MaxEntries = 2000;
    };

    TSharedPtr<FUnrealMCPOutputLogCapture, ESPMode::ThreadSafe> OutputLogCapture;

    /**
     * @brief 获取全局输出日志捕获器。
     * @return TSharedPtr<FUnrealMCPOutputLogCapture, ESPMode::ThreadSafe> 捕获器实例。
     */
    TSharedPtr<FUnrealMCPOutputLogCapture, ESPMode::ThreadSafe> GetOutputLogCapture()
    {
        if (!OutputLogCapture.IsValid())
        {
            OutputLogCapture = MakeShared<FUnrealMCPOutputLogCapture, ESPMode::ThreadSafe>();
        }
        return OutputLogCapture;
    }
}

/**
 * @brief 构造函数。
 */
FUnrealMCPEditorCommands::FUnrealMCPEditorCommands()
{
    UnrealMCPEditorCommandsPrivate::GetOutputLogCapture();
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
    else if (CommandType == TEXT("start_vr_preview"))
    {
        return HandleStartVRPreview(Params);
    }
    else if (CommandType == TEXT("start_standalone_game"))
    {
        return HandleStartStandaloneGame(Params);
    }
    else if (CommandType == TEXT("stop_pie"))
    {
        return HandleStopPIE(Params);
    }
    else if (CommandType == TEXT("get_play_state"))
    {
        return HandleGetPlayState(Params);
    }
    else if (CommandType == TEXT("start_live_coding"))
    {
        return HandleStartLiveCoding(Params);
    }
    else if (CommandType == TEXT("compile_live_coding"))
    {
        return HandleCompileLiveCoding(Params);
    }
    else if (CommandType == TEXT("get_live_coding_state"))
    {
        return HandleGetLiveCodingState(Params);
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
    else if (CommandType == TEXT("duplicate_actor"))
    {
        return HandleDuplicateActor(Params);
    }
    else if (CommandType == TEXT("select_actor"))
    {
        return HandleSelectActor(Params);
    }
    else if (CommandType == TEXT("get_selected_actors"))
    {
        return HandleGetSelectedActors(Params);
    }
    else if (CommandType == TEXT("get_editor_selection"))
    {
        return HandleGetEditorSelection(Params);
    }
    else if (CommandType == TEXT("attach_actor"))
    {
        return HandleAttachActor(Params);
    }
    else if (CommandType == TEXT("detach_actor"))
    {
        return HandleDetachActor(Params);
    }
    else if (CommandType == TEXT("set_actors_transform"))
    {
        return HandleSetActorsTransform(Params);
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
    else if (CommandType == TEXT("take_highres_screenshot"))
    {
        return HandleTakeHighResScreenshot(Params);
    }
    else if (CommandType == TEXT("capture_viewport_sequence"))
    {
        return HandleCaptureViewportSequence(Params);
    }
    else if (CommandType == TEXT("open_asset_editor"))
    {
        return HandleOpenAssetEditor(Params);
    }
    else if (CommandType == TEXT("close_asset_editor"))
    {
        return HandleCloseAssetEditor(Params);
    }
    else if (CommandType == TEXT("execute_console_command"))
    {
        return HandleExecuteConsoleCommand(Params);
    }
    else if (CommandType == TEXT("execute_unreal_python"))
    {
        return HandleExecuteUnrealPython(Params);
    }
    else if (CommandType == TEXT("run_editor_utility_widget"))
    {
        return HandleRunEditorUtilityWidget(Params);
    }
    else if (CommandType == TEXT("run_editor_utility_blueprint"))
    {
        return HandleRunEditorUtilityBlueprint(Params);
    }
    else if (CommandType == TEXT("set_viewport_mode"))
    {
        return HandleSetViewportMode(Params);
    }
    else if (CommandType == TEXT("get_viewport_camera"))
    {
        return HandleGetViewportCamera(Params);
    }
    else if (CommandType == TEXT("get_output_log"))
    {
        return HandleGetOutputLog(Params);
    }
    else if (CommandType == TEXT("clear_output_log"))
    {
        return HandleClearOutputLog(Params);
    }
    else if (CommandType == TEXT("get_message_log"))
    {
        return HandleGetMessageLog(Params);
    }
    else if (CommandType == TEXT("show_editor_notification"))
    {
        return HandleShowEditorNotification(Params);
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
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("make_directory"),
        Params);
}

/**
 * @brief 复制资产到目标路径。
 * @param [in] Params 复制参数。
 * @return TSharedPtr<FJsonObject> 复制结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("duplicate_asset"),
        Params);
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
 * @brief 启动 VR Preview 会话。
 * @param [in] Params 启动参数（当前预留扩展）。
 * @return TSharedPtr<FJsonObject> 启动结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleStartVRPreview(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor instance is not available"));
    }

    if (GEditor->PlayWorld || GEditor->IsPlaySessionRequestQueued())
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), true);
        ResultObj->SetBoolField(TEXT("already_playing"), true);
        ResultObj->SetBoolField(TEXT("is_vr_preview"), GEditor->IsVRPreviewActive());
        ResultObj->SetStringField(TEXT("message"), TEXT("VR Preview 已在运行或已排队"));
        if (GEditor->PlayWorld)
        {
            UnrealMCPEditorCommandsPrivate::AppendWorldInfo(ResultObj, GEditor->PlayWorld, TEXT("pie"));
        }
        return ResultObj;
    }

    FRequestPlaySessionParams PlaySessionParams;
    PlaySessionParams.SessionPreviewTypeOverride = EPlaySessionPreviewType::VRPreview;

    GEditor->RequestPlaySession(PlaySessionParams);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("already_playing"), false);
    ResultObj->SetBoolField(TEXT("play_world_available"), GEditor->PlayWorld != nullptr);
    ResultObj->SetBoolField(TEXT("request_queued"), GEditor->IsPlaySessionRequestQueued());
    ResultObj->SetBoolField(TEXT("is_vr_preview"), GEditor->IsVRPreviewActive());
    ResultObj->SetStringField(TEXT("message"), TEXT("VR Preview start requested"));
    return ResultObj;
}

/**
 * @brief 启动 Standalone Game（新进程）会话。
 * @param [in] Params 启动参数（可选 map_override/additional_command_line_parameters）。
 * @return TSharedPtr<FJsonObject> 启动结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleStartStandaloneGame(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor instance is not available"));
    }

    if (GEditor->IsPlaySessionInProgress() || GEditor->IsPlayingOnLocalPCSession())
    {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), true);
        ResultObj->SetBoolField(TEXT("already_playing"), true);
        ResultObj->SetBoolField(TEXT("request_queued"), GEditor->IsPlaySessionRequestQueued());
        ResultObj->SetStringField(TEXT("message"), TEXT("已有 Play 会话在运行或排队中"));

        if (const TOptional<FRequestPlaySessionParams> PlaySessionRequest = GEditor->GetPlaySessionRequest(); PlaySessionRequest.IsSet())
        {
            UnrealMCPEditorCommandsPrivate::AppendPlaySessionDestinationInfo(ResultObj, PlaySessionRequest.GetValue());
        }
        else if (const TOptional<FPlayInEditorSessionInfo> SessionInfo = GEditor->GetPlayInEditorSessionInfo(); SessionInfo.IsSet())
        {
            UnrealMCPEditorCommandsPrivate::AppendPlaySessionDestinationInfo(ResultObj, SessionInfo->OriginalRequestParams);
        }
        else
        {
            ResultObj->SetStringField(TEXT("play_session_destination"), TEXT("unknown"));
            ResultObj->SetBoolField(TEXT("is_standalone_game"), false);
        }

        return ResultObj;
    }

    FRequestPlaySessionParams PlaySessionParams;
    PlaySessionParams.SessionDestination = EPlaySessionDestinationType::NewProcess;

    FString MapOverride;
    if (Params.IsValid() && Params->TryGetStringField(TEXT("map_override"), MapOverride))
    {
        MapOverride = MapOverride.TrimStartAndEnd();
        if (!MapOverride.IsEmpty())
        {
            PlaySessionParams.GlobalMapOverride = MapOverride;
        }
    }

    FString AdditionalCommandLineParameters;
    if (Params.IsValid() &&
        Params->TryGetStringField(TEXT("additional_command_line_parameters"), AdditionalCommandLineParameters))
    {
        AdditionalCommandLineParameters = AdditionalCommandLineParameters.TrimStartAndEnd();
        if (!AdditionalCommandLineParameters.IsEmpty())
        {
            PlaySessionParams.AdditionalStandaloneCommandLineParameters = AdditionalCommandLineParameters;
        }
    }

    GEditor->RequestPlaySession(PlaySessionParams);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("already_playing"), false);
    ResultObj->SetBoolField(TEXT("request_queued"), GEditor->IsPlaySessionRequestQueued());
    ResultObj->SetStringField(TEXT("message"), TEXT("Standalone Game start requested"));
    UnrealMCPEditorCommandsPrivate::AppendPlaySessionDestinationInfo(ResultObj, PlaySessionParams);

    if (!PlaySessionParams.GlobalMapOverride.IsEmpty())
    {
        ResultObj->SetStringField(TEXT("map_override"), PlaySessionParams.GlobalMapOverride);
    }

    if (PlaySessionParams.AdditionalStandaloneCommandLineParameters.IsSet())
    {
        ResultObj->SetStringField(
            TEXT("additional_command_line_parameters"),
            PlaySessionParams.AdditionalStandaloneCommandLineParameters.GetValue()
        );
    }

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

    if (GEditor->IsPlayingOnLocalPCSession())
    {
        GEditor->EndPlayOnLocalPc();

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), true);
        ResultObj->SetBoolField(TEXT("already_stopped"), false);
        ResultObj->SetBoolField(TEXT("stopped_standalone_game"), true);
        ResultObj->SetStringField(TEXT("message"), TEXT("Standalone Game stop requested"));
        return ResultObj;
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
    const bool bIsPlaySessionQueued = GEditor->IsPlaySessionRequestQueued();
    const bool bIsVRPreview = GEditor->IsVRPreviewActive();
    bool bIsStandaloneGame = GEditor->IsPlayingOnLocalPCSession();
    FString PlaySessionDestination = TEXT("none");

    if (bIsStandaloneGame)
    {
        PlaySessionDestination = TEXT("new_process");
    }
    else if (const TOptional<FPlayInEditorSessionInfo> SessionInfo = GEditor->GetPlayInEditorSessionInfo(); SessionInfo.IsSet())
    {
        PlaySessionDestination = UnrealMCPEditorCommandsPrivate::PlaySessionDestinationToString(
            SessionInfo->OriginalRequestParams.SessionDestination
        );
        bIsStandaloneGame = SessionInfo->OriginalRequestParams.SessionDestination == EPlaySessionDestinationType::NewProcess &&
            SessionInfo->OriginalRequestParams.SessionPreviewTypeOverride.Get(EPlaySessionPreviewType::NoPreview) != EPlaySessionPreviewType::MobilePreview;
    }
    else if (const TOptional<FRequestPlaySessionParams> PlaySessionRequest = GEditor->GetPlaySessionRequest(); PlaySessionRequest.IsSet())
    {
        PlaySessionDestination = UnrealMCPEditorCommandsPrivate::PlaySessionDestinationToString(
            PlaySessionRequest->SessionDestination
        );
        bIsStandaloneGame = PlaySessionRequest->SessionDestination == EPlaySessionDestinationType::NewProcess &&
            PlaySessionRequest->SessionPreviewTypeOverride.Get(EPlaySessionPreviewType::NoPreview) != EPlaySessionPreviewType::MobilePreview;
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("is_playing"), bIsPlaying);
    ResultObj->SetBoolField(TEXT("is_play_session_queued"), bIsPlaySessionQueued);
    ResultObj->SetBoolField(TEXT("is_vr_preview"), bIsVRPreview);
    ResultObj->SetBoolField(TEXT("is_standalone_game"), bIsStandaloneGame);
    ResultObj->SetStringField(TEXT("play_session_destination"), PlaySessionDestination);
    ResultObj->SetStringField(
        TEXT("play_mode"),
        bIsVRPreview ? TEXT("vr_preview") :
        (bIsStandaloneGame ? (bIsPlaySessionQueued ? TEXT("standalone_game_queued") : TEXT("standalone_game")) :
        (bIsPlaying ? TEXT("pie") : (bIsPlaySessionQueued ? TEXT("queued") : TEXT("stopped")))));
    if (bIsPlaying)
    {
        UnrealMCPEditorCommandsPrivate::AppendWorldInfo(ResultObj, GEditor->PlayWorld, TEXT("pie"));
    }

    return ResultObj;
}

/**
 * @brief 启用当前编辑器会话的 Live Coding。
 * @param [in] Params 启动参数（支持 show_console）。
 * @return TSharedPtr<FJsonObject> 启动结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleStartLiveCoding(const TSharedPtr<FJsonObject>& Params)
{
#if !PLATFORM_WINDOWS
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Live Coding 仅支持 Windows 编辑器"));
#else
    FString ErrorMessage;
    ILiveCodingModule* LiveCodingModule = UnrealMCPEditorCommandsPrivate::LoadLiveCodingModule(ErrorMessage);
    if (!LiveCodingModule)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    bool bShowConsole = true;
    if (Params.IsValid())
    {
        Params->TryGetBoolField(TEXT("show_console"), bShowConsole);
    }

    const bool bWasStarted = LiveCodingModule->HasStarted();
    const bool bWasEnabledForSession = LiveCodingModule->IsEnabledForSession();

    LiveCodingModule->EnableForSession(true);

    if (bShowConsole && LiveCodingModule->HasStarted())
    {
        LiveCodingModule->ShowConsole();
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    UnrealMCPEditorCommandsPrivate::AppendLiveCodingState(ResultObj, *LiveCodingModule);
    ResultObj->SetBoolField(TEXT("success"), LiveCodingModule->HasStarted() && LiveCodingModule->IsEnabledForSession());
    ResultObj->SetBoolField(TEXT("show_console"), bShowConsole);
    ResultObj->SetBoolField(TEXT("was_started"), bWasStarted);
    ResultObj->SetBoolField(TEXT("was_enabled_for_session"), bWasEnabledForSession);

    if (!ResultObj->GetBoolField(TEXT("success")))
    {
        const FString EnableErrorText = ResultObj->GetStringField(TEXT("enable_error"));
        ResultObj->SetStringField(
            TEXT("error"),
            EnableErrorText.IsEmpty() ? TEXT("启用 Live Coding 失败") : EnableErrorText
        );
    }

    return ResultObj;
#endif
}

/**
 * @brief 触发一次 Live Coding 编译。
 * @param [in] Params 编译参数（支持 wait_for_completion/show_console）。
 * @return TSharedPtr<FJsonObject> 编译请求结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleCompileLiveCoding(const TSharedPtr<FJsonObject>& Params)
{
#if !PLATFORM_WINDOWS
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Live Coding 仅支持 Windows 编辑器"));
#else
    FString ErrorMessage;
    ILiveCodingModule* LiveCodingModule = UnrealMCPEditorCommandsPrivate::LoadLiveCodingModule(ErrorMessage);
    if (!LiveCodingModule)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    bool bWaitForCompletion = false;
    bool bShowConsole = true;
    if (Params.IsValid())
    {
        Params->TryGetBoolField(TEXT("wait_for_completion"), bWaitForCompletion);
        Params->TryGetBoolField(TEXT("show_console"), bShowConsole);
    }

    if (bShowConsole)
    {
        LiveCodingModule->ShowConsole();
    }

    ELiveCodingCompileFlags CompileFlags = ELiveCodingCompileFlags::None;
    if (bWaitForCompletion)
    {
        CompileFlags |= ELiveCodingCompileFlags::WaitForCompletion;
    }

    ELiveCodingCompileResult CompileResult = ELiveCodingCompileResult::Failure;
    const bool bCompileRequestAccepted = LiveCodingModule->Compile(CompileFlags, &CompileResult);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bCompileRequestAccepted);
    ResultObj->SetBoolField(TEXT("wait_for_completion"), bWaitForCompletion);
    ResultObj->SetBoolField(TEXT("show_console"), bShowConsole);
    ResultObj->SetStringField(
        TEXT("compile_result"),
        UnrealMCPEditorCommandsPrivate::LiveCodingCompileResultToString(CompileResult)
    );
    UnrealMCPEditorCommandsPrivate::AppendLiveCodingState(ResultObj, *LiveCodingModule);

    if (!bCompileRequestAccepted)
    {
        const FString EnableErrorText = ResultObj->GetStringField(TEXT("enable_error"));
        ResultObj->SetStringField(
            TEXT("error"),
            EnableErrorText.IsEmpty() ? TEXT("Live Coding 编译请求失败") : EnableErrorText
        );
    }

    return ResultObj;
#endif
}

/**
 * @brief 查询 Live Coding 的当前状态。
 * @param [in] Params 查询参数（当前未使用）。
 * @return TSharedPtr<FJsonObject> 状态结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetLiveCodingState(const TSharedPtr<FJsonObject>& Params)
{
#if !PLATFORM_WINDOWS
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Live Coding 仅支持 Windows 编辑器"));
#else
    FString ErrorMessage;
    ILiveCodingModule* LiveCodingModule = UnrealMCPEditorCommandsPrivate::LoadLiveCodingModule(ErrorMessage);
    if (!LiveCodingModule)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    UnrealMCPEditorCommandsPrivate::AppendLiveCodingState(ResultObj, *LiveCodingModule);
    return ResultObj;
#endif
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
    ActorType = ActorType.TrimStartAndEnd();

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

    if (ActorType.Equals(TEXT("StaticMeshActor"), ESearchCase::IgnoreCase))
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
    else if (ActorType.Equals(TEXT("PointLight"), ESearchCase::IgnoreCase))
    {
        NewActor = World->SpawnActor<APointLight>(APointLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType.Equals(TEXT("SpotLight"), ESearchCase::IgnoreCase))
    {
        NewActor = World->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType.Equals(TEXT("DirectionalLight"), ESearchCase::IgnoreCase))
    {
        NewActor = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType.Equals(TEXT("CameraActor"), ESearchCase::IgnoreCase))
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
 * @brief 复制指定 Actor。
 * @param [in] Params 复制参数。
 * @return TSharedPtr<FJsonObject> 复制结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleDuplicateActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ErrorMessage;
    UWorld* ResolvedWorld = nullptr;
    FString ResolvedWorldType;
    AActor* SourceActor = ResolveActorByParams(Params, ErrorMessage, &ResolvedWorld, &ResolvedWorldType);
    if (!SourceActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    UEditorActorSubsystem* ActorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;
    if (!ActorSubsystem)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("EditorActorSubsystem is unavailable"));
    }

    FVector Offset = FVector::ZeroVector;
    if (Params->HasField(TEXT("offset")))
    {
        Offset = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("offset"));
    }

    AActor* DuplicatedActor = ActorSubsystem->DuplicateActor(SourceActor, ResolvedWorld, Offset);
    if (!DuplicatedActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to duplicate actor: %s"), *SourceActor->GetName()));
    }

    TSharedPtr<FJsonObject> Result = FUnrealMCPCommonUtils::ActorToJsonObject(DuplicatedActor, true);
    UnrealMCPEditorCommandsPrivate::AppendWorldInfo(Result, ResolvedWorld, ResolvedWorldType);
    Result->SetStringField(TEXT("source_actor"), SourceActor->GetName());
    return Result;
}

/**
 * @brief 选中指定 Actor。
 * @param [in] Params 选中参数。
 * @return TSharedPtr<FJsonObject> 选中结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSelectActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ErrorMessage;
    UWorld* ResolvedWorld = nullptr;
    FString ResolvedWorldType;
    AActor* TargetActor = ResolveActorByParams(Params, ErrorMessage, &ResolvedWorld, &ResolvedWorldType);
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    const bool bReplaceSelection = Params->HasTypedField<EJson::Boolean>(TEXT("replace_selection"))
        ? Params->GetBoolField(TEXT("replace_selection"))
        : false;
    const bool bNotify = Params->HasTypedField<EJson::Boolean>(TEXT("notify"))
        ? Params->GetBoolField(TEXT("notify"))
        : true;
    const bool bSelectEvenIfHidden = Params->HasTypedField<EJson::Boolean>(TEXT("select_even_if_hidden"))
        ? Params->GetBoolField(TEXT("select_even_if_hidden"))
        : true;

    if (!GEditor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor instance is not available"));
    }

    if (bReplaceSelection)
    {
        GEditor->SelectNone(false, true, false);
    }

    GEditor->GetSelectedActors()->Modify();
    GEditor->SelectActor(TargetActor, true, bNotify, bSelectEvenIfHidden);

    TSharedPtr<FJsonObject> Result = FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
    UnrealMCPEditorCommandsPrivate::AppendWorldInfo(Result, ResolvedWorld, ResolvedWorldType);
    Result->SetBoolField(TEXT("selected"), true);
    return Result;
}

/**
 * @brief 获取当前选中的 Actor 列表。
 * @param [in] Params 查询参数。
 * @return TSharedPtr<FJsonObject> 选中 Actor 结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetSelectedActors(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor instance is not available"));
    }

    const bool bIncludeComponents = Params->HasTypedField<EJson::Boolean>(TEXT("include_components"))
        ? Params->GetBoolField(TEXT("include_components"))
        : false;
    const bool bDetailedComponents = Params->HasTypedField<EJson::Boolean>(TEXT("detailed_components"))
        ? Params->GetBoolField(TEXT("detailed_components"))
        : true;

    USelection* SelectedActors = GEditor->GetSelectedActors();
    TArray<TSharedPtr<FJsonValue>> ActorValues;

    if (SelectedActors)
    {
        for (FSelectionIterator It(*SelectedActors); It; ++It)
        {
            AActor* Actor = Cast<AActor>(*It);
            if (!Actor)
            {
                continue;
            }

            ActorValues.Add(FUnrealMCPCommonUtils::ActorToJson(Actor, bIncludeComponents, bDetailedComponents));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("actors"), ActorValues);
    Result->SetNumberField(TEXT("actor_count"), ActorValues.Num());
    return Result;
}

/**
 * @brief 获取编辑器当前选中的 Actor 与资产。
 * @param [in] Params 查询参数。
 * @return TSharedPtr<FJsonObject> 聚合后的选中结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetEditorSelection(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor instance is not available"));
    }

    const bool bIncludeComponents = Params->HasTypedField<EJson::Boolean>(TEXT("include_components"))
        ? Params->GetBoolField(TEXT("include_components"))
        : false;
    const bool bDetailedComponents = Params->HasTypedField<EJson::Boolean>(TEXT("detailed_components"))
        ? Params->GetBoolField(TEXT("detailed_components"))
        : true;
    const bool bIncludeTags = Params->HasTypedField<EJson::Boolean>(TEXT("include_tags"))
        ? Params->GetBoolField(TEXT("include_tags"))
        : false;

    TArray<TSharedPtr<FJsonValue>> ActorValues;
    if (USelection* SelectedActors = GEditor->GetSelectedActors())
    {
        for (FSelectionIterator It(*SelectedActors); It; ++It)
        {
            AActor* Actor = Cast<AActor>(*It);
            if (!Actor)
            {
                continue;
            }

            ActorValues.Add(FUnrealMCPCommonUtils::ActorToJson(Actor, bIncludeComponents, bDetailedComponents));
        }
    }

    TArray<TSharedPtr<FJsonValue>> AssetValues;
    const TArray<FAssetData> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssetData();
    AssetValues.Reserve(SelectedAssets.Num());

    for (const FAssetData& AssetData : SelectedAssets)
    {
        TSharedPtr<FJsonObject> AssetObject = UnrealMCPEditorCommandsPrivate::CreateAssetIdentityObject(AssetData);
        if (bIncludeTags)
        {
            UnrealMCPEditorCommandsPrivate::AppendAssetTagsToObject(AssetData, AssetObject, 128);
        }
        AssetValues.Add(MakeShared<FJsonValueObject>(AssetObject));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("actors"), ActorValues);
    Result->SetNumberField(TEXT("actor_count"), ActorValues.Num());
    Result->SetArrayField(TEXT("assets"), AssetValues);
    Result->SetNumberField(TEXT("asset_count"), AssetValues.Num());
    Result->SetNumberField(TEXT("selection_count"), ActorValues.Num() + AssetValues.Num());

    if (UWorld* EditorWorld = UnrealMCPEditorCommandsPrivate::GetEditorWorld())
    {
        UnrealMCPEditorCommandsPrivate::AppendWorldInfo(Result, EditorWorld, TEXT("editor"));
    }

    return Result;
}

/**
 * @brief 将子 Actor 挂接到父 Actor。
 * @param [in] Params 挂接参数。
 * @return TSharedPtr<FJsonObject> 挂接结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleAttachActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ErrorMessage;
    UWorld* ResolvedWorld = nullptr;
    FString ResolvedWorldType;
    AActor* ChildActor = ResolveActorByParams(Params, ErrorMessage, &ResolvedWorld, &ResolvedWorldType);
    if (!ChildActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    TSharedPtr<FJsonObject> ParentParams = MakeShared<FJsonObject>();
    FString ParentName;
    FString ParentActorPath;
    if (Params->TryGetStringField(TEXT("parent_name"), ParentName))
    {
        ParentParams->SetStringField(TEXT("name"), ParentName);
    }
    if (Params->TryGetStringField(TEXT("parent_actor_path"), ParentActorPath))
    {
        ParentParams->SetStringField(TEXT("actor_path"), ParentActorPath);
    }
    if (Params->HasTypedField<EJson::String>(TEXT("world_type")))
    {
        ParentParams->SetStringField(TEXT("world_type"), Params->GetStringField(TEXT("world_type")));
    }

    AActor* ParentActor = ResolveActorByParams(ParentParams, ErrorMessage, nullptr, nullptr);
    if (!ParentActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to resolve parent actor: %s"), *ErrorMessage));
    }

    const bool bKeepWorldTransform = Params->HasTypedField<EJson::Boolean>(TEXT("keep_world_transform"))
        ? Params->GetBoolField(TEXT("keep_world_transform"))
        : true;
    FString SocketName;
    Params->TryGetStringField(TEXT("socket_name"), SocketName);

    const FAttachmentTransformRules AttachmentRules = bKeepWorldTransform
        ? FAttachmentTransformRules::KeepWorldTransform
        : FAttachmentTransformRules::KeepRelativeTransform;

    if (!ChildActor->AttachToActor(ParentActor, AttachmentRules, FName(*SocketName)))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to attach actor"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("child_actor"), ChildActor->GetName());
    Result->SetStringField(TEXT("parent_actor"), ParentActor->GetName());
    Result->SetStringField(TEXT("socket_name"), SocketName);
    UnrealMCPEditorCommandsPrivate::AppendWorldInfo(Result, ResolvedWorld, ResolvedWorldType);
    return Result;
}

/**
 * @brief 将 Actor 从父 Actor 分离。
 * @param [in] Params 分离参数。
 * @return TSharedPtr<FJsonObject> 分离结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleDetachActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ErrorMessage;
    UWorld* ResolvedWorld = nullptr;
    FString ResolvedWorldType;
    AActor* TargetActor = ResolveActorByParams(Params, ErrorMessage, &ResolvedWorld, &ResolvedWorldType);
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    const bool bKeepWorldTransform = Params->HasTypedField<EJson::Boolean>(TEXT("keep_world_transform"))
        ? Params->GetBoolField(TEXT("keep_world_transform"))
        : true;
    const FDetachmentTransformRules DetachmentRules = bKeepWorldTransform
        ? FDetachmentTransformRules::KeepWorldTransform
        : FDetachmentTransformRules::KeepRelativeTransform;

    TargetActor->DetachFromActor(DetachmentRules);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actor"), TargetActor->GetName());
    UnrealMCPEditorCommandsPrivate::AppendWorldInfo(Result, ResolvedWorld, ResolvedWorldType);
    return Result;
}

/**
 * @brief 批量设置多个 Actor 的 Transform。
 * @param [in] Params 批量变换参数。
 * @return TSharedPtr<FJsonObject> 更新结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorsTransform(const TSharedPtr<FJsonObject>& Params)
{
    const TArray<TSharedPtr<FJsonValue>>* ActorNames = nullptr;
    if (!Params->TryGetArrayField(TEXT("actor_names"), ActorNames) || !ActorNames || ActorNames->Num() == 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_names' parameter"));
    }

    const bool bHasLocation = Params->HasField(TEXT("location"));
    const bool bHasRotation = Params->HasField(TEXT("rotation"));
    const bool bHasScale = Params->HasField(TEXT("scale"));

    const FVector Location = bHasLocation ? FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")) : FVector::ZeroVector;
    const FRotator Rotation = bHasRotation ? FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation")) : FRotator::ZeroRotator;
    const FVector Scale = bHasScale ? FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")) : FVector(1.0f, 1.0f, 1.0f);

    TArray<TSharedPtr<FJsonValue>> UpdatedActors;
    FString FirstResolvedWorldType;
    UWorld* FirstResolvedWorld = nullptr;

    for (const TSharedPtr<FJsonValue>& ActorNameValue : *ActorNames)
    {
        const FString ActorName = ActorNameValue.IsValid() ? ActorNameValue->AsString() : TEXT("");
        if (ActorName.IsEmpty())
        {
            continue;
        }

        TSharedPtr<FJsonObject> SingleActorParams = MakeShared<FJsonObject>();
        SingleActorParams->SetStringField(TEXT("name"), ActorName);
        if (Params->HasTypedField<EJson::String>(TEXT("world_type")))
        {
            SingleActorParams->SetStringField(TEXT("world_type"), Params->GetStringField(TEXT("world_type")));
        }

        FString ErrorMessage;
        UWorld* ResolvedWorld = nullptr;
        FString ResolvedWorldType;
        AActor* Actor = ResolveActorByParams(SingleActorParams, ErrorMessage, &ResolvedWorld, &ResolvedWorldType);
        if (!Actor)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to resolve actor '%s': %s"), *ActorName, *ErrorMessage));
        }

        if (!FirstResolvedWorld)
        {
            FirstResolvedWorld = ResolvedWorld;
            FirstResolvedWorldType = ResolvedWorldType;
        }

        if (bHasLocation)
        {
            Actor->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
        }
        if (bHasRotation)
        {
            Actor->SetActorRotation(Rotation, ETeleportType::TeleportPhysics);
        }
        if (bHasScale)
        {
            Actor->SetActorScale3D(Scale);
        }

        UpdatedActors.Add(FUnrealMCPCommonUtils::ActorToJson(Actor, false, true));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("actors"), UpdatedActors);
    Result->SetNumberField(TEXT("actor_count"), UpdatedActors.Num());
    UnrealMCPEditorCommandsPrivate::AppendWorldInfo(Result, FirstResolvedWorld, FirstResolvedWorldType);
    return Result;
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

/**
 * @brief 请求一张高分辨率视口截图。
 * @param [in] Params 截图参数。
 * @return TSharedPtr<FJsonObject> 请求结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleTakeHighResScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    FString FilePath;
    if (!Params->TryGetStringField(TEXT("filepath"), FilePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'filepath' parameter"));
    }

    bool bCaptureHDR = false;
    Params->TryGetBoolField(TEXT("capture_hdr"), bCaptureHDR);
    const FString DefaultExtension = bCaptureHDR ? TEXT(".exr") : TEXT(".png");
    const FString NormalizedFilePath = UnrealMCPEditorCommandsPrivate::NormalizeCaptureFilePath(FilePath, DefaultExtension);

    FIntPoint RequestedResolution = FIntPoint::ZeroValue;
    if (Params->HasField(TEXT("resolution")))
    {
        FString ErrorMessage;
        if (!UnrealMCPEditorCommandsPrivate::TryGetIntPointField(Params, TEXT("resolution"), RequestedResolution, ErrorMessage))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
        }

        if (RequestedResolution.X <= 0 || RequestedResolution.Y <= 0)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Parameter 'resolution' must contain positive values"));
        }
    }

    double ResolutionMultiplierNumber = 1.0;
    if (Params->HasField(TEXT("resolution_multiplier")))
    {
        if (!Params->TryGetNumberField(TEXT("resolution_multiplier"), ResolutionMultiplierNumber))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Parameter 'resolution_multiplier' must be numeric"));
        }

        if (ResolutionMultiplierNumber <= 0.0)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Parameter 'resolution_multiplier' must be greater than 0"));
        }
    }

    FString ErrorMessage;
    if (!UnrealMCPEditorCommandsPrivate::RequestViewportScreenshot(
            NormalizedFilePath,
            true,
            RequestedResolution,
            static_cast<float>(ResolutionMultiplierNumber),
            false,
            bCaptureHDR,
            ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("filepath"), NormalizedFilePath);
    ResultObj->SetBoolField(TEXT("capture_hdr"), bCaptureHDR);
    ResultObj->SetBoolField(TEXT("written"), !bCaptureHDR);
    ResultObj->SetBoolField(TEXT("request_queued"), bCaptureHDR);
    ResultObj->SetNumberField(TEXT("resolution_multiplier"), ResolutionMultiplierNumber);
    if (RequestedResolution.X > 0 && RequestedResolution.Y > 0)
    {
        UnrealMCPEditorCommandsPrivate::SetIntPointField(ResultObj, TEXT("resolution"), RequestedResolution);
    }

    return ResultObj;
}

/**
 * @brief 请求捕获当前活动视口的序列帧。
 * @param [in] Params 序列捕获参数。
 * @return TSharedPtr<FJsonObject> 请求结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleCaptureViewportSequence(const TSharedPtr<FJsonObject>& Params)
{
    if (UnrealMCPEditorCommandsPrivate::GViewportSequenceCaptureState.IsValid())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Viewport sequence capture is already running"));
    }

    FString OutputDirectory;
    if (!Params->TryGetStringField(TEXT("output_dir"), OutputDirectory))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'output_dir' parameter"));
    }

    double FrameCountNumber = 0.0;
    if (!Params->TryGetNumberField(TEXT("frame_count"), FrameCountNumber))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'frame_count' parameter"));
    }

    const int32 FrameCount = FMath::RoundToInt(FrameCountNumber);
    if (FrameCount <= 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Parameter 'frame_count' must be greater than 0"));
    }

    double IntervalSeconds = 0.0;
    if (Params->HasField(TEXT("interval_seconds")) && !Params->TryGetNumberField(TEXT("interval_seconds"), IntervalSeconds))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Parameter 'interval_seconds' must be numeric"));
    }
    if (IntervalSeconds < 0.0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Parameter 'interval_seconds' must be non-negative"));
    }

    bool bUseHighRes = false;
    Params->TryGetBoolField(TEXT("use_high_res"), bUseHighRes);

    bool bShowUI = false;
    Params->TryGetBoolField(TEXT("show_ui"), bShowUI);

    bool bCaptureHDR = false;
    Params->TryGetBoolField(TEXT("capture_hdr"), bCaptureHDR);

    FIntPoint RequestedResolution = FIntPoint::ZeroValue;
    if (Params->HasField(TEXT("resolution")))
    {
        FString ErrorMessage;
        if (!UnrealMCPEditorCommandsPrivate::TryGetIntPointField(Params, TEXT("resolution"), RequestedResolution, ErrorMessage))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
        }

        if (RequestedResolution.X <= 0 || RequestedResolution.Y <= 0)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Parameter 'resolution' must contain positive values"));
        }
    }

    double ResolutionMultiplierNumber = 1.0;
    if (Params->HasField(TEXT("resolution_multiplier")))
    {
        if (!Params->TryGetNumberField(TEXT("resolution_multiplier"), ResolutionMultiplierNumber))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Parameter 'resolution_multiplier' must be numeric"));
        }

        if (ResolutionMultiplierNumber <= 0.0)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Parameter 'resolution_multiplier' must be greater than 0"));
        }
    }

    FString BaseFilename = TEXT("ViewportSequence");
    Params->TryGetStringField(TEXT("base_filename"), BaseFilename);
    BaseFilename = BaseFilename.TrimStartAndEnd();
    if (BaseFilename.IsEmpty())
    {
        BaseFilename = TEXT("ViewportSequence");
    }

    const FString NormalizedOutputDirectory = FPaths::ConvertRelativePathToFull(OutputDirectory);
    if (!IFileManager::Get().DirectoryExists(*NormalizedOutputDirectory) &&
        !IFileManager::Get().MakeDirectory(*NormalizedOutputDirectory, true))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to create output directory: %s"), *NormalizedOutputDirectory)
        );
    }

    TSharedPtr<UnrealMCPEditorCommandsPrivate::FViewportSequenceCaptureState> SequenceState =
        MakeShared<UnrealMCPEditorCommandsPrivate::FViewportSequenceCaptureState>();
    SequenceState->SequenceId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    SequenceState->OutputDirectory = NormalizedOutputDirectory;
    SequenceState->BaseFilename = BaseFilename;
    SequenceState->FrameCount = FrameCount;
    SequenceState->IntervalSeconds = IntervalSeconds;
    SequenceState->NextCaptureTimeSeconds = 0.0;
    SequenceState->bUseHighRes = bUseHighRes;
    SequenceState->bShowUI = bShowUI;
    SequenceState->bCaptureHDR = bCaptureHDR;
    SequenceState->RequestedResolution = RequestedResolution;
    SequenceState->ResolutionMultiplier = static_cast<float>(ResolutionMultiplierNumber);
    SequenceState->PlannedFilePaths.Reserve(FrameCount);
    for (int32 FrameIndex = 1; FrameIndex <= FrameCount; ++FrameIndex)
    {
        SequenceState->PlannedFilePaths.Add(
            UnrealMCPEditorCommandsPrivate::BuildViewportSequenceFramePath(
                SequenceState->OutputDirectory,
                SequenceState->BaseFilename,
                FrameIndex,
                bCaptureHDR
            )
        );
    }

    if (!(bUseHighRes && !bCaptureHDR))
    {
        SequenceState->ScreenshotDelegateHandle = FScreenshotRequest::OnScreenshotCaptured().AddStatic(
            &UnrealMCPEditorCommandsPrivate::HandleViewportSequenceScreenshotCaptured
        );
    }
    SequenceState->TickHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateStatic(&UnrealMCPEditorCommandsPrivate::TickViewportSequenceCapture),
        0.0f
    );
    UnrealMCPEditorCommandsPrivate::GViewportSequenceCaptureState = SequenceState;

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("request_queued"), true);
    ResultObj->SetStringField(TEXT("sequence_id"), SequenceState->SequenceId);
    ResultObj->SetStringField(TEXT("output_dir"), SequenceState->OutputDirectory);
    ResultObj->SetStringField(TEXT("base_filename"), SequenceState->BaseFilename);
    ResultObj->SetNumberField(TEXT("frame_count"), SequenceState->FrameCount);
    ResultObj->SetNumberField(TEXT("interval_seconds"), SequenceState->IntervalSeconds);
    ResultObj->SetBoolField(TEXT("use_high_res"), SequenceState->bUseHighRes);
    ResultObj->SetBoolField(TEXT("show_ui"), SequenceState->bShowUI);
    ResultObj->SetBoolField(TEXT("capture_hdr"), SequenceState->bCaptureHDR);
    ResultObj->SetNumberField(TEXT("resolution_multiplier"), SequenceState->ResolutionMultiplier);
    if (RequestedResolution.X > 0 && RequestedResolution.Y > 0)
    {
        UnrealMCPEditorCommandsPrivate::SetIntPointField(ResultObj, TEXT("resolution"), RequestedResolution);
    }
    UnrealMCPEditorCommandsPrivate::SetStringArrayField(
        ResultObj,
        TEXT("planned_filepaths"),
        SequenceState->PlannedFilePaths
    );

    UE_LOG(
        LogTemp,
        Display,
        TEXT("已启动视口序列捕获：SequenceId=%s Frames=%d HighRes=%d OutputDir=%s"),
        *SequenceState->SequenceId,
        SequenceState->FrameCount,
        SequenceState->bUseHighRes ? 1 : 0,
        *SequenceState->OutputDirectory
    );

    return ResultObj;
}

/**
 * @brief 打开指定资产的编辑器。
 * @param [in] Params 打开参数。
 * @return TSharedPtr<FJsonObject> 执行结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleOpenAssetEditor(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }

    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath));
    }

    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
    if (!AssetEditorSubsystem)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AssetEditorSubsystem is unavailable"));
    }

    const bool bOpened = AssetEditorSubsystem->OpenEditorForAsset(Asset);
    if (!bOpened)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to open asset editor: %s"), *AssetPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("object_path"), Asset->GetPathName());
    return Result;
}

/**
 * @brief 关闭指定资产的编辑器标签。
 * @param [in] Params 关闭参数。
 * @return TSharedPtr<FJsonObject> 执行结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleCloseAssetEditor(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }

    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath));
    }

    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
    if (!AssetEditorSubsystem)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AssetEditorSubsystem is unavailable"));
    }

    const int32 ClosedEditorCount = AssetEditorSubsystem->CloseAllEditorsForAsset(Asset);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("object_path"), Asset->GetPathName());
    Result->SetNumberField(TEXT("closed_editor_count"), ClosedEditorCount);
    Result->SetBoolField(TEXT("had_open_editor"), ClosedEditorCount > 0);
    return Result;
}

/**
 * @brief 执行 Unreal 控制台命令。
 * @param [in] Params 执行参数。
 * @return TSharedPtr<FJsonObject> 执行结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleExecuteConsoleCommand(const TSharedPtr<FJsonObject>& Params)
{
    FString Command;
    if (!Params->TryGetStringField(TEXT("command"), Command) || Command.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'command' parameter"));
    }

    FString ResolvedWorldType;
    FString ErrorMessage;
    UWorld* World = UnrealMCPEditorCommandsPrivate::ResolveWorldByParams(Params, ResolvedWorldType, ErrorMessage);
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    const bool bExecuted = (GEditor && GEditor->Exec(World, *Command)) || (GEngine && GEngine->Exec(World, *Command));
    if (!bExecuted)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to execute console command: %s"), *Command));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("command"), Command);
    UnrealMCPEditorCommandsPrivate::AppendWorldInfo(Result, World, ResolvedWorldType);
    return Result;
}

/**
 * @brief 执行 Unreal Python 命令或脚本。
 * @param [in] Params 执行参数。
 * @return TSharedPtr<FJsonObject> 执行结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleExecuteUnrealPython(const TSharedPtr<FJsonObject>& Params)
{
    FString Command;
    if (!Params->TryGetStringField(TEXT("command"), Command) || Command.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'command' parameter"));
    }

    IPythonScriptPlugin* PythonScriptPlugin = IPythonScriptPlugin::Get();
    if (!PythonScriptPlugin)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("PythonScriptPlugin 未启用或未加载"));
    }

    PythonScriptPlugin->ForceEnablePythonAtRuntime();
    if (!PythonScriptPlugin->IsPythonInitialized())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("PythonScriptPlugin 尚未初始化完成"));
    }

    FString RequestedExecutionModeText = TEXT("Auto");
    Params->TryGetStringField(TEXT("execution_mode"), RequestedExecutionModeText);

    EPythonCommandExecutionMode ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
    bool bExecutionModeAutoSelected = false;
    if (!UnrealMCPEditorCommandsPrivate::TryResolvePythonExecutionMode(
        RequestedExecutionModeText,
        Command,
        ExecutionMode,
        bExecutionModeAutoSelected))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Invalid 'execution_mode': %s"), *RequestedExecutionModeText));
    }

    FString FileExecutionScopeText = TEXT("Private");
    Params->TryGetStringField(TEXT("file_execution_scope"), FileExecutionScopeText);

    EPythonFileExecutionScope FileExecutionScope = EPythonFileExecutionScope::Private;
    if (!UnrealMCPEditorCommandsPrivate::TryParsePythonFileExecutionScope(FileExecutionScopeText, FileExecutionScope))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Invalid 'file_execution_scope': %s"), *FileExecutionScopeText));
    }

    const bool bUnattended = Params->HasTypedField<EJson::Boolean>(TEXT("unattended"))
        ? Params->GetBoolField(TEXT("unattended"))
        : false;

    FPythonCommandEx PythonCommand;
    PythonCommand.Command = Command;
    PythonCommand.ExecutionMode = ExecutionMode;
    PythonCommand.FileExecutionScope = FileExecutionScope;
    PythonCommand.Flags = bUnattended ? EPythonCommandFlags::Unattended : EPythonCommandFlags::None;

    const bool bExecutionSuccess = PythonScriptPlugin->ExecPythonCommandEx(PythonCommand);

    TArray<TSharedPtr<FJsonValue>> LogOutputValues;
    TArray<FString> StdOutLines;
    TArray<FString> WarningLines;
    TArray<FString> ErrorLines;
    for (const FPythonLogOutputEntry& Entry : PythonCommand.LogOutput)
    {
        TSharedPtr<FJsonObject> EntryObject = MakeShared<FJsonObject>();
        EntryObject->SetStringField(TEXT("type"), LexToString(Entry.Type));
        EntryObject->SetStringField(TEXT("output"), Entry.Output);
        LogOutputValues.Add(MakeShared<FJsonValueObject>(EntryObject));

        switch (Entry.Type)
        {
            case EPythonLogOutputType::Info:
                StdOutLines.Add(Entry.Output);
                break;
            case EPythonLogOutputType::Warning:
                WarningLines.Add(Entry.Output);
                break;
            case EPythonLogOutputType::Error:
                ErrorLines.Add(Entry.Output);
                break;
            default:
                break;
        }
    }

    const bool bInputContainsNewlines = UnrealMCPEditorCommandsPrivate::IsPythonCommandMultiLine(Command);
    const bool bCommandLooksLikeFile = UnrealMCPEditorCommandsPrivate::LooksLikePythonFileCommand(Command);
    const FString TracebackText = bExecutionSuccess ? FString() : PythonCommand.CommandResult;

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetBoolField(TEXT("execution_success"), bExecutionSuccess);
    Result->SetBoolField(TEXT("script_executed"), bExecutionSuccess);
    Result->SetStringField(TEXT("command"), Command);
    Result->SetStringField(TEXT("requested_execution_mode"), RequestedExecutionModeText);
    Result->SetStringField(TEXT("execution_mode"), LexToString(ExecutionMode));
    Result->SetBoolField(TEXT("execution_mode_auto_selected"), bExecutionModeAutoSelected);
    Result->SetStringField(TEXT("file_execution_scope"), FileExecutionScope == EPythonFileExecutionScope::Public ? TEXT("Public") : TEXT("Private"));
    Result->SetBoolField(TEXT("unattended"), bUnattended);
    Result->SetBoolField(TEXT("input_contains_newlines"), bInputContainsNewlines);
    Result->SetBoolField(TEXT("command_looks_like_file"), bCommandLooksLikeFile);
    Result->SetStringField(TEXT("command_result"), PythonCommand.CommandResult);
    Result->SetStringField(TEXT("stdout"), FString::Join(StdOutLines, TEXT("")));
    Result->SetStringField(TEXT("warning_output"), FString::Join(WarningLines, TEXT("")));
    Result->SetStringField(TEXT("stderr"), FString::Join(ErrorLines, TEXT("")));
    Result->SetStringField(TEXT("traceback"), TracebackText);
    UnrealMCPEditorCommandsPrivate::AppendPythonTextArrayField(Result, TEXT("stdout_lines"), StdOutLines);
    UnrealMCPEditorCommandsPrivate::AppendPythonTextArrayField(Result, TEXT("warning_lines"), WarningLines);
    UnrealMCPEditorCommandsPrivate::AppendPythonTextArrayField(Result, TEXT("stderr_lines"), ErrorLines);
    Result->SetArrayField(TEXT("log_output"), LogOutputValues);
    return Result;
}

/**
 * @brief 运行 Editor Utility Widget 并打开标签页。
 * @param [in] Params 运行参数（包含 asset_path，可选 tab_id）。
 * @return TSharedPtr<FJsonObject> 运行结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleRunEditorUtilityWidget(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }

    UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>() : nullptr;
    if (!EditorUtilitySubsystem)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("EditorUtilitySubsystem is unavailable"));
    }

    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    UEditorUtilityWidgetBlueprint* UtilityWidgetBlueprint = Cast<UEditorUtilityWidgetBlueprint>(Asset);
    if (!UtilityWidgetBlueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset is not an Editor Utility Widget Blueprint: %s"), *AssetPath));
    }

    FString RequestedTabId;
    Params->TryGetStringField(TEXT("tab_id"), RequestedTabId);

    UEditorUtilityWidget* SpawnedWidget = nullptr;
    FName ResolvedTabId = NAME_None;
    if (RequestedTabId.TrimStartAndEnd().IsEmpty())
    {
        SpawnedWidget = EditorUtilitySubsystem->SpawnAndRegisterTabAndGetID(UtilityWidgetBlueprint, ResolvedTabId);
    }
    else
    {
        ResolvedTabId = FName(*RequestedTabId);
        SpawnedWidget = EditorUtilitySubsystem->SpawnAndRegisterTabWithId(UtilityWidgetBlueprint, ResolvedTabId);
    }

    if (!SpawnedWidget)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to run Editor Utility Widget: %s"), *AssetPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("object_path"), UtilityWidgetBlueprint->GetPathName());
    Result->SetStringField(TEXT("tab_id"), ResolvedTabId.ToString());
    Result->SetStringField(TEXT("widget_name"), SpawnedWidget->GetName());
    Result->SetStringField(TEXT("widget_class"), SpawnedWidget->GetClass()->GetPathName());
    return Result;
}

/**
 * @brief 运行 Editor Utility Blueprint 的 Run 入口。
 * @param [in] Params 运行参数（包含 asset_path）。
 * @return TSharedPtr<FJsonObject> 运行结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleRunEditorUtilityBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' parameter"));
    }

    UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>() : nullptr;
    if (!EditorUtilitySubsystem)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("EditorUtilitySubsystem is unavailable"));
    }

    UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath));
    }

    if (Cast<UEditorUtilityWidgetBlueprint>(Asset))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Editor Utility Widget 请改用 run_editor_utility_widget"));
    }

    UEditorUtilityBlueprint* UtilityBlueprint = Cast<UEditorUtilityBlueprint>(Asset);
    if (!UtilityBlueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Asset is not an Editor Utility Blueprint: %s"), *AssetPath));
    }

    if (!EditorUtilitySubsystem->CanRun(Asset))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Editor Utility Blueprint 不可运行或缺少有效 Run 入口: %s"), *AssetPath));
    }

    if (!EditorUtilitySubsystem->TryRun(Asset))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Failed to run Editor Utility Blueprint: %s"), *AssetPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("object_path"), UtilityBlueprint->GetPathName());
    Result->SetStringField(TEXT("generated_class"), UtilityBlueprint->GeneratedClass ? UtilityBlueprint->GeneratedClass->GetPathName() : TEXT(""));
    return Result;
}

/**
 * @brief 设置关卡视口显示模式。
 * @param [in] Params 设置参数（包含 view_mode，可选 apply_to_all）。
 * @return TSharedPtr<FJsonObject> 设置结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetViewportMode(const TSharedPtr<FJsonObject>& Params)
{
    FString RequestedViewMode;
    if (!Params->TryGetStringField(TEXT("view_mode"), RequestedViewMode) || RequestedViewMode.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'view_mode' parameter"));
    }

    EViewModeIndex ViewModeIndex = VMI_Lit;
    if (!UnrealMCPEditorCommandsPrivate::TryResolveViewModeIndex(RequestedViewMode, ViewModeIndex))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Invalid 'view_mode': %s"), *RequestedViewMode));
    }

    const bool bApplyToAll = Params->HasTypedField<EJson::Boolean>(TEXT("apply_to_all"))
        ? Params->GetBoolField(TEXT("apply_to_all"))
        : false;

    TArray<FLevelEditorViewportClient*> TargetViewports;
    if (bApplyToAll)
    {
        if (!GEditor)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor instance is not available"));
        }

        for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
        {
            if (ViewportClient)
            {
                TargetViewports.Add(ViewportClient);
            }
        }
    }
    else
    {
        FString ErrorMessage;
        if (FLevelEditorViewportClient* ActiveViewportClient = UnrealMCPEditorCommandsPrivate::GetActiveLevelViewportClient(ErrorMessage))
        {
            TargetViewports.Add(ActiveViewportClient);
        }
        else
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
        }
    }

    if (TargetViewports.Num() == 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No level viewport is available"));
    }

    for (FLevelEditorViewportClient* ViewportClient : TargetViewports)
    {
        ViewportClient->SetViewMode(ViewModeIndex);
        ViewportClient->Invalidate();
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("view_mode"), UnrealMCPEditorCommandsPrivate::ViewModeIndexToString(ViewModeIndex));
    Result->SetNumberField(TEXT("view_mode_index"), static_cast<int32>(ViewModeIndex));
    Result->SetBoolField(TEXT("apply_to_all"), bApplyToAll);
    Result->SetNumberField(TEXT("updated_viewport_count"), TargetViewports.Num());
    return Result;
}

/**
 * @brief 获取当前活动关卡视口的摄像机信息。
 * @param [in] Params 查询参数（当前未使用）。
 * @return TSharedPtr<FJsonObject> 视口摄像机信息。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetViewportCamera(const TSharedPtr<FJsonObject>& Params)
{
    (void)Params;

    FString ErrorMessage;
    FLevelEditorViewportClient* ViewportClient = UnrealMCPEditorCommandsPrivate::GetActiveLevelViewportClient(ErrorMessage);
    if (!ViewportClient)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    UnrealMCPEditorCommandsPrivate::SetVectorField(Result, TEXT("location"), ViewportClient->GetViewLocation());
    UnrealMCPEditorCommandsPrivate::SetRotatorField(Result, TEXT("rotation"), ViewportClient->GetViewRotation());
    Result->SetNumberField(TEXT("fov"), ViewportClient->ViewFOV);
    Result->SetStringField(TEXT("view_mode"), UnrealMCPEditorCommandsPrivate::ViewModeIndexToString(ViewportClient->GetViewMode()));
    Result->SetNumberField(TEXT("view_mode_index"), static_cast<int32>(ViewportClient->GetViewMode()));
    Result->SetBoolField(TEXT("is_perspective"), ViewportClient->IsPerspective());
    Result->SetBoolField(TEXT("is_realtime"), ViewportClient->IsRealtime());
    Result->SetStringField(TEXT("viewport_type"), UnrealMCPEditorCommandsPrivate::ViewportTypeToString(ViewportClient->ViewportType));

    if (ViewportClient->Viewport)
    {
        UnrealMCPEditorCommandsPrivate::SetIntPointField(Result, TEXT("viewport_size"), ViewportClient->Viewport->GetSizeXY());
    }

    return Result;
}

/**
 * @brief 读取插件内部捕获的输出日志。
 * @param [in] Params 查询参数。
 * @return TSharedPtr<FJsonObject> 日志结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetOutputLog(const TSharedPtr<FJsonObject>& Params)
{
    const int32 Limit = Params->HasTypedField<EJson::Number>(TEXT("limit"))
        ? FMath::Clamp(static_cast<int32>(Params->GetNumberField(TEXT("limit"))), 1, 500)
        : 100;

    FString CategoryFilter;
    FString VerbosityFilter;
    FString ContainsFilter;
    Params->TryGetStringField(TEXT("category"), CategoryFilter);
    Params->TryGetStringField(TEXT("verbosity"), VerbosityFilter);
    Params->TryGetStringField(TEXT("contains"), ContainsFilter);

    const TSharedPtr<UnrealMCPEditorCommandsPrivate::FUnrealMCPOutputLogCapture, ESPMode::ThreadSafe> Capture =
        UnrealMCPEditorCommandsPrivate::GetOutputLogCapture();
    if (!Capture.IsValid())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("输出日志捕获器未初始化"));
    }

    int32 FilteredCount = 0;
    const TArray<UnrealMCPEditorCommandsPrivate::FOutputLogEntrySnapshot> Entries =
        Capture->QueryEntries(Limit, CategoryFilter, VerbosityFilter, ContainsFilter, FilteredCount);

    TArray<TSharedPtr<FJsonValue>> EntryValues;
    for (const UnrealMCPEditorCommandsPrivate::FOutputLogEntrySnapshot& Entry : Entries)
    {
        TSharedPtr<FJsonObject> EntryObject = MakeShared<FJsonObject>();
        EntryObject->SetStringField(TEXT("timestamp"), Entry.Timestamp);
        EntryObject->SetStringField(TEXT("category"), Entry.Category);
        EntryObject->SetStringField(TEXT("verbosity"), Entry.Verbosity);
        EntryObject->SetStringField(TEXT("message"), Entry.Message);
        EntryValues.Add(MakeShared<FJsonValueObject>(EntryObject));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("entries"), EntryValues);
    Result->SetNumberField(TEXT("entry_count"), EntryValues.Num());
    Result->SetNumberField(TEXT("total_count"), FilteredCount);
    Result->SetNumberField(TEXT("limit"), Limit);
    Result->SetStringField(TEXT("category"), CategoryFilter);
    Result->SetStringField(TEXT("verbosity"), VerbosityFilter);
    Result->SetStringField(TEXT("contains"), ContainsFilter);
    return Result;
}

/**
 * @brief 清空插件内部捕获的输出日志缓冲。
 * @param [in] Params 清空参数（当前未使用）。
 * @return TSharedPtr<FJsonObject> 清空结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleClearOutputLog(const TSharedPtr<FJsonObject>& Params)
{
    const TSharedPtr<UnrealMCPEditorCommandsPrivate::FUnrealMCPOutputLogCapture, ESPMode::ThreadSafe> Capture =
        UnrealMCPEditorCommandsPrivate::GetOutputLogCapture();
    if (!Capture.IsValid())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("输出日志捕获器未初始化"));
    }

    const int32 ClearedCount = Capture->ClearEntries();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("cleared_count"), ClearedCount);
    return Result;
}

/**
 * @brief 读取 Message Log 指定列表中的消息。
 * @param [in] Params 查询参数。
 * @return TSharedPtr<FJsonObject> 日志结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetMessageLog(const TSharedPtr<FJsonObject>& Params)
{
    FString LogName = TEXT("PIE");
    Params->TryGetStringField(TEXT("log_name"), LogName);

    const int32 Limit = Params->HasTypedField<EJson::Number>(TEXT("limit"))
        ? FMath::Clamp(static_cast<int32>(Params->GetNumberField(TEXT("limit"))), 1, 500)
        : 100;

    FMessageLogModule* MessageLogModule = FModuleManager::LoadModulePtr<FMessageLogModule>(TEXT("MessageLog"));
    if (!MessageLogModule)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("MessageLog 模块未加载"));
    }

    TSharedRef<IMessageLogListing> LogListing = MessageLogModule->GetLogListing(FName(*LogName));
    const TArray<TSharedRef<FTokenizedMessage>>& Messages = LogListing->GetFilteredMessages();

    TArray<TSharedPtr<FJsonValue>> MessageValues;
    const int32 StartIndex = FMath::Max(0, Messages.Num() - Limit);
    for (int32 Index = Messages.Num() - 1; Index >= StartIndex; --Index)
    {
        const TSharedRef<FTokenizedMessage>& Message = Messages[Index];
        TSharedPtr<FJsonObject> MessageObject = MakeShared<FJsonObject>();
        MessageObject->SetStringField(TEXT("text"), Message->ToText().ToString());
        MessageObject->SetStringField(TEXT("severity"), UnrealMCPEditorCommandsPrivate::MessageSeverityToString(Message->GetSeverity()));
        MessageObject->SetStringField(TEXT("identifier"), Message->GetIdentifier().ToString());
        MessageValues.Add(MakeShared<FJsonValueObject>(MessageObject));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("log_name"), LogName);
    Result->SetArrayField(TEXT("messages"), MessageValues);
    Result->SetNumberField(TEXT("message_count"), MessageValues.Num());
    Result->SetNumberField(TEXT("total_count"), Messages.Num());
    Result->SetNumberField(TEXT("limit"), Limit);
    return Result;
}

/**
 * @brief 发送一条编辑器通知。
 * @param [in] Params 通知参数。
 * @return TSharedPtr<FJsonObject> 执行结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleShowEditorNotification(const TSharedPtr<FJsonObject>& Params)
{
    FString Message;
    if (!Params->TryGetStringField(TEXT("message"), Message) || Message.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'message' parameter"));
    }

    FString Severity = TEXT("info");
    Params->TryGetStringField(TEXT("severity"), Severity);
    Severity = Severity.TrimStartAndEnd().ToLower();

    const float ExpireDuration = Params->HasTypedField<EJson::Number>(TEXT("expire_duration"))
        ? static_cast<float>(Params->GetNumberField(TEXT("expire_duration")))
        : 5.0f;

    const bool bFireAndForget = Params->HasTypedField<EJson::Boolean>(TEXT("fire_and_forget"))
        ? Params->GetBoolField(TEXT("fire_and_forget"))
        : true;

    FNotificationInfo NotificationInfo(FText::FromString(Message));
    NotificationInfo.ExpireDuration = ExpireDuration;
    NotificationInfo.FadeOutDuration = 0.2f;
    NotificationInfo.bFireAndForget = bFireAndForget;
    NotificationInfo.bUseSuccessFailIcons = true;
    NotificationInfo.bUseLargeFont = false;

    const TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(NotificationInfo);
    if (NotificationItem.IsValid())
    {
        if (Severity == TEXT("success"))
        {
            NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
        }
        else if (Severity == TEXT("warning"))
        {
            NotificationItem->SetCompletionState(SNotificationItem::CS_Pending);
        }
        else if (Severity == TEXT("error"))
        {
            NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
        }
        else
        {
            NotificationItem->SetCompletionState(SNotificationItem::CS_None);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"), Message);
    Result->SetStringField(TEXT("severity"), Severity);
    Result->SetNumberField(TEXT("expire_duration"), ExpireDuration);
    Result->SetBoolField(TEXT("fire_and_forget"), bFireAndForget);
    return Result;
}
