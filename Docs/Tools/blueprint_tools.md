# Blueprint 资产工具

## 概览

本页描述 Blueprint 资产本身的创建、组件编辑、编译和常用属性配置。  
“Blueprint 图节点编辑”请看 [node_tools.md](node_tools.md)；“Blueprint 实例化到关卡”请看 [actor_tools.md](actor_tools.md) 中的 `spawn_blueprint_actor`。

- Python 注册位置：`Python/tools/blueprint_tools.py`
- Unreal 命令处理位置：`Source/UnrealMCP/Private/Commands/UnrealMCPBlueprintCommands.cpp`

## 工具列表

| 工具 | 关键参数 | 说明 |
| --- | --- | --- |
| `create_blueprint` | `name`、`parent_class` | 创建新的 Blueprint 资产。 |
| `add_component_to_blueprint` | `blueprint_name`、`component_type`、`component_name`、`location=[]`、`rotation=[]`、`scale=[]`、`component_properties={}` | 为 Blueprint 添加组件。`component_type` 使用类名，不带 `U` 前缀。 |
| `set_static_mesh_properties` | `blueprint_name`、`component_name`、`static_mesh="/Engine/BasicShapes/Cube.Cube"` | 为 `StaticMeshComponent` 设置静态网格。 |
| `set_component_property` | `blueprint_name`、`component_name`、`property_name`、`property_value` | 设置 Blueprint 内某个组件的属性。 |
| `set_physics_properties` | `blueprint_name`、`component_name`、`simulate_physics=True`、`gravity_enabled=True`、`mass=1.0`、`linear_damping=0.01`、`angular_damping=0.0` | 批量设置组件常用物理参数。 |
| `compile_blueprint` | `blueprint_name` | 编译 Blueprint。 |
| `cleanup_blueprint_for_reparent` | `blueprint_name`、`remove_components=None`、`remove_member_nodes=None`、`refresh_nodes=True`、`compile=True`、`save=True` | 处理改父类之后的残留组件节点和成员节点。 |
| `set_blueprint_property` | `blueprint_name`、`property_name`、`property_value` | 设置 Blueprint Class Default Object 上的属性。 |
| `set_pawn_properties` | `blueprint_name`、`auto_possess_player=""`、`use_controller_rotation_yaw=None`、`use_controller_rotation_pitch=None`、`use_controller_rotation_roll=None`、`can_be_damaged=None` | 批量配置 Pawn/Character 常用属性。 |
| `set_game_mode_default_pawn` | `game_mode_name`、`pawn_blueprint_name` | 为 GameMode Blueprint 设置默认 Pawn 类。 |

## 参数注意事项

### `add_component_to_blueprint`

- `location`、`rotation`、`scale` 在 Python 层要求是长度为 3 的列表。
- 传空列表时会回退到默认值：
  - `location` -> `[0.0, 0.0, 0.0]`
  - `rotation` -> `[0.0, 0.0, 0.0]`
  - `scale` -> `[1.0, 1.0, 1.0]`

### `set_pawn_properties`

Python 工具只会把“明确传入”的字段转成具体设置：

- `auto_possess_player` 非空时才会写入
- `use_controller_rotation_*` 和 `can_be_damaged` 只有在不为 `None` 时才会下发

也就是说，不传某个字段并不等于把它重置为 `false`。

## 返回说明

Blueprint 资产类工具大多数直接透传 Unreal 桥接结果：

- 成功：`{"status":"success","result":{...}}`
- 失败：`{"status":"error","error":"..."}`

其中较常见的成功结果包括：

- `create_blueprint`：新资产名称、路径、父类等信息
- `add_component_to_blueprint`：组件名、类型与附加结果
- `compile_blueprint`：编译是否成功
- `cleanup_blueprint_for_reparent`：移除数量、编译与保存状态
- `set_pawn_properties`：`results` 中包含每个属性的单独执行结果

## 调用示例

### 创建一个 Actor Blueprint

```json
{
  "tool": "create_blueprint",
  "args": {
    "name": "BP_TestActor",
    "parent_class": "Actor"
  }
}
```

### 给 Blueprint 添加静态网格组件

```json
{
  "tool": "add_component_to_blueprint",
  "args": {
    "blueprint_name": "BP_TestActor",
    "component_type": "StaticMeshComponent",
    "component_name": "Mesh",
    "location": [0, 0, 0],
    "rotation": [0, 0, 0],
    "scale": [1, 1, 1]
  }
}
```

### 清理改父类后的残留节点

```json
{
  "tool": "cleanup_blueprint_for_reparent",
  "args": {
    "blueprint_name": "BP_TestActor",
    "remove_components": ["OldMesh"],
    "remove_member_nodes": ["OldMesh"],
    "refresh_nodes": true,
    "compile": true,
    "save": true
  }
}
```
