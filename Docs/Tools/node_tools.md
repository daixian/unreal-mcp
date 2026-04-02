# Blueprint 图节点工具

## 概览

本页描述 Blueprint 图节点级别的编辑能力，包括事件节点、函数节点、变量节点、连线以及通用节点生成。

- Python 注册位置：`Python/tools/node_tools.py`
- Unreal 命令处理位置：`Source/UnrealMCP/Private/Commands/UnrealMCPBlueprintNodeCommands.cpp`

## 工具列表

| 工具 | 关键参数 | 说明 |
| --- | --- | --- |
| `add_blueprint_event_node` | `blueprint_name`、`event_name`、`node_position=None` | 添加事件节点。标准事件推荐使用 `ReceiveBeginPlay`、`ReceiveTick` 这一类实际事件名。 |
| `add_blueprint_input_action_node` | `blueprint_name`、`action_name`、`node_position=None` | 添加输入动作节点。 |
| `add_blueprint_function_node` | `blueprint_name`、`target`、`function_name`、`params=None`、`node_position=None` | 添加函数调用节点。`target` 可为组件名或 `self`。 |
| `connect_blueprint_nodes` | `blueprint_name`、`source_node_id`、`source_pin`、`target_node_id`、`target_pin` | 连接两个节点的引脚。 |
| `add_blueprint_variable` | `blueprint_name`、`variable_name`、`variable_type`、`is_exposed=False` | 添加 Blueprint 变量。当前对外工具不支持直接设置默认值。 |
| `add_blueprint_get_self_component_reference` | `blueprint_name`、`component_name`、`node_position=None` | 添加“当前 Blueprint 自有组件”的引用节点。 |
| `add_blueprint_self_reference` | `blueprint_name`、`node_position=None` | 添加 `Self` 引用节点。 |
| `find_blueprint_nodes` | `blueprint_name`、`node_type=None`、`event_type=None`、`include_details=False` | 查找图节点。Python 层会同时把 `event_type` 传给 `event_name` 和 `event_type` 兼容字段。 |
| `spawn_blueprint_node` | `blueprint_name`，以及 `node_kind` / `node_class` / `function_name` / `target` / `variable_name` / `action_name` / `component_name` / `event_name` / `class_name` / `class_path` / `widget_class` / `params` / `node_position` | 通用节点生成接口，可直接表达常见语义节点，也可走反射式节点类。 |
| `describe_blueprint_node` | `blueprint_name`、`node_id` | 读取指定节点及其 Pin 详情。 |
| `set_blueprint_pin_default` | `blueprint_name`、`node_id`、`pin_name`、`default_value=None`、`object_path=None`、`pin_direction="input"` | 设置节点 Pin 的默认值。 |
| `delete_blueprint_node` | `blueprint_name`、`node_id` | 删除单个图节点。 |

## 参数注意事项

### `add_blueprint_event_node`

这里的参数名是 `event_name`，不是旧文档里的 `event_type`。  
如果是标准生命周期事件，应该传 Unreal 实际识别的事件名，例如：

- `ReceiveBeginPlay`
- `ReceiveTick`

### `add_blueprint_variable`

当前公开工具参数只有：

- `blueprint_name`
- `variable_name`
- `variable_type`
- `is_exposed`

旧文档中提到的 `default_value` 并不是当前 Python MCP 工具的正式参数。

### `spawn_blueprint_node`

这是当前最灵活的节点接口，适合以下几类场景：

- 已知语义：例如 `node_kind="event"`、`node_kind="function_call"`、`node_kind="input_action"`
- 已知节点类：例如直接传 `node_class`
- 需要创建 Widget / 构造对象节点：结合 `class_name`、`class_path`、`widget_class`
- 需要初始化输入 Pin 默认值：传 `params`

## 返回说明

这些工具大多直接透传 Unreal 桥接结果：

- 成功：`{"status":"success","result":{...}}`
- 失败：`{"status":"error","error":"..."}`

节点创建类工具通常会在 `result` 中包含：

- `node_id`
- 节点名、节点类型或相关元信息

查询与描述类工具通常会在 `result` 中包含：

- 节点列表、匹配数量
- 节点 Pin 元数据
- 连接信息或默认值信息

## 调用示例

### 添加 BeginPlay 事件节点

```json
{
  "tool": "add_blueprint_event_node",
  "args": {
    "blueprint_name": "BP_TestActor",
    "event_name": "ReceiveBeginPlay",
    "node_position": [100, 100]
  }
}
```

### 添加一个函数调用节点

```json
{
  "tool": "add_blueprint_function_node",
  "args": {
    "blueprint_name": "BP_TestActor",
    "target": "self",
    "function_name": "K2_SetActorLocation",
    "params": {
      "NewLocation": [0, 0, 200]
    },
    "node_position": [320, 160]
  }
}
```

### 设置某个 Pin 的默认值

```json
{
  "tool": "set_blueprint_pin_default",
  "args": {
    "blueprint_name": "BP_TestActor",
    "node_id": "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX",
    "pin_name": "NewLocation",
    "default_value": "0,0,200"
  }
}
```
