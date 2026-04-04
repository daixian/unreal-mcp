/**
 * @file UnrealMCPUMGCommands.cpp
 * @brief UMG 命令处理实现，负责 Widget Blueprint 的创建与编辑。
 */
#include "Commands/UnrealMCPUMGCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Animation/MovieScene2DTransformSection.h"
#include "Animation/MovieScene2DTransformTrack.h"
#include "Animation/WidgetAnimation.h"
#include "Animation/WidgetAnimationBinding.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/TextBlock.h"
#include "EditorUtilityWidget.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "WidgetBlueprint.h"
// We'll create widgets using regular Factory classes
#include "Factories/Factory.h"
// Remove problematic includes that don't exist in UE 5.5
// #include "UMGEditorSubsystem.h"
// #include "WidgetBlueprintFactory.h"
#include "WidgetBlueprintEditor.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CheckBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableText.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "JsonObjectConverter.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/Button.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_Event.h"
#include "K2Node_ComponentBoundEvent.h"
#include "EdGraphSchema_K2.h"
#include "Misc/PackageName.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Styling/SlateColor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "UObject/UnrealType.h"

static bool UnrealMCPUMGTryGetStringField(const TSharedPtr<FJsonObject>& Params, std::initializer_list<const TCHAR*> FieldNames, FString& OutValue)
{
	if (!Params.IsValid())
	{
		return false;
	}

	for (const TCHAR* FieldName : FieldNames)
	{
		if (Params->TryGetStringField(FieldName, OutValue) && !OutValue.IsEmpty())
		{
			return true;
		}
	}

	return false;
}

static FString UnrealMCPUMGNormalizePackagePath(FString PackagePath)
{
	PackagePath = PackagePath.TrimStartAndEnd();
	if (PackagePath.IsEmpty())
	{
		return TEXT("/Game/Widgets");
	}

	if (!PackagePath.StartsWith(TEXT("/")))
	{
		PackagePath = TEXT("/") + PackagePath;
	}

	while (PackagePath.EndsWith(TEXT("/")))
	{
		PackagePath.LeftChopInline(1);
	}

	return PackagePath;
}

static UClass* UnrealMCPUMGResolveParentClass(const FString& ParentClassName)
{
	FString ResolvedName = ParentClassName.TrimStartAndEnd();
	if (ResolvedName.IsEmpty())
	{
		return UUserWidget::StaticClass();
	}

	TArray<FString> Candidates;
	Candidates.Add(ResolvedName);
	if (!ResolvedName.StartsWith(TEXT("U")) && !ResolvedName.StartsWith(TEXT("/")))
	{
		Candidates.Add(TEXT("U") + ResolvedName);
	}

	for (const FString& Candidate : Candidates)
	{
		UClass* FoundClass = Candidate.StartsWith(TEXT("/"))
			? LoadObject<UClass>(nullptr, *Candidate)
			: FindFirstObject<UClass>(*Candidate, EFindFirstObjectOptions::None);
		if (FoundClass && FoundClass->IsChildOf(UUserWidget::StaticClass()))
		{
			return FoundClass;
		}
	}

	return nullptr;
}

static UWidgetBlueprint* UnrealMCPUMGFindWidgetBlueprint(const FString& BlueprintName)
{
	return Cast<UWidgetBlueprint>(FUnrealMCPCommonUtils::FindBlueprint(BlueprintName));
}

static bool UnrealMCPUMGCanUseAnimationName(UWidgetBlueprint* WidgetBlueprint, const FName AnimationName)
{
	if (!WidgetBlueprint || AnimationName.IsNone())
	{
		return false;
	}

	for (const TObjectPtr<UWidgetAnimation>& ExistingAnimation : WidgetBlueprint->Animations)
	{
		if (ExistingAnimation && ExistingAnimation->GetFName() == AnimationName)
		{
			return false;
		}
	}

	if (WidgetBlueprint->ParentClass)
	{
		if (const FObjectPropertyBase* ExistingProperty = CastField<FObjectPropertyBase>(WidgetBlueprint->ParentClass->FindPropertyByName(AnimationName)))
		{
			if (ExistingProperty->PropertyClass && ExistingProperty->PropertyClass->IsChildOf(UWidgetAnimation::StaticClass()))
			{
				return false;
			}
		}
	}

	return true;
}

static UWidgetAnimation* UnrealMCPUMGFindWidgetAnimation(UWidgetBlueprint* WidgetBlueprint, const FString& AnimationName)
{
	if (!WidgetBlueprint)
	{
		return nullptr;
	}

	const FString TrimmedAnimationName = AnimationName.TrimStartAndEnd();
	if (TrimmedAnimationName.IsEmpty())
	{
		return nullptr;
	}

	for (const TObjectPtr<UWidgetAnimation>& Animation : WidgetBlueprint->Animations)
	{
		if (!Animation)
		{
			continue;
		}

		if (Animation->GetName().Equals(TrimmedAnimationName, ESearchCase::CaseSensitive) ||
			Animation->GetDisplayLabel().Equals(TrimmedAnimationName, ESearchCase::CaseSensitive))
		{
			return Animation;
		}
	}

	return nullptr;
}

static bool UnrealMCPUMGTryGetNumericArrayField(
	const TSharedPtr<FJsonObject>& Params,
	std::initializer_list<const TCHAR*> FieldNames,
	const int32 ExpectedCount,
	TArray<double>& OutValues)
{
	OutValues.Reset();

	if (!Params.IsValid())
	{
		return false;
	}

	for (const TCHAR* FieldName : FieldNames)
	{
		const TArray<TSharedPtr<FJsonValue>>* ValueArray = nullptr;
		if (!Params->TryGetArrayField(FieldName, ValueArray) || !ValueArray || ValueArray->Num() != ExpectedCount)
		{
			continue;
		}

		bool bValid = true;
		for (const TSharedPtr<FJsonValue>& Value : *ValueArray)
		{
			if (!Value.IsValid() || Value->Type != EJson::Number)
			{
				bValid = false;
				break;
			}

			OutValues.Add(Value->AsNumber());
		}

		if (bValid)
		{
			return true;
		}

		OutValues.Reset();
	}

	return false;
}

static void UnrealMCPUMGExpandSectionRange(UMovieSceneSection* Section, const FFrameNumber FrameNumber)
{
	if (!Section)
	{
		return;
	}

	const TRange<FFrameNumber> CurrentRange = Section->GetRange();
	if (CurrentRange.IsEmpty())
	{
		Section->SetRange(TRange<FFrameNumber>::Inclusive(FrameNumber, FrameNumber));
		return;
	}

	FFrameNumber LowerBound = CurrentRange.GetLowerBound().IsOpen() ? FrameNumber : CurrentRange.GetLowerBoundValue();
	FFrameNumber UpperBound = CurrentRange.GetUpperBound().IsOpen() ? FrameNumber : CurrentRange.GetUpperBoundValue();

	if (FrameNumber < LowerBound)
	{
		LowerBound = FrameNumber;
	}
	if (FrameNumber > UpperBound)
	{
		UpperBound = FrameNumber;
	}

	Section->SetRange(TRange<FFrameNumber>::Inclusive(LowerBound, UpperBound));
}

static FMovieSceneFloatValue UnrealMCPUMGCreateFloatKeyValue(const float Value, const FString& InterpolationName)
{
	FMovieSceneFloatValue KeyValue(Value);
	const FString NormalizedInterpolation = InterpolationName.TrimStartAndEnd().ToLower();

	if (NormalizedInterpolation == TEXT("constant"))
	{
		KeyValue.InterpMode = RCIM_Constant;
		KeyValue.TangentMode = RCTM_Auto;
	}
	else if (NormalizedInterpolation == TEXT("linear"))
	{
		KeyValue.InterpMode = RCIM_Linear;
		KeyValue.TangentMode = RCTM_Auto;
	}
	else
	{
		KeyValue.InterpMode = RCIM_Cubic;
		KeyValue.TangentMode = RCTM_Auto;
	}

	return KeyValue;
}

static void UnrealMCPUMGUpdateOrAddFloatKey(
	FMovieSceneFloatChannel& Channel,
	const FFrameNumber FrameNumber,
	const float Value,
	const FString& InterpolationName)
{
	FFrameNumber KeyTimes[] = { FrameNumber };
	FMovieSceneFloatValue KeyValues[] = { UnrealMCPUMGCreateFloatKeyValue(Value, InterpolationName) };
	Channel.UpdateOrAddKeys(MakeArrayView(KeyTimes), MakeArrayView(KeyValues));
}

static FGuid UnrealMCPUMGFindAnimationBindingGuid(const UWidgetAnimation* WidgetAnimation, const UWidget* Widget)
{
	if (!WidgetAnimation || !Widget)
	{
		return FGuid();
	}

	const FName WidgetFName = Widget->GetFName();
	for (const FWidgetAnimationBinding& Binding : WidgetAnimation->AnimationBindings)
	{
		if (Binding.WidgetName == WidgetFName && Binding.SlotWidgetName.IsNone())
		{
			return Binding.AnimationGuid;
		}
	}

	return FGuid();
}

static FGuid UnrealMCPUMGEnsureAnimationBinding(UWidgetBlueprint* WidgetBlueprint, UWidgetAnimation* WidgetAnimation, UWidget* Widget)
{
	if (!WidgetBlueprint || !WidgetAnimation || !WidgetAnimation->GetMovieScene() || !Widget)
	{
		return FGuid();
	}

	const FGuid ExistingGuid = UnrealMCPUMGFindAnimationBindingGuid(WidgetAnimation, Widget);
	if (ExistingGuid.IsValid())
	{
		return ExistingGuid;
	}

	UMovieScene* MovieScene = WidgetAnimation->GetMovieScene();
	const FGuid BindingGuid = MovieScene->AddPossessable(Widget->GetName(), Widget->GetClass());
	if (!BindingGuid.IsValid())
	{
		return FGuid();
	}

	FWidgetAnimationBinding NewBinding;
	NewBinding.AnimationGuid = BindingGuid;
	NewBinding.WidgetName = Widget->GetFName();
	NewBinding.bIsRootWidget = WidgetBlueprint->WidgetTree && WidgetBlueprint->WidgetTree->RootWidget == Widget;
	WidgetAnimation->AnimationBindings.Add(NewBinding);
	return BindingGuid;
}

static UMovieSceneFloatSection* UnrealMCPUMGFindOrAddFloatSection(UMovieScene* MovieScene, const FGuid& BindingGuid, const FName PropertyName)
{
	if (!MovieScene || !BindingGuid.IsValid())
	{
		return nullptr;
	}

	UMovieSceneFloatTrack* Track = MovieScene->FindTrack<UMovieSceneFloatTrack>(BindingGuid, PropertyName);
	if (!Track)
	{
		Track = MovieScene->AddTrack<UMovieSceneFloatTrack>(BindingGuid);
		if (!Track)
		{
			return nullptr;
		}

		Track->Modify();
		Track->SetPropertyNameAndPath(PropertyName, PropertyName.ToString());
	}

	for (UMovieSceneSection* Section : Track->GetAllSections())
	{
		if (UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(Section))
		{
			return FloatSection;
		}
	}

	UMovieSceneFloatSection* NewSection = Cast<UMovieSceneFloatSection>(Track->CreateNewSection());
	if (!NewSection)
	{
		return nullptr;
	}

	NewSection->Modify();
	Track->AddSection(*NewSection);
	return NewSection;
}

static UMovieScene2DTransformSection* UnrealMCPUMGFindOrAddTransformSection(UMovieScene* MovieScene, const FGuid& BindingGuid)
{
	static const FName RenderTransformPropertyName(TEXT("RenderTransform"));

	if (!MovieScene || !BindingGuid.IsValid())
	{
		return nullptr;
	}

	UMovieScene2DTransformTrack* Track = MovieScene->FindTrack<UMovieScene2DTransformTrack>(BindingGuid, RenderTransformPropertyName);
	if (!Track)
	{
		Track = MovieScene->AddTrack<UMovieScene2DTransformTrack>(BindingGuid);
		if (!Track)
		{
			return nullptr;
		}

		Track->Modify();
		Track->SetPropertyNameAndPath(RenderTransformPropertyName, RenderTransformPropertyName.ToString());
	}

	for (UMovieSceneSection* Section : Track->GetAllSections())
	{
		if (UMovieScene2DTransformSection* TransformSection = Cast<UMovieScene2DTransformSection>(Section))
		{
			return TransformSection;
		}
	}

	UMovieScene2DTransformSection* NewSection = Cast<UMovieScene2DTransformSection>(Track->CreateNewSection());
	if (!NewSection)
	{
		return nullptr;
	}

	NewSection->Modify();
	Track->AddSection(*NewSection);
	return NewSection;
}

enum class EUnrealMCPUMGAnimationPropertyKind : uint8
{
	RenderOpacity,
	Translation,
	TranslationX,
	TranslationY,
	Scale,
	ScaleX,
	ScaleY,
	Shear,
	ShearX,
	ShearY,
	Rotation,
};

static bool UnrealMCPUMGResolveAnimationProperty(const FString& InPropertyName, EUnrealMCPUMGAnimationPropertyKind& OutPropertyKind, FString& OutCanonicalName)
{
	FString NormalizedProperty = InPropertyName.TrimStartAndEnd().ToLower();
	NormalizedProperty.ReplaceInline(TEXT(" "), TEXT(""));
	NormalizedProperty.ReplaceInline(TEXT("-"), TEXT("_"));

	if (NormalizedProperty == TEXT("render_opacity") || NormalizedProperty == TEXT("opacity"))
	{
		OutPropertyKind = EUnrealMCPUMGAnimationPropertyKind::RenderOpacity;
		OutCanonicalName = TEXT("render_opacity");
		return true;
	}
	if (NormalizedProperty == TEXT("render_transform.translation") || NormalizedProperty == TEXT("translation"))
	{
		OutPropertyKind = EUnrealMCPUMGAnimationPropertyKind::Translation;
		OutCanonicalName = TEXT("render_transform.translation");
		return true;
	}
	if (NormalizedProperty == TEXT("render_transform.translation_x") || NormalizedProperty == TEXT("translation_x"))
	{
		OutPropertyKind = EUnrealMCPUMGAnimationPropertyKind::TranslationX;
		OutCanonicalName = TEXT("render_transform.translation_x");
		return true;
	}
	if (NormalizedProperty == TEXT("render_transform.translation_y") || NormalizedProperty == TEXT("translation_y"))
	{
		OutPropertyKind = EUnrealMCPUMGAnimationPropertyKind::TranslationY;
		OutCanonicalName = TEXT("render_transform.translation_y");
		return true;
	}
	if (NormalizedProperty == TEXT("render_transform.scale") || NormalizedProperty == TEXT("scale"))
	{
		OutPropertyKind = EUnrealMCPUMGAnimationPropertyKind::Scale;
		OutCanonicalName = TEXT("render_transform.scale");
		return true;
	}
	if (NormalizedProperty == TEXT("render_transform.scale_x") || NormalizedProperty == TEXT("scale_x"))
	{
		OutPropertyKind = EUnrealMCPUMGAnimationPropertyKind::ScaleX;
		OutCanonicalName = TEXT("render_transform.scale_x");
		return true;
	}
	if (NormalizedProperty == TEXT("render_transform.scale_y") || NormalizedProperty == TEXT("scale_y"))
	{
		OutPropertyKind = EUnrealMCPUMGAnimationPropertyKind::ScaleY;
		OutCanonicalName = TEXT("render_transform.scale_y");
		return true;
	}
	if (NormalizedProperty == TEXT("render_transform.shear") || NormalizedProperty == TEXT("shear"))
	{
		OutPropertyKind = EUnrealMCPUMGAnimationPropertyKind::Shear;
		OutCanonicalName = TEXT("render_transform.shear");
		return true;
	}
	if (NormalizedProperty == TEXT("render_transform.shear_x") || NormalizedProperty == TEXT("shear_x"))
	{
		OutPropertyKind = EUnrealMCPUMGAnimationPropertyKind::ShearX;
		OutCanonicalName = TEXT("render_transform.shear_x");
		return true;
	}
	if (NormalizedProperty == TEXT("render_transform.shear_y") || NormalizedProperty == TEXT("shear_y"))
	{
		OutPropertyKind = EUnrealMCPUMGAnimationPropertyKind::ShearY;
		OutCanonicalName = TEXT("render_transform.shear_y");
		return true;
	}
	if (NormalizedProperty == TEXT("render_transform.rotation") || NormalizedProperty == TEXT("render_transform.angle") ||
		NormalizedProperty == TEXT("rotation") || NormalizedProperty == TEXT("angle"))
	{
		OutPropertyKind = EUnrealMCPUMGAnimationPropertyKind::Rotation;
		OutCanonicalName = TEXT("render_transform.rotation");
		return true;
	}

	return false;
}

static UCanvasPanel* UnrealMCPUMGGetOrCreateRootCanvas(UWidgetBlueprint* WidgetBlueprint)
{
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return nullptr;
	}

	if (!WidgetBlueprint->WidgetTree->RootWidget)
	{
		UCanvasPanel* RootCanvas = WidgetBlueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetBlueprint->WidgetTree->RootWidget = RootCanvas;
		return RootCanvas;
	}

	return Cast<UCanvasPanel>(WidgetBlueprint->WidgetTree->RootWidget);
}

static FVector2D UnrealMCPUMGGetVector2DField(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName, const FVector2D& DefaultValue)
{
	if (!Params.IsValid() || !Params->HasField(FieldName))
	{
		return DefaultValue;
	}

	const TArray<TSharedPtr<FJsonValue>>* ValueArray = nullptr;
	if (!Params->TryGetArrayField(FieldName, ValueArray) || !ValueArray || ValueArray->Num() < 2)
	{
		return DefaultValue;
	}

	return FVector2D((*ValueArray)[0]->AsNumber(), (*ValueArray)[1]->AsNumber());
}

static FLinearColor UnrealMCPUMGGetColorField(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName, const FLinearColor& DefaultValue)
{
	if (!Params.IsValid() || !Params->HasField(FieldName))
	{
		return DefaultValue;
	}

	const TArray<TSharedPtr<FJsonValue>>* ValueArray = nullptr;
	if (!Params->TryGetArrayField(FieldName, ValueArray) || !ValueArray || ValueArray->Num() < 4)
	{
		return DefaultValue;
	}

	return FLinearColor(
		(*ValueArray)[0]->AsNumber(),
		(*ValueArray)[1]->AsNumber(),
		(*ValueArray)[2]->AsNumber(),
		(*ValueArray)[3]->AsNumber());
}

static void UnrealMCPUMGApplyCanvasSlotLayout(UCanvasPanelSlot* PanelSlot, const TSharedPtr<FJsonObject>& Params)
{
	if (!PanelSlot)
	{
		return;
	}

	PanelSlot->SetPosition(UnrealMCPUMGGetVector2DField(Params, TEXT("position"), FVector2D::ZeroVector));
	if (Params.IsValid() && Params->HasField(TEXT("size")))
	{
		PanelSlot->SetSize(UnrealMCPUMGGetVector2DField(Params, TEXT("size"), FVector2D(200.0f, 50.0f)));
	}
}

static bool UnrealMCPUMGCompileAndSave(UWidgetBlueprint* WidgetBlueprint)
{
	if (!WidgetBlueprint)
	{
		return false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	return UEditorAssetLibrary::SaveAsset(FPackageName::ObjectPathToPackageName(WidgetBlueprint->GetPathName()), false);
}

static TSharedPtr<FJsonObject> UnrealMCPUMGCreateWidgetAddedResponse(
	const FString& BlueprintName,
	const FString& WidgetName,
	const FString& WidgetType)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
	Response->SetStringField(TEXT("widget_name"), WidgetName);
	Response->SetStringField(TEXT("widget_type"), WidgetType);
	return Response;
}

static bool UnrealMCPUMGResolveBlueprintAndWidgetName(
	const TSharedPtr<FJsonObject>& Params,
	const TCHAR* LegacyWidgetField,
	FString& OutBlueprintName,
	FString& OutWidgetName,
	FString& OutErrorMessage);

template<typename WidgetType, typename ConfigureWidgetFunc>
static TSharedPtr<FJsonObject> UnrealMCPUMGAddWidgetToRootCanvas(
	const TSharedPtr<FJsonObject>& Params,
	const TCHAR* LegacyWidgetField,
	const TCHAR* WidgetTypeName,
	ConfigureWidgetFunc&& ConfigureWidgetFuncBody)
{
	FString BlueprintName;
	FString WidgetName;
	FString ErrorMessage;
	if (!UnrealMCPUMGResolveBlueprintAndWidgetName(Params, LegacyWidgetField, BlueprintName, WidgetName, ErrorMessage))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
	}

	UWidgetBlueprint* WidgetBlueprint = UnrealMCPUMGFindWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *BlueprintName));
	}

	WidgetType* Widget = WidgetBlueprint->WidgetTree->ConstructWidget<WidgetType>(WidgetType::StaticClass(), *WidgetName);
	if (!Widget)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to create %s widget"), WidgetTypeName));
	}

	ConfigureWidgetFuncBody(Widget, BlueprintName, WidgetName);

	UCanvasPanel* RootCanvas = UnrealMCPUMGGetOrCreateRootCanvas(WidgetBlueprint);
	if (!RootCanvas)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Root Canvas Panel not found"));
	}

	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Widget);
	UnrealMCPUMGApplyCanvasSlotLayout(PanelSlot, Params);
	UnrealMCPUMGCompileAndSave(WidgetBlueprint);
	return UnrealMCPUMGCreateWidgetAddedResponse(BlueprintName, WidgetName, WidgetTypeName);
}

static bool UnrealMCPUMGResolveBlueprintAndWidgetName(
	const TSharedPtr<FJsonObject>& Params,
	const TCHAR* LegacyWidgetField,
	FString& OutBlueprintName,
	FString& OutWidgetName,
	FString& OutErrorMessage)
{
	if (!Params.IsValid())
	{
		OutErrorMessage = TEXT("Missing params");
		return false;
	}

	if (Params->HasTypedField<EJson::String>(TEXT("blueprint_name")))
	{
		if (!Params->TryGetStringField(TEXT("blueprint_name"), OutBlueprintName) || OutBlueprintName.IsEmpty())
		{
			OutErrorMessage = TEXT("Missing 'blueprint_name' parameter");
			return false;
		}

		if (!UnrealMCPUMGTryGetStringField(Params, {TEXT("widget_name"), LegacyWidgetField}, OutWidgetName))
		{
			OutErrorMessage = FString::Printf(TEXT("Missing '%s' parameter"), LegacyWidgetField);
			return false;
		}

		return true;
	}

	if (!Params->TryGetStringField(TEXT("widget_name"), OutBlueprintName) || OutBlueprintName.IsEmpty())
	{
		OutErrorMessage = TEXT("Missing 'blueprint_name' parameter");
		return false;
	}

	if (!Params->TryGetStringField(LegacyWidgetField, OutWidgetName) || OutWidgetName.IsEmpty())
	{
		OutErrorMessage = FString::Printf(TEXT("Missing '%s' parameter"), LegacyWidgetField);
		return false;
	}

	return true;
}

static bool UnrealMCPUMGResolveBindingConfig(const FString& BindingType, FName& OutPropertyName, FEdGraphPinType& OutPinType, FString& OutErrorMessage)
{
	const FString NormalizedType = BindingType.TrimStartAndEnd();
	if (NormalizedType.IsEmpty() || NormalizedType.Equals(TEXT("Text"), ESearchCase::IgnoreCase))
	{
		OutPropertyName = TEXT("Text");
		OutPinType = FEdGraphPinType(UEdGraphSchema_K2::PC_Text, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType());
		return true;
	}

	if (NormalizedType.Equals(TEXT("Visibility"), ESearchCase::IgnoreCase))
	{
		OutPropertyName = TEXT("Visibility");
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		OutPinType.PinSubCategoryObject = StaticEnum<ESlateVisibility>();
		return true;
	}

	if (NormalizedType.Equals(TEXT("ColorAndOpacity"), ESearchCase::IgnoreCase))
	{
		OutPropertyName = TEXT("ColorAndOpacity");
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FSlateColor>::Get();
		return true;
	}

	if (NormalizedType.Equals(TEXT("ShadowColorAndOpacity"), ESearchCase::IgnoreCase) ||
		NormalizedType.Equals(TEXT("ShadowColor"), ESearchCase::IgnoreCase))
	{
		OutPropertyName = TEXT("ShadowColorAndOpacity");
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();
		return true;
	}

	if (NormalizedType.Equals(TEXT("ToolTipText"), ESearchCase::IgnoreCase))
	{
		OutPropertyName = TEXT("ToolTipText");
		OutPinType = FEdGraphPinType(UEdGraphSchema_K2::PC_Text, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType());
		return true;
	}

	if (NormalizedType.Equals(TEXT("IsEnabled"), ESearchCase::IgnoreCase) ||
		NormalizedType.Equals(TEXT("Enabled"), ESearchCase::IgnoreCase) ||
		NormalizedType.Equals(TEXT("bIsEnabled"), ESearchCase::IgnoreCase))
	{
		OutPropertyName = TEXT("bIsEnabled");
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		return true;
	}

	OutErrorMessage = FString::Printf(
		TEXT("Unsupported 'binding_type': %s，当前支持 Text、Visibility、ColorAndOpacity、ShadowColorAndOpacity、ToolTipText、IsEnabled"),
		*BindingType);
	return false;
}

/**
 * @brief 构造函数。
 */
FUnrealMCPUMGCommands::FUnrealMCPUMGCommands()
{
}

/**
 * @brief 分发 UMG 命令到具体处理函数。
 * @param [in] CommandName 命令名称。
 * @param [in] Params 命令参数。
 * @return TSharedPtr<FJsonObject> 执行结果或错误信息。
 */
TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleCommand(const FString& CommandName, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandName == TEXT("create_umg_widget_blueprint"))
	{
		return HandleCreateUMGWidgetBlueprint(Params);
	}
	else if (CommandName == TEXT("add_text_block_to_widget"))
	{
		return HandleAddTextBlockToWidget(Params);
	}
	else if (CommandName == TEXT("add_widget_to_viewport"))
	{
		return HandleAddWidgetToViewport(Params);
	}
	else if (CommandName == TEXT("remove_widget_from_viewport"))
	{
		return HandleRemoveWidgetFromViewport(Params);
	}
	else if (CommandName == TEXT("add_button_to_widget"))
	{
		return HandleAddButtonToWidget(Params);
	}
	else if (CommandName == TEXT("bind_widget_event"))
	{
		return HandleBindWidgetEvent(Params);
	}
	else if (CommandName == TEXT("set_text_block_binding"))
	{
		return HandleSetTextBlockBinding(Params);
	}
	else if (CommandName == TEXT("bind_widget_property"))
	{
		return HandleBindWidgetProperty(Params);
	}
	else if (CommandName == TEXT("add_image_to_widget"))
	{
		return HandleAddImageToWidget(Params);
	}
	else if (CommandName == TEXT("add_border_to_widget"))
	{
		return HandleAddBorderToWidget(Params);
	}
	else if (CommandName == TEXT("add_canvas_panel_to_widget"))
	{
		return HandleAddCanvasPanelToWidget(Params);
	}
	else if (CommandName == TEXT("add_horizontal_box_to_widget"))
	{
		return HandleAddHorizontalBoxToWidget(Params);
	}
	else if (CommandName == TEXT("add_vertical_box_to_widget"))
	{
		return HandleAddVerticalBoxToWidget(Params);
	}
	else if (CommandName == TEXT("add_overlay_to_widget"))
	{
		return HandleAddOverlayToWidget(Params);
	}
	else if (CommandName == TEXT("add_scroll_box_to_widget"))
	{
		return HandleAddScrollBoxToWidget(Params);
	}
	else if (CommandName == TEXT("add_size_box_to_widget"))
	{
		return HandleAddSizeBoxToWidget(Params);
	}
	else if (CommandName == TEXT("add_spacer_to_widget"))
	{
		return HandleAddSpacerToWidget(Params);
	}
	else if (CommandName == TEXT("add_progress_bar_to_widget"))
	{
		return HandleAddProgressBarToWidget(Params);
	}
	else if (CommandName == TEXT("add_slider_to_widget"))
	{
		return HandleAddSliderToWidget(Params);
	}
	else if (CommandName == TEXT("add_check_box_to_widget"))
	{
		return HandleAddCheckBoxToWidget(Params);
	}
	else if (CommandName == TEXT("add_editable_text_to_widget"))
	{
		return HandleAddEditableTextToWidget(Params);
	}
	else if (CommandName == TEXT("add_rich_text_to_widget"))
	{
		return HandleAddRichTextToWidget(Params);
	}
	else if (CommandName == TEXT("add_multi_line_text_to_widget"))
	{
		return HandleAddMultiLineTextToWidget(Params);
	}
	else if (CommandName == TEXT("add_named_slot_to_widget"))
	{
		return HandleAddNamedSlotToWidget(Params);
	}
	else if (CommandName == TEXT("add_list_view_to_widget"))
	{
		return HandleAddListViewToWidget(Params);
	}
	else if (CommandName == TEXT("add_tile_view_to_widget"))
	{
		return HandleAddTileViewToWidget(Params);
	}
	else if (CommandName == TEXT("add_tree_view_to_widget"))
	{
		return HandleAddTreeViewToWidget(Params);
	}
	else if (CommandName == TEXT("remove_widget_from_blueprint"))
	{
		return HandleRemoveWidgetFromBlueprint(Params);
	}
	else if (CommandName == TEXT("set_widget_slot_layout"))
	{
		return HandleSetWidgetSlotLayout(Params);
	}
	else if (CommandName == TEXT("set_widget_visibility"))
	{
		return HandleSetWidgetVisibility(Params);
	}
	else if (CommandName == TEXT("set_widget_style"))
	{
		return HandleSetWidgetStyle(Params);
	}
	else if (CommandName == TEXT("set_widget_brush"))
	{
		return HandleSetWidgetBrush(Params);
	}
	else if (CommandName == TEXT("create_widget_animation"))
	{
		return HandleCreateWidgetAnimation(Params);
	}
	else if (CommandName == TEXT("add_widget_animation_keyframe"))
	{
		return HandleAddWidgetAnimationKeyframe(Params);
	}
	else if (CommandName == TEXT("open_widget_blueprint_editor"))
	{
		return HandleOpenWidgetBlueprintEditor(Params);
	}

	return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown UMG command: %s"), *CommandName));
}

/**
 * @brief 创建 Widget Blueprint 资产。
 * @param [in] Params 创建参数。
 * @return TSharedPtr<FJsonObject> 创建结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleCreateUMGWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("create_umg_widget_blueprint"),
		Params);
}

/**
 * @brief 向 Widget Blueprint 添加 TextBlock。
 * @param [in] Params 控件参数。
 * @return TSharedPtr<FJsonObject> 添加结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddTextBlockToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_text_block_to_widget"),
		Params);
}

/**
 * @brief 将 Widget 添加到游戏视口。
 * @param [in] Params 视口添加参数。
 * @return TSharedPtr<FJsonObject> 添加结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddWidgetToViewport(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_widget_to_viewport"),
		Params);
}

/**
 * @brief 从游戏视口移除 Widget 实例。
 * @param [in] Params 移除参数。
 * @return TSharedPtr<FJsonObject> 移除结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleRemoveWidgetFromViewport(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("remove_widget_from_viewport"),
		Params);
}

/**
 * @brief 向 Widget Blueprint 添加 Button。
 * @param [in] Params 按钮参数。
 * @return TSharedPtr<FJsonObject> 添加结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddButtonToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_button_to_widget"),
		Params);
}

/**
 * @brief 绑定控件事件到蓝图函数。
 * @param [in] Params 事件绑定参数。
 * @return TSharedPtr<FJsonObject> 绑定结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleBindWidgetEvent(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	FString WidgetName;
	FString ErrorMessage;
	if (!UnrealMCPUMGResolveBlueprintAndWidgetName(Params, TEXT("widget_component_name"), BlueprintName, WidgetName, ErrorMessage))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
	}

	FString EventName;
	if (!Params->TryGetStringField(TEXT("event_name"), EventName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing event_name parameter"));
	}

	FString FunctionName;
	Params->TryGetStringField(TEXT("function_name"), FunctionName);

	UWidgetBlueprint* WidgetBlueprint = UnrealMCPUMGFindWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintName));
	}

	UWidget* Widget = WidgetBlueprint->WidgetTree ? WidgetBlueprint->WidgetTree->FindWidget(*WidgetName) : nullptr;
	if (!Widget)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to find widget: %s"), *WidgetName));
	}

	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	FObjectProperty* VariableProperty = FindFProperty<FObjectProperty>(WidgetBlueprint->SkeletonGeneratedClass, FName(*WidgetName));
	if (!VariableProperty)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to find widget variable: %s"), *WidgetName));
	}

	UK2Node_ComponentBoundEvent* EventNode = const_cast<UK2Node_ComponentBoundEvent*>(
		FKismetEditorUtilities::FindBoundEventForComponent(WidgetBlueprint, FName(*EventName), VariableProperty->GetFName()));
	if (!EventNode)
	{
		FKismetEditorUtilities::CreateNewBoundEventForClass(
			Widget->GetClass(),
			FName(*EventName),
			WidgetBlueprint,
			VariableProperty
		);

		EventNode = const_cast<UK2Node_ComponentBoundEvent*>(
			FKismetEditorUtilities::FindBoundEventForComponent(WidgetBlueprint, FName(*EventName), VariableProperty->GetFName()));
	}

	if (!EventNode)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create event node"));
	}

	if (!FunctionName.IsEmpty())
	{
		EventNode->Modify();
		EventNode->CustomFunctionName = FName(*FunctionName);
		EventNode->ReconstructNode();
	}

	UnrealMCPUMGCompileAndSave(WidgetBlueprint);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
	Response->SetStringField(TEXT("widget_name"), WidgetName);
	Response->SetStringField(TEXT("event_name"), EventName);
	Response->SetStringField(TEXT("function_name"), EventNode->CustomFunctionName.ToString());
	return Response;
}

/**
 * @brief 配置 TextBlock 的属性绑定。
 * @param [in] Params 绑定参数。
 * @return TSharedPtr<FJsonObject> 配置结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetTextBlockBinding(const TSharedPtr<FJsonObject>& Params)
{
	return HandleBindWidgetProperty(Params);
}

/**
 * @brief 为任意已支持属性的控件配置属性绑定。
 * @param [in] Params 绑定参数。
 * @return TSharedPtr<FJsonObject> 配置结果。
 * @note 当前仍只支持 `UnrealMCPUMGResolveBindingConfig(...)` 已知的绑定类型，
 *       但不再强制目标控件必须是 `UTextBlock`。
 */
TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleBindWidgetProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	FString WidgetName;
	FString ErrorMessage;
	if (!UnrealMCPUMGResolveBlueprintAndWidgetName(Params, TEXT("child_widget_name"), BlueprintName, WidgetName, ErrorMessage))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
	}

	FString BindingName;
	if (!UnrealMCPUMGTryGetStringField(Params, {TEXT("binding_name"), TEXT("binding_property")}, BindingName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing binding_name parameter"));
	}

	FString BindingType = TEXT("Text");
	Params->TryGetStringField(TEXT("binding_type"), BindingType);

	UWidgetBlueprint* WidgetBlueprint = UnrealMCPUMGFindWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintName));
	}

	UWidget* TargetWidget = WidgetBlueprint->WidgetTree ? WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName)) : nullptr;
	if (!TargetWidget)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to find widget: %s"), *WidgetName));
	}

	FName BindingPropertyName;
	FEdGraphPinType BindingPinType;
	if (!UnrealMCPUMGResolveBindingConfig(BindingType, BindingPropertyName, BindingPinType, ErrorMessage))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
	}

	FProperty* TargetProperty = TargetWidget->GetClass()->FindPropertyByName(BindingPropertyName);
	if (!TargetProperty)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(
				TEXT("Widget '%s' of class '%s' does not support binding property '%s'"),
				*WidgetName,
				*TargetWidget->GetClass()->GetName(),
				*BindingPropertyName.ToString()));
	}

	if (FBlueprintEditorUtils::FindNewVariableIndex(WidgetBlueprint, FName(*BindingName)) == INDEX_NONE)
	{
		FBlueprintEditorUtils::AddMemberVariable(WidgetBlueprint, FName(*BindingName), BindingPinType);
	}

	FProperty* BindingProperty = WidgetBlueprint->SkeletonGeneratedClass
		? WidgetBlueprint->SkeletonGeneratedClass->FindPropertyByName(FName(*BindingName))
		: nullptr;
	if (!BindingProperty)
	{
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
		BindingProperty = WidgetBlueprint->SkeletonGeneratedClass
			? WidgetBlueprint->SkeletonGeneratedClass->FindPropertyByName(FName(*BindingName))
			: nullptr;
	}
	if (!BindingProperty)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to resolve binding property: %s"), *BindingName));
	}

	FDelegateEditorBinding Binding;
	Binding.ObjectName = WidgetName;
	Binding.PropertyName = BindingPropertyName;
	Binding.SourceProperty = BindingProperty->GetFName();
	Binding.SourcePath = FEditorPropertyPath(TArray<FFieldVariant>{FFieldVariant(BindingProperty)});
	Binding.Kind = EBindingKind::Property;
	UBlueprint::GetGuidFromClassByFieldName<FProperty>(
		WidgetBlueprint->SkeletonGeneratedClass,
		BindingProperty->GetFName(),
		Binding.MemberGuid);

	WidgetBlueprint->Modify();
	WidgetBlueprint->Bindings.Remove(Binding);
	WidgetBlueprint->Bindings.AddUnique(Binding);
	UnrealMCPUMGCompileAndSave(WidgetBlueprint);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
	Response->SetStringField(TEXT("widget_name"), WidgetName);
	Response->SetStringField(TEXT("widget_class"), TargetWidget->GetClass()->GetName());
	Response->SetStringField(TEXT("binding_type"), BindingPropertyName.ToString());
	Response->SetStringField(TEXT("binding_name"), BindingName);
	Response->SetStringField(TEXT("implementation"), TEXT("cpp"));
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddImageToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_image_to_widget"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddBorderToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_border_to_widget"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddCanvasPanelToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_canvas_panel_to_widget"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddHorizontalBoxToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_horizontal_box_to_widget"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddVerticalBoxToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_vertical_box_to_widget"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddOverlayToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_overlay_to_widget"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddScrollBoxToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_scroll_box_to_widget"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddSizeBoxToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_size_box_to_widget"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddSpacerToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_spacer_to_widget"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddProgressBarToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_progress_bar_to_widget"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddSliderToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_slider_to_widget"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddCheckBoxToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_check_box_to_widget"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddEditableTextToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_editable_text_to_widget"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddRichTextToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_rich_text_to_widget"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddMultiLineTextToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_multi_line_text_to_widget"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddNamedSlotToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_named_slot_to_widget"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddListViewToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_list_view_to_widget"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddTileViewToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_tile_view_to_widget"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddTreeViewToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("add_tree_view_to_widget"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleRemoveWidgetFromBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("remove_widget_from_blueprint"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetWidgetSlotLayout(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("set_widget_slot_layout"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetWidgetVisibility(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("set_widget_visibility"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetWidgetStyle(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("set_widget_style"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleSetWidgetBrush(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("set_widget_brush"),
		Params);
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleCreateWidgetAnimation(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!UnrealMCPUMGTryGetStringField(Params, {TEXT("blueprint_name"), TEXT("widget_name")}, BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
	}

	FString RequestedAnimationName;
	UnrealMCPUMGTryGetStringField(Params, {TEXT("animation_name"), TEXT("name")}, RequestedAnimationName);
	RequestedAnimationName = RequestedAnimationName.TrimStartAndEnd();

	double StartTime = 0.0;
	if (Params.IsValid() && Params->HasTypedField<EJson::Number>(TEXT("start_time")))
	{
		StartTime = Params->GetNumberField(TEXT("start_time"));
	}

	double EndTime = 1.0;
	if (Params.IsValid() && Params->HasTypedField<EJson::Number>(TEXT("end_time")))
	{
		EndTime = Params->GetNumberField(TEXT("end_time"));
	}

	int32 DisplayRate = 20;
	if (Params.IsValid() && Params->HasTypedField<EJson::Number>(TEXT("display_rate")))
	{
		DisplayRate = FMath::RoundToInt(Params->GetNumberField(TEXT("display_rate")));
	}

	if (EndTime <= StartTime)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'end_time' 必须大于 'start_time'"));
	}
	if (DisplayRate <= 0)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'display_rate' 必须大于 0"));
	}

	UWidgetBlueprint* WidgetBlueprint = UnrealMCPUMGFindWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintName));
	}

	FString FinalAnimationName = RequestedAnimationName;
	if (FinalAnimationName.IsEmpty())
	{
		const FString BaseName = TEXT("NewAnimation");
		FinalAnimationName = BaseName;
		int32 NameIndex = 1;
		while (!UnrealMCPUMGCanUseAnimationName(WidgetBlueprint, FName(*FinalAnimationName)))
		{
			FinalAnimationName = FString::Printf(TEXT("%s_%d"), *BaseName, NameIndex);
			++NameIndex;
		}
	}
	else if (!UnrealMCPUMGCanUseAnimationName(WidgetBlueprint, FName(*FinalAnimationName)))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget 动画名称已存在或与继承动画属性冲突: %s"), *FinalAnimationName));
	}

	const FName AnimationFName(*FinalAnimationName);
	UWidgetAnimation* NewAnimation = nullptr;
	UMovieScene* NewMovieScene = nullptr;
	{
		const FScopedTransaction Transaction(NSLOCTEXT("UnrealMCPUMGCommands", "CreateWidgetAnimation", "Create Widget Animation"));
		WidgetBlueprint->Modify();

		NewAnimation = NewObject<UWidgetAnimation>(WidgetBlueprint, AnimationFName, RF_Transactional);
		if (!NewAnimation)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("创建 WidgetAnimation 失败"));
		}

		NewAnimation->Modify();
		NewAnimation->SetDisplayLabel(FinalAnimationName);

		NewMovieScene = NewObject<UMovieScene>(NewAnimation, AnimationFName, RF_Transactional);
		if (!NewMovieScene)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("创建 WidgetAnimation 的 MovieScene 失败"));
		}

		NewMovieScene->Modify();
		NewAnimation->MovieScene = NewMovieScene;
		NewMovieScene->SetDisplayRate(FFrameRate(DisplayRate, 1));

		const FFrameTime InFrame = StartTime * NewMovieScene->GetTickResolution();
		const FFrameTime OutFrame = EndTime * NewMovieScene->GetTickResolution();
		NewMovieScene->SetPlaybackRange(TRange<FFrameNumber>(InFrame.FrameNumber, OutFrame.FrameNumber + 1));
		NewMovieScene->GetEditorData().WorkStart = StartTime;
		NewMovieScene->GetEditorData().WorkEnd = EndTime;

		WidgetBlueprint->Animations.Add(NewAnimation);
		WidgetBlueprint->OnVariableAdded(AnimationFName);
	}

	if (!UnrealMCPUMGCompileAndSave(WidgetBlueprint))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("保存 Widget Blueprint 失败: %s"), *WidgetBlueprint->GetName()));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("blueprint_name"), WidgetBlueprint->GetName());
	Response->SetStringField(TEXT("asset_path"), FPackageName::ObjectPathToPackageName(WidgetBlueprint->GetPathName()));
	Response->SetStringField(TEXT("animation_name"), FinalAnimationName);
	Response->SetStringField(TEXT("movie_scene_name"), NewMovieScene ? NewMovieScene->GetName() : FinalAnimationName);
	Response->SetNumberField(TEXT("start_time"), StartTime);
	Response->SetNumberField(TEXT("end_time"), EndTime);
	Response->SetNumberField(TEXT("display_rate"), DisplayRate);
	Response->SetStringField(TEXT("implementation"), TEXT("cpp"));
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddWidgetAnimationKeyframe(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!UnrealMCPUMGTryGetStringField(Params, {TEXT("blueprint_name"), TEXT("widget_name")}, BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
	}

	FString AnimationName;
	if (!UnrealMCPUMGTryGetStringField(Params, {TEXT("animation_name"), TEXT("name")}, AnimationName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'animation_name' parameter"));
	}

	FString TargetWidgetName;
	if (!UnrealMCPUMGTryGetStringField(Params, {TEXT("target_widget_name"), TEXT("child_widget_name"), TEXT("animated_widget_name")}, TargetWidgetName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_widget_name' parameter"));
	}

	FString PropertyName;
	if (!UnrealMCPUMGTryGetStringField(Params, {TEXT("property_name"), TEXT("property"), TEXT("track_property")}, PropertyName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
	}

	if (!Params.IsValid() || !Params->HasTypedField<EJson::Number>(TEXT("time")))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'time' parameter"));
	}

	const double KeyTimeSeconds = Params->GetNumberField(TEXT("time"));
	if (KeyTimeSeconds < 0.0)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'time' 不能小于 0"));
	}

	FString InterpolationName = TEXT("cubic");
	UnrealMCPUMGTryGetStringField(Params, {TEXT("interpolation")}, InterpolationName);

	EUnrealMCPUMGAnimationPropertyKind PropertyKind = EUnrealMCPUMGAnimationPropertyKind::RenderOpacity;
	FString CanonicalPropertyName;
	if (!UnrealMCPUMGResolveAnimationProperty(PropertyName, PropertyKind, CanonicalPropertyName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("不支持的 UMG 动画属性: %s"), *PropertyName));
	}

	UWidgetBlueprint* WidgetBlueprint = UnrealMCPUMGFindWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintName));
	}

	UWidgetAnimation* WidgetAnimation = UnrealMCPUMGFindWidgetAnimation(WidgetBlueprint, AnimationName);
	if (!WidgetAnimation || !WidgetAnimation->GetMovieScene())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Widget 动画不存在: %s"), *AnimationName));
	}

	if (!WidgetBlueprint->WidgetTree)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Widget Blueprint 缺少 WidgetTree"));
	}

	UWidget* TargetWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*TargetWidgetName));
	if (!TargetWidget)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("目标控件不存在: %s"), *TargetWidgetName));
	}

	UMovieScene* MovieScene = WidgetAnimation->GetMovieScene();
	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const FFrameNumber FrameNumber = (KeyTimeSeconds * TickResolution).RoundToFrame();

	const FScopedTransaction Transaction(NSLOCTEXT("UnrealMCPUMGCommands", "AddWidgetAnimationKeyframe", "Add Widget Animation Keyframe"));
	WidgetBlueprint->Modify();
	WidgetAnimation->Modify();
	MovieScene->Modify();

	const FGuid BindingGuid = UnrealMCPUMGEnsureAnimationBinding(WidgetBlueprint, WidgetAnimation, TargetWidget);
	if (!BindingGuid.IsValid())
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("创建 Widget 动画绑定失败"));
	}

	FString TrackClassName;
	TArray<FString> WrittenChannels;

	if (PropertyKind == EUnrealMCPUMGAnimationPropertyKind::RenderOpacity)
	{
		if (!Params->HasTypedField<EJson::Number>(TEXT("value")))
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'render_opacity' 需要数值型 'value'"));
		}

		const float FloatValue = static_cast<float>(Params->GetNumberField(TEXT("value")));
		UMovieSceneFloatSection* FloatSection = UnrealMCPUMGFindOrAddFloatSection(MovieScene, BindingGuid, TEXT("RenderOpacity"));
		if (!FloatSection)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("创建 RenderOpacity 动画轨道失败"));
		}

		FloatSection->Modify();
		UnrealMCPUMGExpandSectionRange(FloatSection, FrameNumber);
		FMovieSceneFloatChannel& Channel = FloatSection->GetChannel();
		UnrealMCPUMGUpdateOrAddFloatKey(Channel, FrameNumber, FloatValue, InterpolationName);

		TrackClassName = TEXT("MovieSceneFloatTrack");
		WrittenChannels.Add(TEXT("render_opacity"));
	}
	else
	{
		UMovieScene2DTransformSection* TransformSection = UnrealMCPUMGFindOrAddTransformSection(MovieScene, BindingGuid);
		if (!TransformSection)
		{
			return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("创建 RenderTransform 动画轨道失败"));
		}

		TransformSection->Modify();
		UnrealMCPUMGExpandSectionRange(TransformSection, FrameNumber);

		EMovieScene2DTransformChannel ChannelMask = EMovieScene2DTransformChannel::None;

		switch (PropertyKind)
		{
		case EUnrealMCPUMGAnimationPropertyKind::Translation:
		{
			TArray<double> Values;
			if (!UnrealMCPUMGTryGetNumericArrayField(Params, {TEXT("value")}, 2, Values))
			{
				return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'render_transform.translation' 需要长度为 2 的数组型 'value'"));
			}

			UnrealMCPUMGUpdateOrAddFloatKey(TransformSection->Translation[0], FrameNumber, static_cast<float>(Values[0]), InterpolationName);
			UnrealMCPUMGUpdateOrAddFloatKey(TransformSection->Translation[1], FrameNumber, static_cast<float>(Values[1]), InterpolationName);
			ChannelMask = EMovieScene2DTransformChannel::Translation;
			WrittenChannels.Add(TEXT("translation_x"));
			WrittenChannels.Add(TEXT("translation_y"));
			break;
		}
		case EUnrealMCPUMGAnimationPropertyKind::TranslationX:
		{
			if (!Params->HasTypedField<EJson::Number>(TEXT("value")))
			{
				return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'render_transform.translation_x' 需要数值型 'value'"));
			}
			UnrealMCPUMGUpdateOrAddFloatKey(TransformSection->Translation[0], FrameNumber, static_cast<float>(Params->GetNumberField(TEXT("value"))), InterpolationName);
			ChannelMask = EMovieScene2DTransformChannel::TranslationX;
			WrittenChannels.Add(TEXT("translation_x"));
			break;
		}
		case EUnrealMCPUMGAnimationPropertyKind::TranslationY:
		{
			if (!Params->HasTypedField<EJson::Number>(TEXT("value")))
			{
				return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'render_transform.translation_y' 需要数值型 'value'"));
			}
			UnrealMCPUMGUpdateOrAddFloatKey(TransformSection->Translation[1], FrameNumber, static_cast<float>(Params->GetNumberField(TEXT("value"))), InterpolationName);
			ChannelMask = EMovieScene2DTransformChannel::TranslationY;
			WrittenChannels.Add(TEXT("translation_y"));
			break;
		}
		case EUnrealMCPUMGAnimationPropertyKind::Scale:
		{
			TArray<double> Values;
			if (!UnrealMCPUMGTryGetNumericArrayField(Params, {TEXT("value")}, 2, Values))
			{
				return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'render_transform.scale' 需要长度为 2 的数组型 'value'"));
			}

			UnrealMCPUMGUpdateOrAddFloatKey(TransformSection->Scale[0], FrameNumber, static_cast<float>(Values[0]), InterpolationName);
			UnrealMCPUMGUpdateOrAddFloatKey(TransformSection->Scale[1], FrameNumber, static_cast<float>(Values[1]), InterpolationName);
			ChannelMask = EMovieScene2DTransformChannel::Scale;
			WrittenChannels.Add(TEXT("scale_x"));
			WrittenChannels.Add(TEXT("scale_y"));
			break;
		}
		case EUnrealMCPUMGAnimationPropertyKind::ScaleX:
		{
			if (!Params->HasTypedField<EJson::Number>(TEXT("value")))
			{
				return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'render_transform.scale_x' 需要数值型 'value'"));
			}
			UnrealMCPUMGUpdateOrAddFloatKey(TransformSection->Scale[0], FrameNumber, static_cast<float>(Params->GetNumberField(TEXT("value"))), InterpolationName);
			ChannelMask = EMovieScene2DTransformChannel::ScaleX;
			WrittenChannels.Add(TEXT("scale_x"));
			break;
		}
		case EUnrealMCPUMGAnimationPropertyKind::ScaleY:
		{
			if (!Params->HasTypedField<EJson::Number>(TEXT("value")))
			{
				return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'render_transform.scale_y' 需要数值型 'value'"));
			}
			UnrealMCPUMGUpdateOrAddFloatKey(TransformSection->Scale[1], FrameNumber, static_cast<float>(Params->GetNumberField(TEXT("value"))), InterpolationName);
			ChannelMask = EMovieScene2DTransformChannel::ScaleY;
			WrittenChannels.Add(TEXT("scale_y"));
			break;
		}
		case EUnrealMCPUMGAnimationPropertyKind::Shear:
		{
			TArray<double> Values;
			if (!UnrealMCPUMGTryGetNumericArrayField(Params, {TEXT("value")}, 2, Values))
			{
				return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'render_transform.shear' 需要长度为 2 的数组型 'value'"));
			}

			UnrealMCPUMGUpdateOrAddFloatKey(TransformSection->Shear[0], FrameNumber, static_cast<float>(Values[0]), InterpolationName);
			UnrealMCPUMGUpdateOrAddFloatKey(TransformSection->Shear[1], FrameNumber, static_cast<float>(Values[1]), InterpolationName);
			ChannelMask = EMovieScene2DTransformChannel::Shear;
			WrittenChannels.Add(TEXT("shear_x"));
			WrittenChannels.Add(TEXT("shear_y"));
			break;
		}
		case EUnrealMCPUMGAnimationPropertyKind::ShearX:
		{
			if (!Params->HasTypedField<EJson::Number>(TEXT("value")))
			{
				return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'render_transform.shear_x' 需要数值型 'value'"));
			}
			UnrealMCPUMGUpdateOrAddFloatKey(TransformSection->Shear[0], FrameNumber, static_cast<float>(Params->GetNumberField(TEXT("value"))), InterpolationName);
			ChannelMask = EMovieScene2DTransformChannel::ShearX;
			WrittenChannels.Add(TEXT("shear_x"));
			break;
		}
		case EUnrealMCPUMGAnimationPropertyKind::ShearY:
		{
			if (!Params->HasTypedField<EJson::Number>(TEXT("value")))
			{
				return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'render_transform.shear_y' 需要数值型 'value'"));
			}
			UnrealMCPUMGUpdateOrAddFloatKey(TransformSection->Shear[1], FrameNumber, static_cast<float>(Params->GetNumberField(TEXT("value"))), InterpolationName);
			ChannelMask = EMovieScene2DTransformChannel::ShearY;
			WrittenChannels.Add(TEXT("shear_y"));
			break;
		}
		case EUnrealMCPUMGAnimationPropertyKind::Rotation:
		{
			if (!Params->HasTypedField<EJson::Number>(TEXT("value")))
			{
				return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'render_transform.rotation' 需要数值型 'value'"));
			}
			UnrealMCPUMGUpdateOrAddFloatKey(TransformSection->Rotation, FrameNumber, static_cast<float>(Params->GetNumberField(TEXT("value"))), InterpolationName);
			ChannelMask = EMovieScene2DTransformChannel::Rotation;
			WrittenChannels.Add(TEXT("rotation"));
			break;
		}
		default:
			return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("当前属性类型尚未实现"));
		}

		const EMovieScene2DTransformChannel CombinedChannels = TransformSection->GetMask().GetChannels() | ChannelMask;
		TransformSection->SetMask(FMovieScene2DTransformMask(CombinedChannels));
		TrackClassName = TEXT("MovieScene2DTransformTrack");
	}

	if (!UnrealMCPUMGCompileAndSave(WidgetBlueprint))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("保存 Widget Blueprint 失败: %s"), *WidgetBlueprint->GetName()));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("blueprint_name"), WidgetBlueprint->GetName());
	Response->SetStringField(TEXT("animation_name"), WidgetAnimation->GetName());
	Response->SetStringField(TEXT("target_widget_name"), TargetWidget->GetName());
	Response->SetStringField(TEXT("property_name"), CanonicalPropertyName);
	Response->SetNumberField(TEXT("time"), KeyTimeSeconds);
	Response->SetNumberField(TEXT("frame_number"), FrameNumber.Value);
	Response->SetStringField(TEXT("interpolation"), InterpolationName);
	Response->SetStringField(TEXT("track_class"), TrackClassName);
	Response->SetStringField(TEXT("implementation"), TEXT("cpp"));

	TArray<TSharedPtr<FJsonValue>> ChannelValues;
	for (const FString& ChannelName : WrittenChannels)
	{
		ChannelValues.Add(MakeShared<FJsonValueString>(ChannelName));
	}
	Response->SetArrayField(TEXT("written_channels"), ChannelValues);
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleOpenWidgetBlueprintEditor(const TSharedPtr<FJsonObject>& Params)
{
	return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
		TEXT("commands.umg.umg_commands"),
		TEXT("handle_umg_command"),
		TEXT("open_widget_blueprint_editor"),
		Params);
}
