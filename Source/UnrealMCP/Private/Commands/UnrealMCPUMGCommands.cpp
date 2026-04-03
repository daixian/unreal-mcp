/**
 * @file UnrealMCPUMGCommands.cpp
 * @brief UMG 命令处理实现，负责 Widget Blueprint 的创建与编辑。
 */
#include "Commands/UnrealMCPUMGCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
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
#include "Styling/SlateColor.h"
#include "Subsystems/AssetEditorSubsystem.h"
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

static UWorld* UnrealMCPUMGGetPlayWorld()
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
	FString BlueprintName;
	if (!UnrealMCPUMGTryGetStringField(Params, {TEXT("name"), TEXT("widget_name")}, BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	FString ParentClassName = TEXT("UserWidget");
	if (Params.IsValid())
	{
		Params->TryGetStringField(TEXT("parent_class"), ParentClassName);
	}

	UClass* ParentClass = UnrealMCPUMGResolveParentClass(ParentClassName);
	if (!ParentClass)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Invalid 'parent_class': %s"), *ParentClassName));
	}

	FString PackagePath = TEXT("/Game/Widgets");
	if (Params.IsValid())
	{
		Params->TryGetStringField(TEXT("path"), PackagePath);
	}
	PackagePath = UnrealMCPUMGNormalizePackagePath(PackagePath);

	const FString AssetName = BlueprintName;
	const FString FullPath = PackagePath + TEXT("/") + AssetName;

	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' already exists"), *BlueprintName));
	}

	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
	}

	UBlueprint* NewBlueprint = FKismetEditorUtilities::CreateBlueprint(
		ParentClass,
		Package,
		FName(*AssetName),
		BPTYPE_Normal,
		UWidgetBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		FName("CreateUMGWidget")
	);

	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(NewBlueprint);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Widget Blueprint"));
	}

	if (!UnrealMCPUMGGetOrCreateRootCanvas(WidgetBlueprint))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create root canvas"));
	}

	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(WidgetBlueprint);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	UEditorAssetLibrary::SaveAsset(FullPath, false);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("name"), BlueprintName);
	ResultObj->SetStringField(TEXT("path"), FullPath);
	ResultObj->SetStringField(TEXT("parent_class"), ParentClass->GetName());
	return ResultObj;
}

/**
 * @brief 向 Widget Blueprint 添加 TextBlock。
 * @param [in] Params 控件参数。
 * @return TSharedPtr<FJsonObject> 添加结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddTextBlockToWidget(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	FString WidgetName;
	FString ErrorMessage;
	if (!UnrealMCPUMGResolveBlueprintAndWidgetName(Params, TEXT("text_block_name"), BlueprintName, WidgetName, ErrorMessage))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
	}

	UWidgetBlueprint* WidgetBlueprint = UnrealMCPUMGFindWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *BlueprintName));
	}

	FString InitialText = TEXT("New Text Block");
	Params->TryGetStringField(TEXT("text"), InitialText);

	UTextBlock* TextBlock = WidgetBlueprint->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *WidgetName);
	if (!TextBlock)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Text Block widget"));
	}

	TextBlock->SetText(FText::FromString(InitialText));
	const int32 FontSize = Params->HasField(TEXT("font_size")) ? static_cast<int32>(Params->GetNumberField(TEXT("font_size"))) : 12;
	FSlateFontInfo FontInfo = TextBlock->GetFont();
	FontInfo.Size = FontSize;
	TextBlock->SetFont(FontInfo);
	TextBlock->SetColorAndOpacity(FSlateColor(UnrealMCPUMGGetColorField(Params, TEXT("color"), FLinearColor::White)));

	UCanvasPanel* RootCanvas = UnrealMCPUMGGetOrCreateRootCanvas(WidgetBlueprint);
	if (!RootCanvas)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Root Canvas Panel not found"));
	}

	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(TextBlock);
	UnrealMCPUMGApplyCanvasSlotLayout(PanelSlot, Params);

	UnrealMCPUMGCompileAndSave(WidgetBlueprint);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("widget_name"), WidgetName);
	ResultObj->SetStringField(TEXT("text"), InitialText);
	return ResultObj;
}

/**
 * @brief 将 Widget 添加到游戏视口。
 * @param [in] Params 视口添加参数。
 * @return TSharedPtr<FJsonObject> 添加结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddWidgetToViewport(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!UnrealMCPUMGTryGetStringField(Params, {TEXT("blueprint_name"), TEXT("widget_name")}, BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	UWidgetBlueprint* WidgetBlueprint = UnrealMCPUMGFindWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *BlueprintName));
	}

	int32 ZOrder = 0;
	Params->TryGetNumberField(TEXT("z_order"), ZOrder);

	UClass* WidgetClass = WidgetBlueprint->GeneratedClass;
	if (!WidgetClass)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get widget class"));
	}

	UWorld* PlayWorld = UnrealMCPUMGGetPlayWorld();
	if (!PlayWorld)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("PIE 未运行，无法将 Widget 添加到视口"));
	}

	UUserWidget* WidgetInstance = CreateWidget<UUserWidget>(PlayWorld, WidgetClass);
	if (!WidgetInstance)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create widget instance"));
	}

	WidgetInstance->AddToViewport(ZOrder);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
	ResultObj->SetStringField(TEXT("class_path"), WidgetClass->GetPathName());
	ResultObj->SetStringField(TEXT("instance_name"), WidgetInstance->GetName());
	ResultObj->SetStringField(TEXT("world_name"), PlayWorld->GetName());
	ResultObj->SetNumberField(TEXT("z_order"), ZOrder);
	return ResultObj;
}

/**
 * @brief 向 Widget Blueprint 添加 Button。
 * @param [in] Params 按钮参数。
 * @return TSharedPtr<FJsonObject> 添加结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddButtonToWidget(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	FString WidgetName;
	FString ErrorMessage;
	if (!UnrealMCPUMGResolveBlueprintAndWidgetName(Params, TEXT("button_name"), BlueprintName, WidgetName, ErrorMessage))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
	}

	FString ButtonText = TEXT("");
	Params->TryGetStringField(TEXT("text"), ButtonText);

	UWidgetBlueprint* WidgetBlueprint = UnrealMCPUMGFindWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintName));
	}

	UButton* Button = WidgetBlueprint->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *WidgetName);
	if (!Button)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Button widget"));
	}

	UTextBlock* ButtonTextBlock = WidgetBlueprint->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(WidgetName + TEXT("_Text")));
	if (ButtonTextBlock)
	{
		ButtonTextBlock->SetText(FText::FromString(ButtonText));
		const int32 FontSize = Params->HasField(TEXT("font_size")) ? static_cast<int32>(Params->GetNumberField(TEXT("font_size"))) : 12;
		FSlateFontInfo FontInfo = ButtonTextBlock->GetFont();
		FontInfo.Size = FontSize;
		ButtonTextBlock->SetFont(FontInfo);
		ButtonTextBlock->SetColorAndOpacity(FSlateColor(UnrealMCPUMGGetColorField(Params, TEXT("color"), FLinearColor::White)));
		Button->AddChild(ButtonTextBlock);
	}

	Button->SetColorAndOpacity(UnrealMCPUMGGetColorField(Params, TEXT("color"), FLinearColor::White));
	Button->SetBackgroundColor(UnrealMCPUMGGetColorField(Params, TEXT("background_color"), FLinearColor(0.1f, 0.1f, 0.1f, 1.0f)));

	UCanvasPanel* RootCanvas = UnrealMCPUMGGetOrCreateRootCanvas(WidgetBlueprint);
	if (!RootCanvas)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Root widget is not a Canvas Panel"));
	}

	UCanvasPanelSlot* ButtonSlot = RootCanvas->AddChildToCanvas(Button);
	UnrealMCPUMGApplyCanvasSlotLayout(ButtonSlot, Params);

	UnrealMCPUMGCompileAndSave(WidgetBlueprint);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
	Response->SetStringField(TEXT("widget_name"), WidgetName);
	Response->SetStringField(TEXT("text"), ButtonText);
	return Response;
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
	FString BlueprintName;
	FString WidgetName;
	FString ErrorMessage;
	if (!UnrealMCPUMGResolveBlueprintAndWidgetName(Params, TEXT("text_block_name"), BlueprintName, WidgetName, ErrorMessage))
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

	UTextBlock* TextBlock = Cast<UTextBlock>(WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName)));
	if (!TextBlock)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to find TextBlock widget: %s"), *WidgetName));
	}

	FName BindingPropertyName;
	FEdGraphPinType BindingPinType;
	if (!UnrealMCPUMGResolveBindingConfig(BindingType, BindingPropertyName, BindingPinType, ErrorMessage))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
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
	Response->SetStringField(TEXT("binding_type"), BindingPropertyName.ToString());
	Response->SetStringField(TEXT("binding_name"), BindingName);
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddImageToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return UnrealMCPUMGAddWidgetToRootCanvas<UImage>(
		Params,
		TEXT("image_name"),
		TEXT("Image"),
		[&Params](UImage* Image, const FString&, const FString&)
		{
			Image->SetColorAndOpacity(UnrealMCPUMGGetColorField(Params, TEXT("color"), FLinearColor::White));
		});
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddBorderToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return UnrealMCPUMGAddWidgetToRootCanvas<UBorder>(
		Params,
		TEXT("border_name"),
		TEXT("Border"),
		[&Params](UBorder* Border, const FString&, const FString&)
		{
			Border->SetBrushColor(UnrealMCPUMGGetColorField(Params, TEXT("brush_color"), FLinearColor::White));
			Border->SetContentColorAndOpacity(UnrealMCPUMGGetColorField(Params, TEXT("content_color"), FLinearColor::White));
		});
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddCanvasPanelToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return UnrealMCPUMGAddWidgetToRootCanvas<UCanvasPanel>(
		Params,
		TEXT("canvas_panel_name"),
		TEXT("CanvasPanel"),
		[](UCanvasPanel*, const FString&, const FString&) {});
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddHorizontalBoxToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return UnrealMCPUMGAddWidgetToRootCanvas<UHorizontalBox>(
		Params,
		TEXT("horizontal_box_name"),
		TEXT("HorizontalBox"),
		[](UHorizontalBox*, const FString&, const FString&) {});
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddVerticalBoxToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return UnrealMCPUMGAddWidgetToRootCanvas<UVerticalBox>(
		Params,
		TEXT("vertical_box_name"),
		TEXT("VerticalBox"),
		[](UVerticalBox*, const FString&, const FString&) {});
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddOverlayToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return UnrealMCPUMGAddWidgetToRootCanvas<UOverlay>(
		Params,
		TEXT("overlay_name"),
		TEXT("Overlay"),
		[](UOverlay*, const FString&, const FString&) {});
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddScrollBoxToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return UnrealMCPUMGAddWidgetToRootCanvas<UScrollBox>(
		Params,
		TEXT("scroll_box_name"),
		TEXT("ScrollBox"),
		[](UScrollBox*, const FString&, const FString&) {});
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddSizeBoxToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return UnrealMCPUMGAddWidgetToRootCanvas<USizeBox>(
		Params,
		TEXT("size_box_name"),
		TEXT("SizeBox"),
		[&Params](USizeBox* SizeBox, const FString&, const FString&)
		{
			const FVector2D RequestedSize = UnrealMCPUMGGetVector2DField(Params, TEXT("size"), FVector2D(200.0f, 50.0f));
			if (Params.IsValid() && Params->HasField(TEXT("width_override")))
			{
				SizeBox->SetWidthOverride(static_cast<float>(Params->GetNumberField(TEXT("width_override"))));
			}
			else
			{
				SizeBox->SetWidthOverride(RequestedSize.X);
			}

			if (Params.IsValid() && Params->HasField(TEXT("height_override")))
			{
				SizeBox->SetHeightOverride(static_cast<float>(Params->GetNumberField(TEXT("height_override"))));
			}
			else
			{
				SizeBox->SetHeightOverride(RequestedSize.Y);
			}
		});
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddSpacerToWidget(const TSharedPtr<FJsonObject>& Params)
{
	return UnrealMCPUMGAddWidgetToRootCanvas<USpacer>(
		Params,
		TEXT("spacer_name"),
		TEXT("Spacer"),
		[&Params](USpacer* Spacer, const FString&, const FString&)
		{
			Spacer->SetSize(UnrealMCPUMGGetVector2DField(Params, TEXT("size"), FVector2D(32.0f, 32.0f)));
		});
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddProgressBarToWidget(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	FString WidgetName;
	FString ErrorMessage;
	if (!UnrealMCPUMGResolveBlueprintAndWidgetName(Params, TEXT("progress_bar_name"), BlueprintName, WidgetName, ErrorMessage))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
	}

	UWidgetBlueprint* WidgetBlueprint = UnrealMCPUMGFindWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *BlueprintName));
	}

	UProgressBar* ProgressBar = WidgetBlueprint->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), *WidgetName);
	if (!ProgressBar)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Progress Bar widget"));
	}

	const float Percent = Params.IsValid() && Params->HasField(TEXT("percent"))
		? static_cast<float>(Params->GetNumberField(TEXT("percent")))
		: 0.5f;
	ProgressBar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
	ProgressBar->SetFillColorAndOpacity(UnrealMCPUMGGetColorField(Params, TEXT("fill_color"), FLinearColor::White));

	UCanvasPanel* RootCanvas = UnrealMCPUMGGetOrCreateRootCanvas(WidgetBlueprint);
	if (!RootCanvas)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Root Canvas Panel not found"));
	}

	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(ProgressBar);
	UnrealMCPUMGApplyCanvasSlotLayout(PanelSlot, Params);
	UnrealMCPUMGCompileAndSave(WidgetBlueprint);

	TSharedPtr<FJsonObject> Response = UnrealMCPUMGCreateWidgetAddedResponse(BlueprintName, WidgetName, TEXT("ProgressBar"));
	Response->SetNumberField(TEXT("percent"), ProgressBar->GetPercent());
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddSliderToWidget(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = UnrealMCPUMGAddWidgetToRootCanvas<USlider>(
		Params,
		TEXT("slider_name"),
		TEXT("Slider"),
		[&Params](USlider* Slider, const FString&, const FString&)
		{
			const float Value = Params.IsValid() && Params->HasField(TEXT("value"))
				? static_cast<float>(Params->GetNumberField(TEXT("value")))
				: 0.0f;
			Slider->SetValue(FMath::Clamp(Value, 0.0f, 1.0f));
		});
	if (Response.IsValid() && Response->GetBoolField(TEXT("success")))
	{
		Response->SetNumberField(
			TEXT("value"),
			Params.IsValid() && Params->HasField(TEXT("value"))
				? FMath::Clamp(static_cast<float>(Params->GetNumberField(TEXT("value"))), 0.0f, 1.0f)
				: 0.0f);
	}
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddCheckBoxToWidget(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = UnrealMCPUMGAddWidgetToRootCanvas<UCheckBox>(
		Params,
		TEXT("check_box_name"),
		TEXT("CheckBox"),
		[&Params](UCheckBox* CheckBox, const FString&, const FString&)
		{
			bool bIsChecked = false;
			if (Params.IsValid())
			{
				Params->TryGetBoolField(TEXT("is_checked"), bIsChecked);
			}
			CheckBox->SetIsChecked(bIsChecked);
		});
	if (Response.IsValid() && Response->GetBoolField(TEXT("success")))
	{
		bool bIsChecked = false;
		if (Params.IsValid())
		{
			Params->TryGetBoolField(TEXT("is_checked"), bIsChecked);
		}
		Response->SetBoolField(TEXT("is_checked"), bIsChecked);
	}
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddEditableTextToWidget(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	FString WidgetName;
	FString ErrorMessage;
	if (!UnrealMCPUMGResolveBlueprintAndWidgetName(Params, TEXT("editable_text_name"), BlueprintName, WidgetName, ErrorMessage))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
	}

	UWidgetBlueprint* WidgetBlueprint = UnrealMCPUMGFindWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *BlueprintName));
	}

	UEditableText* EditableText = WidgetBlueprint->WidgetTree->ConstructWidget<UEditableText>(UEditableText::StaticClass(), *WidgetName);
	if (!EditableText)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Editable Text widget"));
	}

	FString InitialText;
	Params->TryGetStringField(TEXT("text"), InitialText);
	EditableText->SetText(FText::FromString(InitialText));

	FString HintText;
	if (Params->TryGetStringField(TEXT("hint_text"), HintText))
	{
		EditableText->SetHintText(FText::FromString(HintText));
	}

	bool bIsReadOnly = false;
	if (Params->TryGetBoolField(TEXT("is_read_only"), bIsReadOnly))
	{
		EditableText->SetIsReadOnly(bIsReadOnly);
	}

	UCanvasPanel* RootCanvas = UnrealMCPUMGGetOrCreateRootCanvas(WidgetBlueprint);
	if (!RootCanvas)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Root Canvas Panel not found"));
	}

	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(EditableText);
	UnrealMCPUMGApplyCanvasSlotLayout(PanelSlot, Params);
	UnrealMCPUMGCompileAndSave(WidgetBlueprint);

	TSharedPtr<FJsonObject> Response = UnrealMCPUMGCreateWidgetAddedResponse(BlueprintName, WidgetName, TEXT("EditableText"));
	Response->SetStringField(TEXT("text"), InitialText);
	Response->SetBoolField(TEXT("is_read_only"), bIsReadOnly);
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddRichTextToWidget(const TSharedPtr<FJsonObject>& Params)
{
	FString InitialText;
	if (Params.IsValid())
	{
		Params->TryGetStringField(TEXT("text"), InitialText);
	}

	TSharedPtr<FJsonObject> Response = UnrealMCPUMGAddWidgetToRootCanvas<URichTextBlock>(
		Params,
		TEXT("rich_text_name"),
		TEXT("RichTextBlock"),
		[&Params, &InitialText](URichTextBlock* RichTextBlock, const FString&, const FString&)
		{
			RichTextBlock->SetText(FText::FromString(InitialText));
			RichTextBlock->SetDefaultColorAndOpacity(FSlateColor(UnrealMCPUMGGetColorField(Params, TEXT("color"), FLinearColor::White)));
		});
	if (Response.IsValid() && Response->GetBoolField(TEXT("success")))
	{
		Response->SetStringField(TEXT("text"), InitialText);
	}
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleAddMultiLineTextToWidget(const TSharedPtr<FJsonObject>& Params)
{
	FString InitialText;
	FString HintText;
	bool bIsReadOnly = false;
	if (Params.IsValid())
	{
		Params->TryGetStringField(TEXT("text"), InitialText);
		Params->TryGetStringField(TEXT("hint_text"), HintText);
		Params->TryGetBoolField(TEXT("is_read_only"), bIsReadOnly);
	}

	TSharedPtr<FJsonObject> Response = UnrealMCPUMGAddWidgetToRootCanvas<UMultiLineEditableTextBox>(
		Params,
		TEXT("multi_line_text_name"),
		TEXT("MultiLineEditableTextBox"),
		[&InitialText, &HintText, bIsReadOnly](UMultiLineEditableTextBox* MultiLineText, const FString&, const FString&)
		{
			MultiLineText->SetText(FText::FromString(InitialText));
			MultiLineText->SetHintText(FText::FromString(HintText));
			MultiLineText->SetIsReadOnly(bIsReadOnly);
		});
	if (Response.IsValid() && Response->GetBoolField(TEXT("success")))
	{
		Response->SetStringField(TEXT("text"), InitialText);
		Response->SetStringField(TEXT("hint_text"), HintText);
		Response->SetBoolField(TEXT("is_read_only"), bIsReadOnly);
	}
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPUMGCommands::HandleOpenWidgetBlueprintEditor(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintName;
	if (!UnrealMCPUMGTryGetStringField(Params, {TEXT("blueprint_name"), TEXT("widget_name")}, BlueprintName))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	UWidgetBlueprint* WidgetBlueprint = UnrealMCPUMGFindWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *BlueprintName));
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (!AssetEditorSubsystem)
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("AssetEditorSubsystem is unavailable"));
	}

	if (!AssetEditorSubsystem->OpenEditorForAsset(WidgetBlueprint))
	{
		return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to open Widget Blueprint editor: %s"), *BlueprintName));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("blueprint_name"), BlueprintName);
	Result->SetStringField(TEXT("asset_path"), FPackageName::ObjectPathToPackageName(WidgetBlueprint->GetPathName()));
	Result->SetStringField(TEXT("object_path"), WidgetBlueprint->GetPathName());
	return Result;
}
