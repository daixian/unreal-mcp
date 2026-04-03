/**
 * @file UnrealMCPProjectCommands.cpp
 * @brief 项目级 MCP 命令处理实现。
 */
#include "Commands/UnrealMCPProjectCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameMapsSettings.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpectatorPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/TextProperty.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputSubsystemInterface.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"

static bool UnrealMCPTryGetMappingName(const TSharedPtr<FJsonObject>& Params, FString& OutMappingName)
{
    return Params->TryGetStringField(TEXT("mapping_name"), OutMappingName) ||
        Params->TryGetStringField(TEXT("action_name"), OutMappingName);
}

static bool UnrealMCPTryGetProjectPropertyName(const TSharedPtr<FJsonObject>& Params, FString& OutPropertyName)
{
    return Params->TryGetStringField(TEXT("property_name"), OutPropertyName) ||
        Params->TryGetStringField(TEXT("setting_name"), OutPropertyName);
}

static bool UnrealMCPParseInputKey(const FString& KeyName, FKey& OutKey)
{
    OutKey = FKey(*KeyName);
    return OutKey.IsValid();
}

static void UnrealMCPSaveInputSettings(UInputSettings* InputSettings)
{
    InputSettings->ForceRebuildKeymaps();
    InputSettings->SaveKeyMappings();
    InputSettings->SaveConfig();
}

static TSharedPtr<FJsonObject> UnrealMCPSerializeActionMapping(const FInputActionKeyMapping& ActionMapping)
{
    TSharedPtr<FJsonObject> MappingObject = MakeShared<FJsonObject>();
    MappingObject->SetStringField(TEXT("mapping_name"), ActionMapping.ActionName.ToString());
    MappingObject->SetStringField(TEXT("key"), ActionMapping.Key.GetFName().ToString());
    MappingObject->SetStringField(TEXT("input_type"), TEXT("Action"));
    MappingObject->SetBoolField(TEXT("shift"), ActionMapping.bShift);
    MappingObject->SetBoolField(TEXT("ctrl"), ActionMapping.bCtrl);
    MappingObject->SetBoolField(TEXT("alt"), ActionMapping.bAlt);
    MappingObject->SetBoolField(TEXT("cmd"), ActionMapping.bCmd);
    return MappingObject;
}

static TSharedPtr<FJsonObject> UnrealMCPSerializeAxisMapping(const FInputAxisKeyMapping& AxisMapping)
{
    TSharedPtr<FJsonObject> MappingObject = MakeShared<FJsonObject>();
    MappingObject->SetStringField(TEXT("mapping_name"), AxisMapping.AxisName.ToString());
    MappingObject->SetStringField(TEXT("key"), AxisMapping.Key.GetFName().ToString());
    MappingObject->SetStringField(TEXT("input_type"), TEXT("Axis"));
    MappingObject->SetNumberField(TEXT("scale"), AxisMapping.Scale);
    return MappingObject;
}

static FString UnrealMCPInputActionValueTypeToString(EInputActionValueType ValueType)
{
    switch (ValueType)
    {
    case EInputActionValueType::Boolean:
        return TEXT("Boolean");
    case EInputActionValueType::Axis1D:
        return TEXT("Axis1D");
    case EInputActionValueType::Axis2D:
        return TEXT("Axis2D");
    case EInputActionValueType::Axis3D:
        return TEXT("Axis3D");
    default:
        return TEXT("Unknown");
    }
}

static bool UnrealMCPParseInputActionValueType(const FString& ValueTypeText, EInputActionValueType& OutValueType)
{
    const FString NormalizedValueType = ValueTypeText.TrimStartAndEnd().ToLower();
    if (NormalizedValueType == TEXT("boolean") || NormalizedValueType == TEXT("bool") || NormalizedValueType == TEXT("digital"))
    {
        OutValueType = EInputActionValueType::Boolean;
        return true;
    }

    if (NormalizedValueType == TEXT("axis1d") || NormalizedValueType == TEXT("1d") || NormalizedValueType == TEXT("float"))
    {
        OutValueType = EInputActionValueType::Axis1D;
        return true;
    }

    if (NormalizedValueType == TEXT("axis2d") || NormalizedValueType == TEXT("2d") || NormalizedValueType == TEXT("vector2d"))
    {
        OutValueType = EInputActionValueType::Axis2D;
        return true;
    }

    if (NormalizedValueType == TEXT("axis3d") || NormalizedValueType == TEXT("3d") || NormalizedValueType == TEXT("vector") || NormalizedValueType == TEXT("vector3d"))
    {
        OutValueType = EInputActionValueType::Axis3D;
        return true;
    }

    return false;
}

static FString UnrealMCPAccumulationBehaviorToString(EInputActionAccumulationBehavior Behavior)
{
    switch (Behavior)
    {
    case EInputActionAccumulationBehavior::TakeHighestAbsoluteValue:
        return TEXT("TakeHighestAbsoluteValue");
    case EInputActionAccumulationBehavior::Cumulative:
        return TEXT("Cumulative");
    default:
        return TEXT("Unknown");
    }
}

static bool UnrealMCPParseAccumulationBehavior(const FString& BehaviorText, EInputActionAccumulationBehavior& OutBehavior)
{
    const FString NormalizedBehavior = BehaviorText.TrimStartAndEnd().ToLower();
    if (NormalizedBehavior == TEXT("takehighestabsolutevalue") || NormalizedBehavior == TEXT("highest") || NormalizedBehavior == TEXT("default"))
    {
        OutBehavior = EInputActionAccumulationBehavior::TakeHighestAbsoluteValue;
        return true;
    }

    if (NormalizedBehavior == TEXT("cumulative") || NormalizedBehavior == TEXT("sum"))
    {
        OutBehavior = EInputActionAccumulationBehavior::Cumulative;
        return true;
    }

    return false;
}

static FString UnrealMCPRegistrationTrackingModeToString(EMappingContextRegistrationTrackingMode TrackingMode)
{
    switch (TrackingMode)
    {
    case EMappingContextRegistrationTrackingMode::Untracked:
        return TEXT("Untracked");
    case EMappingContextRegistrationTrackingMode::CountRegistrations:
        return TEXT("CountRegistrations");
    default:
        return TEXT("Unknown");
    }
}

static bool UnrealMCPParseRegistrationTrackingMode(const FString& TrackingModeText, EMappingContextRegistrationTrackingMode& OutTrackingMode)
{
    const FString NormalizedTrackingMode = TrackingModeText.TrimStartAndEnd().ToLower();
    if (NormalizedTrackingMode == TEXT("untracked") || NormalizedTrackingMode == TEXT("default"))
    {
        OutTrackingMode = EMappingContextRegistrationTrackingMode::Untracked;
        return true;
    }

    if (NormalizedTrackingMode == TEXT("countregistrations") || NormalizedTrackingMode == TEXT("count_registrations") || NormalizedTrackingMode == TEXT("count"))
    {
        OutTrackingMode = EMappingContextRegistrationTrackingMode::CountRegistrations;
        return true;
    }

    return false;
}

static bool UnrealMCPResolveSupportedSettingsObject(
    const FString& SettingsClassText,
    UObject*& OutSettingsObject,
    FString& OutResolvedSettingsClass,
    FString& OutErrorMessage)
{
    OutSettingsObject = nullptr;
    const FString NormalizedSettingsClass = SettingsClassText.TrimStartAndEnd();
    if (NormalizedSettingsClass.IsEmpty())
    {
        OutErrorMessage = TEXT("缺少 'settings_class' 参数");
        return false;
    }

    const FString LowerName = NormalizedSettingsClass.ToLower();
    if (LowerName == TEXT("gamemapssettings") ||
        LowerName == TEXT("ugamemapssettings") ||
        LowerName == TEXT("maps") ||
        LowerName == TEXT("mapsandmodes") ||
        LowerName == TEXT("/script/enginesettings.gamemapssettings"))
    {
        OutSettingsObject = GetMutableDefault<UGameMapsSettings>();
        OutResolvedSettingsClass = TEXT("GameMapsSettings");
        return OutSettingsObject != nullptr;
    }

    if (LowerName == TEXT("inputsettings") ||
        LowerName == TEXT("uinputsettings") ||
        LowerName == TEXT("input") ||
        LowerName == TEXT("/script/engine.inputsettings"))
    {
        OutSettingsObject = GetMutableDefault<UInputSettings>();
        OutResolvedSettingsClass = TEXT("InputSettings");
        return OutSettingsObject != nullptr;
    }

    OutErrorMessage = FString::Printf(
        TEXT("暂不支持 settings_class=%s，目前支持: GameMapsSettings/InputSettings"),
        *NormalizedSettingsClass);
    return false;
}

static bool UnrealMCPConvertJsonValueToImportText(
    const TSharedPtr<FJsonValue>& Value,
    FString& OutImportText,
    FString& OutErrorMessage)
{
    if (!Value.IsValid())
    {
        OutErrorMessage = TEXT("缺少 'value' 参数");
        return false;
    }

    switch (Value->Type)
    {
    case EJson::Boolean:
        OutImportText = Value->AsBool() ? TEXT("True") : TEXT("False");
        return true;

    case EJson::Number:
        OutImportText = FString::SanitizeFloat(Value->AsNumber());
        return true;

    case EJson::String:
        OutImportText = Value->AsString();
        return true;

    case EJson::Null:
        OutImportText = TEXT("None");
        return true;

    default:
        OutErrorMessage = TEXT("当前仅支持 bool/number/string/null 作为项目设置写入值");
        return false;
    }
}

static bool UnrealMCPExportProjectPropertyValue(
    UObject* SettingsObject,
    FProperty* Property,
    TSharedPtr<FJsonValue>& OutValue,
    FString& OutExportedValue)
{
    if (!SettingsObject || !Property)
    {
        return false;
    }

    void* PropertyAddress = Property->ContainerPtrToValuePtr<void>(SettingsObject);
    Property->ExportTextItem_Direct(OutExportedValue, PropertyAddress, PropertyAddress, SettingsObject, PPF_None);

    if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
    {
        OutValue = MakeShared<FJsonValueBoolean>(BoolProperty->GetPropertyValue(PropertyAddress));
        return true;
    }

    if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
    {
        if (NumericProperty->IsInteger())
        {
            OutValue = MakeShared<FJsonValueNumber>(static_cast<double>(NumericProperty->GetSignedIntPropertyValue(PropertyAddress)));
            return true;
        }

        OutValue = MakeShared<FJsonValueNumber>(NumericProperty->GetFloatingPointPropertyValue(PropertyAddress));
        return true;
    }

    if (const FStrProperty* StringProperty = CastField<FStrProperty>(Property))
    {
        OutValue = MakeShared<FJsonValueString>(StringProperty->GetPropertyValue(PropertyAddress));
        return true;
    }

    if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
    {
        OutValue = MakeShared<FJsonValueString>(NameProperty->GetPropertyValue(PropertyAddress).ToString());
        return true;
    }

    if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))
    {
        OutValue = MakeShared<FJsonValueString>(TextProperty->GetPropertyValue(PropertyAddress).ToString());
        return true;
    }

    if (const FClassProperty* ClassProperty = CastField<FClassProperty>(Property))
    {
        if (UClass* ClassValue = Cast<UClass>(ClassProperty->GetObjectPropertyValue(PropertyAddress)))
        {
            OutValue = MakeShared<FJsonValueString>(ClassValue->GetPathName());
        }
        else
        {
            OutValue = MakeShared<FJsonValueString>(TEXT(""));
        }
        return true;
    }

    OutValue = MakeShared<FJsonValueString>(OutExportedValue);
    return true;
}

static bool UnrealMCPNormalizeMapReference(
    const FString& MapReferenceText,
    FString& OutPackageName,
    FString& OutObjectPath,
    FString& OutErrorMessage)
{
    const FString TrimmedReference = MapReferenceText.TrimStartAndEnd();
    if (TrimmedReference.IsEmpty())
    {
        OutErrorMessage = TEXT("地图路径不能为空");
        return false;
    }

    FString CandidatePackageName = TrimmedReference;
    if (TrimmedReference.Contains(TEXT(".")))
    {
        CandidatePackageName = FPackageName::ObjectPathToPackageName(TrimmedReference);
    }

    if (!CandidatePackageName.StartsWith(TEXT("/Game")))
    {
        OutErrorMessage = TEXT("地图路径必须以 /Game 开头");
        return false;
    }

    if (!FPackageName::IsValidLongPackageName(CandidatePackageName))
    {
        OutErrorMessage = FString::Printf(TEXT("无效地图路径: %s"), *TrimmedReference);
        return false;
    }

    const FString AssetName = FPackageName::GetLongPackageAssetName(CandidatePackageName);
    if (AssetName.IsEmpty())
    {
        OutErrorMessage = FString::Printf(TEXT("无法从地图路径解析资源名: %s"), *TrimmedReference);
        return false;
    }

    const FString CandidateObjectPath = CandidatePackageName + TEXT(".") + AssetName;
    if (!UEditorAssetLibrary::DoesAssetExist(CandidateObjectPath))
    {
        OutErrorMessage = FString::Printf(TEXT("地图资源不存在: %s"), *CandidateObjectPath);
        return false;
    }

    OutPackageName = CandidatePackageName;
    OutObjectPath = CandidateObjectPath;
    return true;
}

static bool UnrealMCPResolveClassByReference(
    const FString& ClassReferenceText,
    UClass*& OutClass,
    UClass* RequiredBaseClass,
    FString& OutErrorMessage)
{
    OutClass = nullptr;
    const FString TrimmedReference = ClassReferenceText.TrimStartAndEnd();
    if (TrimmedReference.IsEmpty())
    {
        OutErrorMessage = TEXT("类引用不能为空");
        return false;
    }

    auto ValidateResolvedClass = [&](UClass* CandidateClass) -> bool
    {
        if (!CandidateClass)
        {
            return false;
        }

        if (RequiredBaseClass && !CandidateClass->IsChildOf(RequiredBaseClass))
        {
            OutErrorMessage = FString::Printf(
                TEXT("类 %s 不是 %s 的子类"),
                *CandidateClass->GetPathName(),
                *RequiredBaseClass->GetName());
            return false;
        }

        OutClass = CandidateClass;
        return true;
    };

    if (UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(TrimmedReference))
    {
        if (ValidateResolvedClass(Blueprint->GeneratedClass))
        {
            return true;
        }

        if (!OutErrorMessage.IsEmpty())
        {
            return false;
        }
    }

    if (TrimmedReference.StartsWith(TEXT("/")))
    {
        TArray<FString> CandidateClassPaths;
        CandidateClassPaths.Add(TrimmedReference);

        if (!TrimmedReference.EndsWith(TEXT("_C")))
        {
            CandidateClassPaths.Add(TrimmedReference + TEXT("_C"));
        }

        FString PackagePath = TrimmedReference;
        FString ObjectName;
        if (TrimmedReference.Contains(TEXT(".")))
        {
            PackagePath = FPackageName::ObjectPathToPackageName(TrimmedReference);
            ObjectName = FPackageName::ObjectPathToObjectName(TrimmedReference);
        }
        else
        {
            ObjectName = FPackageName::GetLongPackageAssetName(TrimmedReference);
        }

        if (!ObjectName.IsEmpty() && ObjectName.EndsWith(TEXT("_C")))
        {
            ObjectName = ObjectName.LeftChop(2);
        }

        if (!PackagePath.IsEmpty() && !ObjectName.IsEmpty())
        {
            CandidateClassPaths.Add(PackagePath + TEXT(".") + ObjectName);
            CandidateClassPaths.Add(PackagePath + TEXT(".") + ObjectName + TEXT("_C"));
        }

        for (const FString& CandidateClassPath : CandidateClassPaths)
        {
            if (UClass* CandidateClass = FSoftClassPath(CandidateClassPath).TryLoadClass<UObject>())
            {
                if (ValidateResolvedClass(CandidateClass))
                {
                    return true;
                }

                if (!OutErrorMessage.IsEmpty())
                {
                    return false;
                }
            }
        }
    }

    TArray<FString> CandidateClassNames;
    CandidateClassNames.Add(TrimmedReference);
    if (!TrimmedReference.StartsWith(TEXT("A")) && !TrimmedReference.StartsWith(TEXT("U")))
    {
        CandidateClassNames.Add(TEXT("A") + TrimmedReference);
        CandidateClassNames.Add(TEXT("U") + TrimmedReference);
    }

    for (const FString& CandidateClassName : CandidateClassNames)
    {
        if (UClass* CandidateClass = FindFirstObject<UClass>(*CandidateClassName, EFindFirstObjectOptions::None))
        {
            if (ValidateResolvedClass(CandidateClass))
            {
                return true;
            }

            if (!OutErrorMessage.IsEmpty())
            {
                return false;
            }
        }
    }

    OutErrorMessage = FString::Printf(TEXT("找不到类: %s"), *TrimmedReference);
    return false;
}

static bool UnrealMCPResolveBlueprintByClassReference(
    const FString& ClassReferenceText,
    UBlueprint*& OutBlueprint,
    FString& OutErrorMessage)
{
    OutBlueprint = nullptr;
    const FString TrimmedReference = ClassReferenceText.TrimStartAndEnd();
    if (TrimmedReference.IsEmpty())
    {
        OutErrorMessage = TEXT("Blueprint 引用不能为空");
        return false;
    }

    auto TryLoadBlueprintAsset = [](const FString& AssetPath) -> UBlueprint*
    {
        if (AssetPath.IsEmpty())
        {
            return nullptr;
        }

        if (UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath))
        {
            return Cast<UBlueprint>(LoadedAsset);
        }

        return nullptr;
    };

    if (UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(TrimmedReference))
    {
        OutBlueprint = Blueprint;
        return true;
    }

    if (TrimmedReference.StartsWith(TEXT("/")))
    {
        TArray<FString> CandidateAssetPaths;
        CandidateAssetPaths.Add(TrimmedReference);

        FString PackagePath = TrimmedReference;
        FString ObjectName;
        if (TrimmedReference.Contains(TEXT(".")))
        {
            PackagePath = FPackageName::ObjectPathToPackageName(TrimmedReference);
            ObjectName = FPackageName::ObjectPathToObjectName(TrimmedReference);
        }
        else
        {
            ObjectName = FPackageName::GetLongPackageAssetName(TrimmedReference);
        }

        if (!ObjectName.IsEmpty() && ObjectName.EndsWith(TEXT("_C")))
        {
            ObjectName = ObjectName.LeftChop(2);
        }

        if (!PackagePath.IsEmpty() && !ObjectName.IsEmpty())
        {
            CandidateAssetPaths.Add(PackagePath);
            CandidateAssetPaths.Add(PackagePath + TEXT(".") + ObjectName);
        }

        for (const FString& CandidateAssetPath : CandidateAssetPaths)
        {
            if (UBlueprint* Blueprint = TryLoadBlueprintAsset(CandidateAssetPath))
            {
                OutBlueprint = Blueprint;
                return true;
            }
        }
    }

    OutErrorMessage = FString::Printf(TEXT("找不到 Blueprint 资源: %s"), *TrimmedReference);
    return false;
}

static bool UnrealMCPSetProjectPropertyValue(
    UObject* SettingsObject,
    FProperty* Property,
    const TSharedPtr<FJsonValue>& Value,
    FString& OutExportedValue,
    FString& OutErrorMessage)
{
    if (!SettingsObject || !Property)
    {
        OutErrorMessage = TEXT("设置对象或属性无效");
        return false;
    }

    void* PropertyAddress = Property->ContainerPtrToValuePtr<void>(SettingsObject);
    if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
    {
        BoolProperty->SetPropertyValue(PropertyAddress, Value->AsBool());
    }
    else if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
    {
        if (NumericProperty->IsInteger())
        {
            NumericProperty->SetIntPropertyValue(PropertyAddress, static_cast<int64>(Value->AsNumber()));
        }
        else
        {
            NumericProperty->SetFloatingPointPropertyValue(PropertyAddress, Value->AsNumber());
        }
    }
    else if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
    {
        StringProperty->SetPropertyValue(PropertyAddress, Value->AsString());
    }
    else if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
    {
        NameProperty->SetPropertyValue(PropertyAddress, FName(*Value->AsString()));
    }
    else if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
    {
        TextProperty->SetPropertyValue(PropertyAddress, FText::FromString(Value->AsString()));
    }
    else if (FClassProperty* ClassProperty = CastField<FClassProperty>(Property))
    {
        UClass* ResolvedClass = nullptr;
        if (!UnrealMCPResolveClassByReference(Value->AsString(), ResolvedClass, ClassProperty->MetaClass, OutErrorMessage))
        {
            return false;
        }

        ClassProperty->SetObjectPropertyValue(PropertyAddress, ResolvedClass);
    }
    else
    {
        FString ImportText;
        if (!UnrealMCPConvertJsonValueToImportText(Value, ImportText, OutErrorMessage))
        {
            return false;
        }

        if (Property->ImportText_Direct(*ImportText, PropertyAddress, SettingsObject, PPF_None) == nullptr)
        {
            OutErrorMessage = FString::Printf(TEXT("属性 %s 导入失败，输入值=%s"), *Property->GetName(), *ImportText);
            return false;
        }
    }

    Property->ExportTextItem_Direct(OutExportedValue, PropertyAddress, PropertyAddress, SettingsObject, PPF_None);
    return true;
}

static bool UnrealMCPNormalizePackagePath(
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

static bool UnrealMCPBuildAssetPath(
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

template <typename TObjectType>
static bool UnrealMCPResolveAssetByReference(
    const FString& AssetReference,
    TObjectType*& OutAsset,
    FString& OutErrorMessage)
{
    OutAsset = nullptr;
    const FString TrimmedReference = AssetReference.TrimStartAndEnd();
    if (TrimmedReference.IsEmpty())
    {
        OutErrorMessage = TEXT("资源引用不能为空");
        return false;
    }

    auto TryLoadTypedAsset = [](const FString& CandidatePath) -> TObjectType*
    {
        if (CandidatePath.IsEmpty())
        {
            return nullptr;
        }

        if (UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(CandidatePath))
        {
            return Cast<TObjectType>(LoadedAsset);
        }

        return LoadObject<TObjectType>(nullptr, *CandidatePath);
    };

    if (TrimmedReference.StartsWith(TEXT("/")))
    {
        TArray<FString> CandidatePaths;
        CandidatePaths.Add(TrimmedReference);

        FString PackagePath = TrimmedReference;
        const int32 DotIndex = PackagePath.Find(TEXT("."), ESearchCase::CaseSensitive);
        if (DotIndex != INDEX_NONE)
        {
            PackagePath = PackagePath.Left(DotIndex);
        }

        if (!PackagePath.IsEmpty())
        {
            CandidatePaths.Add(PackagePath);
            const FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);
            if (!AssetName.IsEmpty())
            {
                CandidatePaths.Add(PackagePath + TEXT(".") + AssetName);
            }
        }

        for (const FString& CandidatePath : CandidatePaths)
        {
            if (TObjectType* Asset = TryLoadTypedAsset(CandidatePath))
            {
                OutAsset = Asset;
                return true;
            }
        }

        OutErrorMessage = FString::Printf(TEXT("找不到资源: %s"), *TrimmedReference);
        return false;
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> AssetsByClass;
    AssetRegistryModule.Get().GetAssetsByClass(FTopLevelAssetPath(TObjectType::StaticClass()), AssetsByClass, true);

    TArray<FAssetData> MatchedAssets;
    for (const FAssetData& AssetData : AssetsByClass)
    {
        if (AssetData.AssetName.ToString().Equals(TrimmedReference, ESearchCase::IgnoreCase) ||
            AssetData.PackageName.ToString().Equals(TrimmedReference, ESearchCase::IgnoreCase) ||
            AssetData.GetObjectPathString().Equals(TrimmedReference, ESearchCase::IgnoreCase))
        {
            MatchedAssets.Add(AssetData);
        }
    }

    if (MatchedAssets.Num() > 1)
    {
        OutErrorMessage = FString::Printf(TEXT("资源引用不唯一，请改用完整路径: %s"), *TrimmedReference);
        return false;
    }

    if (MatchedAssets.Num() == 1)
    {
        if (TObjectType* Asset = Cast<TObjectType>(MatchedAssets[0].GetAsset()))
        {
            OutAsset = Asset;
            return true;
        }
    }

    OutErrorMessage = FString::Printf(TEXT("找不到资源: %s"), *TrimmedReference);
    return false;
}

static FString UnrealMCPGetRequestedWorldType(const TSharedPtr<FJsonObject>& Params)
{
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

static UWorld* UnrealMCPGetEditorWorld()
{
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

static UWorld* UnrealMCPGetPIEWorld()
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

static FString UnrealMCPWorldTypeToString(EWorldType::Type WorldType)
{
    switch (WorldType)
    {
    case EWorldType::Editor:
        return TEXT("Editor");
    case EWorldType::PIE:
        return TEXT("PIE");
    case EWorldType::GamePreview:
        return TEXT("GamePreview");
    case EWorldType::Game:
        return TEXT("Game");
    default:
        return TEXT("Unknown");
    }
}

static UWorld* UnrealMCPResolveWorldByParams(
    const TSharedPtr<FJsonObject>& Params,
    FString& OutResolvedWorldType,
    FString& OutErrorMessage)
{
    const FString RequestedWorldType = UnrealMCPGetRequestedWorldType(Params);

    if (RequestedWorldType == TEXT("editor"))
    {
        UWorld* EditorWorld = UnrealMCPGetEditorWorld();
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
        UWorld* PIEWorld = UnrealMCPGetPIEWorld();
        if (!PIEWorld)
        {
            OutErrorMessage = TEXT("当前没有运行 PIE/VR Preview，请先启动运行会话");
            return nullptr;
        }

        OutResolvedWorldType = TEXT("pie");
        return PIEWorld;
    }

    if (RequestedWorldType == TEXT("auto"))
    {
        if (UWorld* PIEWorld = UnrealMCPGetPIEWorld())
        {
            OutResolvedWorldType = TEXT("pie");
            return PIEWorld;
        }

        if (UWorld* EditorWorld = UnrealMCPGetEditorWorld())
        {
            OutResolvedWorldType = TEXT("editor");
            return EditorWorld;
        }

        OutErrorMessage = TEXT("无法解析世界：PIE 和编辑器世界都不可用");
        return nullptr;
    }

    OutErrorMessage = FString::Printf(TEXT("无效 world_type: %s，可选值为 auto/editor/pie"), *RequestedWorldType);
    return nullptr;
}

static void UnrealMCPAppendWorldInfo(
    const TSharedPtr<FJsonObject>& ResultObject,
    UWorld* World,
    const FString& ResolvedWorldType)
{
    if (!ResultObject.IsValid() || !World)
    {
        return;
    }

    ResultObject->SetStringField(TEXT("resolved_world_type"), ResolvedWorldType);
    ResultObject->SetStringField(TEXT("world_type"), UnrealMCPWorldTypeToString(World->WorldType));
    ResultObject->SetStringField(TEXT("world_name"), World->GetName());
    ResultObject->SetStringField(TEXT("world_path"), World->GetPathName());
}

static AActor* UnrealMCPResolveActorByParams(
    const TSharedPtr<FJsonObject>& Params,
    UWorld* World,
    FString& OutErrorMessage)
{
    if (!World)
    {
        OutErrorMessage = TEXT("目标世界无效");
        return nullptr;
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

        OutErrorMessage = FString::Printf(TEXT("找不到 Actor: %s"), *ActorPath);
        return nullptr;
    }

    FString ActorName;
    if (Params->TryGetStringField(TEXT("name"), ActorName) && !ActorName.IsEmpty())
    {
        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->GetName() == ActorName)
            {
                return Actor;
            }
        }

        OutErrorMessage = FString::Printf(TEXT("找不到 Actor: %s"), *ActorName);
        return nullptr;
    }

    OutErrorMessage = TEXT("缺少 'name' 或 'actor_path' 参数");
    return nullptr;
}

/**
 * @brief 构造函数。
 */
FUnrealMCPProjectCommands::FUnrealMCPProjectCommands()
{
}

/**
 * @brief 分发项目级命令到具体处理函数。
 * @param [in] CommandType 命令类型。
 * @param [in] Params 命令参数。
 * @return TSharedPtr<FJsonObject> 命令结果或错误信息。
 */
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_input_mapping") || CommandType == TEXT("create_input_axis_mapping"))
    {
        if (CommandType == TEXT("create_input_axis_mapping"))
        {
            Params->SetStringField(TEXT("input_type"), TEXT("Axis"));
        }
        return HandleCreateInputMapping(Params);
    }

    if (CommandType == TEXT("list_input_mappings"))
    {
        return HandleListInputMappings(Params);
    }

    if (CommandType == TEXT("remove_input_mapping"))
    {
        return HandleRemoveInputMapping(Params);
    }

    if (CommandType == TEXT("create_input_action_asset"))
    {
        return HandleCreateInputActionAsset(Params);
    }

    if (CommandType == TEXT("create_input_mapping_context"))
    {
        return HandleCreateInputMappingContext(Params);
    }

    if (CommandType == TEXT("add_mapping_to_context"))
    {
        return HandleAddMappingToContext(Params);
    }

    if (CommandType == TEXT("assign_mapping_context"))
    {
        return HandleAssignMappingContext(Params);
    }

    if (CommandType == TEXT("get_project_setting"))
    {
        return HandleGetProjectSetting(Params);
    }

    if (CommandType == TEXT("set_project_setting"))
    {
        return HandleSetProjectSetting(Params);
    }

    if (CommandType == TEXT("set_default_maps"))
    {
        return HandleSetDefaultMaps(Params);
    }

    if (CommandType == TEXT("set_game_framework_defaults"))
    {
        return HandleSetGameFrameworkDefaults(Params);
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown project command: %s"), *CommandType));
}

/**
 * @brief 创建输入映射配置（Action/Axis）。
 * @param [in] Params 输入映射参数。
 * @return TSharedPtr<FJsonObject> 执行结果。
 * @note 该实现会将配置直接写入默认输入设置并保存。
 */
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCreateInputMapping(const TSharedPtr<FJsonObject>& Params)
{
    FString MappingName;
    if (!UnrealMCPTryGetMappingName(Params, MappingName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'mapping_name' 或 'action_name' 参数"));
    }

    FString KeyName;
    if (!Params->TryGetStringField(TEXT("key"), KeyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'key' 参数"));
    }

    FKey MappingKey;
    if (!UnrealMCPParseInputKey(KeyName, MappingKey))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("无效按键名: %s"), *KeyName));
    }

    UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();
    if (!InputSettings)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("获取输入设置失败"));
    }

    FString InputType = TEXT("Action");
    Params->TryGetStringField(TEXT("input_type"), InputType);
    const bool bIsAxisMapping = InputType.Equals(TEXT("Axis"), ESearchCase::IgnoreCase);

    if (bIsAxisMapping)
    {
        float Scale = 1.0f;
        Params->TryGetNumberField(TEXT("scale"), Scale);

        FInputAxisKeyMapping AxisMapping;
        AxisMapping.AxisName = FName(*MappingName);
        AxisMapping.Key = MappingKey;
        AxisMapping.Scale = Scale;

        InputSettings->AddAxisMapping(AxisMapping, false);
    }
    else
    {
        FInputActionKeyMapping ActionMapping;
        ActionMapping.ActionName = FName(*MappingName);
        ActionMapping.Key = MappingKey;

        if (Params->HasField(TEXT("shift")))
        {
            ActionMapping.bShift = Params->GetBoolField(TEXT("shift"));
        }
        if (Params->HasField(TEXT("ctrl")))
        {
            ActionMapping.bCtrl = Params->GetBoolField(TEXT("ctrl"));
        }
        if (Params->HasField(TEXT("alt")))
        {
            ActionMapping.bAlt = Params->GetBoolField(TEXT("alt"));
        }
        if (Params->HasField(TEXT("cmd")))
        {
            ActionMapping.bCmd = Params->GetBoolField(TEXT("cmd"));
        }

        InputSettings->AddActionMapping(ActionMapping, false);
    }

    UnrealMCPSaveInputSettings(InputSettings);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("mapping_name"), MappingName);
    ResultObj->SetStringField(TEXT("key"), KeyName);
    ResultObj->SetStringField(TEXT("input_type"), bIsAxisMapping ? TEXT("Axis") : TEXT("Action"));

    if (bIsAxisMapping)
    {
        float Scale = 1.0f;
        Params->TryGetNumberField(TEXT("scale"), Scale);
        ResultObj->SetNumberField(TEXT("scale"), Scale);
    }
    else
    {
        ResultObj->SetBoolField(TEXT("shift"), Params->HasField(TEXT("shift")) ? Params->GetBoolField(TEXT("shift")) : false);
        ResultObj->SetBoolField(TEXT("ctrl"), Params->HasField(TEXT("ctrl")) ? Params->GetBoolField(TEXT("ctrl")) : false);
        ResultObj->SetBoolField(TEXT("alt"), Params->HasField(TEXT("alt")) ? Params->GetBoolField(TEXT("alt")) : false);
        ResultObj->SetBoolField(TEXT("cmd"), Params->HasField(TEXT("cmd")) ? Params->GetBoolField(TEXT("cmd")) : false);
    }

    return ResultObj;
}

/**
 * @brief 列出当前项目中的输入映射。
 * @param [in] Params 查询过滤参数。
 * @return TSharedPtr<FJsonObject> 输入映射列表。
 */
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleListInputMappings(const TSharedPtr<FJsonObject>& Params)
{
    UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();
    if (!InputSettings)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("获取输入设置失败"));
    }

    FString InputType = TEXT("All");
    Params->TryGetStringField(TEXT("input_type"), InputType);
    const bool bIncludeAction = !InputType.Equals(TEXT("Axis"), ESearchCase::IgnoreCase);
    const bool bIncludeAxis = !InputType.Equals(TEXT("Action"), ESearchCase::IgnoreCase);

    FString MappingNameFilter;
    const bool bHasMappingNameFilter = UnrealMCPTryGetMappingName(Params, MappingNameFilter);

    FString KeyFilterName;
    const bool bHasKeyFilter = Params->TryGetStringField(TEXT("key"), KeyFilterName);
    FKey KeyFilter;
    if (bHasKeyFilter && !UnrealMCPParseInputKey(KeyFilterName, KeyFilter))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("无效按键名: %s"), *KeyFilterName));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> ActionMappingsJson;
    TArray<TSharedPtr<FJsonValue>> AxisMappingsJson;
    int32 TotalCount = 0;

    if (bIncludeAction)
    {
        for (const FInputActionKeyMapping& ActionMapping : InputSettings->GetActionMappings())
        {
            if (bHasMappingNameFilter && !ActionMapping.ActionName.ToString().Equals(MappingNameFilter, ESearchCase::IgnoreCase))
            {
                continue;
            }

            if (bHasKeyFilter && ActionMapping.Key != KeyFilter)
            {
                continue;
            }

            ActionMappingsJson.Add(MakeShared<FJsonValueObject>(UnrealMCPSerializeActionMapping(ActionMapping)));
            ++TotalCount;
        }
    }

    if (bIncludeAxis)
    {
        for (const FInputAxisKeyMapping& AxisMapping : InputSettings->GetAxisMappings())
        {
            if (bHasMappingNameFilter && !AxisMapping.AxisName.ToString().Equals(MappingNameFilter, ESearchCase::IgnoreCase))
            {
                continue;
            }

            if (bHasKeyFilter && AxisMapping.Key != KeyFilter)
            {
                continue;
            }

            AxisMappingsJson.Add(MakeShared<FJsonValueObject>(UnrealMCPSerializeAxisMapping(AxisMapping)));
            ++TotalCount;
        }
    }

    ResultObj->SetStringField(TEXT("input_type"), InputType);
    ResultObj->SetArrayField(TEXT("action_mappings"), ActionMappingsJson);
    ResultObj->SetArrayField(TEXT("axis_mappings"), AxisMappingsJson);
    ResultObj->SetNumberField(TEXT("total_count"), TotalCount);
    return ResultObj;
}

/**
 * @brief 删除输入映射（Action/Axis）。
 * @param [in] Params 删除过滤参数。
 * @return TSharedPtr<FJsonObject> 删除结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleRemoveInputMapping(const TSharedPtr<FJsonObject>& Params)
{
    FString MappingName;
    if (!UnrealMCPTryGetMappingName(Params, MappingName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'mapping_name' 或 'action_name' 参数"));
    }

    UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();
    if (!InputSettings)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("获取输入设置失败"));
    }

    FString InputType = TEXT("Action");
    Params->TryGetStringField(TEXT("input_type"), InputType);
    const bool bIsAxisMapping = InputType.Equals(TEXT("Axis"), ESearchCase::IgnoreCase);

    FString KeyFilterName;
    const bool bHasKeyFilter = Params->TryGetStringField(TEXT("key"), KeyFilterName);
    FKey KeyFilter;
    if (bHasKeyFilter && !UnrealMCPParseInputKey(KeyFilterName, KeyFilter))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("无效按键名: %s"), *KeyFilterName));
    }

    int32 RemovedCount = 0;

    if (bIsAxisMapping)
    {
        float ScaleFilter = 1.0f;
        const bool bHasScaleFilter = Params->TryGetNumberField(TEXT("scale"), ScaleFilter);
        const TArray<FInputAxisKeyMapping> AxisMappings = InputSettings->GetAxisMappings();

        for (const FInputAxisKeyMapping& AxisMapping : AxisMappings)
        {
            if (!AxisMapping.AxisName.ToString().Equals(MappingName, ESearchCase::IgnoreCase))
            {
                continue;
            }

            if (bHasKeyFilter && AxisMapping.Key != KeyFilter)
            {
                continue;
            }

            if (bHasScaleFilter && !FMath::IsNearlyEqual(AxisMapping.Scale, ScaleFilter))
            {
                continue;
            }

            InputSettings->RemoveAxisMapping(AxisMapping, false);
            ++RemovedCount;
        }
    }
    else
    {
        const bool bHasShiftFilter = Params->HasField(TEXT("shift"));
        const bool bHasCtrlFilter = Params->HasField(TEXT("ctrl"));
        const bool bHasAltFilter = Params->HasField(TEXT("alt"));
        const bool bHasCmdFilter = Params->HasField(TEXT("cmd"));
        const bool bShiftFilter = bHasShiftFilter ? Params->GetBoolField(TEXT("shift")) : false;
        const bool bCtrlFilter = bHasCtrlFilter ? Params->GetBoolField(TEXT("ctrl")) : false;
        const bool bAltFilter = bHasAltFilter ? Params->GetBoolField(TEXT("alt")) : false;
        const bool bCmdFilter = bHasCmdFilter ? Params->GetBoolField(TEXT("cmd")) : false;
        const TArray<FInputActionKeyMapping> ActionMappings = InputSettings->GetActionMappings();

        for (const FInputActionKeyMapping& ActionMapping : ActionMappings)
        {
            if (!ActionMapping.ActionName.ToString().Equals(MappingName, ESearchCase::IgnoreCase))
            {
                continue;
            }

            if (bHasKeyFilter && ActionMapping.Key != KeyFilter)
            {
                continue;
            }

            if (bHasShiftFilter && ActionMapping.bShift != bShiftFilter)
            {
                continue;
            }

            if (bHasCtrlFilter && ActionMapping.bCtrl != bCtrlFilter)
            {
                continue;
            }

            if (bHasAltFilter && ActionMapping.bAlt != bAltFilter)
            {
                continue;
            }

            if (bHasCmdFilter && ActionMapping.bCmd != bCmdFilter)
            {
                continue;
            }

            InputSettings->RemoveActionMapping(ActionMapping, false);
            ++RemovedCount;
        }
    }

    if (RemovedCount <= 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("没有找到可删除的输入映射: %s"), *MappingName));
    }

    UnrealMCPSaveInputSettings(InputSettings);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("mapping_name"), MappingName);
    ResultObj->SetStringField(TEXT("input_type"), bIsAxisMapping ? TEXT("Axis") : TEXT("Action"));
    ResultObj->SetNumberField(TEXT("removed_count"), RemovedCount);
    if (bHasKeyFilter)
    {
        ResultObj->SetStringField(TEXT("key"), KeyFilterName);
    }
    return ResultObj;
}

/**
 * @brief 创建 Enhanced Input Action 资源。
 * @param [in] Params 创建参数。
 * @return TSharedPtr<FJsonObject> 创建结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCreateInputActionAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString ActionNameText;
    if (!Params->TryGetStringField(TEXT("action_name"), ActionNameText))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'action_name' 参数"));
    }

    FString PackagePath;
    FString ErrorMessage;
    if (!UnrealMCPNormalizePackagePath(
        Params->HasField(TEXT("path")) ? Params->GetStringField(TEXT("path")) : TEXT(""),
        TEXT("/Game/Input"),
        PackagePath,
        ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    FString AssetName;
    FString AssetPath;
    if (!UnrealMCPBuildAssetPath(ActionNameText, PackagePath, AssetName, AssetPath, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    if (!UEditorAssetLibrary::DoesDirectoryExist(PackagePath) && !UEditorAssetLibrary::MakeDirectory(PackagePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("创建目录失败: %s"), *PackagePath));
    }

    if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Input Action 已存在: %s"), *AssetPath));
    }

    UPackage* Package = CreatePackage(*AssetPath);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("创建资源包失败: %s"), *AssetPath));
    }

    UInputAction* InputAction = NewObject<UInputAction>(Package, *AssetName, RF_Public | RF_Standalone);
    if (!InputAction)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("创建 Input Action 失败"));
    }

    EInputActionValueType ValueType = EInputActionValueType::Boolean;
    if (Params->HasField(TEXT("value_type")) &&
        !UnrealMCPParseInputActionValueType(Params->GetStringField(TEXT("value_type")), ValueType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("无效 value_type，可选值: Boolean/Axis1D/Axis2D/Axis3D"));
    }

    EInputActionAccumulationBehavior AccumulationBehavior = EInputActionAccumulationBehavior::TakeHighestAbsoluteValue;
    if (Params->HasField(TEXT("accumulation_behavior")) &&
        !UnrealMCPParseAccumulationBehavior(Params->GetStringField(TEXT("accumulation_behavior")), AccumulationBehavior))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("无效 accumulation_behavior，可选值: TakeHighestAbsoluteValue/Cumulative"));
    }

    InputAction->ValueType = ValueType;
    InputAction->AccumulationBehavior = AccumulationBehavior;
    Params->TryGetBoolField(TEXT("consume_input"), InputAction->bConsumeInput);
    Params->TryGetBoolField(TEXT("trigger_when_paused"), InputAction->bTriggerWhenPaused);
    Params->TryGetBoolField(TEXT("consume_legacy_keys"), InputAction->bConsumesActionAndAxisMappings);

    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(InputAction);
    if (!UEditorAssetLibrary::SaveAsset(AssetPath, false))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("保存 Input Action 失败: %s"), *AssetPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("action_name"), AssetName);
    ResultObj->SetStringField(TEXT("asset_path"), AssetPath);
    ResultObj->SetStringField(TEXT("value_type"), UnrealMCPInputActionValueTypeToString(InputAction->ValueType));
    ResultObj->SetStringField(TEXT("accumulation_behavior"), UnrealMCPAccumulationBehaviorToString(InputAction->AccumulationBehavior));
    ResultObj->SetBoolField(TEXT("consume_input"), InputAction->bConsumeInput);
    ResultObj->SetBoolField(TEXT("trigger_when_paused"), InputAction->bTriggerWhenPaused);
    ResultObj->SetBoolField(TEXT("consume_legacy_keys"), InputAction->bConsumesActionAndAxisMappings);
    return ResultObj;
}

/**
 * @brief 创建 Input Mapping Context 资源。
 * @param [in] Params 创建参数。
 * @return TSharedPtr<FJsonObject> 创建结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCreateInputMappingContext(const TSharedPtr<FJsonObject>& Params)
{
    FString ContextNameText;
    if (!Params->TryGetStringField(TEXT("context_name"), ContextNameText))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'context_name' 参数"));
    }

    FString PackagePath;
    FString ErrorMessage;
    if (!UnrealMCPNormalizePackagePath(
        Params->HasField(TEXT("path")) ? Params->GetStringField(TEXT("path")) : TEXT(""),
        TEXT("/Game/Input"),
        PackagePath,
        ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    FString AssetName;
    FString AssetPath;
    if (!UnrealMCPBuildAssetPath(ContextNameText, PackagePath, AssetName, AssetPath, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    if (!UEditorAssetLibrary::DoesDirectoryExist(PackagePath) && !UEditorAssetLibrary::MakeDirectory(PackagePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("创建目录失败: %s"), *PackagePath));
    }

    if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Input Mapping Context 已存在: %s"), *AssetPath));
    }

    UPackage* Package = CreatePackage(*AssetPath);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("创建资源包失败: %s"), *AssetPath));
    }

    UInputMappingContext* MappingContext = NewObject<UInputMappingContext>(Package, *AssetName, RF_Public | RF_Standalone);
    if (!MappingContext)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("创建 Input Mapping Context 失败"));
    }

    if (Params->HasField(TEXT("description")))
    {
        MappingContext->ContextDescription = FText::FromString(Params->GetStringField(TEXT("description")));
    }

    EMappingContextRegistrationTrackingMode TrackingMode = EMappingContextRegistrationTrackingMode::Untracked;
    if (Params->HasField(TEXT("registration_tracking_mode")) &&
        !UnrealMCPParseRegistrationTrackingMode(Params->GetStringField(TEXT("registration_tracking_mode")), TrackingMode))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("无效 registration_tracking_mode，可选值: Untracked/CountRegistrations"));
    }

    if (TrackingMode != EMappingContextRegistrationTrackingMode::Untracked)
    {
        FString PropertyErrorMessage;
        if (!FUnrealMCPCommonUtils::SetObjectProperty(
            MappingContext,
            TEXT("RegistrationTrackingMode"),
            MakeShared<FJsonValueString>(UnrealMCPRegistrationTrackingModeToString(TrackingMode)),
            PropertyErrorMessage))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(PropertyErrorMessage);
        }
    }

    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(MappingContext);
    if (!UEditorAssetLibrary::SaveAsset(AssetPath, false))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("保存 Input Mapping Context 失败: %s"), *AssetPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("context_name"), AssetName);
    ResultObj->SetStringField(TEXT("asset_path"), AssetPath);
    ResultObj->SetStringField(TEXT("description"), MappingContext->ContextDescription.ToString());
    ResultObj->SetStringField(TEXT("registration_tracking_mode"), UnrealMCPRegistrationTrackingModeToString(TrackingMode));
    ResultObj->SetNumberField(TEXT("mapping_count"), MappingContext->GetMappings().Num());
    return ResultObj;
}

/**
 * @brief 往 Input Mapping Context 中添加键位映射。
 * @param [in] Params 添加参数。
 * @return TSharedPtr<FJsonObject> 添加结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleAddMappingToContext(const TSharedPtr<FJsonObject>& Params)
{
    FString MappingContextReference;
    if (!Params->TryGetStringField(TEXT("mapping_context"), MappingContextReference))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'mapping_context' 参数"));
    }

    FString InputActionReference;
    if (!Params->TryGetStringField(TEXT("input_action"), InputActionReference))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'input_action' 参数"));
    }

    FString KeyName;
    if (!Params->TryGetStringField(TEXT("key"), KeyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'key' 参数"));
    }

    UInputMappingContext* MappingContext = nullptr;
    FString ErrorMessage;
    if (!UnrealMCPResolveAssetByReference<UInputMappingContext>(MappingContextReference, MappingContext, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    UInputAction* InputAction = nullptr;
    if (!UnrealMCPResolveAssetByReference<UInputAction>(InputActionReference, InputAction, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    FKey MappingKey;
    if (!UnrealMCPParseInputKey(KeyName, MappingKey))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("无效按键名: %s"), *KeyName));
    }

    MappingContext->Modify();
    const int32 MappingIndex = MappingContext->GetMappings().Num();
    FEnhancedActionKeyMapping& NewMapping = MappingContext->MapKey(InputAction, MappingKey);
    UPackage* Package = MappingContext->GetOutermost();
    if (Package)
    {
        Package->MarkPackageDirty();
    }

    const FString MappingContextPath = FPackageName::ObjectPathToPackageName(MappingContext->GetPathName());
    if (!UEditorAssetLibrary::SaveAsset(MappingContextPath, false))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("保存 Input Mapping Context 失败: %s"), *MappingContextPath));
    }

    FName MappingName = NewMapping.GetMappingName();
    if (MappingName.IsNone())
    {
        MappingName = FName(*InputAction->GetName());
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("mapping_context"), MappingContext->GetName());
    ResultObj->SetStringField(TEXT("mapping_context_path"), MappingContextPath);
    ResultObj->SetStringField(TEXT("input_action"), InputAction->GetName());
    ResultObj->SetStringField(TEXT("input_action_path"), FPackageName::ObjectPathToPackageName(InputAction->GetPathName()));
    ResultObj->SetStringField(TEXT("key"), MappingKey.GetFName().ToString());
    ResultObj->SetStringField(TEXT("value_type"), UnrealMCPInputActionValueTypeToString(InputAction->ValueType));
    ResultObj->SetNumberField(TEXT("mapping_index"), MappingIndex);
    ResultObj->SetNumberField(TEXT("mapping_count"), MappingContext->GetMappings().Num());
    ResultObj->SetStringField(TEXT("mapping_name"), MappingName.ToString());
    return ResultObj;
}

/**
 * @brief 在运行中的本地玩家上分配 Mapping Context。
 * @param [in] Params 分配参数。
 * @return TSharedPtr<FJsonObject> 分配结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleAssignMappingContext(const TSharedPtr<FJsonObject>& Params)
{
    FString MappingContextReference;
    if (!Params->TryGetStringField(TEXT("mapping_context"), MappingContextReference))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'mapping_context' 参数"));
    }

    UInputMappingContext* MappingContext = nullptr;
    FString ErrorMessage;
    if (!UnrealMCPResolveAssetByReference<UInputMappingContext>(MappingContextReference, MappingContext, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    FString ResolvedWorldType;
    UWorld* World = UnrealMCPResolveWorldByParams(Params, ResolvedWorldType, ErrorMessage);
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    double PriorityNumber = 0.0;
    Params->TryGetNumberField(TEXT("priority"), PriorityNumber);
    const int32 Priority = static_cast<int32>(PriorityNumber);

    double PlayerIndexNumber = 0.0;
    Params->TryGetNumberField(TEXT("player_index"), PlayerIndexNumber);
    const int32 PlayerIndex = static_cast<int32>(PlayerIndexNumber);

    AActor* TargetActor = nullptr;
    APlayerController* TargetController = nullptr;
    if (Params->HasField(TEXT("name")) || Params->HasField(TEXT("actor_path")))
    {
        TargetActor = UnrealMCPResolveActorByParams(Params, World, ErrorMessage);
        if (!TargetActor)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
        }

        if (APlayerController* PlayerController = Cast<APlayerController>(TargetActor))
        {
            TargetController = PlayerController;
        }
        else if (APawn* Pawn = Cast<APawn>(TargetActor))
        {
            TargetController = Cast<APlayerController>(Pawn->GetController());
            if (!TargetController)
            {
                return FUnrealMCPCommonUtils::CreateErrorResponse(
                    FString::Printf(TEXT("Pawn 当前没有本地 PlayerController: %s"), *Pawn->GetName()));
            }
        }
        else
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("assign_mapping_context 只能绑定到 Pawn 或 PlayerController"));
        }
    }
    else
    {
        TargetController = UGameplayStatics::GetPlayerController(World, PlayerIndex);
    }

    if (!TargetController)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("找不到本地 PlayerController，player_index=%d"), PlayerIndex));
    }

    ULocalPlayer* LocalPlayer = TargetController->GetLocalPlayer();
    if (!LocalPlayer)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("PlayerController 不是本地玩家控制器: %s"), *TargetController->GetName()));
    }

    UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!InputSubsystem)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("获取 UEnhancedInputLocalPlayerSubsystem 失败"));
    }

    bool bClearExisting = false;
    Params->TryGetBoolField(TEXT("clear_existing"), bClearExisting);

    FModifyContextOptions ModifyContextOptions;
    bool bIgnorePressedKeys = ModifyContextOptions.bIgnoreAllPressedKeysUntilRelease;
    bool bForceImmediately = ModifyContextOptions.bForceImmediately;
    bool bNotifyUserSettings = ModifyContextOptions.bNotifyUserSettings;
    Params->TryGetBoolField(TEXT("ignore_all_pressed_keys_until_release"), bIgnorePressedKeys);
    Params->TryGetBoolField(TEXT("force_immediately"), bForceImmediately);
    Params->TryGetBoolField(TEXT("notify_user_settings"), bNotifyUserSettings);
    ModifyContextOptions.bIgnoreAllPressedKeysUntilRelease = bIgnorePressedKeys;
    ModifyContextOptions.bForceImmediately = bForceImmediately;
    ModifyContextOptions.bNotifyUserSettings = bNotifyUserSettings;

    if (bClearExisting)
    {
        InputSubsystem->ClearAllMappings();
    }

    InputSubsystem->AddMappingContext(MappingContext, Priority, ModifyContextOptions);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("mapping_context"), MappingContext->GetName());
    ResultObj->SetStringField(TEXT("mapping_context_path"), FPackageName::ObjectPathToPackageName(MappingContext->GetPathName()));
    ResultObj->SetNumberField(TEXT("priority"), Priority);
    ResultObj->SetNumberField(TEXT("player_index"), PlayerIndex);
    ResultObj->SetBoolField(TEXT("clear_existing"), bClearExisting);
    ResultObj->SetBoolField(TEXT("ignore_all_pressed_keys_until_release"), ModifyContextOptions.bIgnoreAllPressedKeysUntilRelease);
    ResultObj->SetBoolField(TEXT("force_immediately"), ModifyContextOptions.bForceImmediately);
    ResultObj->SetBoolField(TEXT("notify_user_settings"), ModifyContextOptions.bNotifyUserSettings);
    ResultObj->SetStringField(TEXT("controller_name"), TargetController->GetName());
    ResultObj->SetStringField(TEXT("controller_path"), TargetController->GetPathName());
    if (TargetActor)
    {
        ResultObj->SetStringField(TEXT("target_actor_name"), TargetActor->GetName());
        ResultObj->SetStringField(TEXT("target_actor_path"), TargetActor->GetPathName());
        ResultObj->SetStringField(TEXT("target_actor_class"), TargetActor->GetClass()->GetName());
    }

    UnrealMCPAppendWorldInfo(ResultObj, World, ResolvedWorldType);
    return ResultObj;
}

/**
 * @brief 读取项目设置对象上的单个属性。
 * @param [in] Params 查询参数。
 * @return TSharedPtr<FJsonObject> 设置值结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleGetProjectSetting(const TSharedPtr<FJsonObject>& Params)
{
    FString SettingsClassText;
    if (!Params->TryGetStringField(TEXT("settings_class"), SettingsClassText))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'settings_class' 参数"));
    }

    FString PropertyName;
    if (!UnrealMCPTryGetProjectPropertyName(Params, PropertyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'property_name' 参数"));
    }

    UObject* SettingsObject = nullptr;
    FString ResolvedSettingsClass;
    FString ErrorMessage;
    if (!UnrealMCPResolveSupportedSettingsObject(SettingsClassText, SettingsObject, ResolvedSettingsClass, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    FProperty* Property = SettingsObject->GetClass()->FindPropertyByName(FName(*PropertyName));
    if (!Property)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("设置 %s 上不存在属性: %s"), *ResolvedSettingsClass, *PropertyName));
    }

    TSharedPtr<FJsonValue> PropertyValue;
    FString ExportedValue;
    if (!UnrealMCPExportProjectPropertyValue(SettingsObject, Property, PropertyValue, ExportedValue))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("读取属性失败: %s.%s"), *ResolvedSettingsClass, *Property->GetName()));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("settings_class"), ResolvedSettingsClass);
    ResultObj->SetStringField(TEXT("property_name"), Property->GetName());
    ResultObj->SetStringField(TEXT("property_cpp_type"), Property->GetCPPType());
    ResultObj->SetStringField(TEXT("exported_value"), ExportedValue);
    ResultObj->SetField(TEXT("value"), PropertyValue);
    ResultObj->SetStringField(TEXT("config_file"), SettingsObject->GetDefaultConfigFilename());
    return ResultObj;
}

/**
 * @brief 写入项目设置对象上的单个属性。
 * @param [in] Params 写入参数。
 * @return TSharedPtr<FJsonObject> 写入结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleSetProjectSetting(const TSharedPtr<FJsonObject>& Params)
{
    FString SettingsClassText;
    if (!Params->TryGetStringField(TEXT("settings_class"), SettingsClassText))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'settings_class' 参数"));
    }

    FString PropertyName;
    if (!UnrealMCPTryGetProjectPropertyName(Params, PropertyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'property_name' 参数"));
    }

    TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
    if (!Value.IsValid())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("缺少 'value' 参数"));
    }

    UObject* SettingsObject = nullptr;
    FString ResolvedSettingsClass;
    FString ErrorMessage;
    if (!UnrealMCPResolveSupportedSettingsObject(SettingsClassText, SettingsObject, ResolvedSettingsClass, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    FProperty* Property = SettingsObject->GetClass()->FindPropertyByName(FName(*PropertyName));
    if (!Property)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("设置 %s 上不存在属性: %s"), *ResolvedSettingsClass, *PropertyName));
    }

    FString ExportedValue;
    if (!UnrealMCPSetProjectPropertyValue(SettingsObject, Property, Value, ExportedValue, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    if (UInputSettings* InputSettings = Cast<UInputSettings>(SettingsObject))
    {
        UnrealMCPSaveInputSettings(InputSettings);
    }
    else
    {
        SettingsObject->SaveConfig();
    }

    TSharedPtr<FJsonValue> PropertyValue;
    FString VerifiedExportedValue;
    UnrealMCPExportProjectPropertyValue(SettingsObject, Property, PropertyValue, VerifiedExportedValue);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("settings_class"), ResolvedSettingsClass);
    ResultObj->SetStringField(TEXT("property_name"), Property->GetName());
    ResultObj->SetStringField(TEXT("property_cpp_type"), Property->GetCPPType());
    ResultObj->SetStringField(TEXT("exported_value"), VerifiedExportedValue);
    ResultObj->SetField(TEXT("value"), PropertyValue);
    ResultObj->SetStringField(TEXT("config_file"), SettingsObject->GetDefaultConfigFilename());
    return ResultObj;
}

/**
 * @brief 设置项目默认地图相关配置。
 * @param [in] Params 默认地图参数。
 * @return TSharedPtr<FJsonObject> 设置结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleSetDefaultMaps(const TSharedPtr<FJsonObject>& Params)
{
    UGameMapsSettings* GameMapsSettings = GetMutableDefault<UGameMapsSettings>();
    if (!GameMapsSettings)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("获取 GameMapsSettings 失败"));
    }

    bool bHasAnyUpdate = false;
    FString ErrorMessage;
    TArray<TSharedPtr<FJsonValue>> UpdatedFields;

    auto ApplyMapSetting = [&](const TCHAR* ParamName, const TCHAR* PropertyName) -> bool
    {
        if (!Params->HasField(ParamName))
        {
            return true;
        }

        FProperty* Property = GameMapsSettings->GetClass()->FindPropertyByName(FName(PropertyName));
        if (!Property)
        {
            ErrorMessage = FString::Printf(TEXT("GameMapsSettings 上不存在属性: %s"), PropertyName);
            return false;
        }

        FString PackageName;
        FString ObjectPath;
        if (!UnrealMCPNormalizeMapReference(Params->GetStringField(ParamName), PackageName, ObjectPath, ErrorMessage))
        {
            return false;
        }

        FString ExportedValue;
        if (!UnrealMCPSetProjectPropertyValue(
            GameMapsSettings,
            Property,
            MakeShared<FJsonValueString>(ObjectPath),
            ExportedValue,
            ErrorMessage))
        {
            return false;
        }

        bHasAnyUpdate = true;
        UpdatedFields.Add(MakeShared<FJsonValueString>(ParamName));
        return true;
    };

    if (!ApplyMapSetting(TEXT("game_default_map"), TEXT("GameDefaultMap")) ||
        !ApplyMapSetting(TEXT("server_default_map"), TEXT("ServerDefaultMap")) ||
        !ApplyMapSetting(TEXT("editor_startup_map"), TEXT("EditorStartupMap")) ||
        !ApplyMapSetting(TEXT("transition_map"), TEXT("TransitionMap")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    if (Params->HasField(TEXT("local_map_options")))
    {
        FProperty* Property = GameMapsSettings->GetClass()->FindPropertyByName(TEXT("LocalMapOptions"));
        if (!Property)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("GameMapsSettings 上不存在属性: LocalMapOptions"));
        }

        FString ExportedValue;
        if (!UnrealMCPSetProjectPropertyValue(
            GameMapsSettings,
            Property,
            MakeShared<FJsonValueString>(Params->GetStringField(TEXT("local_map_options"))),
            ExportedValue,
            ErrorMessage))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
        }

        bHasAnyUpdate = true;
        UpdatedFields.Add(MakeShared<FJsonValueString>(TEXT("local_map_options")));
    }

    if (!bHasAnyUpdate)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("至少需要传入 game_default_map/server_default_map/editor_startup_map/transition_map/local_map_options 之一"));
    }

    GameMapsSettings->SaveConfig();

    auto ExportSettingValue = [&](const TCHAR* PropertyName) -> FString
    {
        if (FProperty* Property = GameMapsSettings->GetClass()->FindPropertyByName(FName(PropertyName)))
        {
            FString ExportedValue;
            TSharedPtr<FJsonValue> PropertyValue;
            UnrealMCPExportProjectPropertyValue(GameMapsSettings, Property, PropertyValue, ExportedValue);
            return ExportedValue;
        }

        return FString();
    };

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetArrayField(TEXT("updated_fields"), UpdatedFields);
    ResultObj->SetStringField(TEXT("game_default_map"), ExportSettingValue(TEXT("GameDefaultMap")));
    ResultObj->SetStringField(TEXT("server_default_map"), ExportSettingValue(TEXT("ServerDefaultMap")));
    ResultObj->SetStringField(TEXT("editor_startup_map"), ExportSettingValue(TEXT("EditorStartupMap")));
    ResultObj->SetStringField(TEXT("transition_map"), ExportSettingValue(TEXT("TransitionMap")));
    ResultObj->SetStringField(TEXT("local_map_options"), ExportSettingValue(TEXT("LocalMapOptions")));
    ResultObj->SetStringField(TEXT("config_file"), GameMapsSettings->GetDefaultConfigFilename());
    return ResultObj;
}

/**
 * @brief 设置项目级 Game Framework 默认类。
 * @param [in] Params Game Framework 参数。
 * @return TSharedPtr<FJsonObject> 设置结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleSetGameFrameworkDefaults(const TSharedPtr<FJsonObject>& Params)
{
    UGameMapsSettings* GameMapsSettings = GetMutableDefault<UGameMapsSettings>();
    if (!GameMapsSettings)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("获取 GameMapsSettings 失败"));
    }

    bool bUpdatedConfig = false;
    bool bUpdatedBlueprint = false;
    FString ErrorMessage;
    TArray<TSharedPtr<FJsonValue>> UpdatedFields;

    auto ApplyMapsAndModesClassSetting = [&](const TCHAR* ParamName, const TCHAR* PropertyName, UClass* RequiredBaseClass) -> bool
    {
        if (!Params->HasField(ParamName))
        {
            return true;
        }

        FProperty* Property = GameMapsSettings->GetClass()->FindPropertyByName(FName(PropertyName));
        if (!Property)
        {
            ErrorMessage = FString::Printf(TEXT("GameMapsSettings 上不存在属性: %s"), PropertyName);
            return false;
        }

        UClass* ResolvedClass = nullptr;
        if (!UnrealMCPResolveClassByReference(Params->GetStringField(ParamName), ResolvedClass, RequiredBaseClass, ErrorMessage))
        {
            return false;
        }

        FString ExportedValue;
        if (!UnrealMCPSetProjectPropertyValue(
            GameMapsSettings,
            Property,
            MakeShared<FJsonValueString>(ResolvedClass->GetPathName()),
            ExportedValue,
            ErrorMessage))
        {
            return false;
        }

        bUpdatedConfig = true;
        UpdatedFields.Add(MakeShared<FJsonValueString>(ParamName));
        return true;
    };

    if (!ApplyMapsAndModesClassSetting(TEXT("game_instance_class"), TEXT("GameInstanceClass"), UGameInstance::StaticClass()) ||
        !ApplyMapsAndModesClassSetting(TEXT("game_mode_class"), TEXT("GlobalDefaultGameMode"), AGameModeBase::StaticClass()) ||
        !ApplyMapsAndModesClassSetting(TEXT("server_game_mode_class"), TEXT("GlobalDefaultServerGameMode"), AGameModeBase::StaticClass()))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    const bool bNeedsGameModeBlueprint =
        Params->HasField(TEXT("default_pawn_class")) ||
        Params->HasField(TEXT("hud_class")) ||
        Params->HasField(TEXT("player_controller_class")) ||
        Params->HasField(TEXT("game_state_class")) ||
        Params->HasField(TEXT("spectator_class")) ||
        Params->HasField(TEXT("replay_spectator_player_controller_class"));

    UBlueprint* TargetGameModeBlueprint = nullptr;
    FString TargetGameModeReference;
    Params->TryGetStringField(TEXT("target_game_mode"), TargetGameModeReference);

    if (bNeedsGameModeBlueprint)
    {
        if (TargetGameModeReference.IsEmpty())
        {
            if (Params->HasField(TEXT("game_mode_class")))
            {
                TargetGameModeReference = Params->GetStringField(TEXT("game_mode_class"));
            }
            else
            {
                TargetGameModeReference = UGameMapsSettings::GetGlobalDefaultGameMode();
            }
        }

        if (TargetGameModeReference.IsEmpty())
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                TEXT("设置 DefaultPawn/HUD/PlayerController 等默认类时，需要先提供 game_mode_class 或 target_game_mode"));
        }

        if (!UnrealMCPResolveBlueprintByClassReference(TargetGameModeReference, TargetGameModeBlueprint, ErrorMessage))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("目标 GameMode 必须是 Blueprint 资源，当前无法持久化修改: %s；%s"),
                    *TargetGameModeReference,
                    *ErrorMessage));
        }

        AGameModeBase* GameModeCDO = Cast<AGameModeBase>(
            TargetGameModeBlueprint->GeneratedClass ? TargetGameModeBlueprint->GeneratedClass->GetDefaultObject() : nullptr);
        if (!GameModeCDO)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("目标 Blueprint 不是有效的 GameModeBase"));
        }

        auto ApplyGameModeClassDefault = [&](const TCHAR* ParamName, const TCHAR* PropertyName, UClass* RequiredBaseClass) -> bool
        {
            if (!Params->HasField(ParamName))
            {
                return true;
            }

            FProperty* Property = GameModeCDO->GetClass()->FindPropertyByName(FName(PropertyName));
            if (!Property)
            {
                ErrorMessage = FString::Printf(TEXT("GameMode 默认对象上不存在属性: %s"), PropertyName);
                return false;
            }

            UClass* ResolvedClass = nullptr;
            if (!UnrealMCPResolveClassByReference(Params->GetStringField(ParamName), ResolvedClass, RequiredBaseClass, ErrorMessage))
            {
                return false;
            }

            FString ExportedValue;
            if (!UnrealMCPSetProjectPropertyValue(
                GameModeCDO,
                Property,
                MakeShared<FJsonValueString>(ResolvedClass->GetPathName()),
                ExportedValue,
                ErrorMessage))
            {
                return false;
            }

            bUpdatedBlueprint = true;
            UpdatedFields.Add(MakeShared<FJsonValueString>(ParamName));
            return true;
        };

        if (!ApplyGameModeClassDefault(TEXT("default_pawn_class"), TEXT("DefaultPawnClass"), APawn::StaticClass()) ||
            !ApplyGameModeClassDefault(TEXT("hud_class"), TEXT("HUDClass"), AHUD::StaticClass()) ||
            !ApplyGameModeClassDefault(TEXT("player_controller_class"), TEXT("PlayerControllerClass"), APlayerController::StaticClass()) ||
            !ApplyGameModeClassDefault(TEXT("game_state_class"), TEXT("GameStateClass"), AGameStateBase::StaticClass()) ||
            !ApplyGameModeClassDefault(TEXT("spectator_class"), TEXT("SpectatorClass"), ASpectatorPawn::StaticClass()) ||
            !ApplyGameModeClassDefault(TEXT("replay_spectator_player_controller_class"), TEXT("ReplaySpectatorPlayerControllerClass"), APlayerController::StaticClass()))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
        }
    }

    if (!bUpdatedConfig && !bUpdatedBlueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("至少需要传入 game_instance_class/game_mode_class/server_game_mode_class/default_pawn_class/hud_class/player_controller_class/game_state_class/spectator_class/replay_spectator_player_controller_class 之一"));
    }

    if (bUpdatedConfig)
    {
        GameMapsSettings->SaveConfig();
    }

    if (bUpdatedBlueprint && TargetGameModeBlueprint)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(TargetGameModeBlueprint);
        FKismetEditorUtilities::CompileBlueprint(TargetGameModeBlueprint);
        UEditorAssetLibrary::SaveAsset(FPackageName::ObjectPathToPackageName(TargetGameModeBlueprint->GetPathName()), false);
    }

    auto ExportMapsAndModesValue = [&](const TCHAR* PropertyName) -> FString
    {
        if (FProperty* Property = GameMapsSettings->GetClass()->FindPropertyByName(FName(PropertyName)))
        {
            FString ExportedValue;
            TSharedPtr<FJsonValue> PropertyValue;
            UnrealMCPExportProjectPropertyValue(GameMapsSettings, Property, PropertyValue, ExportedValue);
            return ExportedValue;
        }

        return FString();
    };

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetArrayField(TEXT("updated_fields"), UpdatedFields);
    ResultObj->SetStringField(TEXT("game_instance_class"), ExportMapsAndModesValue(TEXT("GameInstanceClass")));
    ResultObj->SetStringField(TEXT("game_mode_class"), ExportMapsAndModesValue(TEXT("GlobalDefaultGameMode")));
    ResultObj->SetStringField(TEXT("server_game_mode_class"), ExportMapsAndModesValue(TEXT("GlobalDefaultServerGameMode")));
    ResultObj->SetStringField(TEXT("config_file"), GameMapsSettings->GetDefaultConfigFilename());

    if (TargetGameModeBlueprint && TargetGameModeBlueprint->GeneratedClass)
    {
        AGameModeBase* GameModeCDO = Cast<AGameModeBase>(TargetGameModeBlueprint->GeneratedClass->GetDefaultObject());
        if (GameModeCDO)
        {
            ResultObj->SetStringField(TEXT("target_game_mode_blueprint"), TargetGameModeBlueprint->GetPathName());
            ResultObj->SetStringField(TEXT("target_game_mode_class"), TargetGameModeBlueprint->GeneratedClass->GetPathName());
            ResultObj->SetStringField(TEXT("default_pawn_class"), GameModeCDO->DefaultPawnClass ? GameModeCDO->DefaultPawnClass->GetPathName() : TEXT(""));
            ResultObj->SetStringField(TEXT("hud_class"), GameModeCDO->HUDClass ? GameModeCDO->HUDClass->GetPathName() : TEXT(""));
            ResultObj->SetStringField(TEXT("player_controller_class"), GameModeCDO->PlayerControllerClass ? GameModeCDO->PlayerControllerClass->GetPathName() : TEXT(""));
            ResultObj->SetStringField(TEXT("game_state_class"), GameModeCDO->GameStateClass ? GameModeCDO->GameStateClass->GetPathName() : TEXT(""));
            ResultObj->SetStringField(TEXT("spectator_class"), GameModeCDO->SpectatorClass ? GameModeCDO->SpectatorClass->GetPathName() : TEXT(""));
            ResultObj->SetStringField(
                TEXT("replay_spectator_player_controller_class"),
                GameModeCDO->ReplaySpectatorPlayerControllerClass ? GameModeCDO->ReplaySpectatorPlayerControllerClass->GetPathName() : TEXT(""));
        }
    }

    return ResultObj;
}
