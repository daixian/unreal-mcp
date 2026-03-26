/**
 * @file CleanupBlueprintForReparentCommandlet.cpp
 * @brief Blueprint 重设父类后的清理命令行工具实现。
 */
#include "CleanupBlueprintForReparentCommandlet.h"

#include "Commands/UnrealMCPCommonUtils.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "EditorAssetLibrary.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SpringArmComponent.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_InputAxisKeyEvent.h"
#include "K2Node_InputKey.h"
#include "K2Node_Self.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Parse.h"
#include "UObject/UObjectGlobals.h"

static bool IsTargetFunctionCall(const UK2Node_CallFunction* CallFunctionNode, const FName FunctionName)
{
    if (!CallFunctionNode)
    {
        return false;
    }

    const UFunction* TargetFunction = CallFunctionNode->GetTargetFunction();
    const FName TargetFunctionName = TargetFunction
        ? TargetFunction->GetFName()
        : CallFunctionNode->FunctionReference.GetMemberName();
    return TargetFunctionName == FunctionName;
}

static UEdGraphPin* FindExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
    if (!Node)
    {
        return nullptr;
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin &&
            Pin->Direction == Direction &&
            Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
        {
            return Pin;
        }
    }

    return nullptr;
}

static UEdGraphPin* FindWidgetTargetPin(UEdGraphNode* Node)
{
    if (!Node)
    {
        return nullptr;
    }

    if (UEdGraphPin* SelfPin = Node->FindPin(UEdGraphSchema_K2::PN_Self))
    {
        return SelfPin;
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (!Pin || Pin->Direction != EGPD_Input)
        {
            continue;
        }

        if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Object)
        {
            continue;
        }

        UClass* PinClass = Cast<UClass>(Pin->PinType.PinSubCategoryObject.Get());
        if (PinClass && PinClass->IsChildOf(UUserWidget::StaticClass()))
        {
            return Pin;
        }
    }

    return nullptr;
}

static void TransferPinLinks(const UEdGraphSchema_K2* Schema, UEdGraphPin* OldPin, UEdGraphPin* NewPin)
{
    if (!Schema || !OldPin || !NewPin)
    {
        return;
    }

    const TArray<UEdGraphPin*> LinkedPins = OldPin->LinkedTo;
    for (UEdGraphPin* LinkedPin : LinkedPins)
    {
        if (!LinkedPin)
        {
            continue;
        }

        if (OldPin->Direction == EGPD_Input)
        {
            Schema->TryCreateConnection(LinkedPin, NewPin);
        }
        else
        {
            Schema->TryCreateConnection(NewPin, LinkedPin);
        }
    }

    if (OldPin->Direction == EGPD_Input)
    {
        if (OldPin->DefaultObject)
        {
            NewPin->DefaultObject = OldPin->DefaultObject;
        }

        if (!OldPin->DefaultValue.IsEmpty())
        {
            NewPin->DefaultValue = OldPin->DefaultValue;
        }

        if (!OldPin->DefaultTextValue.IsEmpty())
        {
            NewPin->DefaultTextValue = OldPin->DefaultTextValue;
        }
    }
}

static UClass* FindWorldUIFunctionLibraryClass()
{
    static const TCHAR* ClassNameCandidates[] = {
        TEXT("XRWorldUIFunctionLibrary"),
        TEXT("UXRWorldUIFunctionLibrary")
    };

    for (const TCHAR* ClassName : ClassNameCandidates)
    {
        if (UClass* FoundClass = FindFirstObject<UClass>(ClassName, EFindFirstObjectOptions::None))
        {
            return FoundClass;
        }
    }

    static const TCHAR* ClassPathCandidates[] = {
        TEXT("/Script/KmaxXR.XRWorldUIFunctionLibrary"),
        TEXT("/Script/KmaxXR.UXRWorldUIFunctionLibrary")
    };

    for (const TCHAR* ClassPath : ClassPathCandidates)
    {
        if (UClass* LoadedClass = LoadObject<UClass>(nullptr, ClassPath))
        {
            return LoadedClass;
        }
    }

    return nullptr;
}

static UFunction* FindWorldUIReplacementFunction(const TCHAR* FunctionName)
{
    UClass* FunctionLibraryClass = FindWorldUIFunctionLibraryClass();
    return FunctionLibraryClass ? FunctionLibraryClass->FindFunctionByName(*FString(FunctionName)) : nullptr;
}

static bool ReplaceViewportCall(
    UBlueprint* Blueprint,
    UEdGraph* Graph,
    UK2Node_CallFunction* OldNode,
    UFunction* ReplacementFunction,
    const UEdGraphSchema_K2* Schema)
{
    if (!Blueprint || !Graph || !OldNode || !ReplacementFunction || !Schema)
    {
        return false;
    }

    UK2Node_CallFunction* NewNode = FUnrealMCPCommonUtils::CreateFunctionCallNode(
        Graph,
        ReplacementFunction,
        FVector2D(OldNode->NodePosX, OldNode->NodePosY));
    if (!NewNode)
    {
        return false;
    }

    TransferPinLinks(Schema, FindExecPin(OldNode, EGPD_Input), FindExecPin(NewNode, EGPD_Input));
    TransferPinLinks(Schema, FindExecPin(OldNode, EGPD_Output), FindExecPin(NewNode, EGPD_Output));

    UEdGraphPin* NewWidgetPin = FUnrealMCPCommonUtils::FindPin(NewNode, TEXT("Widget"), EGPD_Input);
    UEdGraphPin* OldWidgetPin = FindWidgetTargetPin(OldNode);

    if (NewWidgetPin && OldWidgetPin)
    {
        TransferPinLinks(Schema, OldWidgetPin, NewWidgetPin);
    }

    if (NewWidgetPin &&
        NewWidgetPin->LinkedTo.Num() == 0 &&
        Blueprint->GeneratedClass &&
        Blueprint->GeneratedClass->IsChildOf(UUserWidget::StaticClass()))
    {
        UK2Node_Self* SelfNode = FUnrealMCPCommonUtils::CreateSelfReferenceNode(
            Graph,
            FVector2D(OldNode->NodePosX - 240.0f, OldNode->NodePosY));
        UEdGraphPin* SelfPin = FUnrealMCPCommonUtils::FindPin(SelfNode, UEdGraphSchema_K2::PN_Self.ToString(), EGPD_Output);
        if (SelfPin)
        {
            Schema->TryCreateConnection(SelfPin, NewWidgetPin);
        }
    }

    Graph->RemoveNode(OldNode);
    return true;
}

UCleanupBlueprintForReparentCommandlet::UCleanupBlueprintForReparentCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UCleanupBlueprintForReparentCommandlet::Main(const FString& Params)
{
    FString BlueprintPath;
    FParse::Value(*Params, TEXT("Blueprint="), BlueprintPath);

    FString MapPath;
    FParse::Value(*Params, TEXT("Map="), MapPath);

    if (BlueprintPath.IsEmpty() && MapPath.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("缺少 -Blueprint=/Game/... 或 -Map=/Game/... 参数"));
        return 1;
    }

    FString RemoveComponentsParam;
    FParse::Value(*Params, TEXT("RemoveComponents="), RemoveComponentsParam);

    FString RemoveMembersParam;
    FParse::Value(*Params, TEXT("RemoveMembers="), RemoveMembersParam);

    const bool bEnsureSpringArm = FParse::Param(*Params, TEXT("EnsureSpringArm"));
    const bool bStripCameraOps = FParse::Param(*Params, TEXT("StripCameraOps"));

    FString RemoveFunctionsParam;
    FParse::Value(*Params, TEXT("RemoveFunctions="), RemoveFunctionsParam);
    const bool bReplaceViewportCalls = FParse::Param(*Params, TEXT("ReplaceViewportCalls"));

    TSet<FName> ComponentNamesToRemove;
    TArray<FString> ComponentNames;
    RemoveComponentsParam.ParseIntoArray(ComponentNames, TEXT(","), true);
    for (const FString& ComponentName : ComponentNames)
    {
        if (!ComponentName.IsEmpty())
        {
            ComponentNamesToRemove.Add(FName(*ComponentName));
        }
    }

    TSet<FName> MemberNamesToRemove;
    TArray<FString> MemberNames;
    RemoveMembersParam.ParseIntoArray(MemberNames, TEXT(","), true);
    for (const FString& MemberName : MemberNames)
    {
        if (!MemberName.IsEmpty())
        {
            MemberNamesToRemove.Add(FName(*MemberName));
        }
    }

    TSet<FName> FunctionNamesToRemove;
    TArray<FString> FunctionNames;
    RemoveFunctionsParam.ParseIntoArray(FunctionNames, TEXT(","), true);
    for (const FString& FunctionName : FunctionNames)
    {
        if (!FunctionName.IsEmpty())
        {
            FunctionNamesToRemove.Add(FName(*FunctionName));
        }
    }

    if (bStripCameraOps)
    {
        static const TCHAR* DefaultFunctionNames[] = {
            TEXT("K2_AddActorLocalRotation"),
            TEXT("K2_AddActorWorldRotation"),
            TEXT("K2_SetActorTransform"),
            TEXT("K2_SetActorLocation"),
            TEXT("K2_SetActorRotation"),
            TEXT("GetTransform"),
            TEXT("K2_GetComponentToWorld"),
            TEXT("K2_SetRelativeLocation"),
            TEXT("K2_SetRelativeRotation"),
            TEXT("K2_SetRelativeTransform"),
            TEXT("K2_SetWorldLocation"),
            TEXT("K2_SetWorldRotation"),
            TEXT("K2_SetWorldTransform"),
            TEXT("AddLocalOffset"),
            TEXT("AddLocalRotation"),
            TEXT("AddRelativeLocation"),
            TEXT("AddRelativeRotation"),
            TEXT("NearlyEqual_TransformTransform")
        };

        for (const TCHAR* FunctionName : DefaultFunctionNames)
        {
            FunctionNamesToRemove.Add(FName(FunctionName));
        }
    }

    UWorld* LoadedWorld = nullptr;
    UBlueprint* Blueprint = nullptr;
    const bool bOperateOnMap = !MapPath.IsEmpty();

    if (bOperateOnMap)
    {
        LoadedWorld = UEditorLoadingAndSavingUtils::LoadMap(MapPath);
        if (!LoadedWorld || !LoadedWorld->PersistentLevel)
        {
            UE_LOG(LogTemp, Error, TEXT("地图不存在或加载失败: %s"), *MapPath);
            return 2;
        }

        Blueprint = LoadedWorld->PersistentLevel->GetLevelScriptBlueprint();
    }
    else
    {
        Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintPath);
    }

    if (!Blueprint)
    {
        UE_LOG(LogTemp, Error, TEXT("未找到 Blueprint: %s"), bOperateOnMap ? *MapPath : *BlueprintPath);
        return 2;
    }

    UFunction* AddToWorldUIFunction = nullptr;
    UFunction* RemoveFromWorldUIFunction = nullptr;
    bool bCanReplaceViewportCalls = false;
    if (bReplaceViewportCalls)
    {
        AddToWorldUIFunction = FindWorldUIReplacementFunction(TEXT("AddToWorldUI"));
        RemoveFromWorldUIFunction = FindWorldUIReplacementFunction(TEXT("RemoveFromWorldUI"));
        bCanReplaceViewportCalls = (AddToWorldUIFunction != nullptr && RemoveFromWorldUIFunction != nullptr);

        if (!bCanReplaceViewportCalls)
        {
            UE_LOG(LogTemp, Warning, TEXT("未找到 XRWorldUIFunctionLibrary，跳过 ReplaceViewportCalls"));
        }
    }

    int32 RemovedComponentCount = 0;
    int32 RemovedNodeCount = 0;
    int32 ReplacedViewportCallCount = 0;
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
                UE_LOG(LogTemp, Display, TEXT("移除组件节点: %s"), *Node->GetVariableName().ToString());
                Blueprint->SimpleConstructionScript->RemoveNodeAndPromoteChildren(Node);
                ++RemovedComponentCount;
                bStructuralChange = true;
            }
        }
    }

    if (bEnsureSpringArm &&
        Blueprint->SimpleConstructionScript &&
        !Blueprint->SimpleConstructionScript->FindSCSNode(TEXT("SpringArm")))
    {
        USCS_Node* NewSpringArmNode = Blueprint->SimpleConstructionScript->CreateNode(USpringArmComponent::StaticClass(), TEXT("SpringArm"));
        if (NewSpringArmNode)
        {
            const TArray<USCS_Node*>& RootNodes = Blueprint->SimpleConstructionScript->GetRootNodes();
            if (RootNodes.Num() > 0 && RootNodes[0])
            {
                RootNodes[0]->AddChildNode(NewSpringArmNode);
            }
            else
            {
                Blueprint->SimpleConstructionScript->AddNode(NewSpringArmNode);
            }

            UE_LOG(LogTemp, Display, TEXT("补齐缺失组件节点: SpringArm"));
            ++RemovedComponentCount;
            bStructuralChange = true;
        }
    }

    TSet<FName> NodeMembersToRemove = MemberNamesToRemove;
    NodeMembersToRemove.Append(ComponentNamesToRemove);

    if (bCanReplaceViewportCalls || NodeMembersToRemove.Num() > 0 || FunctionNamesToRemove.Num() > 0 || bStripCameraOps)
    {
        TArray<UEdGraph*> AllGraphs;
        Blueprint->GetAllGraphs(AllGraphs);

        for (UEdGraph* Graph : AllGraphs)
        {
            if (!Graph)
            {
                continue;
            }

            if (bCanReplaceViewportCalls)
            {
                const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
                TArray<UK2Node_CallFunction*> ViewportNodesToReplace;
                for (UEdGraphNode* GraphNode : Graph->Nodes)
                {
                    if (UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(GraphNode))
                    {
                        if (IsTargetFunctionCall(CallFunctionNode, TEXT("AddToViewport")) ||
                            IsTargetFunctionCall(CallFunctionNode, TEXT("RemoveFromParent")))
                        {
                            ViewportNodesToReplace.Add(CallFunctionNode);
                        }
                    }
                }

                for (UK2Node_CallFunction* ViewportNode : ViewportNodesToReplace)
                {
                    UFunction* ReplacementFunction = IsTargetFunctionCall(ViewportNode, TEXT("AddToViewport"))
                        ? AddToWorldUIFunction
                        : RemoveFromWorldUIFunction;

                    if (ReplaceViewportCall(Blueprint, Graph, ViewportNode, ReplacementFunction, K2Schema))
                    {
                        ++ReplacedViewportCallCount;
                        bStructuralChange = true;
                    }
                }
            }

            if (NodeMembersToRemove.Num() > 0 || FunctionNamesToRemove.Num() > 0 || bStripCameraOps)
            {
                TSet<UEdGraphNode*> NodesToRemove;
                auto QueueNodeForRemoval = [&NodesToRemove](UEdGraphNode* Node)
                {
                    if (Node)
                    {
                        NodesToRemove.Add(Node);
                    }
                };

                for (UEdGraphNode* GraphNode : Graph->Nodes)
                {
                    if (const UK2Node_VariableGet* VariableGetNode = Cast<UK2Node_VariableGet>(GraphNode))
                    {
                        if (NodeMembersToRemove.Contains(VariableGetNode->VariableReference.GetMemberName()))
                        {
                            QueueNodeForRemoval(GraphNode);
                        }
                    }
                    else if (const UK2Node_VariableSet* VariableSetNode = Cast<UK2Node_VariableSet>(GraphNode))
                    {
                        if (NodeMembersToRemove.Contains(VariableSetNode->VariableReference.GetMemberName()))
                        {
                            QueueNodeForRemoval(GraphNode);
                        }
                    }
                    else if (const UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(GraphNode))
                    {
                        const UFunction* TargetFunction = CallFunctionNode->GetTargetFunction();
                        const FName FunctionName = TargetFunction
                            ? TargetFunction->GetFName()
                            : CallFunctionNode->FunctionReference.GetMemberName();
                        const UClass* OwnerClass = TargetFunction
                            ? TargetFunction->GetOuterUClass()
                            : CallFunctionNode->FunctionReference.GetMemberParentClass(CallFunctionNode->GetBlueprintClassFromNode());
                        const bool bMatchesFunctionName = FunctionNamesToRemove.Contains(FunctionName);
                        const bool bIsCameraClassCall = bStripCameraOps &&
                            OwnerClass &&
                            (OwnerClass->IsChildOf(USceneComponent::StaticClass()) ||
                             OwnerClass->IsChildOf(UCameraComponent::StaticClass()) ||
                             OwnerClass->IsChildOf(USpringArmComponent::StaticClass()));
                        const bool bIsActorTransformCall = bStripCameraOps &&
                            OwnerClass &&
                            OwnerClass->IsChildOf(AActor::StaticClass()) &&
                            FunctionNamesToRemove.Contains(FunctionName);
                        bool bHasBrokenRefInput = false;

                        if (bStripCameraOps)
                        {
                            for (const UEdGraphPin* Pin : GraphNode->Pins)
                            {
                                if (!Pin || Pin->Direction != EGPD_Input)
                                {
                                    continue;
                                }

                                if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                                {
                                    continue;
                                }

                                if (!Pin->PinType.bIsReference)
                                {
                                    continue;
                                }

                                const bool bHasLinkedTarget = Pin->LinkedTo.Num() > 0;
                                const bool bHasDefaultTarget =
                                    Pin->DefaultObject != nullptr ||
                                    !Pin->DefaultValue.IsEmpty() ||
                                    !Pin->DefaultTextValue.IsEmpty();

                                if (!bHasLinkedTarget && !bHasDefaultTarget)
                                {
                                    bHasBrokenRefInput = true;
                                    break;
                                }
                            }
                        }

                        if (bMatchesFunctionName || bIsCameraClassCall || bIsActorTransformCall || bHasBrokenRefInput)
                        {
                            QueueNodeForRemoval(GraphNode);
                        }
                        else if (OwnerClass && OwnerClass->IsChildOf(USceneComponent::StaticClass()))
                        {
                            if (UEdGraphPin* SelfPin = GraphNode->FindPin(UEdGraphSchema_K2::PN_Self))
                            {
                                const bool bHasLinkedTarget = SelfPin->LinkedTo.Num() > 0;
                                const bool bHasDefaultTarget = SelfPin->DefaultObject != nullptr || !SelfPin->DefaultValue.IsEmpty();
                                if (!bHasLinkedTarget && !bHasDefaultTarget)
                                {
                                    QueueNodeForRemoval(GraphNode);
                                }
                            }
                        }
                    }
                    else if (bStripCameraOps)
                    {
                        if (const UK2Node_InputAxisKeyEvent* InputAxisNode = Cast<UK2Node_InputAxisKeyEvent>(GraphNode))
                        {
                            const FName AxisKeyName = InputAxisNode->AxisKey.GetFName();
                            if (AxisKeyName == TEXT("MouseX") || AxisKeyName == TEXT("MouseY"))
                            {
                                QueueNodeForRemoval(GraphNode);
                            }
                        }
                        else if (const UK2Node_InputKey* InputKeyNode = Cast<UK2Node_InputKey>(GraphNode))
                        {
                            const FName InputKeyName = InputKeyNode->InputKey.GetFName();
                            if (InputKeyName == TEXT("MouseScrollUp") || InputKeyName == TEXT("MouseScrollDown"))
                            {
                                QueueNodeForRemoval(GraphNode);
                            }
                        }
                    }
                }

                auto IsProtectedNode = [](UEdGraphNode* Node)
                {
                    return Node &&
                        (Cast<UK2Node_Event>(Node) ||
                         Cast<UK2Node_ComponentBoundEvent>(Node) ||
                         Cast<UK2Node_FunctionEntry>(Node) ||
                         Cast<UK2Node_FunctionResult>(Node));
                };

                TArray<UEdGraphNode*> RemovalFrontier = NodesToRemove.Array();
                for (int32 NodeIndex = 0; NodeIndex < RemovalFrontier.Num(); ++NodeIndex)
                {
                    UEdGraphNode* CurrentNode = RemovalFrontier[NodeIndex];
                    if (!CurrentNode)
                    {
                        continue;
                    }

                    for (UEdGraphPin* OutputPin : CurrentNode->Pins)
                    {
                        if (!OutputPin || OutputPin->Direction != EGPD_Output)
                        {
                            continue;
                        }

                        for (UEdGraphPin* LinkedPin : OutputPin->LinkedTo)
                        {
                            UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
                            if (!LinkedNode || LinkedNode == CurrentNode || IsProtectedNode(LinkedNode) || NodesToRemove.Contains(LinkedNode))
                            {
                                continue;
                            }

                            NodesToRemove.Add(LinkedNode);
                            RemovalFrontier.Add(LinkedNode);
                        }
                    }
                }

                for (UEdGraphNode* NodeToRemove : NodesToRemove)
                {
                    UE_LOG(LogTemp, Display, TEXT("移除图节点: %s"), *NodeToRemove->GetName());
                    Graph->RemoveNode(NodeToRemove);
                    ++RemovedNodeCount;
                    bStructuralChange = true;
                }
            }
        }
    }

    if (bStructuralChange)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    }

    FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    const bool bSaved = bOperateOnMap
        ? (LoadedWorld && UEditorLoadingAndSavingUtils::SaveMap(LoadedWorld, MapPath))
        : UEditorAssetLibrary::SaveLoadedAsset(Blueprint, false);

    UE_LOG(LogTemp, Display, TEXT("清理完成。移除组件=%d 移除节点=%d 替换视口调用=%d 已保存=%s BlueprintStatus=%d"),
        RemovedComponentCount,
        RemovedNodeCount,
        ReplacedViewportCallCount,
        bSaved ? TEXT("true") : TEXT("false"),
        static_cast<int32>(Blueprint->Status));

    if (!bSaved)
    {
        return 3;
    }

    return Blueprint->Status == BS_Error ? 4 : 0;
}
