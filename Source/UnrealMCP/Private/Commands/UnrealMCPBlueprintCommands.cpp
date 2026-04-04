/**
 * @file UnrealMCPBlueprintCommands.cpp
 * @brief Blueprint 资产与组件命令处理实现。
 */
#include "Commands/UnrealMCPBlueprintCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EditorUtilityActor.h"
#include "EditorUtilityBlueprint.h"
#include "EditorUtilityCamera.h"
#include "EditorUtilityObject.h"
#include "EditorUtilityWidget.h"
#include "Factories/BlueprintFactory.h"
#include "UObject/Interface.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/UObjectGlobals.h"

/**
 * @brief 解析 Blueprint 父类字符串到实际 UClass。
 * @param [in] ParentClassName 用户传入的父类名或类路径。
 * @param [out] OutParentClass 解析成功后的父类对象。
 * @param [out] OutErrorMessage 解析失败时的错误信息。
 * @return bool 是否解析成功。
 */
static bool UnrealMCPBlueprintResolveParentClass(
    const FString& ParentClassName,
    UClass*& OutParentClass,
    FString& OutErrorMessage)
{
    OutParentClass = nullptr;

    const FString TrimmedParentClassName = ParentClassName.TrimStartAndEnd();
    if (TrimmedParentClassName.IsEmpty())
    {
        OutParentClass = AActor::StaticClass();
        return true;
    }

    if (TrimmedParentClassName.Contains(TEXT(".")) || TrimmedParentClassName.StartsWith(TEXT("/Script/")))
    {
        OutParentClass = LoadObject<UClass>(nullptr, *TrimmedParentClassName);
        if (OutParentClass)
        {
            return true;
        }
    }

    TArray<FString> ClassCandidates;
    ClassCandidates.Add(TrimmedParentClassName);
    if (TrimmedParentClassName.StartsWith(TEXT("A")) || TrimmedParentClassName.StartsWith(TEXT("U")))
    {
        ClassCandidates.Add(TrimmedParentClassName.RightChop(1));
    }
    else
    {
        ClassCandidates.Add(TEXT("A") + TrimmedParentClassName);
        ClassCandidates.Add(TEXT("U") + TrimmedParentClassName);
    }

    for (const FString& Candidate : ClassCandidates)
    {
        OutParentClass = FindFirstObject<UClass>(*Candidate, EFindFirstObjectOptions::None);
        if (OutParentClass)
        {
            return true;
        }

        FString ClassBaseName = Candidate;
        if (ClassBaseName.StartsWith(TEXT("A")) || ClassBaseName.StartsWith(TEXT("U")))
        {
            ClassBaseName = ClassBaseName.RightChop(1);
        }

        const TArray<FString> ScriptModules = {
            TEXT("Engine"),
            TEXT("CoreUObject"),
            TEXT("UMG"),
            TEXT("Blutility"),
            FApp::GetProjectName()
        };

        for (const FString& ModuleName : ScriptModules)
        {
            if (ModuleName.IsEmpty())
            {
                continue;
            }

            const FString ScriptClassPath = FString::Printf(TEXT("/Script/%s.%s"), *ModuleName, *ClassBaseName);
            OutParentClass = LoadObject<UClass>(nullptr, *ScriptClassPath);
            if (OutParentClass)
            {
                return true;
            }
        }
    }

    OutErrorMessage = FString::Printf(TEXT("无法解析 Blueprint 父类 '%s'"), *TrimmedParentClassName);
    return false;
}

/**
 * @brief 解析 Blueprint 接口引用到实际接口类。
 * @param [in] InterfaceReference 用户传入的接口类名、类路径或 Blueprint Interface 资产路径。
 * @param [out] OutInterfaceClass 解析成功后的接口类。
 * @param [out] OutErrorMessage 解析失败时的错误信息。
 * @return bool 是否解析成功。
 */
static bool UnrealMCPBlueprintResolveInterfaceClass(
    const FString& InterfaceReference,
    UClass*& OutInterfaceClass,
    FString& OutErrorMessage)
{
    OutInterfaceClass = nullptr;

    const FString TrimmedReference = InterfaceReference.TrimStartAndEnd();
    if (TrimmedReference.IsEmpty())
    {
        OutErrorMessage = TEXT("缺少 'interface_class' 参数");
        return false;
    }

    if (UnrealMCPBlueprintResolveParentClass(TrimmedReference, OutInterfaceClass, OutErrorMessage) && OutInterfaceClass)
    {
        if (OutInterfaceClass->IsChildOf(UInterface::StaticClass()) || OutInterfaceClass->HasAnyClassFlags(CLASS_Interface))
        {
            return true;
        }
    }

    if (UBlueprint* InterfaceBlueprint = FUnrealMCPCommonUtils::FindBlueprint(TrimmedReference))
    {
        UClass* CandidateClass = InterfaceBlueprint->GeneratedClass;
        if (!CandidateClass)
        {
            CandidateClass = InterfaceBlueprint->SkeletonGeneratedClass;
        }

        if (CandidateClass && (CandidateClass->IsChildOf(UInterface::StaticClass()) || CandidateClass->HasAnyClassFlags(CLASS_Interface)))
        {
            OutInterfaceClass = CandidateClass;
            return true;
        }
    }

    OutErrorMessage = FString::Printf(TEXT("无法解析 Blueprint 接口 '%s'"), *TrimmedReference);
    return false;
}

/**
 * @brief 根据父类推导 Blueprint 类型与生成类类型。
 * @param [in] ParentClass Blueprint 父类。
 * @param [out] OutBlueprintClass 要创建的 Blueprint 资产类型。
 * @param [out] OutGeneratedClass Blueprint 生成类类型。
 * @param [out] OutBlueprintType Blueprint 逻辑类型。
 * @param [out] OutErrorMessage 推导失败时的错误信息。
 * @return bool 是否推导成功。
 * @note 这里显式拦截无效父类，避免工厂内部弹出模态对话框卡住 MCP 回包。
 */
static bool UnrealMCPBlueprintResolveCreationSpec(
    UClass* ParentClass,
    TSubclassOf<UBlueprint>& OutBlueprintClass,
    TSubclassOf<UBlueprintGeneratedClass>& OutGeneratedClass,
    EBlueprintType& OutBlueprintType,
    FString& OutErrorMessage)
{
    if (!ParentClass)
    {
        OutErrorMessage = TEXT("Blueprint 父类不能为空");
        return false;
    }

    if (ParentClass->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
    {
        OutErrorMessage = FString::Printf(TEXT("父类 '%s' 已废弃，不能用于创建新的 Blueprint"), *ParentClass->GetPathName());
        return false;
    }

    if (ParentClass->IsChildOf(UEditorUtilityWidget::StaticClass()))
    {
        OutErrorMessage = TEXT("EditorUtilityWidget 请使用 create_umg_widget_blueprint 创建，而不是 create_blueprint");
        return false;
    }

    OutBlueprintClass = UBlueprint::StaticClass();
    OutGeneratedClass = UBlueprintGeneratedClass::StaticClass();
    OutBlueprintType = BPTYPE_Normal;

    const FString ParentClassPath = ParentClass->GetPathName();
    const bool bIsEditorFunctionLibrary = ParentClassPath == TEXT("/Script/Blutility.EditorFunctionLibrary");
    if (bIsEditorFunctionLibrary)
    {
        OutBlueprintClass = UEditorUtilityBlueprint::StaticClass();
        OutBlueprintType = BPTYPE_FunctionLibrary;
        return true;
    }

    if (ParentClass->IsChildOf(UEditorUtilityObject::StaticClass()) ||
        ParentClass->IsChildOf(AEditorUtilityActor::StaticClass()) ||
        ParentClass->IsChildOf(AEditorUtilityCamera::StaticClass()))
    {
        OutBlueprintClass = UEditorUtilityBlueprint::StaticClass();
        return true;
    }

    if (ParentClass->IsChildOf(UBlueprintFunctionLibrary::StaticClass()))
    {
        OutBlueprintType = BPTYPE_FunctionLibrary;
    }

    if (!FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass))
    {
        OutErrorMessage = FString::Printf(TEXT("父类 '%s' 当前不允许创建 Blueprint"), *ParentClass->GetPathName());
        return false;
    }

    return true;
}

static bool UnrealMCPBlueprintDefaultValueToString(
    const FEdGraphPinType& PinType,
    const TSharedPtr<FJsonValue>& Value,
    FString& OutDefaultValue,
    FString& OutErrorMessage)
{
    if (!Value.IsValid())
    {
        OutErrorMessage = TEXT("Missing 'default_value' parameter");
        return false;
    }

    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
    {
        OutDefaultValue = Value->AsBool() ? TEXT("true") : TEXT("false");
        return true;
    }

    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
    {
        OutDefaultValue = FString::FromInt(static_cast<int32>(Value->AsNumber()));
        return true;
    }

    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Float)
    {
        OutDefaultValue = FString::SanitizeFloat(Value->AsNumber());
        return true;
    }

    if (PinType.PinCategory == UEdGraphSchema_K2::PC_String ||
        PinType.PinCategory == UEdGraphSchema_K2::PC_Name ||
        PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
    {
        OutDefaultValue = Value->AsString();
        return true;
    }

    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && PinType.PinSubCategoryObject == TBaseStructure<FVector>::Get())
    {
        const TArray<TSharedPtr<FJsonValue>>* ArrayValue = nullptr;
        if (!Value->TryGetArray(ArrayValue) || !ArrayValue || ArrayValue->Num() < 3)
        {
            OutErrorMessage = TEXT("Vector default value must be an array with 3 numbers");
            return false;
        }

        OutDefaultValue = FString::Printf(TEXT("(X=%s,Y=%s,Z=%s)"),
            *FString::SanitizeFloat((*ArrayValue)[0]->AsNumber()),
            *FString::SanitizeFloat((*ArrayValue)[1]->AsNumber()),
            *FString::SanitizeFloat((*ArrayValue)[2]->AsNumber()));
        return true;
    }

    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && PinType.PinSubCategoryObject == TBaseStructure<FRotator>::Get())
    {
        const TArray<TSharedPtr<FJsonValue>>* ArrayValue = nullptr;
        if (!Value->TryGetArray(ArrayValue) || !ArrayValue || ArrayValue->Num() < 3)
        {
            OutErrorMessage = TEXT("Rotator default value must be an array with 3 numbers");
            return false;
        }

        OutDefaultValue = FString::Printf(TEXT("(Pitch=%s,Yaw=%s,Roll=%s)"),
            *FString::SanitizeFloat((*ArrayValue)[0]->AsNumber()),
            *FString::SanitizeFloat((*ArrayValue)[1]->AsNumber()),
            *FString::SanitizeFloat((*ArrayValue)[2]->AsNumber()));
        return true;
    }

    OutErrorMessage = FString::Printf(TEXT("Unsupported variable type for default value: %s"), *PinType.PinCategory.ToString());
    return false;
}

/**
 * @brief 把 Blueprint 编译状态枚举转换为文本。
 * @param [in] Status Blueprint 当前状态。
 * @return FString 状态文本。
 */
static FString UnrealMCPBlueprintStatusToString(EBlueprintStatus Status)
{
    switch (Status)
    {
    case BS_Unknown:
        return TEXT("Unknown");
    case BS_Dirty:
        return TEXT("Dirty");
    case BS_Error:
        return TEXT("Error");
    case BS_UpToDate:
        return TEXT("UpToDate");
    case BS_BeingCreated:
        return TEXT("BeingCreated");
    case BS_UpToDateWithWarnings:
        return TEXT("UpToDateWithWarnings");
    default:
        return TEXT("Unknown");
    }
}

/**
 * @brief 把编译消息严重级别转换为稳定字符串。
 * @param [in] Severity 编译消息严重级别。
 * @return FString 严重级别文本。
 */
static FString UnrealMCPBlueprintCompilerSeverityToString(EMessageSeverity::Type Severity)
{
    switch (Severity)
    {
    case EMessageSeverity::Error:
        return TEXT("error");
    case EMessageSeverity::Warning:
        return TEXT("warning");
    case EMessageSeverity::PerformanceWarning:
        return TEXT("performance_warning");
    case EMessageSeverity::Info:
        return TEXT("info");
    default:
        return TEXT("unknown");
    }
}

/**
 * @brief 序列化编译日志消息，便于 MCP 返回错误与警告详情。
 * @param [in] Results Blueprint 编译日志。
 * @param [out] OutMessages 全量消息数组。
 * @param [out] OutErrors 错误消息数组。
 * @param [out] OutWarnings 警告消息数组。
 */
static void UnrealMCPBlueprintSerializeCompilerMessages(
    const FCompilerResultsLog& Results,
    TArray<TSharedPtr<FJsonValue>>& OutMessages,
    TArray<TSharedPtr<FJsonValue>>& OutErrors,
    TArray<TSharedPtr<FJsonValue>>& OutWarnings)
{
    int32 MessageIndex = 0;
    for (const TSharedRef<FTokenizedMessage>& Message : Results.Messages)
    {
        const FString MessageText = Message->ToText().ToString().TrimStartAndEnd();
        if (MessageText.IsEmpty())
        {
            continue;
        }

        const EMessageSeverity::Type Severity = Message->GetSeverity();
        TSharedPtr<FJsonObject> MessageObject = MakeShared<FJsonObject>();
        MessageObject->SetNumberField(TEXT("index"), MessageIndex++);
        MessageObject->SetStringField(TEXT("severity"), UnrealMCPBlueprintCompilerSeverityToString(Severity));
        MessageObject->SetStringField(TEXT("message"), MessageText);

        const TSharedPtr<FJsonValueObject> MessageValue = MakeShared<FJsonValueObject>(MessageObject);
        OutMessages.Add(MessageValue);

        if (Severity == EMessageSeverity::Error)
        {
            OutErrors.Add(MessageValue);
        }
        else if (Severity == EMessageSeverity::Warning || Severity == EMessageSeverity::PerformanceWarning)
        {
            OutWarnings.Add(MessageValue);
        }
    }
}

/**
 * @brief 构造函数。
 */
FUnrealMCPBlueprintCommands::FUnrealMCPBlueprintCommands()
{
}

/**
 * @brief 分发 Blueprint 相关命令。
 * @param [in] CommandType 命令类型。
 * @param [in] Params 命令参数。
 * @return TSharedPtr<FJsonObject> 执行结果或错误信息。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_blueprint"))
    {
        return HandleCreateBlueprint(Params);
    }
    else if (CommandType == TEXT("create_child_blueprint"))
    {
        return HandleCreateChildBlueprint(Params);
    }
    else if (CommandType == TEXT("add_component_to_blueprint"))
    {
        return HandleAddComponentToBlueprint(Params);
    }
    else if (CommandType == TEXT("remove_component_from_blueprint"))
    {
        return HandleRemoveComponentFromBlueprint(Params);
    }
    else if (CommandType == TEXT("attach_component_in_blueprint"))
    {
        return HandleAttachComponentInBlueprint(Params);
    }
    else if (CommandType == TEXT("set_component_property"))
    {
        return HandleSetComponentProperty(Params);
    }
    else if (CommandType == TEXT("set_physics_properties"))
    {
        return HandleSetPhysicsProperties(Params);
    }
    else if (CommandType == TEXT("compile_blueprint"))
    {
        return HandleCompileBlueprint(Params);
    }
    else if (CommandType == TEXT("compile_blueprints"))
    {
        return HandleCompileBlueprints(Params);
    }
    else if (CommandType == TEXT("cleanup_blueprint_for_reparent"))
    {
        return HandleCleanupBlueprintForReparent(Params);
    }
    else if (CommandType == TEXT("set_blueprint_property"))
    {
        return HandleSetBlueprintProperty(Params);
    }
    else if (CommandType == TEXT("set_static_mesh_properties"))
    {
        return HandleSetStaticMeshProperties(Params);
    }
    else if (CommandType == TEXT("set_pawn_properties"))
    {
        return HandleSetPawnProperties(Params);
    }
    else if (CommandType == TEXT("set_game_mode_default_pawn"))
    {
        return HandleSetGameModeDefaultPawn(Params);
    }
    else if (CommandType == TEXT("add_blueprint_variable"))
    {
        return HandleAddBlueprintVariable(Params);
    }
    else if (CommandType == TEXT("delete_blueprint_variable"))
    {
        return HandleDeleteBlueprintVariable(Params);
    }
    else if (CommandType == TEXT("remove_unused_blueprint_variables"))
    {
        return HandleRemoveUnusedBlueprintVariables(Params);
    }
    else if (CommandType == TEXT("add_blueprint_interface"))
    {
        return HandleAddBlueprintInterface(Params);
    }
    else if (CommandType == TEXT("set_blueprint_variable_default"))
    {
        return HandleSetBlueprintVariableDefault(Params);
    }
    else if (CommandType == TEXT("add_blueprint_function"))
    {
        return HandleAddBlueprintFunction(Params);
    }
    else if (CommandType == TEXT("delete_blueprint_function"))
    {
        return HandleDeleteBlueprintFunction(Params);
    }
    else if (CommandType == TEXT("get_blueprint_compile_errors"))
    {
        return HandleGetBlueprintCompileErrors(Params);
    }
    else if (CommandType == TEXT("rename_blueprint_member"))
    {
        return HandleRenameBlueprintMember(Params);
    }
    else if (CommandType == TEXT("save_blueprint"))
    {
        return HandleSaveBlueprint(Params);
    }
    else if (CommandType == TEXT("open_blueprint_editor"))
    {
        return HandleOpenBlueprintEditor(Params);
    }
    else if (CommandType == TEXT("reparent_blueprint"))
    {
        return HandleReparentBlueprint(Params);
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown blueprint command: %s"), *CommandType));
}

/**
 * @brief 创建 Blueprint 资产。
 * @param [in] Params 创建参数。
 * @return TSharedPtr<FJsonObject> 创建结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("create_blueprint"),
        Params);
}

/**
 * @brief 基于现有 Blueprint 创建子 Blueprint 资产。
 * @param [in] Params 创建参数。
 * @return TSharedPtr<FJsonObject> 创建结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleCreateChildBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("create_child_blueprint"),
        Params);
}

/**
 * @brief 向 Blueprint 添加组件。
 * @param [in] Params 组件参数。
 * @return TSharedPtr<FJsonObject> 添加结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleAddComponentToBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("add_component_to_blueprint"),
        Params);
}

/**
 * @brief 从 Blueprint 中删除组件。
 * @param [in] Params 删除参数。
 * @return TSharedPtr<FJsonObject> 删除结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleRemoveComponentFromBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("remove_component_from_blueprint"),
        Params);
}

/**
 * @brief 调整 Blueprint 组件的父子关系。
 * @param [in] Params 挂接参数。
 * @return TSharedPtr<FJsonObject> 挂接结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleAttachComponentInBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("attach_component_in_blueprint"),
        Params);
}

/**
 * @brief 设置 Blueprint 组件属性。
 * @param [in] Params 属性参数。
 * @return TSharedPtr<FJsonObject> 设置结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSetComponentProperty(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("set_component_property"),
        Params);
}

/**
 * @brief 设置组件物理参数。
 * @param [in] Params 物理参数。
 * @return TSharedPtr<FJsonObject> 设置结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSetPhysicsProperties(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("set_physics_properties"),
        Params);
}

/**
 * @brief 编译 Blueprint。
 * @param [in] Params 编译参数。
 * @return TSharedPtr<FJsonObject> 编译结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("compile_blueprint"),
        Params);
}

/**
 * @brief 清理 Blueprint 重设父类后残留的节点。
 * @param [in] Params 清理参数。
 * @return TSharedPtr<FJsonObject> 清理结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleCleanupBlueprintForReparent(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    TSet<FName> ComponentNamesToRemove;
    const TArray<TSharedPtr<FJsonValue>>* RemoveComponentsJson = nullptr;
    if (Params->TryGetArrayField(TEXT("remove_components"), RemoveComponentsJson))
    {
        for (const TSharedPtr<FJsonValue>& Entry : *RemoveComponentsJson)
        {
            const FString ComponentName = Entry.IsValid() ? Entry->AsString() : FString();
            if (!ComponentName.IsEmpty())
            {
                ComponentNamesToRemove.Add(FName(*ComponentName));
            }
        }
    }

    TSet<FName> MemberNamesToRemove;
    const TArray<TSharedPtr<FJsonValue>>* RemoveMemberNodesJson = nullptr;
    if (Params->TryGetArrayField(TEXT("remove_member_nodes"), RemoveMemberNodesJson))
    {
        for (const TSharedPtr<FJsonValue>& Entry : *RemoveMemberNodesJson)
        {
            const FString MemberName = Entry.IsValid() ? Entry->AsString() : FString();
            if (!MemberName.IsEmpty())
            {
                MemberNamesToRemove.Add(FName(*MemberName));
            }
        }
    }

    bool bRefreshNodes = true;
    bool bCompileBlueprint = true;
    bool bSaveAsset = true;
    Params->TryGetBoolField(TEXT("refresh_nodes"), bRefreshNodes);
    Params->TryGetBoolField(TEXT("compile"), bCompileBlueprint);
    Params->TryGetBoolField(TEXT("save"), bSaveAsset);

    int32 RemovedComponentCount = 0;
    int32 RemovedNodeCount = 0;
    bool bStructuralChange = false;

    if (Blueprint->SimpleConstructionScript && ComponentNamesToRemove.Num() > 0)
    {
        const TArray<USCS_Node*> ExistingNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
        for (USCS_Node* Node : ExistingNodes)
        {
            if (!Node)
            {
                continue;
            }

            if (ComponentNamesToRemove.Contains(Node->GetVariableName()))
            {
                Blueprint->SimpleConstructionScript->RemoveNodeAndPromoteChildren(Node);
                ++RemovedComponentCount;
                bStructuralChange = true;
            }
        }
    }

    TSet<FName> NodeMembersToRemove = MemberNamesToRemove;
    NodeMembersToRemove.Append(ComponentNamesToRemove);

    if (NodeMembersToRemove.Num() > 0)
    {
        TArray<UEdGraph*> AllGraphs;
        Blueprint->GetAllGraphs(AllGraphs);

        for (UEdGraph* Graph : AllGraphs)
        {
            if (!Graph)
            {
                continue;
            }

            TArray<UEdGraphNode*> NodesToRemove;
            for (UEdGraphNode* GraphNode : Graph->Nodes)
            {
                if (const UK2Node_VariableGet* VariableGetNode = Cast<UK2Node_VariableGet>(GraphNode))
                {
                    if (NodeMembersToRemove.Contains(VariableGetNode->VariableReference.GetMemberName()))
                    {
                        NodesToRemove.Add(GraphNode);
                    }
                }
                else if (const UK2Node_VariableSet* VariableSetNode = Cast<UK2Node_VariableSet>(GraphNode))
                {
                    if (NodeMembersToRemove.Contains(VariableSetNode->VariableReference.GetMemberName()))
                    {
                        NodesToRemove.Add(GraphNode);
                    }
                }
            }

            for (UEdGraphNode* NodeToRemove : NodesToRemove)
            {
                Graph->RemoveNode(NodeToRemove);
                ++RemovedNodeCount;
                bStructuralChange = true;
            }
        }
    }

    if (bStructuralChange)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    }
    else
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    }

    if (bRefreshNodes)
    {
        FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
    }

    if (bCompileBlueprint)
    {
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
    }

    bool bSaved = false;
    if (bSaveAsset)
    {
        bSaved = UEditorAssetLibrary::SaveLoadedAsset(Blueprint, false);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("name"), Blueprint->GetName());
    ResultObj->SetStringField(TEXT("path"), Blueprint->GetPathName());
    ResultObj->SetNumberField(TEXT("removed_components"), RemovedComponentCount);
    ResultObj->SetNumberField(TEXT("removed_nodes"), RemovedNodeCount);
    ResultObj->SetBoolField(TEXT("saved"), bSaved);
    return ResultObj;
}

/**
 * @brief 批量编译多个 Blueprint。
 * @param [in] Params 批量编译参数。
 * @return TSharedPtr<FJsonObject> 编译结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleCompileBlueprints(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("compile_blueprints"),
        Params);
}

/**
 * @brief 设置 Blueprint 默认对象属性。
 * @param [in] Params 属性设置参数。
 * @return TSharedPtr<FJsonObject> 设置结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSetBlueprintProperty(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("set_blueprint_property"),
        Params);
}

/**
 * @brief 设置静态网格组件资源与参数。
 * @param [in] Params 静态网格参数。
 * @return TSharedPtr<FJsonObject> 设置结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSetStaticMeshProperties(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("set_static_mesh_properties"),
        Params);
}

/**
 * @brief 设置 Pawn 常用属性。
 * @param [in] Params Pawn 属性参数。
 * @return TSharedPtr<FJsonObject> 设置结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSetPawnProperties(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("set_pawn_properties"),
        Params);
}

/**
 * @brief 设置 GameMode 的默认 Pawn。
 * @param [in] Params GameMode 配置参数。
 * @return TSharedPtr<FJsonObject> 设置结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSetGameModeDefaultPawn(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("set_game_mode_default_pawn"),
        Params);
}

/**
 * @brief 添加 Blueprint 成员变量。
 * @param [in] Params 变量参数。
 * @return TSharedPtr<FJsonObject> 添加结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleAddBlueprintVariable(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("add_blueprint_variable"),
        Params);
}

/**
 * @brief 删除 Blueprint 成员变量。
 * @param [in] Params 删除参数。
 * @return TSharedPtr<FJsonObject> 删除结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleDeleteBlueprintVariable(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString VariableName;
    if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    const int32 VariableIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, FName(*VariableName));
    if (VariableIndex == INDEX_NONE)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint variable not found: %s"), *VariableName));
    }

    FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, FName(*VariableName));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Result->SetStringField(TEXT("variable_name"), VariableName);
    return Result;
}

/**
 * @brief 清理 Blueprint 中未被引用的成员变量。
 * @param [in] Params 清理参数。
 * @return TSharedPtr<FJsonObject> 清理结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleRemoveUnusedBlueprintVariables(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("remove_unused_blueprint_variables"),
        Params);
}

/**
 * @brief 为 Blueprint 添加接口实现。
 * @param [in] Params 接口参数。
 * @return TSharedPtr<FJsonObject> 添加结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleAddBlueprintInterface(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString InterfaceReference;
    if (!Params->TryGetStringField(TEXT("interface_class"), InterfaceReference))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'interface_class' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    if (!FBlueprintEditorUtils::DoesSupportImplementingInterfaces(Blueprint))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Blueprint 不支持实现接口: %s"), *Blueprint->GetName()));
    }

    UClass* InterfaceClass = nullptr;
    FString ErrorMessage;
    if (!UnrealMCPBlueprintResolveInterfaceClass(InterfaceReference, InterfaceClass, ErrorMessage))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    if (!InterfaceClass || !(InterfaceClass->IsChildOf(UInterface::StaticClass()) || InterfaceClass->HasAnyClassFlags(CLASS_Interface)))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("目标类不是接口: %s"), *InterfaceReference));
    }

    const FTopLevelAssetPath InterfaceClassPath = InterfaceClass->GetClassPathName();

    bool bAlreadyImplemented = false;
    for (const FBPInterfaceDescription& InterfaceDescription : Blueprint->ImplementedInterfaces)
    {
        const UClass* ExistingInterface = InterfaceDescription.Interface.Get();
        if (ExistingInterface && ExistingInterface->GetClassPathName() == InterfaceClassPath)
        {
            bAlreadyImplemented = true;
            break;
        }
    }

    bool bImplemented = false;
    if (!bAlreadyImplemented)
    {
        bImplemented = FBlueprintEditorUtils::ImplementNewInterface(Blueprint, InterfaceClassPath);
        if (!bImplemented)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("添加 Blueprint 接口失败: %s"), *InterfaceClass->GetPathName()));
        }
    }

    FBlueprintEditorUtils::ConformImplementedInterfaces(Blueprint);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    const bool bSaved = UEditorAssetLibrary::SaveLoadedAsset(Blueprint, false);
    if (!bSaved)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("保存 Blueprint 失败: %s"), *Blueprint->GetName()));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());
    Result->SetStringField(TEXT("asset_path"), FPackageName::ObjectPathToPackageName(Blueprint->GetPathName()));
    Result->SetStringField(TEXT("interface_class"), InterfaceClass->GetPathName());
    Result->SetStringField(TEXT("interface_name"), InterfaceClass->GetName());
    Result->SetBoolField(TEXT("implemented"), !bAlreadyImplemented && bImplemented);
    Result->SetBoolField(TEXT("already_implemented"), bAlreadyImplemented);
    Result->SetBoolField(TEXT("compiled"), true);
    Result->SetBoolField(TEXT("saved"), true);
    Result->SetStringField(TEXT("implementation"), TEXT("cpp"));
    return Result;
}

/**
 * @brief 设置 Blueprint 成员变量默认值。
 * @param [in] Params 默认值参数。
 * @return TSharedPtr<FJsonObject> 设置结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSetBlueprintVariableDefault(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("set_blueprint_variable_default"),
        Params);
}

/**
 * @brief 为 Blueprint 添加函数图。
 * @param [in] Params 函数参数。
 * @return TSharedPtr<FJsonObject> 创建结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleAddBlueprintFunction(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("add_blueprint_function"),
        Params);
}

/**
 * @brief 删除 Blueprint 中指定函数图。
 * @param [in] Params 函数参数。
 * @return TSharedPtr<FJsonObject> 删除结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleDeleteBlueprintFunction(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("delete_blueprint_function"),
        Params);
}

/**
 * @brief 获取 Blueprint 当前编译错误与警告详情。
 * @param [in] Params 查询参数。
 * @return TSharedPtr<FJsonObject> 编译状态与消息列表。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleGetBlueprintCompileErrors(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    FCompilerResultsLog Results;
    Results.bAnnotateMentionedNodes = false;
    Results.bSilentMode = true;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);

    TArray<TSharedPtr<FJsonValue>> Messages;
    TArray<TSharedPtr<FJsonValue>> Errors;
    TArray<TSharedPtr<FJsonValue>> Warnings;
    UnrealMCPBlueprintSerializeCompilerMessages(Results, Messages, Errors, Warnings);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());
    Result->SetStringField(TEXT("asset_path"), FPackageName::ObjectPathToPackageName(Blueprint->GetPathName()));
    Result->SetStringField(TEXT("compile_status"), UnrealMCPBlueprintStatusToString(Blueprint->Status));
    Result->SetBoolField(TEXT("has_errors"), Results.NumErrors > 0 || Blueprint->Status == BS_Error);
    Result->SetNumberField(TEXT("error_count"), Results.NumErrors);
    Result->SetNumberField(TEXT("warning_count"), Results.NumWarnings);
    Result->SetNumberField(TEXT("message_count"), Messages.Num());
    Result->SetArrayField(TEXT("errors"), Errors);
    Result->SetArrayField(TEXT("warnings"), Warnings);
    Result->SetArrayField(TEXT("messages"), Messages);
    Result->SetStringField(TEXT("implementation"), TEXT("cpp"));
    return Result;
}

/**
 * @brief 重命名 Blueprint 成员。
 * @param [in] Params 重命名参数。
 * @return TSharedPtr<FJsonObject> 重命名结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleRenameBlueprintMember(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString OldName;
    if (!Params->TryGetStringField(TEXT("old_name"), OldName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'old_name' parameter"));
    }

    FString NewName;
    if (!Params->TryGetStringField(TEXT("new_name"), NewName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'new_name' parameter"));
    }

    FString MemberType = TEXT("auto");
    Params->TryGetStringField(TEXT("member_type"), MemberType);
    MemberType = MemberType.TrimStartAndEnd().ToLower();

    if (OldName.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'old_name' cannot be empty"));
    }
    if (NewName.TrimStartAndEnd().IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'new_name' cannot be empty"));
    }

    if (MemberType.IsEmpty())
    {
        MemberType = TEXT("auto");
    }

    if (MemberType == TEXT("variable") || MemberType == TEXT("auto"))
    {
        UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
        if (!Blueprint)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
        }

        const FName OldVariableName(*OldName);
        const FName NewVariableName(*NewName);
        const int32 ExistingVariableIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, OldVariableName);
        if (ExistingVariableIndex != INDEX_NONE)
        {
            if (FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, NewVariableName) != INDEX_NONE)
            {
                return FUnrealMCPCommonUtils::CreateErrorResponse(
                    FString::Printf(TEXT("Blueprint variable already exists: %s"), *NewName));
            }

            FBlueprintEditorUtils::RenameMemberVariable(Blueprint, OldVariableName, NewVariableName);
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
            FKismetEditorUtilities::CompileBlueprint(Blueprint);

            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetBoolField(TEXT("success"), true);
            Result->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());
            Result->SetStringField(TEXT("asset_path"), FPackageName::ObjectPathToPackageName(Blueprint->GetPathName()));
            Result->SetStringField(TEXT("old_name"), OldName);
            Result->SetStringField(TEXT("new_name"), NewName);
            Result->SetStringField(TEXT("member_type"), TEXT("variable"));
            Result->SetBoolField(TEXT("renamed"), true);
            Result->SetBoolField(TEXT("compiled"), true);
            Result->SetStringField(TEXT("compile_api"), TEXT("FKismetEditorUtilities::CompileBlueprint"));
            Result->SetStringField(TEXT("implementation"), TEXT("python_cpp_bridge"));
            return Result;
        }

        if (MemberType == TEXT("variable"))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Blueprint variable not found: %s"), *OldName));
        }
    }

    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("rename_blueprint_member"),
        Params);
}

/**
 * @brief 保存 Blueprint 资产。
 * @param [in] Params 保存参数。
 * @return TSharedPtr<FJsonObject> 保存结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSaveBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("save_blueprint"),
        Params);
}

/**
 * @brief 打开 Blueprint 编辑器。
 * @param [in] Params 打开参数。
 * @return TSharedPtr<FJsonObject> 打开结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleOpenBlueprintEditor(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("open_blueprint_editor"),
        Params);
}

/**
 * @brief 修改 Blueprint 的父类。
 * @param [in] Params 改父类参数。
 * @return TSharedPtr<FJsonObject> 改父类结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleReparentBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    return FUnrealMCPCommonUtils::ExecuteLocalPythonCommand(
        TEXT("commands.blueprint.blueprint_commands"),
        TEXT("handle_blueprint_command"),
        TEXT("reparent_blueprint"),
        Params);
}
