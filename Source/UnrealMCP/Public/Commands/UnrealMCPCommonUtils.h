#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/** @brief 前向声明：Actor 类型。 */
class AActor;
/** @brief 前向声明：Actor 组件类型。 */
class UActorComponent;
/** @brief 前向声明：Blueprint 类型。 */
class UBlueprint;
/** @brief 前向声明：蓝图图对象。 */
class UEdGraph;
/** @brief 前向声明：蓝图图节点。 */
class UEdGraphNode;
/** @brief 前向声明：蓝图图引脚。 */
class UEdGraphPin;
/** @brief 前向声明：事件节点。 */
class UK2Node_Event;
/** @brief 前向声明：函数调用节点。 */
class UK2Node_CallFunction;
/** @brief 前向声明：变量读取节点。 */
class UK2Node_VariableGet;
/** @brief 前向声明：变量写入节点。 */
class UK2Node_VariableSet;
/** @brief 前向声明：输入动作节点。 */
class UK2Node_InputAction;
/** @brief 前向声明：Self 引用节点。 */
class UK2Node_Self;
/** @brief 前向声明：反射函数对象。 */
class UFunction;

/**
 * @brief UnrealMCP 命令公共工具集合。
 * @note 提供 JSON、Actor、Blueprint、图节点与属性反射等通用能力。
 */
class UNREALMCP_API FUnrealMCPCommonUtils
{
public:
    /**
     * @brief 创建标准错误响应对象。
     * @param [in] Message 错误信息文本。
     * @return TSharedPtr<FJsonObject> 错误响应 JSON。
     */
    static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& Message);

    /**
     * @brief 创建标准成功响应对象。
     * @param [in] Data 可选业务数据。
     * @return TSharedPtr<FJsonObject> 成功响应 JSON。
     */
    static TSharedPtr<FJsonObject> CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data = nullptr);

    /**
     * @brief 调用插件 `Content/Python` 下的本地 Python 命令实现。
     * @param [in] ModuleName Python 模块名，例如 `commands.assets.asset_commands`。
     * @param [in] FunctionName Python 函数名。
     * @param [in] CommandName 业务命令名，会原样传给 Python 侧分发器。
     * @param [in] Params 命令参数 JSON。
     * @return TSharedPtr<FJsonObject> Python 返回的 JSON 结果；失败时返回标准错误响应。
     * @note 该接口用于把“厚 C++ 逻辑”迁移为“薄 C++ 外壳 + 本地 Python 实现”。
     */
    static TSharedPtr<FJsonObject> ExecuteLocalPythonCommand(
        const FString& ModuleName,
        const FString& FunctionName,
        const FString& CommandName,
        const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 从 JSON 字段读取 int 数组。
     * @param [in] JsonObject 源 JSON 对象。
     * @param [in] FieldName 字段名称。
     * @param [out] OutArray 输出整型数组。
     */
    static void GetIntArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<int32>& OutArray);

    /**
     * @brief 从 JSON 字段读取 float 数组。
     * @param [in] JsonObject 源 JSON 对象。
     * @param [in] FieldName 字段名称。
     * @param [out] OutArray 输出浮点数组。
     */
    static void GetFloatArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<float>& OutArray);

    /**
     * @brief 从 JSON 字段解析二维向量。
     * @param [in] JsonObject 源 JSON 对象。
     * @param [in] FieldName 字段名称。
     * @return FVector2D 解析结果。
     */
    static FVector2D GetVector2DFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName);

    /**
     * @brief 从 JSON 字段解析三维向量。
     * @param [in] JsonObject 源 JSON 对象。
     * @param [in] FieldName 字段名称。
     * @return FVector 解析结果。
     */
    static FVector GetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName);

    /**
     * @brief 从 JSON 字段解析旋转器。
     * @param [in] JsonObject 源 JSON 对象。
     * @param [in] FieldName 字段名称。
     * @return FRotator 解析结果。
     */
    static FRotator GetRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName);
    
    /**
     * @brief 将 Actor 转换为 JSON 值。
     * @param [in] Actor 目标 Actor 指针。
     * @param [in] bIncludeComponents 是否包含组件数组。
     * @param [in] bDetailedComponents 组件是否包含详细字段。
     * @return TSharedPtr<FJsonValue> 序列化后的 JSON 值。
     */
    static TSharedPtr<FJsonValue> ActorToJson(AActor* Actor, bool bIncludeComponents = false, bool bDetailedComponents = true);

    /**
     * @brief 将 Actor 转换为 JSON 对象。
     * @param [in] Actor 目标 Actor 指针。
     * @param [in] bDetailed 是否包含详细属性。
     * @param [in] bIncludeComponents 是否包含组件数组。
     * @param [in] bDetailedComponents 组件是否包含详细字段。
     * @return TSharedPtr<FJsonObject> 序列化后的 JSON 对象。
     */
    static TSharedPtr<FJsonObject> ActorToJsonObject(
        AActor* Actor,
        bool bDetailed = false,
        bool bIncludeComponents = false,
        bool bDetailedComponents = true);

    /**
     * @brief 将组件转换为 JSON 值。
     * @param [in] Component 目标组件。
     * @param [in] bDetailed 是否包含详细字段。
     * @return TSharedPtr<FJsonValue> 序列化后的 JSON 值。
     */
    static TSharedPtr<FJsonValue> ComponentToJson(UActorComponent* Component, bool bDetailed = true);
    
    /**
     * @brief 按名称查找 Blueprint 资产。
     * @param [in] BlueprintName 蓝图名称或路径关键字。
     * @return UBlueprint* 找到则返回指针，否则返回 nullptr。
     */
    static UBlueprint* FindBlueprint(const FString& BlueprintName);

    /**
     * @brief 按精确名称查找 Blueprint 资产。
     * @param [in] BlueprintName 蓝图名称。
     * @return UBlueprint* 找到则返回指针，否则返回 nullptr。
     */
    static UBlueprint* FindBlueprintByName(const FString& BlueprintName);

    /**
     * @brief 查找或创建 Blueprint 的事件图。
     * @param [in] Blueprint 目标 Blueprint。
     * @return UEdGraph* 事件图对象。
     */
    static UEdGraph* FindOrCreateEventGraph(UBlueprint* Blueprint);
    
    /**
     * @brief 在图中创建事件节点。
     * @param [in] Graph 目标图对象。
     * @param [in] EventName 事件名称。
     * @param [in] Position 节点位置。
     * @return UK2Node_Event* 创建后的事件节点。
     */
    static UK2Node_Event* CreateEventNode(UEdGraph* Graph, const FString& EventName, const FVector2D& Position);

    /**
     * @brief 在图中创建函数调用节点。
     * @param [in] Graph 目标图对象。
     * @param [in] Function 函数反射对象。
     * @param [in] Position 节点位置。
     * @return UK2Node_CallFunction* 创建后的函数调用节点。
     */
    static UK2Node_CallFunction* CreateFunctionCallNode(UEdGraph* Graph, UFunction* Function, const FVector2D& Position);

    /**
     * @brief 在图中创建变量读取节点。
     * @param [in] Graph 目标图对象。
     * @param [in] Blueprint 变量所属蓝图。
     * @param [in] VariableName 变量名。
     * @param [in] Position 节点位置。
     * @return UK2Node_VariableGet* 创建后的变量读取节点。
     */
    static UK2Node_VariableGet* CreateVariableGetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position);

    /**
     * @brief 在图中创建变量写入节点。
     * @param [in] Graph 目标图对象。
     * @param [in] Blueprint 变量所属蓝图。
     * @param [in] VariableName 变量名。
     * @param [in] Position 节点位置。
     * @return UK2Node_VariableSet* 创建后的变量写入节点。
     */
    static UK2Node_VariableSet* CreateVariableSetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position);

    /**
     * @brief 在图中创建输入动作节点。
     * @param [in] Graph 目标图对象。
     * @param [in] ActionName 输入动作名。
     * @param [in] Position 节点位置。
     * @return UK2Node_InputAction* 创建后的输入动作节点。
     */
    static UK2Node_InputAction* CreateInputActionNode(UEdGraph* Graph, const FString& ActionName, const FVector2D& Position);

    /**
     * @brief 在图中创建 Self 引用节点。
     * @param [in] Graph 目标图对象。
     * @param [in] Position 节点位置。
     * @return UK2Node_Self* 创建后的 Self 节点。
     */
    static UK2Node_Self* CreateSelfReferenceNode(UEdGraph* Graph, const FVector2D& Position);

    /**
     * @brief 连接两个图节点的指定引脚。
     * @param [in] Graph 目标图对象。
     * @param [in] SourceNode 源节点。
     * @param [in] SourcePinName 源引脚名。
     * @param [in] TargetNode 目标节点。
     * @param [in] TargetPinName 目标引脚名。
     * @return bool 连接成功返回 true。
     */
    static bool ConnectGraphNodes(UEdGraph* Graph, UEdGraphNode* SourceNode, const FString& SourcePinName, 
                                UEdGraphNode* TargetNode, const FString& TargetPinName);

    /**
     * @brief 在节点中按名称查找引脚。
     * @param [in] Node 目标节点。
     * @param [in] PinName 引脚名称。
     * @param [in] Direction 引脚方向过滤（默认不过滤）。
     * @return UEdGraphPin* 找到则返回指针，否则返回 nullptr。
     */
    static UEdGraphPin* FindPin(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction = EGPD_MAX);

    /**
     * @brief 查找图中已存在的指定事件节点。
     * @param [in] Graph 目标图对象。
     * @param [in] EventName 事件名。
     * @return UK2Node_Event* 找到则返回指针，否则返回 nullptr。
     */
    static UK2Node_Event* FindExistingEventNode(UEdGraph* Graph, const FString& EventName);

    /**
     * @brief 通过反射设置对象属性值。
     * @param [in] Object 目标对象。
     * @param [in] PropertyName 属性名称。
     * @param [in] Value 要写入的 JSON 值。
     * @param [out] OutErrorMessage 失败时返回错误信息。
     * @return bool 设置成功返回 true。
     */
    static bool SetObjectProperty(UObject* Object, const FString& PropertyName, 
                                 const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage);
};
