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
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
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

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	UEditorAssetLibrary::SaveAsset(FPackageName::ObjectPathToPackageName(WidgetBlueprint->GetPathName()), false);

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

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	UEditorAssetLibrary::SaveAsset(FPackageName::ObjectPathToPackageName(WidgetBlueprint->GetPathName()), false);

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

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	UEditorAssetLibrary::SaveAsset(FPackageName::ObjectPathToPackageName(WidgetBlueprint->GetPathName()), false);

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
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	UEditorAssetLibrary::SaveAsset(FPackageName::ObjectPathToPackageName(WidgetBlueprint->GetPathName()), false);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
	Response->SetStringField(TEXT("widget_name"), WidgetName);
	Response->SetStringField(TEXT("binding_type"), BindingPropertyName.ToString());
	Response->SetStringField(TEXT("binding_name"), BindingName);
	return Response;
}
