/**
 * @file UnrealMCPCommonUtils.cpp
 * @brief UnrealMCP 公共工具实现，提供 JSON/Blueprint/反射辅助能力。
 */
#include "Commands/UnrealMCPCommonUtils.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Selection.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabase.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/** @brief JSON 工具函数分组。 */

/**
 * @brief 创建统一错误响应对象。
 * @param [in] Message 错误信息。
 * @return TSharedPtr<FJsonObject> 错误响应。
 */
TSharedPtr<FJsonObject> FUnrealMCPCommonUtils::CreateErrorResponse(const FString& Message)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), false);
    ResponseObject->SetStringField(TEXT("error"), Message);
    return ResponseObject;
}

/**
 * @brief 创建统一成功响应对象。
 * @param [in] Data 可选业务数据。
 * @return TSharedPtr<FJsonObject> 成功响应。
 */
TSharedPtr<FJsonObject> FUnrealMCPCommonUtils::CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), true);
    
    if (Data.IsValid())
    {
        ResponseObject->SetObjectField(TEXT("data"), Data);
    }
    
    return ResponseObject;
}

/**
 * @brief 从 JSON 数组字段读取 int32 数组。
 * @param [in] JsonObject 源 JSON 对象。
 * @param [in] FieldName 字段名称。
 * @param [out] OutArray 输出整型数组。
 */
void FUnrealMCPCommonUtils::GetIntArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<int32>& OutArray)
{
    OutArray.Reset();
    
    if (!JsonObject->HasField(FieldName))
    {
        return;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
        {
            OutArray.Add((int32)Value->AsNumber());
        }
    }
}

/**
 * @brief 从 JSON 数组字段读取 float 数组。
 * @param [in] JsonObject 源 JSON 对象。
 * @param [in] FieldName 字段名称。
 * @param [out] OutArray 输出浮点数组。
 */
void FUnrealMCPCommonUtils::GetFloatArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<float>& OutArray)
{
    OutArray.Reset();
    
    if (!JsonObject->HasField(FieldName))
    {
        return;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
        {
            OutArray.Add((float)Value->AsNumber());
        }
    }
}

/**
 * @brief 解析二维向量字段。
 * @param [in] JsonObject 源 JSON 对象。
 * @param [in] FieldName 字段名称。
 * @return FVector2D 解析结果。
 */
FVector2D FUnrealMCPCommonUtils::GetVector2DFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FVector2D Result(0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 2)
    {
        Result.X = (float)(*JsonArray)[0]->AsNumber();
        Result.Y = (float)(*JsonArray)[1]->AsNumber();
    }
    
    return Result;
}

/**
 * @brief 解析三维向量字段。
 * @param [in] JsonObject 源 JSON 对象。
 * @param [in] FieldName 字段名称。
 * @return FVector 解析结果。
 */
FVector FUnrealMCPCommonUtils::GetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FVector Result(0.0f, 0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.X = (float)(*JsonArray)[0]->AsNumber();
        Result.Y = (float)(*JsonArray)[1]->AsNumber();
        Result.Z = (float)(*JsonArray)[2]->AsNumber();
    }
    
    return Result;
}

/**
 * @brief 解析旋转字段。
 * @param [in] JsonObject 源 JSON 对象。
 * @param [in] FieldName 字段名称。
 * @return FRotator 解析结果。
 */
FRotator FUnrealMCPCommonUtils::GetRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FRotator Result(0.0f, 0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.Pitch = (float)(*JsonArray)[0]->AsNumber();
        Result.Yaw = (float)(*JsonArray)[1]->AsNumber();
        Result.Roll = (float)(*JsonArray)[2]->AsNumber();
    }
    
    return Result;
}

// Blueprint Utilities
/**
 * @brief 按名称查找 Blueprint（代理到精确查找函数）。
 * @param [in] BlueprintName 蓝图名称。
 * @return UBlueprint* 查找结果。
 */
UBlueprint* FUnrealMCPCommonUtils::FindBlueprint(const FString& BlueprintName)
{
    return FindBlueprintByName(BlueprintName);
}

/**
 * @brief 在资产注册表中按名称精确查找 Blueprint。
 * @param [in] BlueprintName 蓝图名称。
 * @return UBlueprint* 查找结果。
 */
UBlueprint* FUnrealMCPCommonUtils::FindBlueprintByName(const FString& BlueprintName)
{
    if (BlueprintName.IsEmpty())
    {
        return nullptr;
    }

    auto TryLoadBlueprint = [](const FString& AssetPath) -> UBlueprint*
    {
        if (AssetPath.IsEmpty())
        {
            return nullptr;
        }

        if (UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(AssetPath))
        {
            return Cast<UBlueprint>(LoadedObject);
        }

        return LoadObject<UBlueprint>(nullptr, *AssetPath);
    };

    if (BlueprintName.StartsWith(TEXT("/")))
    {
        if (UBlueprint* Blueprint = TryLoadBlueprint(BlueprintName))
        {
            return Blueprint;
        }

        FString AssetPath = BlueprintName;
        const int32 DotIndex = AssetPath.Find(TEXT("."), ESearchCase::CaseSensitive);
        if (DotIndex != INDEX_NONE)
        {
            AssetPath = AssetPath.Left(DotIndex);
        }

        const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);
        if (!AssetName.IsEmpty())
        {
            return TryLoadBlueprint(AssetPath + TEXT(".") + AssetName);
        }

        return nullptr;
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> BlueprintAssets;
    AssetRegistryModule.Get().GetAssetsByClass(FTopLevelAssetPath(UBlueprint::StaticClass()), BlueprintAssets, true);

    TArray<FAssetData> MatchedAssets;
    for (const FAssetData& AssetData : BlueprintAssets)
    {
        const FString AssetName = AssetData.AssetName.ToString();
        const FString PackageName = AssetData.PackageName.ToString();
        const FString ObjectPath = AssetData.GetObjectPathString();

        if (AssetName.Equals(BlueprintName, ESearchCase::IgnoreCase) ||
            PackageName.Equals(BlueprintName, ESearchCase::IgnoreCase) ||
            ObjectPath.Equals(BlueprintName, ESearchCase::IgnoreCase))
        {
            MatchedAssets.Add(AssetData);
        }
    }

    if (MatchedAssets.Num() == 1)
    {
        if (UObject* LoadedObject = MatchedAssets[0].GetAsset())
        {
            return Cast<UBlueprint>(LoadedObject);
        }
    }

    if (UBlueprint* Blueprint = TryLoadBlueprint(TEXT("/Game/Blueprints/") + BlueprintName))
    {
        return Blueprint;
    }

    if (UBlueprint* Blueprint = TryLoadBlueprint(TEXT("/Game/Blueprints/") + BlueprintName + TEXT(".") + BlueprintName))
    {
        return Blueprint;
    }

    return nullptr;
}

/**
 * @brief 查找或创建 Blueprint 事件图。
 * @param [in] Blueprint 目标蓝图。
 * @return UEdGraph* 事件图对象。
 */
UEdGraph* FUnrealMCPCommonUtils::FindOrCreateEventGraph(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return nullptr;
    }
    
    // Try to find the event graph
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph->GetName().Contains(TEXT("EventGraph")))
        {
            return Graph;
        }
    }
    
    // Create a new event graph if none exists
    UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FName(TEXT("EventGraph")), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(Blueprint, NewGraph);
    return NewGraph;
}

// Blueprint node utilities
/**
 * @brief 创建事件节点并放置到指定位置。
 * @param [in] Graph 目标图。
 * @param [in] EventName 事件名。
 * @param [in] Position 节点位置。
 * @return UK2Node_Event* 创建后的节点。
 */
UK2Node_Event* FUnrealMCPCommonUtils::CreateEventNode(UEdGraph* Graph, const FString& EventName, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
    if (!Blueprint)
    {
        return nullptr;
    }
    
    // Check for existing event node with this exact name
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
        if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
        {
            UE_LOG(LogTemp, Display, TEXT("Using existing event node with name %s (ID: %s)"), 
                *EventName, *EventNode->NodeGuid.ToString());
            return EventNode;
        }
    }

    // No existing node found, create a new one
    UK2Node_Event* EventNode = nullptr;
    
    // Find the function to create the event
    UClass* BlueprintClass = Blueprint->GeneratedClass;
    UFunction* EventFunction = BlueprintClass->FindFunctionByName(FName(*EventName));
    
    if (EventFunction)
    {
        EventNode = NewObject<UK2Node_Event>(Graph);
        EventNode->EventReference.SetExternalMember(FName(*EventName), BlueprintClass);
        EventNode->NodePosX = Position.X;
        EventNode->NodePosY = Position.Y;
        Graph->AddNode(EventNode, true);
        EventNode->PostPlacedNewNode();
        EventNode->AllocateDefaultPins();
        UE_LOG(LogTemp, Display, TEXT("Created new event node with name %s (ID: %s)"), 
            *EventName, *EventNode->NodeGuid.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to find function for event name: %s"), *EventName);
    }
    
    return EventNode;
}

/**
 * @brief 创建函数调用节点。
 * @param [in] Graph 目标图。
 * @param [in] Function 目标函数。
 * @param [in] Position 节点位置。
 * @return UK2Node_CallFunction* 创建后的节点。
 */
UK2Node_CallFunction* FUnrealMCPCommonUtils::CreateFunctionCallNode(UEdGraph* Graph, UFunction* Function, const FVector2D& Position)
{
    if (!Graph || !Function)
    {
        return nullptr;
    }
    
    UK2Node_CallFunction* FunctionNode = NewObject<UK2Node_CallFunction>(Graph);
    FunctionNode->SetFromFunction(Function);
    FunctionNode->NodePosX = Position.X;
    FunctionNode->NodePosY = Position.Y;
    Graph->AddNode(FunctionNode, true);
    FunctionNode->CreateNewGuid();
    FunctionNode->PostPlacedNewNode();
    FunctionNode->AllocateDefaultPins();
    
    return FunctionNode;
}

/**
 * @brief 创建变量读取节点。
 * @param [in] Graph 目标图。
 * @param [in] Blueprint 变量所属蓝图。
 * @param [in] VariableName 变量名。
 * @param [in] Position 节点位置。
 * @return UK2Node_VariableGet* 创建后的节点。
 */
UK2Node_VariableGet* FUnrealMCPCommonUtils::CreateVariableGetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position)
{
    if (!Graph || !Blueprint)
    {
        return nullptr;
    }
    
    UK2Node_VariableGet* VariableGetNode = NewObject<UK2Node_VariableGet>(Graph);
    
    FName VarName(*VariableName);
    FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName);
    
    if (Property)
    {
        VariableGetNode->VariableReference.SetFromField<FProperty>(Property, false);
        VariableGetNode->NodePosX = Position.X;
        VariableGetNode->NodePosY = Position.Y;
        Graph->AddNode(VariableGetNode, true);
        VariableGetNode->PostPlacedNewNode();
        VariableGetNode->AllocateDefaultPins();
        
        return VariableGetNode;
    }
    
    return nullptr;
}

/**
 * @brief 创建变量写入节点。
 * @param [in] Graph 目标图。
 * @param [in] Blueprint 变量所属蓝图。
 * @param [in] VariableName 变量名。
 * @param [in] Position 节点位置。
 * @return UK2Node_VariableSet* 创建后的节点。
 */
UK2Node_VariableSet* FUnrealMCPCommonUtils::CreateVariableSetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position)
{
    if (!Graph || !Blueprint)
    {
        return nullptr;
    }
    
    UK2Node_VariableSet* VariableSetNode = NewObject<UK2Node_VariableSet>(Graph);
    
    FName VarName(*VariableName);
    FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName);
    
    if (Property)
    {
        VariableSetNode->VariableReference.SetFromField<FProperty>(Property, false);
        VariableSetNode->NodePosX = Position.X;
        VariableSetNode->NodePosY = Position.Y;
        Graph->AddNode(VariableSetNode, true);
        VariableSetNode->PostPlacedNewNode();
        VariableSetNode->AllocateDefaultPins();
        
        return VariableSetNode;
    }
    
    return nullptr;
}

/**
 * @brief 创建输入动作节点。
 * @param [in] Graph 目标图。
 * @param [in] ActionName 动作名。
 * @param [in] Position 节点位置。
 * @return UK2Node_InputAction* 创建后的节点。
 */
UK2Node_InputAction* FUnrealMCPCommonUtils::CreateInputActionNode(UEdGraph* Graph, const FString& ActionName, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UK2Node_InputAction* InputActionNode = NewObject<UK2Node_InputAction>(Graph);
    InputActionNode->InputActionName = FName(*ActionName);
    InputActionNode->NodePosX = Position.X;
    InputActionNode->NodePosY = Position.Y;
    Graph->AddNode(InputActionNode, true);
    InputActionNode->CreateNewGuid();
    InputActionNode->PostPlacedNewNode();
    InputActionNode->AllocateDefaultPins();
    
    return InputActionNode;
}

/**
 * @brief 创建 Self 引用节点。
 * @param [in] Graph 目标图。
 * @param [in] Position 节点位置。
 * @return UK2Node_Self* 创建后的节点。
 */
UK2Node_Self* FUnrealMCPCommonUtils::CreateSelfReferenceNode(UEdGraph* Graph, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UK2Node_Self* SelfNode = NewObject<UK2Node_Self>(Graph);
    SelfNode->NodePosX = Position.X;
    SelfNode->NodePosY = Position.Y;
    Graph->AddNode(SelfNode, true);
    SelfNode->CreateNewGuid();
    SelfNode->PostPlacedNewNode();
    SelfNode->AllocateDefaultPins();
    
    return SelfNode;
}

/**
 * @brief 连接两个节点的指定引脚。
 * @param [in] Graph 目标图。
 * @param [in] SourceNode 源节点。
 * @param [in] SourcePinName 源引脚名。
 * @param [in] TargetNode 目标节点。
 * @param [in] TargetPinName 目标引脚名。
 * @return bool 连接是否成功。
 */
bool FUnrealMCPCommonUtils::ConnectGraphNodes(UEdGraph* Graph, UEdGraphNode* SourceNode, const FString& SourcePinName, 
                                            UEdGraphNode* TargetNode, const FString& TargetPinName)
{
    if (!Graph || !SourceNode || !TargetNode)
    {
        return false;
    }
    
    UEdGraphPin* SourcePin = FindPin(SourceNode, SourcePinName, EGPD_Output);
    UEdGraphPin* TargetPin = FindPin(TargetNode, TargetPinName, EGPD_Input);
    
    if (SourcePin && TargetPin)
    {
        SourcePin->MakeLinkTo(TargetPin);
        return true;
    }
    
    return false;
}

/**
 * @brief 在节点中查找指定名称/方向的引脚。
 * @param [in] Node 目标节点。
 * @param [in] PinName 引脚名称。
 * @param [in] Direction 引脚方向过滤条件。
 * @return UEdGraphPin* 查找结果。
 */
UEdGraphPin* FUnrealMCPCommonUtils::FindPin(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction)
{
    if (!Node)
    {
        return nullptr;
    }
    
    // Log all pins for debugging
    UE_LOG(LogTemp, Display, TEXT("FindPin: Looking for pin '%s' (Direction: %d) in node '%s'"), 
           *PinName, (int32)Direction, *Node->GetName());
    
    for (UEdGraphPin* Pin : Node->Pins)
    {
        UE_LOG(LogTemp, Display, TEXT("  - Available pin: '%s', Direction: %d, Category: %s"), 
               *Pin->PinName.ToString(), (int32)Pin->Direction, *Pin->PinType.PinCategory.ToString());
    }
    
    // First try exact match
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString() == PinName && (Direction == EGPD_MAX || Pin->Direction == Direction))
        {
            UE_LOG(LogTemp, Display, TEXT("  - Found exact matching pin: '%s'"), *Pin->PinName.ToString());
            return Pin;
        }
    }
    
    // If no exact match and we're looking for a component reference, try case-insensitive match
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase) && 
            (Direction == EGPD_MAX || Pin->Direction == Direction))
        {
            UE_LOG(LogTemp, Display, TEXT("  - Found case-insensitive matching pin: '%s'"), *Pin->PinName.ToString());
            return Pin;
        }
    }
    
    // If we're looking for a component output and didn't find it by name, try to find the first data output pin
    if (Direction == EGPD_Output && Cast<UK2Node_VariableGet>(Node) != nullptr)
    {
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
            {
                UE_LOG(LogTemp, Display, TEXT("  - Found fallback data output pin: '%s'"), *Pin->PinName.ToString());
                return Pin;
            }
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("  - No matching pin found for '%s'"), *PinName);
    return nullptr;
}

// Actor utilities
/**
 * @brief 将 FVector 序列化为 JSON 数组。
 * @param [in] VectorValue 向量值。
 * @return TArray<TSharedPtr<FJsonValue>> JSON 数组。
 */
static TArray<TSharedPtr<FJsonValue>> UnrealMCPVectorToJsonArray(const FVector& VectorValue)
{
    TArray<TSharedPtr<FJsonValue>> JsonArray;
    JsonArray.Add(MakeShared<FJsonValueNumber>(VectorValue.X));
    JsonArray.Add(MakeShared<FJsonValueNumber>(VectorValue.Y));
    JsonArray.Add(MakeShared<FJsonValueNumber>(VectorValue.Z));
    return JsonArray;
}

/**
 * @brief 将 FRotator 序列化为 JSON 数组。
 * @param [in] RotatorValue 旋转值。
 * @return TArray<TSharedPtr<FJsonValue>> JSON 数组。
 */
static TArray<TSharedPtr<FJsonValue>> UnrealMCPRotatorToJsonArray(const FRotator& RotatorValue)
{
    TArray<TSharedPtr<FJsonValue>> JsonArray;
    JsonArray.Add(MakeShared<FJsonValueNumber>(RotatorValue.Pitch));
    JsonArray.Add(MakeShared<FJsonValueNumber>(RotatorValue.Yaw));
    JsonArray.Add(MakeShared<FJsonValueNumber>(RotatorValue.Roll));
    return JsonArray;
}

/**
 * @brief 将 FLinearColor 序列化为 JSON 数组。
 * @param [in] ColorValue 颜色值。
 * @return TArray<TSharedPtr<FJsonValue>> JSON 数组。
 */
static TArray<TSharedPtr<FJsonValue>> UnrealMCPColorToJsonArray(const FLinearColor& ColorValue)
{
    TArray<TSharedPtr<FJsonValue>> JsonArray;
    JsonArray.Add(MakeShared<FJsonValueNumber>(ColorValue.R));
    JsonArray.Add(MakeShared<FJsonValueNumber>(ColorValue.G));
    JsonArray.Add(MakeShared<FJsonValueNumber>(ColorValue.B));
    JsonArray.Add(MakeShared<FJsonValueNumber>(ColorValue.A));
    return JsonArray;
}

/**
 * @brief 将组件移动性枚举转为文本。
 * @param [in] Mobility 组件移动性。
 * @return FString 文本形式的移动性。
 */
static FString UnrealMCPMobilityToString(EComponentMobility::Type Mobility)
{
    switch (Mobility)
    {
    case EComponentMobility::Static:
        return TEXT("Static");
    case EComponentMobility::Stationary:
        return TEXT("Stationary");
    case EComponentMobility::Movable:
        return TEXT("Movable");
    default:
        return TEXT("Unknown");
    }
}

/**
 * @brief 将组件转换为 JSON 值。
 * @param [in] Component 目标组件。
 * @param [in] bDetailed 是否包含详细字段。
 * @return TSharedPtr<FJsonValue> 序列化后的 JSON 值。
 */
TSharedPtr<FJsonValue> FUnrealMCPCommonUtils::ComponentToJson(UActorComponent* Component, bool bDetailed)
{
    if (!Component)
    {
        return MakeShared<FJsonValueNull>();
    }

    TSharedPtr<FJsonObject> ComponentObject = MakeShared<FJsonObject>();
    ComponentObject->SetStringField(TEXT("name"), Component->GetName());
    ComponentObject->SetStringField(TEXT("class"), Component->GetClass()->GetName());
    ComponentObject->SetStringField(TEXT("path"), Component->GetPathName());

    AActor* OwnerActor = Component->GetOwner();
    ComponentObject->SetStringField(TEXT("owner_name"), OwnerActor ? OwnerActor->GetName() : TEXT(""));
    ComponentObject->SetStringField(TEXT("owner_path"), OwnerActor ? OwnerActor->GetPathName() : TEXT(""));
    ComponentObject->SetBoolField(TEXT("active"), Component->IsActive());
    ComponentObject->SetBoolField(TEXT("registered"), Component->IsRegistered());
    ComponentObject->SetBoolField(TEXT("component_tick_enabled"), Component->IsComponentTickEnabled());

    TArray<TSharedPtr<FJsonValue>> ComponentTagArray;
    for (const FName& TagName : Component->ComponentTags)
    {
        ComponentTagArray.Add(MakeShared<FJsonValueString>(TagName.ToString()));
    }
    ComponentObject->SetArrayField(TEXT("component_tags"), ComponentTagArray);

    USceneComponent* SceneComponent = Cast<USceneComponent>(Component);
    if (SceneComponent)
    {
        ComponentObject->SetStringField(TEXT("mobility"), UnrealMCPMobilityToString(SceneComponent->Mobility));
        ComponentObject->SetBoolField(TEXT("visible"), SceneComponent->IsVisible());

        USceneComponent* AttachParent = SceneComponent->GetAttachParent();
        ComponentObject->SetStringField(TEXT("attach_parent_name"), AttachParent ? AttachParent->GetName() : TEXT(""));
        ComponentObject->SetStringField(TEXT("attach_parent_path"), AttachParent ? AttachParent->GetPathName() : TEXT(""));

        ComponentObject->SetArrayField(TEXT("relative_location"), UnrealMCPVectorToJsonArray(SceneComponent->GetRelativeLocation()));
        ComponentObject->SetArrayField(TEXT("relative_rotation"), UnrealMCPRotatorToJsonArray(SceneComponent->GetRelativeRotation()));
        ComponentObject->SetArrayField(TEXT("relative_scale"), UnrealMCPVectorToJsonArray(SceneComponent->GetRelativeScale3D()));

        if (bDetailed)
        {
            ComponentObject->SetArrayField(TEXT("world_location"), UnrealMCPVectorToJsonArray(SceneComponent->GetComponentLocation()));
            ComponentObject->SetArrayField(TEXT("world_rotation"), UnrealMCPRotatorToJsonArray(SceneComponent->GetComponentRotation()));
            ComponentObject->SetArrayField(TEXT("world_scale"), UnrealMCPVectorToJsonArray(SceneComponent->GetComponentScale()));
        }
    }

    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
    if (PrimitiveComponent)
    {
        ComponentObject->SetNumberField(TEXT("collision_enabled"), static_cast<int32>(PrimitiveComponent->GetCollisionEnabled()));
        ComponentObject->SetStringField(TEXT("collision_profile_name"), PrimitiveComponent->GetCollisionProfileName().ToString());
        ComponentObject->SetBoolField(TEXT("generate_overlap_events"), PrimitiveComponent->GetGenerateOverlapEvents());
        ComponentObject->SetBoolField(TEXT("hidden_in_game"), PrimitiveComponent->bHiddenInGame);
        ComponentObject->SetBoolField(TEXT("cast_shadow"), PrimitiveComponent->CastShadow);
    }

    UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component);
    if (StaticMeshComponent)
    {
        UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
        ComponentObject->SetStringField(TEXT("static_mesh_name"), StaticMesh ? StaticMesh->GetName() : TEXT(""));
        ComponentObject->SetStringField(TEXT("static_mesh_path"), StaticMesh ? StaticMesh->GetPathName() : TEXT(""));

        TArray<TSharedPtr<FJsonValue>> MaterialArray;
        const int32 MaterialCount = StaticMeshComponent->GetNumMaterials();
        for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
        {
            UMaterialInterface* Material = StaticMeshComponent->GetMaterial(MaterialIndex);
            TSharedPtr<FJsonObject> MaterialObject = MakeShared<FJsonObject>();
            MaterialObject->SetNumberField(TEXT("index"), MaterialIndex);
            MaterialObject->SetStringField(TEXT("name"), Material ? Material->GetName() : TEXT(""));
            MaterialObject->SetStringField(TEXT("path"), Material ? Material->GetPathName() : TEXT(""));
            MaterialArray.Add(MakeShared<FJsonValueObject>(MaterialObject));
        }
        ComponentObject->SetArrayField(TEXT("materials"), MaterialArray);
    }

    ULightComponent* LightComponent = Cast<ULightComponent>(Component);
    if (LightComponent)
    {
        ComponentObject->SetNumberField(TEXT("intensity"), LightComponent->Intensity);
        ComponentObject->SetArrayField(TEXT("light_color"), UnrealMCPColorToJsonArray(LightComponent->GetLightColor()));
    }

    return MakeShared<FJsonValueObject>(ComponentObject);
}

/**
 * @brief 将 Actor 转换为 JSON 值。
 * @param [in] Actor 目标 Actor。
 * @param [in] bIncludeComponents 是否包含组件数组。
 * @param [in] bDetailedComponents 组件是否包含详细字段。
 * @return TSharedPtr<FJsonValue> 序列化结果。
 */
TSharedPtr<FJsonValue> FUnrealMCPCommonUtils::ActorToJson(AActor* Actor, bool bIncludeComponents, bool bDetailedComponents)
{
    TSharedPtr<FJsonObject> ActorObject = ActorToJsonObject(Actor, false, bIncludeComponents, bDetailedComponents);
    if (ActorObject.IsValid())
    {
        return MakeShared<FJsonValueObject>(ActorObject);
    }

    return MakeShared<FJsonValueNull>();
}

/**
 * @brief 将 Actor 转换为 JSON 对象。
 * @param [in] Actor 目标 Actor。
 * @param [in] bDetailed 是否输出详细属性。
 * @param [in] bIncludeComponents 是否包含组件数组。
 * @param [in] bDetailedComponents 组件是否包含详细字段。
 * @return TSharedPtr<FJsonObject> 序列化结果。
 */
TSharedPtr<FJsonObject> FUnrealMCPCommonUtils::ActorToJsonObject(
    AActor* Actor,
    bool bDetailed,
    bool bIncludeComponents,
    bool bDetailedComponents)
{
    if (!Actor)
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
    ActorObject->SetStringField(TEXT("name"), Actor->GetName());
    ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
    ActorObject->SetStringField(TEXT("path"), Actor->GetPathName());
    ActorObject->SetArrayField(TEXT("location"), UnrealMCPVectorToJsonArray(Actor->GetActorLocation()));
    ActorObject->SetArrayField(TEXT("rotation"), UnrealMCPRotatorToJsonArray(Actor->GetActorRotation()));
    ActorObject->SetArrayField(TEXT("scale"), UnrealMCPVectorToJsonArray(Actor->GetActorScale3D()));

    TArray<TSharedPtr<FJsonValue>> ActorTagArray;
    for (const FName& TagName : Actor->Tags)
    {
        ActorTagArray.Add(MakeShared<FJsonValueString>(TagName.ToString()));
    }
    ActorObject->SetArrayField(TEXT("tags"), ActorTagArray);

    if (Actor->GetLevel())
    {
        ActorObject->SetStringField(TEXT("level_path"), Actor->GetLevel()->GetPathName());
    }

#if WITH_EDITOR
    ActorObject->SetStringField(TEXT("label"), Actor->GetActorLabel());
    ActorObject->SetStringField(TEXT("folder_path"), Actor->GetFolderPath().ToString());
#endif

    if (bDetailed)
    {
        ActorObject->SetBoolField(TEXT("hidden"), Actor->IsHidden());
        ActorObject->SetBoolField(TEXT("tick_enabled"), Actor->IsActorTickEnabled());
    }

    if (bIncludeComponents)
    {
        TArray<UActorComponent*> ActorComponents;
        Actor->GetComponents(ActorComponents);

        TArray<TSharedPtr<FJsonValue>> ComponentArray;
        for (UActorComponent* ActorComponent : ActorComponents)
        {
            ComponentArray.Add(ComponentToJson(ActorComponent, bDetailedComponents));
        }

        ActorObject->SetArrayField(TEXT("components"), ComponentArray);
        ActorObject->SetNumberField(TEXT("component_count"), ComponentArray.Num());
    }

    return ActorObject;
}

/**
 * @brief 在事件图中查找已存在事件节点。
 * @param [in] Graph 目标图。
 * @param [in] EventName 事件名。
 * @return UK2Node_Event* 查找结果。
 */
UK2Node_Event* FUnrealMCPCommonUtils::FindExistingEventNode(UEdGraph* Graph, const FString& EventName)
{
    if (!Graph)
    {
        return nullptr;
    }

    // Look for existing event nodes
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
        if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
        {
            UE_LOG(LogTemp, Display, TEXT("Found existing event node with name: %s"), *EventName);
            return EventNode;
        }
    }

    return nullptr;
}

/**
 * @brief 通过反射设置对象属性值。
 * @param [in] Object 目标对象。
 * @param [in] PropertyName 属性名称。
 * @param [in] Value JSON 值。
 * @param [out] OutErrorMessage 失败时输出错误信息。
 * @return bool 是否设置成功。
 */
bool FUnrealMCPCommonUtils::SetObjectProperty(UObject* Object, const FString& PropertyName, 
                                             const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage)
{
    if (!Object)
    {
        OutErrorMessage = TEXT("Invalid object");
        return false;
    }

    FProperty* Property = Object->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        OutErrorMessage = FString::Printf(TEXT("Property not found: %s"), *PropertyName);
        return false;
    }

    void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Object);
    
    // Handle different property types
    if (Property->IsA<FBoolProperty>())
    {
        ((FBoolProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsBool());
        return true;
    }
    else if (Property->IsA<FIntProperty>())
    {
        int32 IntValue = static_cast<int32>(Value->AsNumber());
        FIntProperty* IntProperty = CastField<FIntProperty>(Property);
        if (IntProperty)
        {
            IntProperty->SetPropertyValue_InContainer(Object, IntValue);
            return true;
        }
    }
    else if (Property->IsA<FFloatProperty>())
    {
        ((FFloatProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsNumber());
        return true;
    }
    else if (Property->IsA<FStrProperty>())
    {
        ((FStrProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsString());
        return true;
    }
    else if (Property->IsA<FNameProperty>())
    {
        ((FNameProperty*)Property)->SetPropertyValue(PropertyAddr, FName(*Value->AsString()));
        return true;
    }
    else if (Property->IsA<FByteProperty>())
    {
        FByteProperty* ByteProp = CastField<FByteProperty>(Property);
        UEnum* EnumDef = ByteProp ? ByteProp->GetIntPropertyEnum() : nullptr;
        
        // If this is a TEnumAsByte property (has associated enum)
        if (EnumDef)
        {
            // Handle numeric value
            if (Value->Type == EJson::Number)
            {
                uint8 ByteValue = static_cast<uint8>(Value->AsNumber());
                ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
                
                UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric value: %d"), 
                      *PropertyName, ByteValue);
                return true;
            }
            // Handle string enum value
            else if (Value->Type == EJson::String)
            {
                FString EnumValueName = Value->AsString();
                
                // Try to convert numeric string to number first
                if (EnumValueName.IsNumeric())
                {
                    uint8 ByteValue = FCString::Atoi(*EnumValueName);
                    ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric string value: %s -> %d"), 
                          *PropertyName, *EnumValueName, ByteValue);
                    return true;
                }
                
                // Handle qualified enum names (e.g., "Player0" or "EAutoReceiveInput::Player0")
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    // Try with full name as fallback
                    EnumValue = EnumDef->GetValueByNameString(Value->AsString());
                }
                
                if (EnumValue != INDEX_NONE)
                {
                    ByteProp->SetPropertyValue(PropertyAddr, static_cast<uint8>(EnumValue));
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to name value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                else
                {
                    // Log all possible enum values for debugging
                    UE_LOG(LogTemp, Warning, TEXT("Could not find enum value for '%s'. Available options:"), *EnumValueName);
                    for (int32 i = 0; i < EnumDef->NumEnums(); i++)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("  - %s (value: %d)"), 
                               *EnumDef->GetNameStringByIndex(i), EnumDef->GetValueByIndex(i));
                    }
                    
                    OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
                    return false;
                }
            }
        }
        else
        {
            // Regular byte property
            uint8 ByteValue = static_cast<uint8>(Value->AsNumber());
            ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
            return true;
        }
    }
    else if (Property->IsA<FEnumProperty>())
    {
        FEnumProperty* EnumProp = CastField<FEnumProperty>(Property);
        UEnum* EnumDef = EnumProp ? EnumProp->GetEnum() : nullptr;
        FNumericProperty* UnderlyingNumericProp = EnumProp ? EnumProp->GetUnderlyingProperty() : nullptr;
        
        if (EnumDef && UnderlyingNumericProp)
        {
            // Handle numeric value
            if (Value->Type == EJson::Number)
            {
                int64 EnumValue = static_cast<int64>(Value->AsNumber());
                UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                
                UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric value: %lld"), 
                      *PropertyName, EnumValue);
                return true;
            }
            // Handle string enum value
            else if (Value->Type == EJson::String)
            {
                FString EnumValueName = Value->AsString();
                
                // Try to convert numeric string to number first
                if (EnumValueName.IsNumeric())
                {
                    int64 EnumValue = FCString::Atoi64(*EnumValueName);
                    UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to numeric string value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                
                // Handle qualified enum names
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    // Try with full name as fallback
                    EnumValue = EnumDef->GetValueByNameString(Value->AsString());
                }
                
                if (EnumValue != INDEX_NONE)
                {
                    UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                    
                    UE_LOG(LogTemp, Display, TEXT("Setting enum property %s to name value: %s -> %lld"), 
                          *PropertyName, *EnumValueName, EnumValue);
                    return true;
                }
                else
                {
                    // Log all possible enum values for debugging
                    UE_LOG(LogTemp, Warning, TEXT("Could not find enum value for '%s'. Available options:"), *EnumValueName);
                    for (int32 i = 0; i < EnumDef->NumEnums(); i++)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("  - %s (value: %d)"), 
                               *EnumDef->GetNameStringByIndex(i), EnumDef->GetValueByIndex(i));
                    }
                    
                    OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
                    return false;
                }
            }
        }
    }
    
    OutErrorMessage = FString::Printf(TEXT("Unsupported property type: %s for property %s"), 
                                    *Property->GetClass()->GetName(), *PropertyName);
    return false;
} 
