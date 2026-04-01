/**
 * @file UnrealMCPAssetCommands.cpp
 * @brief 资产查询与摘要 MCP 命令处理实现。
 */
#include "Commands/UnrealMCPAssetCommands.h"

#include "Commands/UnrealMCPCommonUtils.h"
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
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstance.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "EditorAssetLibrary.h"
#include "Subsystems/EditorAssetSubsystem.h"
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
    if (CommandType == TEXT("get_blueprint_summary")) return HandleGetBlueprintSummary(Params);
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
