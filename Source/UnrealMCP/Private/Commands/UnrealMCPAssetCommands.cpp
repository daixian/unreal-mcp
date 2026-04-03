/**
 * @file UnrealMCPAssetCommands.cpp
 * @brief 资产查询与摘要 MCP 命令处理实现。
 */
#include "Commands/UnrealMCPAssetCommands.h"

#include "Commands/UnrealMCPCommonUtils.h"
#include "Algo/AnyOf.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataTable.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EditorReimportHandler.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "InterchangeManager.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialParameters.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "EditorAssetLibrary.h"
#include "EditorUtilityLibrary.h"
#include "FileHelpers.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "Engine/Texture.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

// Sections are appended below in multiple patches to keep patch size manageable.

static IAssetRegistry& GetAssetRegistryRef()
{
    return FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
}

static UEditorAssetSubsystem* GetAssetSubsystem()
{
    return GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
}

static FString GetAssetClassName(const FAssetData& AssetData)
{
    return AssetData.AssetClassPath.GetAssetName().ToString();
}

static TSharedPtr<FJsonObject> CreateAssetIdentityObject(const FAssetData& AssetData)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset_name"), AssetData.AssetName.ToString());
    Result->SetStringField(TEXT("asset_class"), GetAssetClassName(AssetData));
    Result->SetStringField(TEXT("asset_class_path"), AssetData.AssetClassPath.ToString());
    Result->SetStringField(TEXT("asset_path"), AssetData.PackageName.ToString());
    Result->SetStringField(TEXT("package_name"), AssetData.PackageName.ToString());
    Result->SetStringField(TEXT("package_path"), AssetData.PackagePath.ToString());
    Result->SetStringField(TEXT("object_path"), AssetData.GetObjectPathString());
    return Result;
}

static void AddTagsToObject(const FAssetData& AssetData, const TSharedPtr<FJsonObject>& Result, int32 MaxTagCount)
{
    TSharedPtr<FJsonObject> Tags = MakeShared<FJsonObject>();
    int32 Count = 0;
    for (const auto& TagValue : AssetData.TagsAndValues)
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

static TArray<TSharedPtr<FJsonValue>> ToJsonStringArray(const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    Result.Reserve(Values.Num());
    for (const FString& Value : Values)
    {
        Result.Add(MakeShared<FJsonValueString>(Value));
    }
    return Result;
}

static bool TryGetStringArrayField(
    const TSharedPtr<FJsonObject>& Params,
    const FString& FieldName,
    TArray<FString>& OutValues
)
{
    OutValues.Reset();

    if (!Params.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* JsonValues = nullptr;
    if (!Params->TryGetArrayField(FieldName, JsonValues) || !JsonValues)
    {
        return false;
    }

    for (const TSharedPtr<FJsonValue>& JsonValue : *JsonValues)
    {
        if (!JsonValue.IsValid())
        {
            continue;
        }

        const FString Value = JsonValue->AsString().TrimStartAndEnd();
        if (!Value.IsEmpty())
        {
            OutValues.Add(Value);
        }
    }

    return true;
}

static void CollectDirectoryFilesRecursively(const FString& DirectoryPath, TSet<FString>& OutFiles)
{
    OutFiles.Reset();

    if (DirectoryPath.IsEmpty() || !IFileManager::Get().DirectoryExists(*DirectoryPath))
    {
        return;
    }

    TArray<FString> FoundFiles;
    IFileManager::Get().FindFilesRecursive(FoundFiles, *DirectoryPath, TEXT("*"), true, false, false);
    for (const FString& FoundFile : FoundFiles)
    {
        OutFiles.Add(FPaths::ConvertRelativePathToFull(FoundFile));
    }
}

static FString DependencyCategoryToString(UE::AssetRegistry::EDependencyCategory Category)
{
    using namespace UE::AssetRegistry;
    if (Category == EDependencyCategory::Package)
    {
        return TEXT("package");
    }
    if (Category == EDependencyCategory::Manage)
    {
        return TEXT("manage");
    }
    if (Category == EDependencyCategory::SearchableName)
    {
        return TEXT("searchable_name");
    }
    return TEXT("unknown");
}

static FString DependencyRelationToString(UE::AssetRegistry::EDependencyCategory Category, UE::AssetRegistry::EDependencyProperty Properties)
{
    using namespace UE::AssetRegistry;
    if (Category == EDependencyCategory::Package)
    {
        return EnumHasAnyFlags(Properties, EDependencyProperty::Hard) ? TEXT("hard") : TEXT("soft");
    }
    if (Category == EDependencyCategory::Manage)
    {
        return EnumHasAnyFlags(Properties, EDependencyProperty::Direct) ? TEXT("direct_manage") : TEXT("manage");
    }
    if (Category == EDependencyCategory::SearchableName)
    {
        return TEXT("searchable_name");
    }
    return TEXT("unknown");
}

static TArray<TSharedPtr<FJsonValue>> DependencyPropertiesToArray(UE::AssetRegistry::EDependencyProperty Properties)
{
    using namespace UE::AssetRegistry;
    TArray<TSharedPtr<FJsonValue>> Result;
    if (EnumHasAnyFlags(Properties, EDependencyProperty::Hard))
    {
        Result.Add(MakeShared<FJsonValueString>(TEXT("Hard")));
    }
    if (EnumHasAnyFlags(Properties, EDependencyProperty::Game))
    {
        Result.Add(MakeShared<FJsonValueString>(TEXT("Game")));
    }
    if (EnumHasAnyFlags(Properties, EDependencyProperty::Build))
    {
        Result.Add(MakeShared<FJsonValueString>(TEXT("Build")));
    }
    if (EnumHasAnyFlags(Properties, EDependencyProperty::Direct))
    {
        Result.Add(MakeShared<FJsonValueString>(TEXT("Direct")));
    }
    if (EnumHasAnyFlags(Properties, EDependencyProperty::CookRule))
    {
        Result.Add(MakeShared<FJsonValueString>(TEXT("CookRule")));
    }
    return Result;
}

static void CollectDetailedDependencies(const FAssetData& AssetData, bool bCollectReferencers, TArray<TSharedPtr<FJsonValue>>& OutValues)
{
    TArray<FAssetDependency> Dependencies;
    const FAssetIdentifier AssetIdentifier(AssetData.PackageName);
    if (bCollectReferencers)
    {
        GetAssetRegistryRef().GetReferencers(AssetIdentifier, Dependencies, UE::AssetRegistry::EDependencyCategory::All, UE::AssetRegistry::FDependencyQuery());
    }
    else
    {
        GetAssetRegistryRef().GetDependencies(AssetIdentifier, Dependencies, UE::AssetRegistry::EDependencyCategory::All, UE::AssetRegistry::FDependencyQuery());
    }

    Dependencies.Sort([](const FAssetDependency& A, const FAssetDependency& B)
    {
        return A.AssetId.ToString() < B.AssetId.ToString();
    });

    for (const FAssetDependency& Dependency : Dependencies)
    {
        TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("asset_id"), Dependency.AssetId.ToString());
        if (Dependency.AssetId.PackageName != NAME_None)
        {
            Item->SetStringField(TEXT("package_name"), Dependency.AssetId.PackageName.ToString());
        }
        if (Dependency.AssetId.ObjectName != NAME_None)
        {
            Item->SetStringField(TEXT("object_name"), Dependency.AssetId.ObjectName.ToString());
        }
        Item->SetStringField(TEXT("category"), DependencyCategoryToString(Dependency.Category));
        Item->SetStringField(TEXT("relation_type"), DependencyRelationToString(Dependency.Category, Dependency.Properties));
        Item->SetArrayField(TEXT("properties"), DependencyPropertiesToArray(Dependency.Properties));
        OutValues.Add(MakeShared<FJsonValueObject>(Item));
    }
}

static void CollectSimpleDependencyPaths(const FAssetData& AssetData, bool bCollectReferencers, TArray<FString>& OutPaths)
{
    TArray<TSharedPtr<FJsonValue>> DetailedValues;
    CollectDetailedDependencies(AssetData, bCollectReferencers, DetailedValues);

    TSet<FString> UniquePaths;
    for (const TSharedPtr<FJsonValue>& Value : DetailedValues)
    {
        const TSharedPtr<FJsonObject>* ObjectValue = nullptr;
        if (!Value.IsValid() || !Value->TryGetObject(ObjectValue) || !ObjectValue)
        {
            continue;
        }

        FString PackageName;
        if ((*ObjectValue)->TryGetStringField(TEXT("package_name"), PackageName) && !PackageName.IsEmpty())
        {
            UniquePaths.Add(PackageName);
        }
    }

    OutPaths = UniquePaths.Array();
    OutPaths.Sort();
}

static FTopLevelAssetPath ResolveClassPath(const FString& ClassName)
{
    if (ClassName.IsEmpty())
    {
        return FTopLevelAssetPath();
    }
    if (ClassName.StartsWith(TEXT("/")))
    {
        return FTopLevelAssetPath(ClassName);
    }
    return FAssetData::TryConvertShortClassNameToPathName(FName(*ClassName), ELogVerbosity::NoLogging);
}

static bool ResolveAssetReference(const FString& AssetReference, FAssetData& OutAssetData, FString& OutErrorMessage)
{
    if (AssetReference.IsEmpty())
    {
        OutErrorMessage = TEXT("Missing asset reference");
        return false;
    }

    if (UEditorAssetSubsystem* AssetSubsystem = GetAssetSubsystem())
    {
        FAssetData AssetData;
        if (AssetReference.StartsWith(TEXT("/")))
        {
            AssetData = AssetSubsystem->FindAssetData(AssetReference);
            if (AssetData.IsValid())
            {
                OutAssetData = AssetData;
                return true;
            }
        }

        if (AssetReference.StartsWith(TEXT("/")) && !AssetReference.Contains(TEXT(".")))
        {
            const FString AssetName = FPackageName::GetLongPackageAssetName(AssetReference);
            if (!AssetName.IsEmpty())
            {
                AssetData = AssetSubsystem->FindAssetData(AssetReference + TEXT(".") + AssetName);
                if (AssetData.IsValid())
                {
                    OutAssetData = AssetData;
                    return true;
                }
            }
        }
    }

    TArray<FAssetData> AllAssets;
    GetAssetRegistryRef().GetAllAssets(AllAssets, true);

    TArray<FAssetData> MatchedAssets;
    for (const FAssetData& AssetData : AllAssets)
    {
        if (AssetData.AssetName.ToString().Equals(AssetReference, ESearchCase::IgnoreCase) ||
            AssetData.PackageName.ToString().Equals(AssetReference, ESearchCase::IgnoreCase) ||
            AssetData.GetObjectPathString().Equals(AssetReference, ESearchCase::IgnoreCase))
        {
            MatchedAssets.Add(AssetData);
        }
    }

    if (MatchedAssets.Num() == 1)
    {
        OutAssetData = MatchedAssets[0];
        return true;
    }

    if (MatchedAssets.Num() > 1)
    {
        MatchedAssets.Sort([](const FAssetData& A, const FAssetData& B)
        {
            return A.GetObjectPathString() < B.GetObjectPathString();
        });

        TArray<FString> Candidates;
        for (int32 Index = 0; Index < FMath::Min(5, MatchedAssets.Num()); ++Index)
        {
            Candidates.Add(MatchedAssets[Index].GetObjectPathString());
        }
        OutErrorMessage = FString::Printf(TEXT("Asset reference '%s' matched multiple assets: %s"), *AssetReference, *FString::Join(Candidates, TEXT(", ")));
        return false;
    }

    OutErrorMessage = FString::Printf(TEXT("Asset not found: %s"), *AssetReference);
    return false;
}

static bool ResolveAssetDataFromParams(const TSharedPtr<FJsonObject>& Params, FAssetData& OutAssetData, FString& OutErrorMessage)
{
    FString AssetReference;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetReference))
    {
        if (!Params->TryGetStringField(TEXT("object_path"), AssetReference))
        {
            if (!Params->TryGetStringField(TEXT("path"), AssetReference))
            {
                if (!Params->TryGetStringField(TEXT("asset_name"), AssetReference))
                {
                    Params->TryGetStringField(TEXT("name"), AssetReference);
                }
            }
        }
    }

    if (AssetReference.IsEmpty())
    {
        OutErrorMessage = TEXT("Missing 'asset_path' parameter");
        return false;
    }
    return ResolveAssetReference(AssetReference, OutAssetData, OutErrorMessage);
}

static bool ResolveAssetDataListFromParams(
    const TSharedPtr<FJsonObject>& Params,
    const FString& FieldName,
    TArray<FAssetData>& OutAssetDataList,
    FString& OutErrorMessage
)
{
    OutAssetDataList.Reset();

    TArray<FString> AssetReferences;
    TryGetStringArrayField(Params, FieldName, AssetReferences);

    for (const FString& AssetReference : AssetReferences)
    {
        FAssetData AssetData;
        FString ResolveErrorMessage;
        if (!ResolveAssetReference(AssetReference, AssetData, ResolveErrorMessage))
        {
            OutErrorMessage = ResolveErrorMessage;
            return false;
        }
        OutAssetDataList.Add(AssetData);
    }

    return true;
}

static bool UnrealMCPAssetNormalizePackagePath(
    const FString& PackagePathText,
    const FString& DefaultPackagePath,
    FString& OutPackagePath,
    FString& OutErrorMessage)
{
    OutPackagePath = PackagePathText.TrimStartAndEnd();
    if (OutPackagePath.IsEmpty())
    {
        OutPackagePath = DefaultPackagePath;
    }

    if (!OutPackagePath.StartsWith(TEXT("/Game")))
    {
        OutErrorMessage = TEXT("'path' 必须以 /Game 开头");
        return false;
    }

    while (OutPackagePath.EndsWith(TEXT("/")))
    {
        OutPackagePath.LeftChopInline(1);
    }

    if (!FPackageName::IsValidLongPackageName(OutPackagePath))
    {
        OutErrorMessage = FString::Printf(TEXT("无效资源目录路径: %s"), *OutPackagePath);
        return false;
    }

    return true;
}

static bool UnrealMCPAssetBuildAssetPath(
    const FString& AssetNameText,
    const FString& PackagePath,
    FString& OutAssetName,
    FString& OutAssetPath,
    FString& OutErrorMessage)
{
    OutAssetName = AssetNameText.TrimStartAndEnd();
    if (OutAssetName.IsEmpty())
    {
        OutErrorMessage = TEXT("资源名称不能为空");
        return false;
    }

    OutAssetPath = PackagePath + TEXT("/") + OutAssetName;
    if (!FPackageName::IsValidObjectPath(OutAssetPath + TEXT(".") + OutAssetName))
    {
        OutErrorMessage = FString::Printf(TEXT("无效资源路径: %s"), *OutAssetPath);
        return false;
    }

    return true;
}

static FString MaterialParameterAssociationToString(EMaterialParameterAssociation Association)
{
    switch (Association)
    {
        case EMaterialParameterAssociation::LayerParameter:
            return TEXT("layer");
        case EMaterialParameterAssociation::BlendParameter:
            return TEXT("blend");
        case EMaterialParameterAssociation::GlobalParameter:
        default:
            return TEXT("global");
    }
}

static TArray<TSharedPtr<FJsonValue>> MakeLinearColorArray(const FLinearColor& Color)
{
    return TArray<TSharedPtr<FJsonValue>>
    {
        MakeShared<FJsonValueNumber>(Color.R),
        MakeShared<FJsonValueNumber>(Color.G),
        MakeShared<FJsonValueNumber>(Color.B),
        MakeShared<FJsonValueNumber>(Color.A)
    };
}

static bool TryParseLinearColor(
    const TSharedPtr<FJsonObject>& Params,
    const FString& FieldName,
    FLinearColor& OutColor,
    FString& OutErrorMessage)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Params.IsValid() || !Params->TryGetArrayField(FieldName, Values) || !Values)
    {
        OutErrorMessage = FString::Printf(TEXT("缺少 '%s' 参数"), *FieldName);
        return false;
    }

    if (Values->Num() != 4)
    {
        OutErrorMessage = FString::Printf(TEXT("'%s' 必须是 4 个数字 [R, G, B, A]"), *FieldName);
        return false;
    }

    OutColor = FLinearColor(
        static_cast<float>((*Values)[0]->AsNumber()),
        static_cast<float>((*Values)[1]->AsNumber()),
        static_cast<float>((*Values)[2]->AsNumber()),
        static_cast<float>((*Values)[3]->AsNumber()));
    return true;
}

static bool ResolveMaterialInterfaceByReference(
    const FString& AssetReference,
    UMaterialInterface*& OutMaterial,
    FString& OutErrorMessage)
{
    OutMaterial = nullptr;

    FAssetData AssetData;
    if (!ResolveAssetReference(AssetReference, AssetData, OutErrorMessage))
    {
        return false;
    }

    OutMaterial = Cast<UMaterialInterface>(AssetData.GetAsset());
    if (!OutMaterial)
    {
        OutErrorMessage = FString::Printf(TEXT("资源不是材质或材质实例: %s"), *AssetData.GetObjectPathString());
        return false;
    }

    return true;
}

static bool ResolveMaterialInterfaceFromParams(
    const TSharedPtr<FJsonObject>& Params,
    UMaterialInterface*& OutMaterial,
    FAssetData& OutAssetData,
    FString& OutErrorMessage)
{
    if (!ResolveAssetDataFromParams(Params, OutAssetData, OutErrorMessage))
    {
        return false;
    }

    OutMaterial = Cast<UMaterialInterface>(OutAssetData.GetAsset());
    if (!OutMaterial)
    {
        OutErrorMessage = FString::Printf(TEXT("资源不是材质或材质实例: %s"), *OutAssetData.GetObjectPathString());
        return false;
    }

    return true;
}

static bool ResolveMaterialInstanceFromParams(
    const TSharedPtr<FJsonObject>& Params,
    UMaterialInstanceConstant*& OutMaterialInstance,
    FAssetData& OutAssetData,
    FString& OutErrorMessage)
{
    UMaterialInterface* Material = nullptr;
    if (!ResolveMaterialInterfaceFromParams(Params, Material, OutAssetData, OutErrorMessage))
    {
        return false;
    }

    OutMaterialInstance = Cast<UMaterialInstanceConstant>(Material);
    if (!OutMaterialInstance)
    {
        OutErrorMessage = FString::Printf(TEXT("资源不是 MaterialInstanceConstant: %s"), *OutAssetData.GetObjectPathString());
        return false;
    }

    return true;
}

static TSharedPtr<FJsonObject> CreateMaterialParameterObject(
    const FMaterialParameterInfo& ParameterInfo,
    const FMaterialParameterMetadata& Metadata)
{
    TSharedPtr<FJsonObject> ParameterObject = MakeShared<FJsonObject>();
    ParameterObject->SetStringField(TEXT("name"), ParameterInfo.Name.ToString());
    ParameterObject->SetStringField(TEXT("association"), MaterialParameterAssociationToString(ParameterInfo.Association));
    ParameterObject->SetNumberField(TEXT("association_index"), ParameterInfo.Index);
#if WITH_EDITORONLY_DATA
    ParameterObject->SetStringField(TEXT("group"), Metadata.Group.ToString());
    ParameterObject->SetStringField(TEXT("description"), Metadata.Description);
    ParameterObject->SetStringField(TEXT("source_asset_path"), Metadata.AssetPath);
    ParameterObject->SetBoolField(TEXT("override"), Metadata.bOverride);
    ParameterObject->SetNumberField(TEXT("sort_priority"), Metadata.SortPriority);
#endif
    return ParameterObject;
}

static FString ReimportResultToString(bool bCanReimport, bool bStarted, bool bCompleted)
{
    if (!bCanReimport)
    {
        return TEXT("not_supported");
    }

    if (!bStarted)
    {
        return TEXT("failed");
    }

    return bCompleted ? TEXT("completed") : TEXT("pending");
}

static bool TryParseRedirectFixupMode(const FString& FixupModeText, ERedirectFixupMode& OutFixupMode, FString& OutNormalizedMode)
{
    const FString Normalized = FixupModeText.TrimStartAndEnd().ToLower();

    if (Normalized.IsEmpty() || Normalized == TEXT("delete_fixed_up_redirectors") || Normalized == TEXT("delete"))
    {
        OutFixupMode = ERedirectFixupMode::DeleteFixedUpRedirectors;
        OutNormalizedMode = TEXT("delete_fixed_up_redirectors");
        return true;
    }

    if (Normalized == TEXT("leave_fixed_up_redirectors") || Normalized == TEXT("leave"))
    {
        OutFixupMode = ERedirectFixupMode::LeaveFixedUpRedirectors;
        OutNormalizedMode = TEXT("leave_fixed_up_redirectors");
        return true;
    }

    if (Normalized == TEXT("prompt_for_deleting_redirectors") || Normalized == TEXT("prompt"))
    {
        OutFixupMode = ERedirectFixupMode::PromptForDeletingRedirectors;
        OutNormalizedMode = TEXT("prompt_for_deleting_redirectors");
        return true;
    }

    return false;
}

static TSharedPtr<FJsonValue> ExportPropertyValue(const FProperty* Property, const void* ValuePtr, int32 Depth)
{
    if (!Property || !ValuePtr || Depth > 2)
    {
        return nullptr;
    }
    if (const FBoolProperty* P = CastField<FBoolProperty>(Property)) return MakeShared<FJsonValueBoolean>(P->GetPropertyValue(ValuePtr));
    if (const FIntProperty* P = CastField<FIntProperty>(Property)) return MakeShared<FJsonValueNumber>(P->GetPropertyValue(ValuePtr));
    if (const FInt64Property* P = CastField<FInt64Property>(Property)) return MakeShared<FJsonValueNumber>(static_cast<double>(P->GetPropertyValue(ValuePtr)));
    if (const FFloatProperty* P = CastField<FFloatProperty>(Property)) return MakeShared<FJsonValueNumber>(P->GetPropertyValue(ValuePtr));
    if (const FDoubleProperty* P = CastField<FDoubleProperty>(Property)) return MakeShared<FJsonValueNumber>(P->GetPropertyValue(ValuePtr));
    if (const FStrProperty* P = CastField<FStrProperty>(Property)) return MakeShared<FJsonValueString>(P->GetPropertyValue(ValuePtr));
    if (const FNameProperty* P = CastField<FNameProperty>(Property)) return MakeShared<FJsonValueString>(P->GetPropertyValue(ValuePtr).ToString());
    if (const FTextProperty* P = CastField<FTextProperty>(Property)) return MakeShared<FJsonValueString>(P->GetPropertyValue(ValuePtr).ToString());
    if (const FEnumProperty* P = CastField<FEnumProperty>(Property)) return MakeShared<FJsonValueString>(P->GetEnum()->GetNameStringByValue(P->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr)));
    if (const FByteProperty* P = CastField<FByteProperty>(Property))
    {
        if (P->Enum)
        {
            return MakeShared<FJsonValueString>(P->Enum->GetNameStringByValue(P->GetPropertyValue(ValuePtr)));
        }
        return MakeShared<FJsonValueNumber>(P->GetPropertyValue(ValuePtr));
    }
    if (const FStructProperty* P = CastField<FStructProperty>(Property))
    {
        if (P->Struct == TBaseStructure<FVector>::Get())
        {
            const FVector& V = *static_cast<const FVector*>(ValuePtr);
            return MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z)});
        }
        if (P->Struct == TBaseStructure<FRotator>::Get())
        {
            const FRotator& R = *static_cast<const FRotator*>(ValuePtr);
            return MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(R.Pitch), MakeShared<FJsonValueNumber>(R.Yaw), MakeShared<FJsonValueNumber>(R.Roll)});
        }
        if (P->Struct == TBaseStructure<FLinearColor>::Get())
        {
            const FLinearColor& C = *static_cast<const FLinearColor*>(ValuePtr);
            return MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(C.R), MakeShared<FJsonValueNumber>(C.G), MakeShared<FJsonValueNumber>(C.B), MakeShared<FJsonValueNumber>(C.A)});
        }
        if (P->Struct == TBaseStructure<FTransform>::Get())
        {
            const FTransform& T = *static_cast<const FTransform*>(ValuePtr);
            TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
            const FVector L = T.GetLocation();
            const FRotator R = T.Rotator();
            const FVector S = T.GetScale3D();
            O->SetArrayField(TEXT("location"), TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(L.X), MakeShared<FJsonValueNumber>(L.Y), MakeShared<FJsonValueNumber>(L.Z)});
            O->SetArrayField(TEXT("rotation"), TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(R.Pitch), MakeShared<FJsonValueNumber>(R.Yaw), MakeShared<FJsonValueNumber>(R.Roll)});
            O->SetArrayField(TEXT("scale"), TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(S.X), MakeShared<FJsonValueNumber>(S.Y), MakeShared<FJsonValueNumber>(S.Z)});
            return MakeShared<FJsonValueObject>(O);
        }
    }
    if (const FObjectPropertyBase* P = CastField<FObjectPropertyBase>(Property))
    {
        const UObject* Obj = P->GetObjectPropertyValue(ValuePtr);
        return MakeShared<FJsonValueString>(Obj ? Obj->GetPathName() : FString());
    }
    if (const FSoftObjectProperty* P = CastField<FSoftObjectProperty>(Property))
    {
        const FSoftObjectPtr& SoftObj = P->GetPropertyValue(ValuePtr);
        return MakeShared<FJsonValueString>(SoftObj.ToSoftObjectPath().ToString());
    }
    if (const FArrayProperty* P = CastField<FArrayProperty>(Property))
    {
        FScriptArrayHelper Helper(P, ValuePtr);
        TArray<TSharedPtr<FJsonValue>> Items;
        for (int32 Index = 0; Index < FMath::Min(Helper.Num(), 16); ++Index)
        {
            TSharedPtr<FJsonValue> Item = ExportPropertyValue(P->Inner, Helper.GetRawPtr(Index), Depth + 1);
            Items.Add(Item.IsValid() ? Item : MakeShared<FJsonValueNull>());
        }
        return MakeShared<FJsonValueArray>(Items);
    }
    return nullptr;
}

static void AddWhitelistedProperties(UObject* Object, const TSharedPtr<FJsonObject>& Result, int32 MaxProperties)
{
    if (!Object || !Result.IsValid())
    {
        return;
    }

    int32 Count = 0;
    for (TFieldIterator<FProperty> It(Object->GetClass()); It && Count < MaxProperties; ++It)
    {
        const FProperty* Property = *It;
        if (!Property ||
            Property->HasAnyPropertyFlags(CPF_Transient | CPF_Parm | CPF_ReturnParm) ||
            !Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
        {
            continue;
        }

        TSharedPtr<FJsonValue> Value = ExportPropertyValue(Property, Property->ContainerPtrToValuePtr<void>(Object), 0);
        if (!Value.IsValid())
        {
            continue;
        }

        Result->SetField(Property->GetName(), Value);
        ++Count;
    }
}

static TArray<TSharedPtr<FJsonValue>> CreateVector2DArray(const FVector2D& Value)
{
    return TArray<TSharedPtr<FJsonValue>>
    {
        MakeShared<FJsonValueNumber>(Value.X),
        MakeShared<FJsonValueNumber>(Value.Y)
    };
}

static TArray<TSharedPtr<FJsonValue>> CreateLinearColorArray(const FLinearColor& Value)
{
    return TArray<TSharedPtr<FJsonValue>>
    {
        MakeShared<FJsonValueNumber>(Value.R),
        MakeShared<FJsonValueNumber>(Value.G),
        MakeShared<FJsonValueNumber>(Value.B),
        MakeShared<FJsonValueNumber>(Value.A)
    };
}

static TSharedPtr<FJsonObject> CreateMarginObject(const FMargin& Value)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("left"), Value.Left);
    Result->SetNumberField(TEXT("top"), Value.Top);
    Result->SetNumberField(TEXT("right"), Value.Right);
    Result->SetNumberField(TEXT("bottom"), Value.Bottom);
    return Result;
}

static TSharedPtr<FJsonObject> CreateAnchorsObject(const FAnchors& Value)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("minimum"), CreateVector2DArray(Value.Minimum));
    Result->SetArrayField(TEXT("maximum"), CreateVector2DArray(Value.Maximum));
    return Result;
}

static TSharedPtr<FJsonObject> CreateRenderTransformObject(const FWidgetTransform& Value)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("translation"), CreateVector2DArray(Value.Translation));
    Result->SetArrayField(TEXT("scale"), CreateVector2DArray(Value.Scale));
    Result->SetArrayField(TEXT("shear"), CreateVector2DArray(Value.Shear));
    Result->SetNumberField(TEXT("angle"), Value.Angle);
    return Result;
}

static FString GetEnumValueName(const UEnum* Enum, int64 Value)
{
    return Enum ? Enum->GetNameStringByValue(Value) : FString();
}

static void AddWidgetSlotData(UWidget* Widget, const TSharedPtr<FJsonObject>& Item)
{
    if (!Widget || !Item.IsValid())
    {
        return;
    }

    if (UPanelSlot* Slot = Widget->Slot)
    {
        Item->SetStringField(TEXT("slot_class"), Slot->GetClass()->GetName());

        TSharedPtr<FJsonObject> SlotObject = MakeShared<FJsonObject>();
        if (const UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
        {
            SlotObject->SetStringField(TEXT("type"), TEXT("CanvasPanelSlot"));
            SlotObject->SetArrayField(TEXT("position"), CreateVector2DArray(CanvasSlot->GetPosition()));
            SlotObject->SetArrayField(TEXT("size"), CreateVector2DArray(CanvasSlot->GetSize()));
            SlotObject->SetObjectField(TEXT("offsets"), CreateMarginObject(CanvasSlot->GetOffsets()));
            SlotObject->SetObjectField(TEXT("anchors"), CreateAnchorsObject(CanvasSlot->GetAnchors()));
            SlotObject->SetArrayField(TEXT("alignment"), CreateVector2DArray(CanvasSlot->GetAlignment()));
            SlotObject->SetBoolField(TEXT("auto_size"), CanvasSlot->GetAutoSize());
            SlotObject->SetNumberField(TEXT("z_order"), CanvasSlot->GetZOrder());
        }
        else
        {
            SlotObject->SetStringField(TEXT("type"), Slot->GetClass()->GetName());
        }

        Item->SetObjectField(TEXT("slot"), SlotObject);
    }
}

static void AddWidgetCommonData(UWidget* Widget, const TSharedPtr<FJsonObject>& Item)
{
    if (!Widget || !Item.IsValid())
    {
        return;
    }

    Item->SetBoolField(TEXT("is_enabled"), Widget->GetIsEnabled());
    Item->SetStringField(
        TEXT("visibility"),
        GetEnumValueName(StaticEnum<ESlateVisibility>(), static_cast<int64>(Widget->GetVisibility())));
    Item->SetNumberField(TEXT("render_opacity"), Widget->GetRenderOpacity());
    Item->SetObjectField(TEXT("render_transform"), CreateRenderTransformObject(Widget->GetRenderTransform()));
    Item->SetArrayField(TEXT("render_transform_pivot"), CreateVector2DArray(Widget->GetRenderTransformPivot()));
    Item->SetStringField(TEXT("tooltip_text"), Widget->GetToolTipText().ToString());
    Item->SetStringField(
        TEXT("clipping"),
        GetEnumValueName(StaticEnum<EWidgetClipping>(), static_cast<int64>(Widget->GetClipping())));
    Item->SetStringField(
        TEXT("flow_direction_preference"),
        GetEnumValueName(StaticEnum<EFlowDirectionPreference>(), static_cast<int64>(Widget->GetFlowDirectionPreference())));
}

static void AddWidgetSpecificData(UWidget* Widget, const TSharedPtr<FJsonObject>& Item)
{
    if (!Widget || !Item.IsValid())
    {
        return;
    }

    if (const UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
    {
        Item->SetNumberField(TEXT("children_count"), PanelWidget->GetChildrenCount());
    }

    if (const UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
    {
        TSharedPtr<FJsonObject> Details = MakeShared<FJsonObject>();
        Details->SetStringField(TEXT("text"), TextBlock->GetText().ToString());
        Details->SetArrayField(TEXT("color_and_opacity"), CreateLinearColorArray(TextBlock->GetColorAndOpacity().GetSpecifiedColor()));
        Details->SetNumberField(TEXT("font_size"), TextBlock->GetFont().Size);
        Details->SetNumberField(TEXT("min_desired_width"), TextBlock->GetMinDesiredWidth());
        Item->SetObjectField(TEXT("details"), Details);
        return;
    }

    if (const UButton* Button = Cast<UButton>(Widget))
    {
        TSharedPtr<FJsonObject> Details = MakeShared<FJsonObject>();
        Details->SetArrayField(TEXT("color_and_opacity"), CreateLinearColorArray(Button->GetColorAndOpacity()));
        Details->SetArrayField(TEXT("background_color"), CreateLinearColorArray(Button->GetBackgroundColor()));
        Details->SetStringField(
            TEXT("click_method"),
            GetEnumValueName(StaticEnum<EButtonClickMethod::Type>(), static_cast<int64>(Button->GetClickMethod())));
        Details->SetStringField(
            TEXT("touch_method"),
            GetEnumValueName(StaticEnum<EButtonTouchMethod::Type>(), static_cast<int64>(Button->GetTouchMethod())));
        Details->SetStringField(
            TEXT("press_method"),
            GetEnumValueName(StaticEnum<EButtonPressMethod::Type>(), static_cast<int64>(Button->GetPressMethod())));
        Details->SetBoolField(TEXT("is_focusable"), Button->GetIsFocusable());
        Item->SetObjectField(TEXT("details"), Details);
    }
}

static void CollectWidgetTree(UWidget* Widget, const FString& ParentName, int32 Depth, int32 ChildIndex, int32 MaxCount, int32& InOutCount, TArray<TSharedPtr<FJsonValue>>& OutValues)
{
    if (!Widget || InOutCount >= MaxCount)
    {
        return;
    }

    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    Item->SetStringField(TEXT("name"), Widget->GetName());
    Item->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
    Item->SetStringField(TEXT("parent"), ParentName);
    Item->SetNumberField(TEXT("depth"), Depth);
    Item->SetNumberField(TEXT("child_index"), ChildIndex);
    AddWidgetCommonData(Widget, Item);
    AddWidgetSlotData(Widget, Item);
    AddWidgetSpecificData(Widget, Item);
    OutValues.Add(MakeShared<FJsonValueObject>(Item));
    ++InOutCount;

    if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
    {
        for (int32 LocalChildIndex = 0; LocalChildIndex < PanelWidget->GetChildrenCount(); ++LocalChildIndex)
        {
            CollectWidgetTree(PanelWidget->GetChildAt(LocalChildIndex), Widget->GetName(), Depth + 1, LocalChildIndex, MaxCount, InOutCount, OutValues);
        }
    }
}

static TSharedPtr<FJsonObject> CreateBlueprintSummary(UBlueprint* Blueprint, const FAssetData& AssetData)
{
    TSharedPtr<FJsonObject> Result = CreateAssetIdentityObject(AssetData);
    Result->SetStringField(TEXT("summary_kind"), Cast<UWidgetBlueprint>(Blueprint) ? TEXT("WidgetBlueprint") : TEXT("Blueprint"));
    Result->SetStringField(TEXT("parent_class"), Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : FString());
    Result->SetStringField(TEXT("generated_class"), Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetPathName() : FString());

    TArray<TSharedPtr<FJsonValue>> Components;
    if (Blueprint->SimpleConstructionScript)
    {
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (!Node) continue;
            TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
            Item->SetStringField(TEXT("class"), Node->ComponentClass ? Node->ComponentClass->GetName() : FString());
            Item->SetStringField(TEXT("attach_parent"), Node->ParentComponentOrVariableName.ToString());
            Components.Add(MakeShared<FJsonValueObject>(Item));
        }
    }
    Result->SetArrayField(TEXT("components"), Components);

    TArray<TSharedPtr<FJsonValue>> Variables;
    for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("name"), Variable.VarName.ToString());
        Item->SetStringField(TEXT("type"), UEdGraphSchema_K2::TypeToText(Variable.VarType).ToString());
        Item->SetStringField(TEXT("default_value"), Variable.DefaultValue);
        Variables.Add(MakeShared<FJsonValueObject>(Item));
    }
    Result->SetArrayField(TEXT("variables"), Variables);

    int32 TotalNodeCount = 0;
    TArray<TSharedPtr<FJsonValue>> Ubergraphs;
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (!Graph) continue;
        TotalNodeCount += Graph->Nodes.Num();
        TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("name"), Graph->GetName());
        Item->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
        Ubergraphs.Add(MakeShared<FJsonValueObject>(Item));
    }
    Result->SetArrayField(TEXT("ubergraph_pages"), Ubergraphs);

    TArray<TSharedPtr<FJsonValue>> FunctionGraphs;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (!Graph) continue;
        TotalNodeCount += Graph->Nodes.Num();
        TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("name"), Graph->GetName());
        Item->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
        FunctionGraphs.Add(MakeShared<FJsonValueObject>(Item));
    }
    Result->SetArrayField(TEXT("function_graphs"), FunctionGraphs);
    Result->SetNumberField(TEXT("total_node_count"), TotalNodeCount);

    if (Blueprint->GeneratedClass)
    {
        TSharedPtr<FJsonObject> DefaultValues = MakeShared<FJsonObject>();
        AddWhitelistedProperties(Blueprint->GeneratedClass->GetDefaultObject(), DefaultValues, 64);
        Result->SetObjectField(TEXT("default_values"), DefaultValues);
    }

    TArray<FString> Dependencies;
    CollectSimpleDependencyPaths(AssetData, false, Dependencies);
    Result->SetArrayField(TEXT("dependencies"), ToJsonStringArray(Dependencies));

    if (UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Blueprint))
    {
        TArray<TSharedPtr<FJsonValue>> WidgetTree;
        int32 WidgetCount = 0;
        if (WidgetBlueprint->WidgetTree && WidgetBlueprint->WidgetTree->RootWidget)
        {
            CollectWidgetTree(WidgetBlueprint->WidgetTree->RootWidget, FString(), 0, -1, 256, WidgetCount, WidgetTree);
        }
        Result->SetArrayField(TEXT("widget_tree"), WidgetTree);

        TArray<TSharedPtr<FJsonValue>> Bindings;
        for (const FDelegateEditorBinding& Binding : WidgetBlueprint->Bindings)
        {
            TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("widget"), Binding.ObjectName);
            Item->SetStringField(TEXT("property"), Binding.PropertyName.ToString());
            Item->SetStringField(TEXT("function"), Binding.FunctionName.ToString());
            Item->SetStringField(TEXT("source_property"), Binding.SourceProperty.ToString());
            Bindings.Add(MakeShared<FJsonValueObject>(Item));
        }
        Result->SetArrayField(TEXT("bindings"), Bindings);

        TArray<TSharedPtr<FJsonValue>> Animations;
        for (const TObjectPtr<UWidgetAnimation>& Animation : WidgetBlueprint->Animations)
        {
            const FString AnimationName = Animation ? Animation->GetName() : FString();
            Animations.Add(MakeShared<FJsonValueString>(AnimationName));
        }
        Result->SetArrayField(TEXT("animations"), Animations);
    }

    return Result;
}

static TSharedPtr<FJsonObject> CreateGenericSummary(UObject* AssetObject, const FAssetData& AssetData)
{
    TSharedPtr<FJsonObject> Result = CreateAssetIdentityObject(AssetData);
    Result->SetStringField(TEXT("summary_kind"), TEXT("Generic"));
    Result->SetStringField(TEXT("loaded_class"), AssetObject ? AssetObject->GetClass()->GetPathName() : FString());
    if (AssetObject)
    {
        TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
        AddWhitelistedProperties(AssetObject, Properties, 32);
        Result->SetObjectField(TEXT("properties"), Properties);
    }
    return Result;
}

static TSharedPtr<FJsonObject> CreateAssetSummary(UObject* AssetObject, const FAssetData& AssetData)
{
    if (UBlueprint* Blueprint = Cast<UBlueprint>(AssetObject))
    {
        return CreateBlueprintSummary(Blueprint, AssetData);
    }
    if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(AssetObject))
    {
        TSharedPtr<FJsonObject> Result = CreateAssetIdentityObject(AssetData);
        Result->SetStringField(TEXT("summary_kind"), TEXT("StaticMesh"));
        Result->SetNumberField(TEXT("lod_count"), StaticMesh->GetNumLODs());
        Result->SetBoolField(TEXT("has_collision"), StaticMesh->GetBodySetup() != nullptr);
        const FBoxSphereBounds Bounds = StaticMesh->GetBounds();
        Result->SetArrayField(TEXT("bounds_origin"), TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(Bounds.Origin.X), MakeShared<FJsonValueNumber>(Bounds.Origin.Y), MakeShared<FJsonValueNumber>(Bounds.Origin.Z)});
        Result->SetArrayField(TEXT("bounds_extent"), TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(Bounds.BoxExtent.X), MakeShared<FJsonValueNumber>(Bounds.BoxExtent.Y), MakeShared<FJsonValueNumber>(Bounds.BoxExtent.Z)});
        return Result;
    }
    if (UDataTable* DataTable = Cast<UDataTable>(AssetObject))
    {
        TSharedPtr<FJsonObject> Result = CreateAssetIdentityObject(AssetData);
        Result->SetStringField(TEXT("summary_kind"), TEXT("DataTable"));
        Result->SetStringField(TEXT("row_struct"), DataTable->GetRowStruct() ? DataTable->GetRowStruct()->GetPathName() : FString());
        Result->SetNumberField(TEXT("row_count"), DataTable->GetRowMap().Num());
        TArray<FString> RowNames;
        for (const FName& RowName : DataTable->GetRowNames()) RowNames.Add(RowName.ToString());
        RowNames.Sort();
        Result->SetArrayField(TEXT("row_names"), ToJsonStringArray(RowNames));
        FString TableJson = DataTable->GetTableAsJSON();
        TArray<TSharedPtr<FJsonValue>> ParsedRows;
        if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TableJson), ParsedRows))
        {
            TArray<TSharedPtr<FJsonValue>> SampleRows;
            for (int32 Index = 0; Index < FMath::Min(5, ParsedRows.Num()); ++Index) SampleRows.Add(ParsedRows[Index]);
            Result->SetArrayField(TEXT("sample_rows"), SampleRows);
        }
        return Result;
    }
    if (UWorld* World = Cast<UWorld>(AssetObject))
    {
        TSharedPtr<FJsonObject> Result = CreateAssetIdentityObject(AssetData);
        Result->SetStringField(TEXT("summary_kind"), TEXT("World"));
        Result->SetStringField(TEXT("world_settings_class"), World->GetWorldSettings() ? World->GetWorldSettings()->GetClass()->GetPathName() : FString());
        int32 ActorCount = 0;
        if (World->PersistentLevel)
        {
            for (AActor* Actor : World->PersistentLevel->Actors) if (Actor) ++ActorCount;
        }
        Result->SetNumberField(TEXT("actor_count"), ActorCount);
        TArray<TSharedPtr<FJsonValue>> Levels;
        for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels()) if (StreamingLevel) Levels.Add(MakeShared<FJsonValueString>(StreamingLevel->GetWorldAssetPackageName()));
        Result->SetArrayField(TEXT("streaming_levels"), Levels);
        return Result;
    }
    if (UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(AssetObject))
    {
        TSharedPtr<FJsonObject> Result = CreateAssetIdentityObject(AssetData);
        Result->SetStringField(TEXT("summary_kind"), TEXT("MaterialInstance"));
        Result->SetStringField(TEXT("parent_material"), MaterialInstance->Parent ? MaterialInstance->Parent->GetPathName() : FString());
        return Result;
    }
    return CreateGenericSummary(AssetObject, AssetData);
}

FUnrealMCPAssetCommands::FUnrealMCPAssetCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("search_assets")) return HandleSearchAssets(Params);
    if (CommandType == TEXT("get_asset_metadata")) return HandleGetAssetMetadata(Params);
    if (CommandType == TEXT("get_asset_dependencies")) return HandleGetAssetDependencies(Params);
    if (CommandType == TEXT("get_asset_referencers")) return HandleGetAssetReferencers(Params);
    if (CommandType == TEXT("get_asset_summary")) return HandleGetAssetSummary(Params);
    if (CommandType == TEXT("save_asset")) return HandleSaveAsset(Params);
    if (CommandType == TEXT("import_asset")) return HandleImportAsset(Params);
    if (CommandType == TEXT("export_asset")) return HandleExportAsset(Params);
    if (CommandType == TEXT("reimport_asset")) return HandleReimportAsset(Params);
    if (CommandType == TEXT("fixup_redirectors")) return HandleFixupRedirectors(Params);
    if (CommandType == TEXT("rename_asset")) return HandleRenameAsset(Params);
    if (CommandType == TEXT("move_asset")) return HandleMoveAsset(Params);
    if (CommandType == TEXT("delete_asset")) return HandleDeleteAsset(Params);
    if (CommandType == TEXT("get_selected_assets")) return HandleGetSelectedAssets(Params);
    if (CommandType == TEXT("sync_content_browser_to_assets")) return HandleSyncContentBrowserToAssets(Params);
    if (CommandType == TEXT("save_all_dirty_assets")) return HandleSaveAllDirtyAssets(Params);
    if (CommandType == TEXT("get_blueprint_summary")) return HandleGetBlueprintSummary(Params);
    if (CommandType == TEXT("create_material")) return HandleCreateMaterial(Params);
    if (CommandType == TEXT("create_material_instance")) return HandleCreateMaterialInstance(Params);
    if (CommandType == TEXT("get_material_parameters")) return HandleGetMaterialParameters(Params);
    if (CommandType == TEXT("set_material_instance_scalar_parameter")) return HandleSetMaterialInstanceScalarParameter(Params);
    if (CommandType == TEXT("set_material_instance_vector_parameter")) return HandleSetMaterialInstanceVectorParameter(Params);
    if (CommandType == TEXT("set_material_instance_texture_parameter")) return HandleSetMaterialInstanceTextureParameter(Params);
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown asset command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSearchAssets(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = TEXT("/Game");
    Params->TryGetStringField(TEXT("path"), Path);
    FString Query;
    if (!Params->TryGetStringField(TEXT("query"), Query)) Params->TryGetStringField(TEXT("name_contains"), Query);
    FString ClassName;
    Params->TryGetStringField(TEXT("class_name"), ClassName);
    bool bRecursivePaths = true;
    Params->TryGetBoolField(TEXT("recursive_paths"), bRecursivePaths);
    bool bIncludeTags = false;
    Params->TryGetBoolField(TEXT("include_tags"), bIncludeTags);
    int32 Limit = Params->HasField(TEXT("limit")) ? FMath::Clamp(static_cast<int32>(Params->GetNumberField(TEXT("limit"))), 1, 200) : 50;

    FARFilter Filter;
    Filter.bRecursivePaths = bRecursivePaths;
    if (!Path.IsEmpty()) Filter.PackagePaths.Add(*Path);
    if (!ClassName.IsEmpty())
    {
        const FTopLevelAssetPath ClassPath = ResolveClassPath(ClassName);
        if (!ClassPath.IsValid())
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown class_name: %s"), *ClassName));
        }
        Filter.ClassPaths.Add(ClassPath);
    }

    TArray<FAssetData> Assets;
    GetAssetRegistryRef().GetAssets(Filter, Assets);

    TArray<FAssetData> FilteredAssets;
    for (const FAssetData& AssetData : Assets)
    {
        if (!Query.IsEmpty() &&
            !AssetData.AssetName.ToString().Contains(Query, ESearchCase::IgnoreCase) &&
            !AssetData.GetObjectPathString().Contains(Query, ESearchCase::IgnoreCase))
        {
            continue;
        }
        FilteredAssets.Add(AssetData);
    }

    FilteredAssets.Sort([](const FAssetData& A, const FAssetData& B) { return A.GetObjectPathString() < B.GetObjectPathString(); });

    TArray<TSharedPtr<FJsonValue>> AssetArray;
    const int32 ReturnCount = FMath::Min(Limit, FilteredAssets.Num());
    for (int32 Index = 0; Index < ReturnCount; ++Index)
    {
        TSharedPtr<FJsonObject> Item = CreateAssetIdentityObject(FilteredAssets[Index]);
        if (bIncludeTags) AddTagsToObject(FilteredAssets[Index], Item, 32);
        AssetArray.Add(MakeShared<FJsonValueObject>(Item));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("path"), Path);
    Result->SetStringField(TEXT("query"), Query);
    Result->SetStringField(TEXT("class_name"), ClassName);
    Result->SetNumberField(TEXT("total_matches"), FilteredAssets.Num());
    Result->SetNumberField(TEXT("returned_count"), ReturnCount);
    Result->SetArrayField(TEXT("assets"), AssetArray);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleGetAssetMetadata(const TSharedPtr<FJsonObject>& Params)
{
    FAssetData AssetData;
    FString ErrorMessage;
    if (!ResolveAssetDataFromParams(Params, AssetData, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    TSharedPtr<FJsonObject> Result = CreateAssetIdentityObject(AssetData);
    AddTagsToObject(AssetData, Result, 128);

    TArray<FString> Dependencies;
    CollectSimpleDependencyPaths(AssetData, false, Dependencies);
    Result->SetArrayField(TEXT("dependencies"), ToJsonStringArray(Dependencies));

    TArray<FString> Referencers;
    CollectSimpleDependencyPaths(AssetData, true, Referencers);
    Result->SetArrayField(TEXT("referencers"), ToJsonStringArray(Referencers));
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleGetAssetDependencies(const TSharedPtr<FJsonObject>& Params)
{
    FAssetData AssetData;
    FString ErrorMessage;
    if (!ResolveAssetDataFromParams(Params, AssetData, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    TArray<TSharedPtr<FJsonValue>> Dependencies;
    CollectDetailedDependencies(AssetData, false, Dependencies);
    TSharedPtr<FJsonObject> Result = CreateAssetIdentityObject(AssetData);
    Result->SetNumberField(TEXT("dependency_count"), Dependencies.Num());
    Result->SetArrayField(TEXT("dependencies"), Dependencies);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleGetAssetReferencers(const TSharedPtr<FJsonObject>& Params)
{
    FAssetData AssetData;
    FString ErrorMessage;
    if (!ResolveAssetDataFromParams(Params, AssetData, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    TArray<TSharedPtr<FJsonValue>> Referencers;
    CollectDetailedDependencies(AssetData, true, Referencers);
    TSharedPtr<FJsonObject> Result = CreateAssetIdentityObject(AssetData);
    Result->SetNumberField(TEXT("referencer_count"), Referencers.Num());
    Result->SetArrayField(TEXT("referencers"), Referencers);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleGetAssetSummary(const TSharedPtr<FJsonObject>& Params)
{
    FAssetData AssetData;
    FString ErrorMessage;
    if (!ResolveAssetDataFromParams(Params, AssetData, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    UObject* AssetObject = AssetData.GetAsset();
    if (!AssetObject)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load asset: %s"), *AssetData.GetObjectPathString()));
    }
    return CreateAssetSummary(AssetObject, AssetData);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSaveAsset(const TSharedPtr<FJsonObject>& Params)
{
    FAssetData AssetData;
    FString ErrorMessage;
    if (!ResolveAssetDataFromParams(Params, AssetData, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    UObject* AssetObject = AssetData.GetAsset();
    if (!AssetObject)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load asset: %s"), *AssetData.GetObjectPathString()));
    }

    const bool bOnlyIfDirty = Params->HasTypedField<EJson::Boolean>(TEXT("only_if_dirty"))
        ? Params->GetBoolField(TEXT("only_if_dirty"))
        : false;

    const bool bIsDirty = AssetObject->GetOutermost() && AssetObject->GetOutermost()->IsDirty();
    const bool bShouldSave = !bOnlyIfDirty || bIsDirty;
    const bool bSaved = bShouldSave ? UEditorAssetLibrary::SaveLoadedAsset(AssetObject, false) : true;

    if (!bSaved)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to save asset: %s"), *AssetData.GetObjectPathString()));
    }

    TSharedPtr<FJsonObject> Result = CreateAssetIdentityObject(AssetData);
    Result->SetBoolField(TEXT("saved"), true);
    Result->SetBoolField(TEXT("was_dirty"), bIsDirty);
    Result->SetBoolField(TEXT("only_if_dirty"), bOnlyIfDirty);
    Result->SetBoolField(TEXT("save_attempted"), bShouldSave);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleImportAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString Filename;
    Params->TryGetStringField(TEXT("filename"), Filename);

    TArray<FString> SourceFiles;
    if (!Filename.TrimStartAndEnd().IsEmpty())
    {
        SourceFiles.Add(Filename.TrimStartAndEnd());
    }

    TArray<FString> AdditionalFiles;
    if (TryGetStringArrayField(Params, TEXT("source_files"), AdditionalFiles))
    {
        SourceFiles.Append(AdditionalFiles);
    }

    if (SourceFiles.Num() == 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'filename' or 'source_files' parameter"));
    }

    FString DestinationPath;
    if (!Params->TryGetStringField(TEXT("destination_path"), DestinationPath) || DestinationPath.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'destination_path' parameter"));
    }
    DestinationPath = DestinationPath.TrimStartAndEnd();

    FString DestinationName;
    Params->TryGetStringField(TEXT("destination_name"), DestinationName);
    DestinationName = DestinationName.TrimStartAndEnd();
    if (!DestinationName.IsEmpty() && SourceFiles.Num() > 1)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'destination_name' 只能在导入单个文件时使用"));
    }

    const bool bReplaceExisting = Params->HasTypedField<EJson::Boolean>(TEXT("replace_existing"))
        ? Params->GetBoolField(TEXT("replace_existing"))
        : true;
    const bool bReplaceExistingSettings = Params->HasTypedField<EJson::Boolean>(TEXT("replace_existing_settings"))
        ? Params->GetBoolField(TEXT("replace_existing_settings"))
        : false;
    const bool bAutomated = Params->HasTypedField<EJson::Boolean>(TEXT("automated"))
        ? Params->GetBoolField(TEXT("automated"))
        : true;
    const bool bSave = Params->HasTypedField<EJson::Boolean>(TEXT("save"))
        ? Params->GetBoolField(TEXT("save"))
        : true;
    const bool bRequestedAsyncImport = Params->HasTypedField<EJson::Boolean>(TEXT("async_import"))
        ? Params->GetBoolField(TEXT("async_import"))
        : true;
    const bool bAsyncImport = true;

    TArray<UAssetImportTask*> Tasks;
    TArray<FString> ResolvedSourceFiles;
    Tasks.Reserve(SourceFiles.Num());

    for (const FString& SourceFile : SourceFiles)
    {
        const FString AbsoluteSourceFile = FPaths::ConvertRelativePathToFull(SourceFile);
        if (!FPaths::FileExists(AbsoluteSourceFile))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("导入源文件不存在: %s"), *AbsoluteSourceFile));
        }

        UAssetImportTask* Task = NewObject<UAssetImportTask>();
        Task->Filename = AbsoluteSourceFile;
        Task->DestinationPath = DestinationPath;
        Task->DestinationName = DestinationName;
        Task->bReplaceExisting = bReplaceExisting;
        Task->bReplaceExistingSettings = bReplaceExistingSettings;
        Task->bAutomated = bAutomated;
        Task->bSave = bSave;
        Task->bAsync = bAsyncImport;
        Tasks.Add(Task);
        ResolvedSourceFiles.Add(AbsoluteSourceFile);
    }

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    AssetToolsModule.Get().ImportAssetTasks(Tasks);

    TArray<TSharedPtr<FJsonValue>> ImportedObjectPathValues;
    TArray<TSharedPtr<FJsonValue>> ImportedAssetValues;
    TArray<TSharedPtr<FJsonValue>> ExpectedObjectPathValues;
    bool bImportCompleted = true;
    for (UAssetImportTask* Task : Tasks)
    {
        if (!Task)
        {
            continue;
        }

        if (Task->AsyncResults.IsValid() && !Task->IsAsyncImportComplete())
        {
            bImportCompleted = false;
        }

        for (const FString& ImportedObjectPath : Task->ImportedObjectPaths)
        {
            ImportedObjectPathValues.Add(MakeShared<FJsonValueString>(ImportedObjectPath));

            FAssetData ImportedAssetData;
            FString ResolveErrorMessage;
            if (ResolveAssetReference(ImportedObjectPath, ImportedAssetData, ResolveErrorMessage))
            {
                ImportedAssetValues.Add(MakeShared<FJsonValueObject>(CreateAssetIdentityObject(ImportedAssetData)));
            }
        }

        if (!DestinationName.IsEmpty())
        {
            const FString ExpectedObjectPath = FString::Printf(
                TEXT("%s.%s"),
                *FString::Printf(TEXT("%s/%s"), *DestinationPath, *DestinationName),
                *DestinationName);
            ExpectedObjectPathValues.Add(MakeShared<FJsonValueString>(ExpectedObjectPath));
        }
    }

    const bool bHasAsyncResults = Algo::AnyOf(Tasks, [](const UAssetImportTask* Task)
    {
        return Task && Task->AsyncResults.IsValid();
    });

    if (ImportedObjectPathValues.Num() == 0 && !bHasAsyncResults)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("导入失败，未返回任何 ImportedObjectPaths"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("destination_path"), DestinationPath);
    Result->SetStringField(TEXT("destination_name"), DestinationName);
    Result->SetBoolField(TEXT("requested_async_import"), bRequestedAsyncImport);
    Result->SetBoolField(TEXT("replace_existing"), bReplaceExisting);
    Result->SetBoolField(TEXT("replace_existing_settings"), bReplaceExistingSettings);
    Result->SetBoolField(TEXT("automated"), bAutomated);
    Result->SetBoolField(TEXT("save"), bSave);
    Result->SetBoolField(TEXT("async_import"), bAsyncImport);
    Result->SetBoolField(TEXT("import_completed"), bImportCompleted);
    Result->SetBoolField(TEXT("has_async_results"), bHasAsyncResults);
    Result->SetArrayField(TEXT("source_files"), ToJsonStringArray(ResolvedSourceFiles));
    Result->SetArrayField(TEXT("imported_object_paths"), ImportedObjectPathValues);
    Result->SetArrayField(TEXT("imported_assets"), ImportedAssetValues);
    Result->SetArrayField(TEXT("expected_object_paths"), ExpectedObjectPathValues);
    Result->SetNumberField(TEXT("imported_count"), ImportedObjectPathValues.Num());
    Result->SetNumberField(TEXT("task_count"), Tasks.Num());
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleExportAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString ExportPath;
    if (!Params->TryGetStringField(TEXT("export_path"), ExportPath) || ExportPath.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'export_path' parameter"));
    }
    ExportPath = FPaths::ConvertRelativePathToFull(ExportPath.TrimStartAndEnd());

    TArray<FAssetData> AssetDataList;
    FString ErrorMessage;

    FString SingleAssetPath;
    Params->TryGetStringField(TEXT("asset_path"), SingleAssetPath);
    if (!SingleAssetPath.TrimStartAndEnd().IsEmpty())
    {
        FAssetData AssetData;
        if (!ResolveAssetReference(SingleAssetPath.TrimStartAndEnd(), AssetData, ErrorMessage))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
        }
        AssetDataList.Add(AssetData);
    }

    TArray<FAssetData> MultipleAssetDataList;
    if (!ResolveAssetDataListFromParams(Params, TEXT("asset_paths"), MultipleAssetDataList, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }
    AssetDataList.Append(MultipleAssetDataList);

    if (AssetDataList.Num() == 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_path' or 'asset_paths' parameter"));
    }

    if (!IFileManager::Get().MakeDirectory(*ExportPath, true))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("无法创建导出目录: %s"), *ExportPath));
    }

    TArray<UObject*> AssetsToExport;
    TArray<TSharedPtr<FJsonValue>> AssetValues;
    AssetsToExport.Reserve(AssetDataList.Num());
    AssetValues.Reserve(AssetDataList.Num());

    for (const FAssetData& AssetData : AssetDataList)
    {
        UObject* AssetObject = AssetData.GetAsset();
        if (!AssetObject)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Failed to load asset for export: %s"), *AssetData.GetObjectPathString()));
        }
        AssetsToExport.Add(AssetObject);
        AssetValues.Add(MakeShared<FJsonValueObject>(CreateAssetIdentityObject(AssetData)));
    }

    const bool bCleanFilename = Params->HasTypedField<EJson::Boolean>(TEXT("clean_filenames"))
        ? Params->GetBoolField(TEXT("clean_filenames"))
        : true;

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    if (!AssetToolsModule.Get().CanExportAssets(AssetDataList))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("存在不支持导出的资产"));
    }

    TSet<FString> BeforeFiles;
    CollectDirectoryFilesRecursively(ExportPath, BeforeFiles);

    if (bCleanFilename)
    {
        AssetToolsModule.Get().ExportAssetsWithCleanFilename(AssetsToExport, ExportPath);
    }
    else
    {
        AssetToolsModule.Get().ExportAssets(AssetsToExport, ExportPath);
    }

    TSet<FString> AfterFiles;
    CollectDirectoryFilesRecursively(ExportPath, AfterFiles);

    TArray<FString> NewFiles;
    for (const FString& FilePath : AfterFiles)
    {
        if (!BeforeFiles.Contains(FilePath))
        {
            NewFiles.Add(FilePath);
        }
    }
    NewFiles.Sort();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("export_path"), ExportPath);
    Result->SetBoolField(TEXT("clean_filenames"), bCleanFilename);
    Result->SetArrayField(TEXT("assets"), AssetValues);
    Result->SetNumberField(TEXT("asset_count"), AssetValues.Num());
    Result->SetArrayField(TEXT("new_files"), ToJsonStringArray(NewFiles));
    Result->SetNumberField(TEXT("new_file_count"), NewFiles.Num());
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleReimportAsset(const TSharedPtr<FJsonObject>& Params)
{
    TArray<FAssetData> AssetDataList;
    FString ErrorMessage;

    FString SingleAssetPath;
    Params->TryGetStringField(TEXT("asset_path"), SingleAssetPath);
    if (!SingleAssetPath.TrimStartAndEnd().IsEmpty())
    {
        FAssetData AssetData;
        if (!ResolveAssetReference(SingleAssetPath.TrimStartAndEnd(), AssetData, ErrorMessage))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
        }
        AssetDataList.Add(AssetData);
    }

    TArray<FAssetData> MultipleAssetDataList;
    if (!ResolveAssetDataListFromParams(Params, TEXT("asset_paths"), MultipleAssetDataList, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }
    AssetDataList.Append(MultipleAssetDataList);

    if (AssetDataList.Num() == 0)
    {
        FAssetData AssetData;
        if (!ResolveAssetDataFromParams(Params, AssetData, ErrorMessage))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
        }
        AssetDataList.Add(AssetData);
    }

    const bool bAskForNewFileIfMissing = Params->HasTypedField<EJson::Boolean>(TEXT("ask_for_new_file_if_missing"))
        ? Params->GetBoolField(TEXT("ask_for_new_file_if_missing"))
        : false;
    const bool bShowNotification = Params->HasTypedField<EJson::Boolean>(TEXT("show_notification"))
        ? Params->GetBoolField(TEXT("show_notification"))
        : true;
    const bool bForceNewFile = Params->HasTypedField<EJson::Boolean>(TEXT("force_new_file"))
        ? Params->GetBoolField(TEXT("force_new_file"))
        : false;
    const bool bAutomated = Params->HasTypedField<EJson::Boolean>(TEXT("automated"))
        ? Params->GetBoolField(TEXT("automated"))
        : true;
    const bool bForceShowDialog = Params->HasTypedField<EJson::Boolean>(TEXT("force_show_dialog"))
        ? Params->GetBoolField(TEXT("force_show_dialog"))
        : false;
    const int32 SourceFileIndex = Params->HasTypedField<EJson::Number>(TEXT("source_file_index"))
        ? static_cast<int32>(Params->GetNumberField(TEXT("source_file_index")))
        : INDEX_NONE;

    FString PreferredReimportFile;
    Params->TryGetStringField(TEXT("preferred_reimport_file"), PreferredReimportFile);
    PreferredReimportFile = PreferredReimportFile.TrimStartAndEnd();
    if (!PreferredReimportFile.IsEmpty())
    {
        PreferredReimportFile = FPaths::ConvertRelativePathToFull(PreferredReimportFile);
    }

    FReimportManager* ReimportManager = FReimportManager::Instance();
    if (!ReimportManager)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("ReimportManager 不可用"));
    }

    TArray<TSharedPtr<FJsonValue>> ReimportedAssets;
    bool bAnySuccess = false;

    for (const FAssetData& AssetData : AssetDataList)
    {
        UObject* AssetObject = AssetData.GetAsset();
        if (!AssetObject)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Failed to load asset for reimport: %s"), *AssetData.GetObjectPathString()));
        }

        TArray<FString> SourceFiles;
        const bool bCanReimport = ReimportManager->CanReimport(AssetObject, &SourceFiles);
        UE::Interchange::FAssetImportResultPtr AsyncResult;
        bool bStarted = false;
        bool bCompleted = false;

        if (bCanReimport)
        {
            AsyncResult = ReimportManager->ReimportAsync(
                AssetObject,
                bAskForNewFileIfMissing,
                bShowNotification,
                PreferredReimportFile,
                nullptr,
                SourceFileIndex,
                bForceNewFile,
                bAutomated,
                bForceShowDialog);
            bStarted = AsyncResult.IsValid() && AsyncResult->IsValid();
            bCompleted = bStarted && AsyncResult->GetStatus() == UE::Interchange::FImportResult::EStatus::Done;
        }

        const FString ReimportResult = ReimportResultToString(bCanReimport, bStarted, bCompleted);

        if (bStarted)
        {
            bAnySuccess = true;
        }

        TSharedPtr<FJsonObject> Item = CreateAssetIdentityObject(AssetData);
        Item->SetBoolField(TEXT("can_reimport"), bCanReimport);
        Item->SetBoolField(TEXT("reimport_started"), bStarted);
        Item->SetBoolField(TEXT("reimport_completed"), bCompleted);
        Item->SetStringField(TEXT("reimport_result"), ReimportResult);
        Item->SetArrayField(TEXT("source_files"), ToJsonStringArray(SourceFiles));
        ReimportedAssets.Add(MakeShared<FJsonValueObject>(Item));
    }

    if (!bAnySuccess)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("重导入未成功处理任何资产"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("assets"), ReimportedAssets);
    Result->SetNumberField(TEXT("asset_count"), ReimportedAssets.Num());
    Result->SetBoolField(TEXT("ask_for_new_file_if_missing"), bAskForNewFileIfMissing);
    Result->SetBoolField(TEXT("show_notification"), bShowNotification);
    Result->SetBoolField(TEXT("force_new_file"), bForceNewFile);
    Result->SetBoolField(TEXT("automated"), bAutomated);
    Result->SetBoolField(TEXT("force_show_dialog"), bForceShowDialog);
    Result->SetNumberField(TEXT("source_file_index"), SourceFileIndex);
    Result->SetStringField(TEXT("preferred_reimport_file"), PreferredReimportFile);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleFixupRedirectors(const TSharedPtr<FJsonObject>& Params)
{
    TArray<FString> DirectoryPaths;
    TryGetStringArrayField(Params, TEXT("directory_paths"), DirectoryPaths);

    FString SingleDirectoryPath;
    Params->TryGetStringField(TEXT("directory_path"), SingleDirectoryPath);
    if (!SingleDirectoryPath.TrimStartAndEnd().IsEmpty())
    {
        DirectoryPaths.Insert(SingleDirectoryPath.TrimStartAndEnd(), 0);
    }

    TArray<FAssetData> RedirectorAssetDataList;
    FString ErrorMessage;

    FString SingleAssetPath;
    Params->TryGetStringField(TEXT("asset_path"), SingleAssetPath);
    if (!SingleAssetPath.TrimStartAndEnd().IsEmpty())
    {
        FAssetData AssetData;
        if (!ResolveAssetReference(SingleAssetPath.TrimStartAndEnd(), AssetData, ErrorMessage))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
        }
        RedirectorAssetDataList.Add(AssetData);
    }

    TArray<FAssetData> ExplicitRedirectors;
    if (!ResolveAssetDataListFromParams(Params, TEXT("asset_paths"), ExplicitRedirectors, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }
    RedirectorAssetDataList.Append(ExplicitRedirectors);

    const bool bRecursivePaths = Params->HasTypedField<EJson::Boolean>(TEXT("recursive_paths"))
        ? Params->GetBoolField(TEXT("recursive_paths"))
        : true;
    const bool bCheckoutDialogPrompt = Params->HasTypedField<EJson::Boolean>(TEXT("checkout_dialog_prompt"))
        ? Params->GetBoolField(TEXT("checkout_dialog_prompt"))
        : false;

    FString FixupModeText = TEXT("delete_fixed_up_redirectors");
    Params->TryGetStringField(TEXT("fixup_mode"), FixupModeText);

    ERedirectFixupMode FixupMode = ERedirectFixupMode::DeleteFixedUpRedirectors;
    FString NormalizedFixupMode;
    if (!TryParseRedirectFixupMode(FixupModeText, FixupMode, NormalizedFixupMode))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Invalid 'fixup_mode': %s"), *FixupModeText));
    }

    IAssetRegistry& AssetRegistry = GetAssetRegistryRef();
    for (const FString& DirectoryPath : DirectoryPaths)
    {
        FARFilter Filter;
        Filter.bRecursivePaths = bRecursivePaths;
        Filter.PackagePaths.Add(*DirectoryPath);
        Filter.ClassPaths.Add(UObjectRedirector::StaticClass()->GetClassPathName());

        TArray<FAssetData> RedirectorAssetsInPath;
        AssetRegistry.GetAssets(Filter, RedirectorAssetsInPath);
        RedirectorAssetDataList.Append(RedirectorAssetsInPath);
    }

    TMap<FString, FAssetData> UniqueRedirectorMap;
    for (const FAssetData& AssetData : RedirectorAssetDataList)
    {
        if (AssetData.AssetClassPath == UObjectRedirector::StaticClass()->GetClassPathName())
        {
            UniqueRedirectorMap.Add(AssetData.GetObjectPathString(), AssetData);
        }
    }

    if (UniqueRedirectorMap.Num() == 0)
    {
        TSharedPtr<FJsonObject> EmptyResult = MakeShared<FJsonObject>();
        EmptyResult->SetBoolField(TEXT("success"), true);
        EmptyResult->SetArrayField(TEXT("redirectors"), TArray<TSharedPtr<FJsonValue>>());
        EmptyResult->SetNumberField(TEXT("redirector_count"), 0);
        EmptyResult->SetNumberField(TEXT("remaining_redirector_count"), 0);
        EmptyResult->SetArrayField(TEXT("directory_paths"), ToJsonStringArray(DirectoryPaths));
        EmptyResult->SetBoolField(TEXT("recursive_paths"), bRecursivePaths);
        EmptyResult->SetBoolField(TEXT("checkout_dialog_prompt"), bCheckoutDialogPrompt);
        EmptyResult->SetStringField(TEXT("fixup_mode"), NormalizedFixupMode);
        EmptyResult->SetStringField(TEXT("message"), TEXT("未找到 Redirector，无需修复"));
        return EmptyResult;
    }

    TArray<UObjectRedirector*> Redirectors;
    TArray<TSharedPtr<FJsonValue>> RedirectorValues;
    TArray<FString> RedirectorPaths;
    Redirectors.Reserve(UniqueRedirectorMap.Num());
    RedirectorValues.Reserve(UniqueRedirectorMap.Num());

    for (const TPair<FString, FAssetData>& Pair : UniqueRedirectorMap)
    {
        UObjectRedirector* Redirector = Cast<UObjectRedirector>(Pair.Value.GetAsset());
        if (!Redirector)
        {
            continue;
        }

        Redirectors.Add(Redirector);
        RedirectorPaths.Add(Pair.Value.PackageName.ToString());
        RedirectorValues.Add(MakeShared<FJsonValueObject>(CreateAssetIdentityObject(Pair.Value)));
    }

    if (Redirectors.Num() == 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("找到 Redirector 记录，但加载对象失败"));
    }

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    AssetToolsModule.Get().FixupReferencers(Redirectors, bCheckoutDialogPrompt, FixupMode);

    int32 RemainingCount = 0;
    for (const FString& RedirectorPath : RedirectorPaths)
    {
        if (UEditorAssetLibrary::DoesAssetExist(RedirectorPath))
        {
            ++RemainingCount;
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("redirectors"), RedirectorValues);
    Result->SetNumberField(TEXT("redirector_count"), RedirectorValues.Num());
    Result->SetNumberField(TEXT("remaining_redirector_count"), RemainingCount);
    Result->SetArrayField(TEXT("directory_paths"), ToJsonStringArray(DirectoryPaths));
    Result->SetBoolField(TEXT("recursive_paths"), bRecursivePaths);
    Result->SetBoolField(TEXT("checkout_dialog_prompt"), bCheckoutDialogPrompt);
    Result->SetStringField(TEXT("fixup_mode"), NormalizedFixupMode);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleRenameAsset(const TSharedPtr<FJsonObject>& Params)
{
    UEditorAssetSubsystem* AssetSubsystem = GetAssetSubsystem();
    if (!AssetSubsystem)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("EditorAssetSubsystem is unavailable"));
    }

    FString SourceAssetPath;
    FString DestinationAssetPath;
    if (!Params->TryGetStringField(TEXT("source_asset_path"), SourceAssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_asset_path' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("destination_asset_path"), DestinationAssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'destination_asset_path' parameter"));
    }

    if (!UEditorAssetLibrary::DoesAssetExist(SourceAssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Source asset does not exist: %s"), *SourceAssetPath));
    }
    if (UEditorAssetLibrary::DoesAssetExist(DestinationAssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Destination asset already exists: %s"), *DestinationAssetPath));
    }
    if (!AssetSubsystem->RenameAsset(SourceAssetPath, DestinationAssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to rename asset from %s to %s"), *SourceAssetPath, *DestinationAssetPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("source_asset_path"), SourceAssetPath);
    Result->SetStringField(TEXT("destination_asset_path"), DestinationAssetPath);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleMoveAsset(const TSharedPtr<FJsonObject>& Params)
{
    return HandleRenameAsset(Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params)
{
    UEditorAssetSubsystem* AssetSubsystem = GetAssetSubsystem();
    if (!AssetSubsystem)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("EditorAssetSubsystem is unavailable"));
    }

    FAssetData AssetData;
    FString ErrorMessage;
    if (!ResolveAssetDataFromParams(Params, AssetData, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    const FString AssetPath = AssetData.PackageName.ToString();
    if (!AssetSubsystem->DeleteAsset(AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to delete asset: %s"), *AssetPath));
    }

    TSharedPtr<FJsonObject> Result = CreateAssetIdentityObject(AssetData);
    Result->SetBoolField(TEXT("deleted"), true);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleGetSelectedAssets(const TSharedPtr<FJsonObject>& Params)
{
    const bool bIncludeTags = Params.IsValid() && Params->HasTypedField<EJson::Boolean>(TEXT("include_tags"))
        ? Params->GetBoolField(TEXT("include_tags"))
        : false;

    TArray<FAssetData> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssetData();
    TArray<TSharedPtr<FJsonValue>> AssetValues;
    AssetValues.Reserve(SelectedAssets.Num());

    for (const FAssetData& AssetData : SelectedAssets)
    {
        TSharedPtr<FJsonObject> AssetObject = CreateAssetIdentityObject(AssetData);
        if (bIncludeTags)
        {
            AddTagsToObject(AssetData, AssetObject, 128);
        }
        AssetValues.Add(MakeShared<FJsonValueObject>(AssetObject));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("assets"), AssetValues);
    Result->SetNumberField(TEXT("asset_count"), AssetValues.Num());
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSyncContentBrowserToAssets(const TSharedPtr<FJsonObject>& Params)
{
    if (!GEditor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Editor instance is not available"));
    }

    const TArray<TSharedPtr<FJsonValue>>* AssetPaths = nullptr;
    if (!Params->TryGetArrayField(TEXT("asset_paths"), AssetPaths) || !AssetPaths)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'asset_paths' parameter"));
    }

    TArray<UObject*> ObjectsToSync;
    TArray<TSharedPtr<FJsonValue>> SyncedAssetPaths;

    for (const TSharedPtr<FJsonValue>& AssetPathValue : *AssetPaths)
    {
        const FString AssetPath = AssetPathValue.IsValid() ? AssetPathValue->AsString() : TEXT("");
        if (AssetPath.IsEmpty())
        {
            continue;
        }

        UObject* AssetObject = UEditorAssetLibrary::LoadAsset(AssetPath);
        if (!AssetObject)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load asset for sync: %s"), *AssetPath));
        }

        ObjectsToSync.Add(AssetObject);
        SyncedAssetPaths.Add(MakeShared<FJsonValueString>(AssetPath));
    }

    if (ObjectsToSync.Num() == 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No valid assets were provided for sync"));
    }

    GEditor->SyncBrowserToObjects(ObjectsToSync);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("asset_paths"), SyncedAssetPaths);
    Result->SetNumberField(TEXT("asset_count"), SyncedAssetPaths.Num());
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSaveAllDirtyAssets(const TSharedPtr<FJsonObject>& Params)
{
    bool bPackagesNeededSaving = false;
    const bool bSaved = FEditorFileUtils::SaveDirtyPackages(
        false,
        true,
        true,
        true,
        false,
        false,
        &bPackagesNeededSaving);

    if (!bSaved)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to save dirty packages"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetBoolField(TEXT("packages_needed_saving"), bPackagesNeededSaving);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleGetBlueprintSummary(const TSharedPtr<FJsonObject>& Params)
{
    FAssetData AssetData;
    FString ErrorMessage;
    if (!ResolveAssetDataFromParams(Params, AssetData, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Asset is not a Blueprint: %s"), *AssetData.GetObjectPathString()));
    }
    return CreateBlueprintSummary(Blueprint, AssetData);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialNameText;
    if (!Params->TryGetStringField(TEXT("name"), MaterialNameText))
    {
        Params->TryGetStringField(TEXT("material_name"), MaterialNameText);
    }

    if (MaterialNameText.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'name' 参数"));
    }

    FString PackagePath;
    FString ErrorMessage;
    if (!UnrealMCPAssetNormalizePackagePath(
        Params->HasField(TEXT("path")) ? Params->GetStringField(TEXT("path")) : TEXT(""),
        TEXT("/Game/Materials"),
        PackagePath,
        ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    FString AssetName;
    FString AssetPath;
    if (!UnrealMCPAssetBuildAssetPath(MaterialNameText, PackagePath, AssetName, AssetPath, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    if (!UEditorAssetLibrary::DoesDirectoryExist(PackagePath) && !UEditorAssetLibrary::MakeDirectory(PackagePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("创建目录失败: %s"), *PackagePath));
    }

    if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("材质已存在: %s"), *AssetPath));
    }

    UPackage* Package = CreatePackage(*AssetPath);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("创建资源包失败: %s"), *AssetPath));
    }

    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(Factory->FactoryCreateNew(UMaterial::StaticClass(), Package, *AssetName, RF_Public | RF_Standalone, nullptr, GWarn))
        : nullptr;
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("创建材质失败"));
    }

    Material->PostEditChange();
    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Material);
    if (!UEditorAssetLibrary::SaveAsset(AssetPath, false))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("保存材质失败: %s"), *AssetPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("asset_name"), AssetName);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("package_path"), PackagePath);
    Result->SetStringField(TEXT("asset_class"), TEXT("Material"));
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialInstanceNameText;
    if (!Params->TryGetStringField(TEXT("name"), MaterialInstanceNameText))
    {
        Params->TryGetStringField(TEXT("material_instance_name"), MaterialInstanceNameText);
    }

    if (MaterialInstanceNameText.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'name' 参数"));
    }

    FString ParentMaterialReference;
    if (!Params->TryGetStringField(TEXT("parent_material"), ParentMaterialReference))
    {
        Params->TryGetStringField(TEXT("parent"), ParentMaterialReference);
    }
    if (ParentMaterialReference.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'parent_material' 参数"));
    }

    UMaterialInterface* ParentMaterial = nullptr;
    FString ErrorMessage;
    if (!ResolveMaterialInterfaceByReference(ParentMaterialReference, ParentMaterial, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    FString PackagePath;
    if (!UnrealMCPAssetNormalizePackagePath(
        Params->HasField(TEXT("path")) ? Params->GetStringField(TEXT("path")) : TEXT(""),
        TEXT("/Game/Materials"),
        PackagePath,
        ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    FString AssetName;
    FString AssetPath;
    if (!UnrealMCPAssetBuildAssetPath(MaterialInstanceNameText, PackagePath, AssetName, AssetPath, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    if (!UEditorAssetLibrary::DoesDirectoryExist(PackagePath) && !UEditorAssetLibrary::MakeDirectory(PackagePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("创建目录失败: %s"), *PackagePath));
    }

    if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("材质实例已存在: %s"), *AssetPath));
    }

    UPackage* Package = CreatePackage(*AssetPath);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("创建资源包失败: %s"), *AssetPath));
    }

    UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
    if (!Factory)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("创建材质实例工厂失败"));
    }

    Factory->InitialParent = ParentMaterial;
    UMaterialInstanceConstant* MaterialInstance = Cast<UMaterialInstanceConstant>(
        Factory->FactoryCreateNew(UMaterialInstanceConstant::StaticClass(), Package, *AssetName, RF_Public | RF_Standalone, nullptr, GWarn));
    if (!MaterialInstance)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("创建材质实例失败"));
    }

    UMaterialEditingLibrary::UpdateMaterialInstance(MaterialInstance);
    MaterialInstance->PostEditChange();
    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(MaterialInstance);
    if (!UEditorAssetLibrary::SaveAsset(AssetPath, false))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("保存材质实例失败: %s"), *AssetPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("asset_name"), AssetName);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetStringField(TEXT("package_path"), PackagePath);
    Result->SetStringField(TEXT("asset_class"), TEXT("MaterialInstanceConstant"));
    Result->SetStringField(TEXT("parent_material"), ParentMaterial->GetPathName());
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleGetMaterialParameters(const TSharedPtr<FJsonObject>& Params)
{
    UMaterialInterface* Material = nullptr;
    FAssetData AssetData;
    FString ErrorMessage;
    if (!ResolveMaterialInterfaceFromParams(Params, Material, AssetData, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    TSharedPtr<FJsonObject> Result = CreateAssetIdentityObject(AssetData);
    const bool bIsMaterialInstance = Material->IsA<UMaterialInstance>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("summary_kind"), bIsMaterialInstance ? TEXT("MaterialInstance") : TEXT("Material"));
    Result->SetBoolField(TEXT("is_material_instance"), bIsMaterialInstance);

    if (const UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material))
    {
        Result->SetStringField(TEXT("parent_material"), MaterialInstance->Parent ? MaterialInstance->Parent->GetPathName() : FString());
    }

    TArray<FAssetData> ChildInstances;
    UMaterialEditingLibrary::GetChildInstances(Material, ChildInstances);
    ChildInstances.Sort([](const FAssetData& A, const FAssetData& B)
    {
        return A.GetObjectPathString() < B.GetObjectPathString();
    });

    TArray<TSharedPtr<FJsonValue>> ChildInstanceValues;
    ChildInstanceValues.Reserve(ChildInstances.Num());
    for (const FAssetData& ChildInstance : ChildInstances)
    {
        ChildInstanceValues.Add(MakeShared<FJsonValueObject>(CreateAssetIdentityObject(ChildInstance)));
    }
    Result->SetArrayField(TEXT("child_instances"), ChildInstanceValues);
    Result->SetNumberField(TEXT("child_instance_count"), ChildInstanceValues.Num());

    auto CollectParameters = [&Material](
        EMaterialParameterType ParameterType,
        TFunctionRef<void(const FMaterialParameterInfo&, const FMaterialParameterMetadata&, const TSharedPtr<FJsonObject>&)> FillValue)
    {
        TMap<FMaterialParameterInfo, FMaterialParameterMetadata> Parameters;
        Material->GetAllParametersOfType(ParameterType, Parameters);

        TArray<FMaterialParameterInfo> ParameterInfos;
        Parameters.GetKeys(ParameterInfos);
        ParameterInfos.Sort([](const FMaterialParameterInfo& A, const FMaterialParameterInfo& B)
        {
            const FString AName = A.Name.ToString();
            const FString BName = B.Name.ToString();
            if (AName != BName)
            {
                return AName < BName;
            }
            if (A.Association != B.Association)
            {
                return static_cast<uint8>(A.Association) < static_cast<uint8>(B.Association);
            }
            return A.Index < B.Index;
        });

        TArray<TSharedPtr<FJsonValue>> Values;
        Values.Reserve(ParameterInfos.Num());
        for (const FMaterialParameterInfo& ParameterInfo : ParameterInfos)
        {
            const FMaterialParameterMetadata* Metadata = Parameters.Find(ParameterInfo);
            if (!Metadata)
            {
                continue;
            }

            TSharedPtr<FJsonObject> ParameterObject = CreateMaterialParameterObject(ParameterInfo, *Metadata);
            FillValue(ParameterInfo, *Metadata, ParameterObject);
            Values.Add(MakeShared<FJsonValueObject>(ParameterObject));
        }
        return Values;
    };

    const TArray<TSharedPtr<FJsonValue>> ScalarParameters = CollectParameters(
        EMaterialParameterType::Scalar,
        [](const FMaterialParameterInfo&, const FMaterialParameterMetadata& Metadata, const TSharedPtr<FJsonObject>& ParameterObject)
        {
            ParameterObject->SetNumberField(TEXT("value"), Metadata.Value.AsScalar());
        });

    const TArray<TSharedPtr<FJsonValue>> VectorParameters = CollectParameters(
        EMaterialParameterType::Vector,
        [](const FMaterialParameterInfo&, const FMaterialParameterMetadata& Metadata, const TSharedPtr<FJsonObject>& ParameterObject)
        {
            ParameterObject->SetArrayField(TEXT("value"), MakeLinearColorArray(Metadata.Value.AsLinearColor()));
        });

    const TArray<TSharedPtr<FJsonValue>> TextureParameters = CollectParameters(
        EMaterialParameterType::Texture,
        [](const FMaterialParameterInfo&, const FMaterialParameterMetadata& Metadata, const TSharedPtr<FJsonObject>& ParameterObject)
        {
            UTexture* Texture = Metadata.Value.Texture;
            ParameterObject->SetStringField(TEXT("texture_name"), Texture ? Texture->GetName() : FString());
            ParameterObject->SetStringField(TEXT("texture_path"), Texture ? Texture->GetPathName() : FString());
        });

    const TArray<TSharedPtr<FJsonValue>> StaticSwitchParameters = CollectParameters(
        EMaterialParameterType::StaticSwitch,
        [](const FMaterialParameterInfo&, const FMaterialParameterMetadata& Metadata, const TSharedPtr<FJsonObject>& ParameterObject)
        {
            ParameterObject->SetBoolField(TEXT("value"), Metadata.Value.AsStaticSwitch());
            ParameterObject->SetBoolField(TEXT("dynamic"), Metadata.bDynamicSwitchParameter);
        });

    Result->SetArrayField(TEXT("scalar_parameters"), ScalarParameters);
    Result->SetArrayField(TEXT("vector_parameters"), VectorParameters);
    Result->SetArrayField(TEXT("texture_parameters"), TextureParameters);
    Result->SetArrayField(TEXT("static_switch_parameters"), StaticSwitchParameters);
    Result->SetNumberField(TEXT("scalar_parameter_count"), ScalarParameters.Num());
    Result->SetNumberField(TEXT("vector_parameter_count"), VectorParameters.Num());
    Result->SetNumberField(TEXT("texture_parameter_count"), TextureParameters.Num());
    Result->SetNumberField(TEXT("static_switch_parameter_count"), StaticSwitchParameters.Num());
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSetMaterialInstanceScalarParameter(const TSharedPtr<FJsonObject>& Params)
{
    UMaterialInstanceConstant* MaterialInstance = nullptr;
    FAssetData AssetData;
    FString ErrorMessage;
    if (!ResolveMaterialInstanceFromParams(Params, MaterialInstance, AssetData, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    FString ParameterNameText;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterNameText) || ParameterNameText.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'parameter_name' 参数"));
    }

    if (!Params->HasField(TEXT("value")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'value' 参数"));
    }

    const float Value = static_cast<float>(Params->GetNumberField(TEXT("value")));
    UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MaterialInstance, *ParameterNameText, Value);

    UMaterialEditingLibrary::UpdateMaterialInstance(MaterialInstance);
    MaterialInstance->PostEditChange();
    MaterialInstance->MarkPackageDirty();
    if (!UEditorAssetLibrary::SaveAsset(AssetData.PackageName.ToString(), false))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("保存材质实例失败: %s"), *AssetData.PackageName.ToString()));
    }

    TSharedPtr<FJsonObject> Result = CreateAssetIdentityObject(AssetData);
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("parameter_name"), ParameterNameText);
    Result->SetNumberField(TEXT("value"), UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(MaterialInstance, *ParameterNameText));
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSetMaterialInstanceVectorParameter(const TSharedPtr<FJsonObject>& Params)
{
    UMaterialInstanceConstant* MaterialInstance = nullptr;
    FAssetData AssetData;
    FString ErrorMessage;
    if (!ResolveMaterialInstanceFromParams(Params, MaterialInstance, AssetData, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    FString ParameterNameText;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterNameText) || ParameterNameText.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'parameter_name' 参数"));
    }

    FLinearColor Value;
    if (!TryParseLinearColor(Params, TEXT("value"), Value, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(MaterialInstance, *ParameterNameText, Value);

    UMaterialEditingLibrary::UpdateMaterialInstance(MaterialInstance);
    MaterialInstance->PostEditChange();
    MaterialInstance->MarkPackageDirty();
    if (!UEditorAssetLibrary::SaveAsset(AssetData.PackageName.ToString(), false))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("保存材质实例失败: %s"), *AssetData.PackageName.ToString()));
    }

    TSharedPtr<FJsonObject> Result = CreateAssetIdentityObject(AssetData);
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("parameter_name"), ParameterNameText);
    Result->SetArrayField(
        TEXT("value"),
        MakeLinearColorArray(UMaterialEditingLibrary::GetMaterialInstanceVectorParameterValue(MaterialInstance, *ParameterNameText)));
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSetMaterialInstanceTextureParameter(const TSharedPtr<FJsonObject>& Params)
{
    UMaterialInstanceConstant* MaterialInstance = nullptr;
    FAssetData AssetData;
    FString ErrorMessage;
    if (!ResolveMaterialInstanceFromParams(Params, MaterialInstance, AssetData, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    FString ParameterNameText;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterNameText) || ParameterNameText.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'parameter_name' 参数"));
    }

    FString TextureReference;
    if (!Params->TryGetStringField(TEXT("texture_asset_path"), TextureReference))
    {
        Params->TryGetStringField(TEXT("texture"), TextureReference);
    }
    if (TextureReference.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'texture_asset_path' 参数"));
    }

    FAssetData TextureAssetData;
    if (!ResolveAssetReference(TextureReference, TextureAssetData, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    UTexture* Texture = Cast<UTexture>(TextureAssetData.GetAsset());
    if (!Texture)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("资源不是 Texture: %s"), *TextureAssetData.GetObjectPathString()));
    }

    UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MaterialInstance, *ParameterNameText, Texture);

    UMaterialEditingLibrary::UpdateMaterialInstance(MaterialInstance);
    MaterialInstance->PostEditChange();
    MaterialInstance->MarkPackageDirty();
    if (!UEditorAssetLibrary::SaveAsset(AssetData.PackageName.ToString(), false))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("保存材质实例失败: %s"), *AssetData.PackageName.ToString()));
    }

    UTexture* CurrentTexture = UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(MaterialInstance, *ParameterNameText);
    TSharedPtr<FJsonObject> Result = CreateAssetIdentityObject(AssetData);
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("parameter_name"), ParameterNameText);
    Result->SetStringField(TEXT("texture_name"), CurrentTexture ? CurrentTexture->GetName() : FString());
    Result->SetStringField(TEXT("texture_path"), CurrentTexture ? CurrentTexture->GetPathName() : FString());
    return Result;
}
