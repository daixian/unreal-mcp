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
#include "Components/MeshComponent.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
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
#include "Kismet/GameplayStatics.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
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
#include "UObject/UObjectIterator.h"
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

static TSharedPtr<FJsonObject> CreateMetadataJsonObject(const TMap<FName, FString>& MetadataValues)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    TArray<FName> MetadataKeys;
    MetadataValues.GetKeys(MetadataKeys);
    MetadataKeys.Sort(FNameLexicalLess());

    for (const FName& MetadataKey : MetadataKeys)
    {
        const FString* Value = MetadataValues.Find(MetadataKey);
        if (Value)
        {
            Result->SetStringField(MetadataKey.ToString(), *Value);
        }
    }

    return Result;
}

static FString ConvertJsonValueToMetadataString(const TSharedPtr<FJsonValue>& JsonValue)
{
    if (!JsonValue.IsValid())
    {
        return FString();
    }

    switch (JsonValue->Type)
    {
    case EJson::String:
        return JsonValue->AsString();
    case EJson::Number:
        return FString::SanitizeFloat(JsonValue->AsNumber());
    case EJson::Boolean:
        return JsonValue->AsBool() ? TEXT("true") : TEXT("false");
    case EJson::Null:
        return FString();
    case EJson::Array:
    {
        FString SerializedValue;
        const TArray<TSharedPtr<FJsonValue>>& ArrayValue = JsonValue->AsArray();
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SerializedValue);
        FJsonSerializer::Serialize(ArrayValue, Writer);
        return SerializedValue;
    }
    case EJson::Object:
    {
        FString SerializedValue;
        const TSharedPtr<FJsonObject> ObjectValue = JsonValue->AsObject();
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SerializedValue);
        FJsonSerializer::Serialize(ObjectValue.ToSharedRef(), Writer);
        return SerializedValue;
    }
    case EJson::None:
    default:
        return JsonValue->AsString();
    }
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

/**
 * @brief 从参数中解析基础材质资产。
 * @param [in] Params 命令参数。
 * @param [out] OutMaterial 解析得到的基础材质。
 * @param [out] OutAssetData 对应资产数据。
 * @param [out] OutErrorMessage 失败时输出错误信息。
 * @return bool 是否解析成功。
 */
static bool ResolveBaseMaterialFromParams(
    const TSharedPtr<FJsonObject>& Params,
    UMaterial*& OutMaterial,
    FAssetData& OutAssetData,
    FString& OutErrorMessage)
{
    UMaterialInterface* MaterialInterface = nullptr;
    if (!ResolveMaterialInterfaceFromParams(Params, MaterialInterface, OutAssetData, OutErrorMessage))
    {
        return false;
    }

    OutMaterial = Cast<UMaterial>(MaterialInterface);
    if (!OutMaterial)
    {
        OutErrorMessage = FString::Printf(TEXT("资源不是基础 Material: %s"), *OutAssetData.GetObjectPathString());
        return false;
    }

    return true;
}

/**
 * @brief 读取整型二维节点坐标字段。
 * @param [in] Params 命令参数。
 * @param [in] FieldName 字段名。
 * @param [out] OutX 输出 X 坐标。
 * @param [out] OutY 输出 Y 坐标。
 * @return bool 字段是否存在且解析成功。
 */
static bool UnrealMCPAssetTryGetIntPointField(
    const TSharedPtr<FJsonObject>& Params,
    const FString& FieldName,
    int32& OutX,
    int32& OutY)
{
    if (!Params.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* JsonValues = nullptr;
    if (!Params->TryGetArrayField(FieldName, JsonValues) || !JsonValues || JsonValues->Num() < 2)
    {
        return false;
    }

    if (!(*JsonValues)[0].IsValid() || !(*JsonValues)[1].IsValid())
    {
        return false;
    }

    OutX = FMath::RoundToInt((*JsonValues)[0]->AsNumber());
    OutY = FMath::RoundToInt((*JsonValues)[1]->AsNumber());
    return true;
}

/**
 * @brief 解析材质表达式节点坐标。
 * @param [in] Params 命令参数。
 * @param [out] OutNodePosX 输出 X 坐标。
 * @param [out] OutNodePosY 输出 Y 坐标。
 */
static void UnrealMCPAssetResolveMaterialNodePosition(
    const TSharedPtr<FJsonObject>& Params,
    int32& OutNodePosX,
    int32& OutNodePosY)
{
    OutNodePosX = 0;
    OutNodePosY = 0;

    if (!Params.IsValid())
    {
        return;
    }

    if (UnrealMCPAssetTryGetIntPointField(Params, TEXT("node_position"), OutNodePosX, OutNodePosY))
    {
        return;
    }

    if (Params->HasTypedField<EJson::Number>(TEXT("node_pos_x")))
    {
        OutNodePosX = FMath::RoundToInt(Params->GetNumberField(TEXT("node_pos_x")));
    }
    if (Params->HasTypedField<EJson::Number>(TEXT("node_pos_y")))
    {
        OutNodePosY = FMath::RoundToInt(Params->GetNumberField(TEXT("node_pos_y")));
    }
}

/**
 * @brief 解析材质表达式类。
 * @param [in] Params 命令参数。
 * @param [out] OutExpressionClass 解析得到的表达式类型。
 * @param [out] OutResolvedClassPath 解析出的类路径。
 * @param [out] OutErrorMessage 失败时输出错误信息。
 * @return bool 是否解析成功。
 */
static bool UnrealMCPAssetResolveMaterialExpressionClass(
    const TSharedPtr<FJsonObject>& Params,
    UClass*& OutExpressionClass,
    FString& OutResolvedClassPath,
    FString& OutErrorMessage)
{
    FString ClassReference;
    if (!Params->TryGetStringField(TEXT("expression_class"), ClassReference))
    {
        if (!Params->TryGetStringField(TEXT("expression_class_path"), ClassReference))
        {
            Params->TryGetStringField(TEXT("class_name"), ClassReference);
        }
    }

    ClassReference = ClassReference.TrimStartAndEnd();
    if (ClassReference.IsEmpty())
    {
        OutErrorMessage = TEXT("缺少 'expression_class' 参数");
        return false;
    }

    TArray<FString> CandidateClassPaths;
    if (ClassReference.StartsWith(TEXT("/")))
    {
        CandidateClassPaths.Add(ClassReference);
    }
    else
    {
        const FString NormalizedClassName = ClassReference.StartsWith(TEXT("MaterialExpression"))
            ? ClassReference
            : TEXT("MaterialExpression") + ClassReference;
        CandidateClassPaths.Add(TEXT("/Script/Engine.") + NormalizedClassName);
        CandidateClassPaths.Add(TEXT("/Script/Engine.") + ClassReference);
    }

    for (const FString& CandidateClassPath : CandidateClassPaths)
    {
        if (UClass* LoadedClass = LoadObject<UClass>(nullptr, *CandidateClassPath))
        {
            if (!LoadedClass->IsChildOf(UMaterialExpression::StaticClass()))
            {
                OutErrorMessage = FString::Printf(TEXT("类型不是材质表达式: %s"), *LoadedClass->GetPathName());
                return false;
            }

            OutExpressionClass = LoadedClass;
            OutResolvedClassPath = LoadedClass->GetPathName();
            return true;
        }
    }

    for (TObjectIterator<UClass> It; It; ++It)
    {
        UClass* CandidateClass = *It;
        if (!CandidateClass || !CandidateClass->IsChildOf(UMaterialExpression::StaticClass()))
        {
            continue;
        }

        if (CandidateClass->GetName().Equals(ClassReference, ESearchCase::IgnoreCase) ||
            CandidateClass->GetName().Equals(TEXT("MaterialExpression") + ClassReference, ESearchCase::IgnoreCase))
        {
            OutExpressionClass = CandidateClass;
            OutResolvedClassPath = CandidateClass->GetPathName();
            return true;
        }
    }

    OutErrorMessage = FString::Printf(TEXT("未找到材质表达式类型: %s"), *ClassReference);
    return false;
}

static FString UnrealMCPAssetGetRequestedWorldType(const TSharedPtr<FJsonObject>& Params)
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

static UWorld* UnrealMCPAssetGetEditorWorld()
{
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

static UWorld* UnrealMCPAssetGetPIEWorld()
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

static UWorld* UnrealMCPAssetResolveWorldByParams(
    const TSharedPtr<FJsonObject>& Params,
    FString& OutResolvedWorldType,
    FString& OutErrorMessage)
{
    const FString RequestedWorldType = UnrealMCPAssetGetRequestedWorldType(Params);
    if (RequestedWorldType == TEXT("editor"))
    {
        UWorld* EditorWorld = UnrealMCPAssetGetEditorWorld();
        if (!EditorWorld)
        {
            OutErrorMessage = TEXT("获取编辑器世界失败");
            return nullptr;
        }

        OutResolvedWorldType = TEXT("editor");
        return EditorWorld;
    }

    if (RequestedWorldType == TEXT("pie"))
    {
        UWorld* PIEWorld = UnrealMCPAssetGetPIEWorld();
        if (!PIEWorld)
        {
            OutErrorMessage = TEXT("PIE 世界未运行，请先启动 Play-In-Editor");
            return nullptr;
        }

        OutResolvedWorldType = TEXT("pie");
        return PIEWorld;
    }

    if (RequestedWorldType == TEXT("auto"))
    {
        if (UWorld* PIEWorld = UnrealMCPAssetGetPIEWorld())
        {
            OutResolvedWorldType = TEXT("pie");
            return PIEWorld;
        }

        if (UWorld* EditorWorld = UnrealMCPAssetGetEditorWorld())
        {
            OutResolvedWorldType = TEXT("editor");
            return EditorWorld;
        }

        OutErrorMessage = TEXT("自动模式下无法解析到可用世界");
        return nullptr;
    }

    OutErrorMessage = FString::Printf(TEXT("无效的 world_type: %s，可选值为 auto、editor、pie"), *RequestedWorldType);
    return nullptr;
}

static AActor* UnrealMCPAssetResolveActorByParams(
    const TSharedPtr<FJsonObject>& Params,
    UWorld*& OutWorld,
    FString& OutResolvedWorldType,
    FString& OutErrorMessage)
{
    OutWorld = UnrealMCPAssetResolveWorldByParams(Params, OutResolvedWorldType, OutErrorMessage);
    if (!OutWorld)
    {
        return nullptr;
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(OutWorld, AActor::StaticClass(), AllActors);

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

        OutErrorMessage = FString::Printf(TEXT("未找到 actor_path 对应的 Actor: %s"), *ActorPath);
        return nullptr;
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName) || ActorName.TrimStartAndEnd().IsEmpty())
    {
        OutErrorMessage = TEXT("缺少 'name' 或 'actor_path' 参数");
        return nullptr;
    }

    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return Actor;
        }
    }

    OutErrorMessage = FString::Printf(TEXT("未找到 Actor: %s"), *ActorName);
    return nullptr;
}

static void UnrealMCPAssetCollectMeshComponents(AActor* Actor, TArray<UMeshComponent*>& OutMeshComponents)
{
    OutMeshComponents.Reset();
    if (!Actor)
    {
        return;
    }

    Actor->GetComponents<UMeshComponent>(OutMeshComponents);
    OutMeshComponents.Sort([](const UMeshComponent& A, const UMeshComponent& B)
    {
        return A.GetName() < B.GetName();
    });
}

static UMeshComponent* UnrealMCPAssetResolveMeshComponent(
    AActor* Actor,
    const FString& ComponentName,
    FString& OutErrorMessage)
{
    TArray<UMeshComponent*> MeshComponents;
    UnrealMCPAssetCollectMeshComponents(Actor, MeshComponents);

    if (ComponentName.TrimStartAndEnd().IsEmpty())
    {
        if (MeshComponents.Num() == 1)
        {
            return MeshComponents[0];
        }

        if (MeshComponents.Num() == 0)
        {
            OutErrorMessage = TEXT("目标 Actor 上没有 MeshComponent");
            return nullptr;
        }

        TArray<FString> ComponentNames;
        ComponentNames.Reserve(MeshComponents.Num());
        for (const UMeshComponent* MeshComponent : MeshComponents)
        {
            ComponentNames.Add(MeshComponent ? MeshComponent->GetName() : FString());
        }

        OutErrorMessage = FString::Printf(
            TEXT("目标 Actor 上存在多个 MeshComponent，请指定 component_name: %s"),
            *FString::Join(ComponentNames, TEXT(", ")));
        return nullptr;
    }

    for (UMeshComponent* MeshComponent : MeshComponents)
    {
        if (MeshComponent && MeshComponent->GetName() == ComponentName)
        {
            return MeshComponent;
        }
    }

    OutErrorMessage = FString::Printf(TEXT("未找到 MeshComponent: %s"), *ComponentName);
    return nullptr;
}

static bool UnrealMCPAssetResolveMaterialAssignmentFromParams(
    const TSharedPtr<FJsonObject>& Params,
    UMaterialInterface*& OutMaterial,
    FAssetData& OutAssetData,
    FString& OutErrorMessage)
{
    FString MaterialReference;
    if (!Params->TryGetStringField(TEXT("material_asset_path"), MaterialReference))
    {
        if (!Params->TryGetStringField(TEXT("material"), MaterialReference))
        {
            Params->TryGetStringField(TEXT("material_path"), MaterialReference);
        }
    }

    if (MaterialReference.TrimStartAndEnd().IsEmpty())
    {
        OutErrorMessage = TEXT("缺少 'material_asset_path' 参数");
        return false;
    }

    if (!ResolveAssetReference(MaterialReference, OutAssetData, OutErrorMessage))
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

static bool UnrealMCPAssetResolveMaterialSlotIndex(
    UMeshComponent* MeshComponent,
    const TSharedPtr<FJsonObject>& Params,
    int32& OutSlotIndex,
    FString& OutSlotName,
    bool bRequireExplicitSlot,
    FString& OutErrorMessage)
{
    OutSlotIndex = 0;
    OutSlotName.Reset();

    if (!MeshComponent)
    {
        OutErrorMessage = TEXT("MeshComponent 无效");
        return false;
    }

    bool bHasSlotIndex = false;
    if (Params->HasTypedField<EJson::Number>(TEXT("slot_index")))
    {
        OutSlotIndex = static_cast<int32>(Params->GetNumberField(TEXT("slot_index")));
        bHasSlotIndex = true;
    }

    bool bHasSlotName = false;
    Params->TryGetStringField(TEXT("slot_name"), OutSlotName);
    OutSlotName = OutSlotName.TrimStartAndEnd();
    bHasSlotName = !OutSlotName.IsEmpty();

    if (bRequireExplicitSlot && !bHasSlotIndex && !bHasSlotName)
    {
        OutErrorMessage = TEXT("缺少 'slot_index' 或 'slot_name' 参数");
        return false;
    }

    const TArray<FName> SlotNames = MeshComponent->GetMaterialSlotNames();
    if (bHasSlotName)
    {
        bool bFoundSlot = false;
        for (int32 Index = 0; Index < SlotNames.Num(); ++Index)
        {
            if (SlotNames[Index].ToString().Equals(OutSlotName, ESearchCase::IgnoreCase))
            {
                OutSlotIndex = Index;
                OutSlotName = SlotNames[Index].ToString();
                bFoundSlot = true;
                break;
            }
        }

        if (!bFoundSlot)
        {
            TArray<FString> AvailableSlotNames;
            AvailableSlotNames.Reserve(SlotNames.Num());
            for (const FName& SlotName : SlotNames)
            {
                AvailableSlotNames.Add(SlotName.ToString());
            }

            OutErrorMessage = FString::Printf(
                TEXT("组件 %s 上未找到材质槽 %s，可用槽位: %s"),
                *MeshComponent->GetName(),
                *OutSlotName,
                AvailableSlotNames.Num() > 0 ? *FString::Join(AvailableSlotNames, TEXT(", ")) : TEXT("<空>"));
            return false;
        }
    }

    if (OutSlotIndex < 0)
    {
        OutErrorMessage = TEXT("'slot_index' 不能小于 0");
        return false;
    }

    if (SlotNames.IsValidIndex(OutSlotIndex))
    {
        OutSlotName = SlotNames[OutSlotIndex].ToString();
    }

    const int32 MaterialCount = MeshComponent->GetNumMaterials();
    if (MaterialCount <= OutSlotIndex && !SlotNames.IsValidIndex(OutSlotIndex))
    {
        OutErrorMessage = FString::Printf(
            TEXT("组件 %s 没有可用的材质槽 %d"),
            *MeshComponent->GetName(),
            OutSlotIndex);
        return false;
    }

    return true;
}

static void UnrealMCPAssetMarkMaterialAssignmentDirty(
    AActor* Actor,
    UMeshComponent* MeshComponent,
    const FString& ResolvedWorldType)
{
    if (ResolvedWorldType != TEXT("editor"))
    {
        return;
    }

    if (Actor)
    {
        Actor->Modify();
        Actor->MarkPackageDirty();
    }

    if (MeshComponent)
    {
        MeshComponent->Modify();
        MeshComponent->MarkRenderStateDirty();
        MeshComponent->MarkPackageDirty();
    }
}

static TSharedPtr<FJsonObject> UnrealMCPAssetCreateMaterialSlotResult(
    UMeshComponent* MeshComponent,
    int32 SlotIndex,
    const FString& SlotName,
    UMaterialInterface* PreviousMaterial,
    UMaterialInterface* CurrentMaterial)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("component_name"), MeshComponent ? MeshComponent->GetName() : FString());
    Result->SetStringField(TEXT("component_path"), MeshComponent ? MeshComponent->GetPathName() : FString());
    Result->SetNumberField(TEXT("slot_index"), SlotIndex);
    Result->SetStringField(TEXT("slot_name"), SlotName);
    Result->SetStringField(TEXT("previous_material_name"), PreviousMaterial ? PreviousMaterial->GetName() : FString());
    Result->SetStringField(TEXT("previous_material_path"), PreviousMaterial ? PreviousMaterial->GetPathName() : FString());
    Result->SetStringField(TEXT("material_name"), CurrentMaterial ? CurrentMaterial->GetName() : FString());
    Result->SetStringField(TEXT("material_path"), CurrentMaterial ? CurrentMaterial->GetPathName() : FString());
    return Result;
}

/**
 * @brief 读取材质表达式的输出名列表。
 * @param [in] Expression 目标表达式。
 * @return TArray<TSharedPtr<FJsonValue>> 输出名数组。
 */
static TArray<TSharedPtr<FJsonValue>> UnrealMCPAssetGetMaterialExpressionOutputNames(UMaterialExpression* Expression)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    if (!Expression)
    {
        return Result;
    }

    TArray<FExpressionOutput>& Outputs = Expression->GetOutputs();
    Result.Reserve(Outputs.Num());
    for (int32 OutputIndex = 0; OutputIndex < Outputs.Num(); ++OutputIndex)
    {
        const FExpressionOutput& Output = Outputs[OutputIndex];
        const FString OutputName = Output.OutputName.IsNone()
            ? FString::Printf(TEXT("output_%d"), OutputIndex)
            : Output.OutputName.ToString();
        Result.Add(MakeShared<FJsonValueString>(OutputName));
    }

    return Result;
}

/**
 * @brief 读取材质表达式的输入名列表。
 * @param [in] Expression 目标表达式。
 * @return TArray<TSharedPtr<FJsonValue>> 输入名数组。
 */
static TArray<TSharedPtr<FJsonValue>> UnrealMCPAssetGetMaterialExpressionInputNames(UMaterialExpression* Expression)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    if (!Expression)
    {
        return Result;
    }

    const int32 InputCount = Expression->CountInputs();
    Result.Reserve(InputCount);
    for (int32 InputIndex = 0; InputIndex < InputCount; ++InputIndex)
    {
        const FName InputName = Expression->GetInputName(InputIndex);
        Result.Add(MakeShared<FJsonValueString>(InputName.IsNone() ? FString() : InputName.ToString()));
    }

    return Result;
}

/**
 * @brief 查找表达式在材质表达式数组中的索引。
 * @param [in] Material 所属材质。
 * @param [in] Expression 目标表达式。
 * @return int32 找到时返回索引，否则返回 `INDEX_NONE`。
 */
static int32 UnrealMCPAssetFindMaterialExpressionIndex(UMaterial* Material, UMaterialExpression* Expression)
{
    if (!Material || !Expression)
    {
        return INDEX_NONE;
    }

    const TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Material->GetExpressions();
    for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ++ExpressionIndex)
    {
        if (Expressions[ExpressionIndex] == Expression)
        {
            return ExpressionIndex;
        }
    }

    return INDEX_NONE;
}

/**
 * @brief 构建材质表达式摘要对象。
 * @param [in] Material 所属材质。
 * @param [in] Expression 目标表达式。
 * @return TSharedPtr<FJsonObject> 表达式摘要。
 */
static TSharedPtr<FJsonObject> UnrealMCPAssetCreateMaterialExpressionObject(
    UMaterial* Material,
    UMaterialExpression* Expression)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("material_name"), Material ? Material->GetName() : FString());
    Result->SetStringField(TEXT("material_path"), Material ? Material->GetPathName() : FString());
    Result->SetStringField(TEXT("expression_name"), Expression ? Expression->GetName() : FString());
    Result->SetStringField(TEXT("expression_path"), Expression ? Expression->GetPathName() : FString());
    Result->SetStringField(TEXT("expression_class"), Expression ? Expression->GetClass()->GetName() : FString());
    Result->SetStringField(TEXT("expression_class_path"), Expression ? Expression->GetClass()->GetPathName() : FString());
    Result->SetNumberField(TEXT("expression_index"), UnrealMCPAssetFindMaterialExpressionIndex(Material, Expression));
    Result->SetNumberField(TEXT("node_pos_x"), Expression ? Expression->MaterialExpressionEditorX : 0);
    Result->SetNumberField(TEXT("node_pos_y"), Expression ? Expression->MaterialExpressionEditorY : 0);
    Result->SetArrayField(TEXT("input_names"), UnrealMCPAssetGetMaterialExpressionInputNames(Expression));
    Result->SetArrayField(TEXT("output_names"), UnrealMCPAssetGetMaterialExpressionOutputNames(Expression));
    Result->SetNumberField(TEXT("input_count"), Expression ? Expression->CountInputs() : 0);
    Result->SetNumberField(TEXT("output_count"), Expression ? Expression->GetOutputs().Num() : 0);
    return Result;
}

/**
 * @brief 在材质内解析指定表达式。
 * @param [in] Material 所属材质。
 * @param [in] Params 命令参数。
 * @param [in] Prefix 字段前缀，例如 `from_` 或 `to_`。
 * @param [out] OutExpression 解析得到的表达式。
 * @param [out] OutErrorMessage 失败时输出错误信息。
 * @return bool 是否解析成功。
 */
static bool UnrealMCPAssetResolveMaterialExpressionFromParams(
    UMaterial* Material,
    const TSharedPtr<FJsonObject>& Params,
    const FString& Prefix,
    UMaterialExpression*& OutExpression,
    FString& OutErrorMessage)
{
    OutExpression = nullptr;
    if (!Material)
    {
        OutErrorMessage = TEXT("材质无效");
        return false;
    }

    FString ExpressionPath;
    Params->TryGetStringField(Prefix + TEXT("expression_path"), ExpressionPath);
    ExpressionPath = ExpressionPath.TrimStartAndEnd();
    if (!ExpressionPath.IsEmpty())
    {
        for (UMaterialExpression* Expression : Material->GetExpressions())
        {
            if (Expression && Expression->GetPathName() == ExpressionPath)
            {
                OutExpression = Expression;
                return true;
            }
        }

        OutErrorMessage = FString::Printf(TEXT("未找到表达式路径 %s"), *ExpressionPath);
        return false;
    }

    if (Params->HasTypedField<EJson::Number>(Prefix + TEXT("expression_index")))
    {
        const int32 ExpressionIndex = static_cast<int32>(Params->GetNumberField(Prefix + TEXT("expression_index")));
        const TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Material->GetExpressions();
        if (!Expressions.IsValidIndex(ExpressionIndex) || !Expressions[ExpressionIndex])
        {
            OutErrorMessage = FString::Printf(TEXT("表达式索引越界: %d"), ExpressionIndex);
            return false;
        }

        OutExpression = Expressions[ExpressionIndex];
        return true;
    }

    FString ExpressionName;
    Params->TryGetStringField(Prefix + TEXT("expression_name"), ExpressionName);
    ExpressionName = ExpressionName.TrimStartAndEnd();
    if (ExpressionName.IsEmpty())
    {
        OutErrorMessage = FString::Printf(
            TEXT("缺少 '%sexpression_path'、'%sexpression_index' 或 '%sexpression_name' 参数"),
            *Prefix,
            *Prefix,
            *Prefix);
        return false;
    }

    TArray<UMaterialExpression*> MatchedExpressions;
    for (UMaterialExpression* Expression : Material->GetExpressions())
    {
        if (Expression && Expression->GetName().Equals(ExpressionName, ESearchCase::IgnoreCase))
        {
            MatchedExpressions.Add(Expression);
        }
    }

    if (MatchedExpressions.Num() == 1)
    {
        OutExpression = MatchedExpressions[0];
        return true;
    }

    if (MatchedExpressions.Num() > 1)
    {
        TArray<FString> CandidatePaths;
        CandidatePaths.Reserve(MatchedExpressions.Num());
        for (const UMaterialExpression* Expression : MatchedExpressions)
        {
            CandidatePaths.Add(Expression ? Expression->GetPathName() : FString());
        }

        OutErrorMessage = FString::Printf(
            TEXT("表达式名称 %s 匹配到多个节点，请改用 expression_path 或 expression_index: %s"),
            *ExpressionName,
            *FString::Join(CandidatePaths, TEXT(", ")));
        return false;
    }

    OutErrorMessage = FString::Printf(TEXT("未找到表达式名称: %s"), *ExpressionName);
    return false;
}

/**
 * @brief 把 JSON 数值数组转成 ImportText 结构体文本。
 * @param [in] StructName 结构体类型名。
 * @param [in] JsonValues 数组值。
 * @param [out] OutImportText 生成的 ImportText。
 * @return bool 是否生成成功。
 */
static bool UnrealMCPAssetBuildStructImportTextFromArray(
    const FString& StructName,
    const TArray<TSharedPtr<FJsonValue>>& JsonValues,
    FString& OutImportText)
{
    if (StructName == TEXT("Vector2D") && JsonValues.Num() >= 2)
    {
        OutImportText = FString::Printf(
            TEXT("(X=%s,Y=%s)"),
            *FString::SanitizeFloat(JsonValues[0]->AsNumber()),
            *FString::SanitizeFloat(JsonValues[1]->AsNumber()));
        return true;
    }

    if ((StructName == TEXT("Vector") || StructName == TEXT("Vector3f")) && JsonValues.Num() >= 3)
    {
        OutImportText = FString::Printf(
            TEXT("(X=%s,Y=%s,Z=%s)"),
            *FString::SanitizeFloat(JsonValues[0]->AsNumber()),
            *FString::SanitizeFloat(JsonValues[1]->AsNumber()),
            *FString::SanitizeFloat(JsonValues[2]->AsNumber()));
        return true;
    }

    if ((StructName == TEXT("LinearColor") || StructName == TEXT("Color")) && JsonValues.Num() >= 3)
    {
        const double Alpha = JsonValues.Num() >= 4 ? JsonValues[3]->AsNumber() : 1.0;
        OutImportText = FString::Printf(
            TEXT("(R=%s,G=%s,B=%s,A=%s)"),
            *FString::SanitizeFloat(JsonValues[0]->AsNumber()),
            *FString::SanitizeFloat(JsonValues[1]->AsNumber()),
            *FString::SanitizeFloat(JsonValues[2]->AsNumber()),
            *FString::SanitizeFloat(Alpha));
        return true;
    }

    return false;
}

/**
 * @brief 把 JSON 对象转成 ImportText 结构体文本。
 * @param [in] StructName 结构体类型名。
 * @param [in] JsonObject 对象值。
 * @param [out] OutImportText 生成的 ImportText。
 * @return bool 是否生成成功。
 */
static bool UnrealMCPAssetBuildStructImportTextFromObject(
    const FString& StructName,
    const TSharedPtr<FJsonObject>& JsonObject,
    FString& OutImportText)
{
    if (!JsonObject.IsValid())
    {
        return false;
    }

    double First = 0.0;
    double Second = 0.0;
    double Third = 0.0;
    double Fourth = 1.0;

    if (StructName == TEXT("Vector2D") &&
        JsonObject->TryGetNumberField(TEXT("X"), First) &&
        JsonObject->TryGetNumberField(TEXT("Y"), Second))
    {
        OutImportText = FString::Printf(TEXT("(X=%s,Y=%s)"), *FString::SanitizeFloat(First), *FString::SanitizeFloat(Second));
        return true;
    }

    if ((StructName == TEXT("Vector") || StructName == TEXT("Vector3f")) &&
        JsonObject->TryGetNumberField(TEXT("X"), First) &&
        JsonObject->TryGetNumberField(TEXT("Y"), Second) &&
        JsonObject->TryGetNumberField(TEXT("Z"), Third))
    {
        OutImportText = FString::Printf(
            TEXT("(X=%s,Y=%s,Z=%s)"),
            *FString::SanitizeFloat(First),
            *FString::SanitizeFloat(Second),
            *FString::SanitizeFloat(Third));
        return true;
    }

    if ((StructName == TEXT("LinearColor") || StructName == TEXT("Color")) &&
        JsonObject->TryGetNumberField(TEXT("R"), First) &&
        JsonObject->TryGetNumberField(TEXT("G"), Second) &&
        JsonObject->TryGetNumberField(TEXT("B"), Third))
    {
        JsonObject->TryGetNumberField(TEXT("A"), Fourth);
        OutImportText = FString::Printf(
            TEXT("(R=%s,G=%s,B=%s,A=%s)"),
            *FString::SanitizeFloat(First),
            *FString::SanitizeFloat(Second),
            *FString::SanitizeFloat(Third),
            *FString::SanitizeFloat(Fourth));
        return true;
    }

    return false;
}

/**
 * @brief 构造材质表达式属性写入所需的 ImportText 文本。
 * @param [in] Property 目标属性。
 * @param [in] Value JSON 值。
 * @param [out] OutImportText 生成的 ImportText。
 * @param [out] OutErrorMessage 失败时输出错误信息。
 * @return bool 是否转换成功。
 */
static bool UnrealMCPAssetBuildImportTextForProperty(
    FProperty* Property,
    const TSharedPtr<FJsonValue>& Value,
    FString& OutImportText,
    FString& OutErrorMessage)
{
    if (!Property || !Value.IsValid())
    {
        OutErrorMessage = TEXT("属性或输入值无效");
        return false;
    }

    switch (Value->Type)
    {
    case EJson::String:
        OutImportText = Value->AsString();
        return true;
    case EJson::Number:
        OutImportText = FString::SanitizeFloat(Value->AsNumber());
        return true;
    case EJson::Boolean:
        OutImportText = Value->AsBool() ? TEXT("True") : TEXT("False");
        return true;
    case EJson::Null:
        OutImportText = TEXT("None");
        return true;
    case EJson::Array:
        if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
        {
            if (UnrealMCPAssetBuildStructImportTextFromArray(
                StructProperty->Struct ? StructProperty->Struct->GetName() : FString(),
                Value->AsArray(),
                OutImportText))
            {
                return true;
            }
        }
        break;
    case EJson::Object:
        if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
        {
            if (UnrealMCPAssetBuildStructImportTextFromObject(
                StructProperty->Struct ? StructProperty->Struct->GetName() : FString(),
                Value->AsObject(),
                OutImportText))
            {
                return true;
            }
        }
        break;
    case EJson::None:
    default:
        break;
    }

    OutErrorMessage = FString::Printf(TEXT("属性 %s 的值格式当前不支持"), *Property->GetName());
    return false;
}

/**
 * @brief 设置材质表达式属性。
 * @param [in] Expression 目标表达式。
 * @param [in] PropertyName 属性名。
 * @param [in] Value JSON 值。
 * @param [out] OutErrorMessage 失败时输出错误信息。
 * @return bool 是否设置成功。
 */
static bool UnrealMCPAssetSetMaterialExpressionProperty(
    UMaterialExpression* Expression,
    const FString& PropertyName,
    const TSharedPtr<FJsonValue>& Value,
    FString& OutErrorMessage)
{
    if (!Expression)
    {
        OutErrorMessage = TEXT("材质表达式无效");
        return false;
    }

    FString CommonErrorMessage;
    if (FUnrealMCPCommonUtils::SetObjectProperty(Expression, PropertyName, Value, CommonErrorMessage))
    {
        return true;
    }

    FProperty* Property = Expression->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        OutErrorMessage = FString::Printf(TEXT("表达式属性不存在: %s"), *PropertyName);
        return false;
    }

    void* PropertyAddress = Property->ContainerPtrToValuePtr<void>(Expression);

    if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
    {
        FString ObjectReference;
        if (Value->Type == EJson::String)
        {
            ObjectReference = Value->AsString();
        }
        else if (Value->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> ObjectValue = Value->AsObject();
            if (ObjectValue.IsValid())
            {
                if (!ObjectValue->TryGetStringField(TEXT("asset_path"), ObjectReference))
                {
                    if (!ObjectValue->TryGetStringField(TEXT("object_path"), ObjectReference))
                    {
                        ObjectValue->TryGetStringField(TEXT("name"), ObjectReference);
                    }
                }
            }
        }

        ObjectReference = ObjectReference.TrimStartAndEnd();
        if (!ObjectReference.IsEmpty())
        {
            FAssetData ReferencedAssetData;
            FString ResolveErrorMessage;
            if (!ResolveAssetReference(ObjectReference, ReferencedAssetData, ResolveErrorMessage))
            {
                OutErrorMessage = ResolveErrorMessage;
                return false;
            }

            UObject* ReferencedObject = ReferencedAssetData.GetAsset();
            if (!ReferencedObject || !ReferencedObject->IsA(ObjectProperty->PropertyClass))
            {
                OutErrorMessage = FString::Printf(
                    TEXT("属性 %s 需要类型 %s，但给到的是 %s"),
                    *PropertyName,
                    *ObjectProperty->PropertyClass->GetName(),
                    ReferencedObject ? *ReferencedObject->GetClass()->GetName() : TEXT("<空>"));
                return false;
            }

            ObjectProperty->SetObjectPropertyValue(PropertyAddress, ReferencedObject);
            return true;
        }
    }

    FString ImportText;
    if (!UnrealMCPAssetBuildImportTextForProperty(Property, Value, ImportText, OutErrorMessage))
    {
        return false;
    }

    if (Property->ImportText_Direct(*ImportText, PropertyAddress, Expression, PPF_None) == nullptr)
    {
        OutErrorMessage = FString::Printf(TEXT("表达式属性 %s 导入失败，输入值=%s"), *PropertyName, *ImportText);
        return false;
    }

    return true;
}

/**
 * @brief 批量应用材质表达式属性覆写。
 * @param [in] Params 命令参数。
 * @param [in] Expression 目标表达式。
 * @param [out] OutAppliedProperties 成功应用的属性列表。
 * @param [out] OutErrorMessage 失败时输出错误信息。
 * @return bool 是否应用成功。
 */
static bool UnrealMCPAssetApplyMaterialExpressionPropertyOverrides(
    const TSharedPtr<FJsonObject>& Params,
    UMaterialExpression* Expression,
    TArray<TSharedPtr<FJsonValue>>& OutAppliedProperties,
    FString& OutErrorMessage)
{
    OutAppliedProperties.Reset();

    const TSharedPtr<FJsonObject>* PropertyValuesObject = nullptr;
    if (!Params.IsValid() || !Params->TryGetObjectField(TEXT("property_values"), PropertyValuesObject) ||
        !PropertyValuesObject || !(*PropertyValuesObject).IsValid())
    {
        return true;
    }

    TArray<FString> PropertyNames;
    (*PropertyValuesObject)->Values.GetKeys(PropertyNames);
    PropertyNames.Sort();

    for (const FString& PropertyName : PropertyNames)
    {
        const TSharedPtr<FJsonValue> PropertyValue = (*PropertyValuesObject)->Values.FindRef(PropertyName);
        if (!PropertyValue.IsValid())
        {
            continue;
        }

        if (!UnrealMCPAssetSetMaterialExpressionProperty(Expression, PropertyName, PropertyValue, OutErrorMessage))
        {
            OutErrorMessage = FString::Printf(TEXT("设置表达式属性失败 %s: %s"), *PropertyName, *OutErrorMessage);
            return false;
        }

        OutAppliedProperties.Add(MakeShared<FJsonValueString>(PropertyName));
    }

    return true;
}

/**
 * @brief 保存材质资产。
 * @param [in] Material 目标材质。
 * @param [out] OutErrorMessage 失败时输出错误信息。
 * @return bool 是否保存成功。
 */
static bool UnrealMCPAssetSaveMaterial(UMaterial* Material, FString& OutErrorMessage)
{
    if (!Material)
    {
        OutErrorMessage = TEXT("材质无效");
        return false;
    }

    Material->MarkPackageDirty();
    if (!UEditorAssetLibrary::SaveAsset(Material->GetPathName(), false))
    {
        OutErrorMessage = FString::Printf(TEXT("保存材质失败: %s"), *Material->GetPathName());
        return false;
    }

    return true;
}

/**
 * @brief 解析材质属性枚举。
 * @param [in] PropertyText 材质属性文本。
 * @param [out] OutProperty 解析得到的材质属性。
 * @param [out] OutNormalizedName 归一化后的属性名。
 * @param [out] OutErrorMessage 失败时输出错误信息。
 * @return bool 是否解析成功。
 */
static bool UnrealMCPAssetResolveMaterialProperty(
    const FString& PropertyText,
    EMaterialProperty& OutProperty,
    FString& OutNormalizedName,
    FString& OutErrorMessage)
{
    const FString Normalized = PropertyText.TrimStartAndEnd().ToLower().Replace(TEXT("-"), TEXT("_")).Replace(TEXT(" "), TEXT("_"));
    TMap<FString, EMaterialProperty> PropertyMap;
    PropertyMap.Add(TEXT("base_color"), MP_BaseColor);
    PropertyMap.Add(TEXT("emissive_color"), MP_EmissiveColor);
    PropertyMap.Add(TEXT("opacity"), MP_Opacity);
    PropertyMap.Add(TEXT("opacity_mask"), MP_OpacityMask);
    PropertyMap.Add(TEXT("metallic"), MP_Metallic);
    PropertyMap.Add(TEXT("specular"), MP_Specular);
    PropertyMap.Add(TEXT("roughness"), MP_Roughness);
    PropertyMap.Add(TEXT("anisotropy"), MP_Anisotropy);
    PropertyMap.Add(TEXT("normal"), MP_Normal);
    PropertyMap.Add(TEXT("tangent"), MP_Tangent);
    PropertyMap.Add(TEXT("world_position_offset"), MP_WorldPositionOffset);
    PropertyMap.Add(TEXT("subsurface_color"), MP_SubsurfaceColor);
    PropertyMap.Add(TEXT("ambient_occlusion"), MP_AmbientOcclusion);
    PropertyMap.Add(TEXT("refraction"), MP_Refraction);
    PropertyMap.Add(TEXT("pixel_depth_offset"), MP_PixelDepthOffset);
    PropertyMap.Add(TEXT("shading_model"), MP_ShadingModel);
    PropertyMap.Add(TEXT("surface_thickness"), MP_SurfaceThickness);
    PropertyMap.Add(TEXT("displacement"), MP_Displacement);
    PropertyMap.Add(TEXT("front_material"), MP_FrontMaterial);
    PropertyMap.Add(TEXT("clear_coat"), MP_CustomData0);
    PropertyMap.Add(TEXT("clear_coat_roughness"), MP_CustomData1);
    PropertyMap.Add(TEXT("custom_data_0"), MP_CustomData0);
    PropertyMap.Add(TEXT("custom_data_1"), MP_CustomData1);

    const EMaterialProperty* MappedProperty = PropertyMap.Find(Normalized);
    if (!MappedProperty)
    {
        OutErrorMessage = FString::Printf(TEXT("不支持的材质属性: %s"), *PropertyText);
        return false;
    }

    OutProperty = *MappedProperty;
    OutNormalizedName = Normalized;
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

/**
 * @brief 判断当前重导入请求是否必须继续走旧 C++ 实现。
 * @param [in] Params MCP 命令参数。
 * @return bool 只要命中 Python 当前尚未覆盖的参数语义，就返回 true。
 */
static bool UnrealMCPAssetShouldUseLegacyReimportParameters(const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return false;
    }

    const bool bAskForNewFileIfMissing =
        Params->HasTypedField<EJson::Boolean>(TEXT("ask_for_new_file_if_missing")) &&
        Params->GetBoolField(TEXT("ask_for_new_file_if_missing"));
    const bool bShowNotification =
        !Params->HasTypedField<EJson::Boolean>(TEXT("show_notification")) ||
        Params->GetBoolField(TEXT("show_notification"));
    const bool bForceNewFile =
        Params->HasTypedField<EJson::Boolean>(TEXT("force_new_file")) &&
        Params->GetBoolField(TEXT("force_new_file"));

    return bAskForNewFileIfMissing || !bShowNotification || bForceNewFile;
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
    if (CommandType == TEXT("create_asset")) return HandleCreateAsset(Params);
    if (CommandType == TEXT("save_asset")) return HandleSaveAsset(Params);
    if (CommandType == TEXT("import_asset")) return HandleImportAsset(Params);
    if (CommandType == TEXT("export_asset")) return HandleExportAsset(Params);
    if (CommandType == TEXT("reimport_asset")) return HandleReimportAsset(Params);
    if (CommandType == TEXT("fixup_redirectors")) return HandleFixupRedirectors(Params);
    if (CommandType == TEXT("rename_asset")) return HandleRenameAsset(Params);
    if (CommandType == TEXT("move_asset")) return HandleMoveAsset(Params);
    if (CommandType == TEXT("delete_asset")) return HandleDeleteAsset(Params);
    if (CommandType == TEXT("batch_rename_assets")) return HandleBatchRenameAssets(Params);
    if (CommandType == TEXT("batch_move_assets")) return HandleBatchMoveAssets(Params);
    if (CommandType == TEXT("set_asset_metadata")) return HandleSetAssetMetadata(Params);
    if (CommandType == TEXT("consolidate_assets")) return HandleConsolidateAssets(Params);
    if (CommandType == TEXT("replace_asset_references")) return HandleReplaceAssetReferences(Params);
    if (CommandType == TEXT("get_selected_assets")) return HandleGetSelectedAssets(Params);
    if (CommandType == TEXT("sync_content_browser_to_assets")) return HandleSyncContentBrowserToAssets(Params);
    if (CommandType == TEXT("save_all_dirty_assets")) return HandleSaveAllDirtyAssets(Params);
    if (CommandType == TEXT("get_blueprint_summary")) return HandleGetBlueprintSummary(Params);
    if (CommandType == TEXT("create_material")) return HandleCreateMaterial(Params);
    if (CommandType == TEXT("create_material_function")) return HandleCreateMaterialFunction(Params);
    if (CommandType == TEXT("create_render_target")) return HandleCreateRenderTarget(Params);
    if (CommandType == TEXT("create_material_instance")) return HandleCreateMaterialInstance(Params);
    if (CommandType == TEXT("get_material_parameters")) return HandleGetMaterialParameters(Params);
    if (CommandType == TEXT("set_material_instance_scalar_parameter")) return HandleSetMaterialInstanceScalarParameter(Params);
    if (CommandType == TEXT("set_material_instance_vector_parameter")) return HandleSetMaterialInstanceVectorParameter(Params);
    if (CommandType == TEXT("set_material_instance_texture_parameter")) return HandleSetMaterialInstanceTextureParameter(Params);
    if (CommandType == TEXT("assign_material_to_actor")) return HandleAssignMaterialToActor(Params);
    if (CommandType == TEXT("assign_material_to_component")) return HandleAssignMaterialToComponent(Params);
    if (CommandType == TEXT("replace_material_slot")) return HandleReplaceMaterialSlot(Params);
    if (CommandType == TEXT("add_material_expression")) return HandleAddMaterialExpression(Params);
    if (CommandType == TEXT("connect_material_expressions")) return HandleConnectMaterialExpressions(Params);
    if (CommandType == TEXT("layout_material_graph")) return HandleLayoutMaterialGraph(Params);
    if (CommandType == TEXT("compile_material")) return HandleCompileMaterial(Params);
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown asset command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSearchAssets(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("search_assets"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleGetAssetMetadata(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("get_asset_metadata"),
        Params);
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

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleCreateAsset(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("create_asset"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSaveAsset(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("save_asset"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleImportAsset(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("import_asset"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleExportAsset(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("export_asset"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleReimportAsset(const TSharedPtr<FJsonObject>& Params)
{
    if (!UnrealMCPAssetShouldUseLegacyReimportParameters(Params))
    {
        const TSharedPtr<FJsonObject> LocalPythonResult = FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
            TEXT("commands.assets.asset_commands"),
            TEXT("handle_asset_command"),
            TEXT("reimport_asset"),
            Params);
        const bool bFallbackToCpp =
            LocalPythonResult.IsValid() &&
            LocalPythonResult->HasTypedField<EJson::Boolean>(TEXT("fallback_to_cpp")) &&
            LocalPythonResult->GetBoolField(TEXT("fallback_to_cpp"));
        if (!bFallbackToCpp)
        {
            return LocalPythonResult;
        }
    }

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
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("rename_asset"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleMoveAsset(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("move_asset"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("delete_asset"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleBatchRenameAssets(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("batch_rename_assets"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleBatchMoveAssets(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("batch_move_assets"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSetAssetMetadata(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("set_asset_metadata"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleConsolidateAssets(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("consolidate_assets"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleReplaceAssetReferences(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("replace_asset_references"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleGetSelectedAssets(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("get_selected_assets"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSyncContentBrowserToAssets(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("sync_content_browser_to_assets"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSaveAllDirtyAssets(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("save_all_dirty_assets"),
        Params);
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
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("create_material"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleCreateMaterialFunction(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("create_material_function"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleCreateRenderTarget(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("create_render_target"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("create_material_instance"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleGetMaterialParameters(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("get_material_parameters"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSetMaterialInstanceScalarParameter(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("set_material_instance_scalar_parameter"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSetMaterialInstanceVectorParameter(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("set_material_instance_vector_parameter"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSetMaterialInstanceTextureParameter(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("set_material_instance_texture_parameter"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleAssignMaterialToActor(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("assign_material_to_actor"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleAssignMaterialToComponent(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("assign_material_to_component"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleReplaceMaterialSlot(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.assets.asset_commands"),
        TEXT("handle_asset_command"),
        TEXT("replace_material_slot"),
        Params);
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleAddMaterialExpression(const TSharedPtr<FJsonObject>& Params)
{
    UMaterial* Material = nullptr;
    FAssetData MaterialAssetData;
    FString ErrorMessage;
    if (!ResolveBaseMaterialFromParams(Params, Material, MaterialAssetData, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    UClass* ExpressionClass = nullptr;
    FString ResolvedClassPath;
    if (!UnrealMCPAssetResolveMaterialExpressionClass(Params, ExpressionClass, ResolvedClassPath, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    FString SelectedAssetReference;
    if (!Params->TryGetStringField(TEXT("selected_asset_path"), SelectedAssetReference))
    {
        Params->TryGetStringField(TEXT("selected_asset"), SelectedAssetReference);
    }

    UObject* SelectedAsset = nullptr;
    if (!SelectedAssetReference.TrimStartAndEnd().IsEmpty())
    {
        FAssetData SelectedAssetData;
        if (!ResolveAssetReference(SelectedAssetReference, SelectedAssetData, ErrorMessage))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
        }
        SelectedAsset = SelectedAssetData.GetAsset();
    }

    int32 NodePosX = 0;
    int32 NodePosY = 0;
    UnrealMCPAssetResolveMaterialNodePosition(Params, NodePosX, NodePosY);

    Material->Modify();
    UMaterialExpression* Expression = UMaterialEditingLibrary::CreateMaterialExpressionEx(
        Material,
        nullptr,
        ExpressionClass,
        SelectedAsset,
        NodePosX,
        NodePosY);
    if (!Expression)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("创建材质表达式失败"));
    }

    TArray<TSharedPtr<FJsonValue>> AppliedProperties;
    if (!UnrealMCPAssetApplyMaterialExpressionPropertyOverrides(Params, Expression, AppliedProperties, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    Expression->PostEditChange();
    if (!UnrealMCPAssetSaveMaterial(Material, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    TSharedPtr<FJsonObject> Result = UnrealMCPAssetCreateMaterialExpressionObject(Material, Expression);
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("selected_asset_path"), SelectedAsset ? SelectedAsset->GetPathName() : FString());
    Result->SetStringField(TEXT("resolved_expression_class_path"), ResolvedClassPath);
    Result->SetArrayField(TEXT("applied_properties"), AppliedProperties);
    Result->SetNumberField(TEXT("expression_count"), Material->GetExpressions().Num());
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleConnectMaterialExpressions(const TSharedPtr<FJsonObject>& Params)
{
    UMaterial* Material = nullptr;
    FAssetData MaterialAssetData;
    FString ErrorMessage;
    if (!ResolveBaseMaterialFromParams(Params, Material, MaterialAssetData, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    UMaterialExpression* FromExpression = nullptr;
    if (!UnrealMCPAssetResolveMaterialExpressionFromParams(Material, Params, TEXT("from_"), FromExpression, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    FString FromOutputName;
    Params->TryGetStringField(TEXT("from_output_name"), FromOutputName);

    FString MaterialPropertyText;
    Params->TryGetStringField(TEXT("material_property"), MaterialPropertyText);
    if (MaterialPropertyText.TrimStartAndEnd().IsEmpty())
    {
        Params->TryGetStringField(TEXT("property"), MaterialPropertyText);
    }
    MaterialPropertyText = MaterialPropertyText.TrimStartAndEnd();

    const bool bHasMaterialProperty = !MaterialPropertyText.IsEmpty();
    const bool bHasToExpressionSpecifier =
        Params->HasField(TEXT("to_expression_path")) ||
        Params->HasField(TEXT("to_expression_name")) ||
        Params->HasField(TEXT("to_expression_index"));

    if (bHasMaterialProperty == bHasToExpressionSpecifier)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("必须且只能提供一组目标：to_expression_* 或 material_property"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetObjectField(TEXT("from_expression"), UnrealMCPAssetCreateMaterialExpressionObject(Material, FromExpression));
    Result->SetStringField(TEXT("from_output_name"), FromOutputName);
    Result->SetStringField(TEXT("material_name"), Material->GetName());
    Result->SetStringField(TEXT("material_path"), MaterialAssetData.GetObjectPathString());

    Material->Modify();

    bool bConnected = false;
    if (bHasMaterialProperty)
    {
        EMaterialProperty MaterialProperty = MP_BaseColor;
        FString NormalizedPropertyName;
        if (!UnrealMCPAssetResolveMaterialProperty(MaterialPropertyText, MaterialProperty, NormalizedPropertyName, ErrorMessage))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
        }

        bConnected = UMaterialEditingLibrary::ConnectMaterialProperty(FromExpression, FromOutputName, MaterialProperty);
        Result->SetStringField(TEXT("connection_kind"), TEXT("material_property"));
        Result->SetStringField(TEXT("material_property"), NormalizedPropertyName);
    }
    else
    {
        UMaterialExpression* ToExpression = nullptr;
        if (!UnrealMCPAssetResolveMaterialExpressionFromParams(Material, Params, TEXT("to_"), ToExpression, ErrorMessage))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
        }

        FString ToInputName;
        Params->TryGetStringField(TEXT("to_input_name"), ToInputName);
        bConnected = UMaterialEditingLibrary::ConnectMaterialExpressions(FromExpression, FromOutputName, ToExpression, ToInputName);
        Result->SetStringField(TEXT("connection_kind"), TEXT("expression"));
        Result->SetStringField(TEXT("to_input_name"), ToInputName);
        Result->SetObjectField(TEXT("to_expression"), UnrealMCPAssetCreateMaterialExpressionObject(Material, ToExpression));
    }

    if (!bConnected)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("材质表达式连接失败，请检查输入输出名称是否正确"));
    }

    if (!UnrealMCPAssetSaveMaterial(Material, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("expression_count"), Material->GetExpressions().Num());
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleLayoutMaterialGraph(const TSharedPtr<FJsonObject>& Params)
{
    UMaterial* Material = nullptr;
    FAssetData MaterialAssetData;
    FString ErrorMessage;
    if (!ResolveBaseMaterialFromParams(Params, Material, MaterialAssetData, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    Material->Modify();
    UMaterialEditingLibrary::LayoutMaterialExpressions(Material);

    if (!UnrealMCPAssetSaveMaterial(Material, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    TSharedPtr<FJsonObject> Result = CreateAssetIdentityObject(MaterialAssetData);
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("expression_count"), Material->GetExpressions().Num());

    TArray<TSharedPtr<FJsonValue>> Expressions;
    Expressions.Reserve(Material->GetExpressions().Num());
    for (UMaterialExpression* Expression : Material->GetExpressions())
    {
        Expressions.Add(MakeShared<FJsonValueObject>(UnrealMCPAssetCreateMaterialExpressionObject(Material, Expression)));
    }
    Result->SetArrayField(TEXT("expressions"), Expressions);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleCompileMaterial(const TSharedPtr<FJsonObject>& Params)
{
    UMaterial* Material = nullptr;
    FAssetData MaterialAssetData;
    FString ErrorMessage;
    if (!ResolveBaseMaterialFromParams(Params, Material, MaterialAssetData, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();

    if (!UnrealMCPAssetSaveMaterial(Material, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    TSharedPtr<FJsonObject> Result = CreateAssetIdentityObject(MaterialAssetData);
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("expression_count"), Material->GetExpressions().Num());
    Result->SetBoolField(TEXT("compiled"), true);
    return Result;
}
