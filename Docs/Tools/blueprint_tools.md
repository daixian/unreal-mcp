# Blueprint 资产工具

## 概览

本页描述 Blueprint 资产本身的创建、组件编辑、编译和常用属性配置。  
“Blueprint 图节点编辑”请看 [node_tools.md](node_tools.md)；“Blueprint 实例化到关卡”请看 [actor_tools.md](actor_tools.md) 中的 `spawn_blueprint_actor`。

- Python 注册位置：`Python/tools/blueprint_tools.py`
- Unreal 命令处理位置：`Source/UnrealMCP/Private/Commands/UnrealMCPBlueprintCommands.cpp`

## 工具列表

| 工具 | 关键参数 | 说明 |
| --- | --- | --- |
| `create_blueprint` | `name`、`parent_class`、`path="/Game/Blueprints"` | 创建新的 Blueprint 资产。（本地Python）支持显式目录，以及当前可创建的 Editor Utility/Object/FunctionLibrary 类父类。 |
| `create_child_blueprint` | `name`、`parent_blueprint_name`、`path=""` | 基于已有 Blueprint 创建子 Blueprint。（本地Python）默认创建到父 Blueprint 同目录。 |
| `add_component_to_blueprint` | `blueprint_name`、`component_type`、`component_name`、`parent_name=""`、`location=[]`、`rotation=[]`、`scale=[]`、`component_properties={}` | 为 Blueprint 添加组件。（本地Python）`component_type` 使用类名，不带 `U` 前缀；`parent_name` 可指定父组件。 |
| `remove_component_from_blueprint` | `blueprint_name`、`component_name` | 从 Blueprint 中删除组件。（本地Python）当前不允许删除根组件。 |
| `attach_component_in_blueprint` | `blueprint_name`、`component_name`、`parent_name`、`socket_name=""`、`keep_world_transform=false` | 调整 Blueprint 中组件的父子关系。（本地Python）当前要求目标组件和父组件都为 `SceneComponent`。 |
| `set_static_mesh_properties` | `blueprint_name`、`component_name`、`static_mesh="/Engine/BasicShapes/Cube.Cube"`、`material=""` | 为 `StaticMeshComponent` 设置静态网格和首个材质槽。（本地Python） |
| `set_component_property` | `blueprint_name`、`component_name`、`property_name`、`property_value` | 设置 Blueprint 内某个组件的属性。（本地Python） |
| `set_physics_properties` | `blueprint_name`、`component_name`、`simulate_physics=True`、`gravity_enabled=True`、`mass=1.0`、`linear_damping=0.01`、`angular_damping=0.0` | 批量设置组件常用物理参数。（本地Python） |
| `compile_blueprint` | `blueprint_name` | 编译 Blueprint。（本地Python） |
| `compile_blueprints` | `blueprint_names`、`stop_on_error=false`、`save=false` | 批量编译多个 Blueprint。（本地Python）可按需在编译后保存。 |
| `cleanup_blueprint_for_reparent` | `blueprint_name`、`remove_components=None`、`remove_member_nodes=None`、`refresh_nodes=True`、`compile=True`、`save=True` | 处理改父类之后的残留组件节点和成员节点。（C++主实现） |
| `set_blueprint_property` | `blueprint_name`、`property_name`、`property_value` | 设置 Blueprint Class Default Object 上的属性。（本地Python）支持 CamelCase 和常见 snake_case 属性名解析。 |
| `set_pawn_properties` | `blueprint_name`、`auto_possess_player=""`、`use_controller_rotation_yaw=None`、`use_controller_rotation_pitch=None`、`use_controller_rotation_roll=None`、`can_be_damaged=None` | 批量配置 Pawn/Character 常用属性。（本地Python） |
| `set_game_mode_default_pawn` | `game_mode_name`、`pawn_blueprint_name` | 为 GameMode Blueprint 设置默认 Pawn 类。（本地Python） |
| `delete_blueprint_variable` | `blueprint_name`、`variable_name` | 删除 Blueprint 成员变量。（C++ 主实现）当前 UE5.7 Python 没有等价删除接口。 |
| `remove_unused_blueprint_variables` | `blueprint_name` | 清理 Blueprint 中所有未被图引用的成员变量。（本地Python）会返回删除数量和回编译结果。 |
| `add_blueprint_interface` | `blueprint_name`、`interface_class` | 为 Blueprint 添加接口实现。（C++主实现）支持原生接口类路径，也支持 Blueprint Interface 资产路径。 |
| `set_blueprint_variable_default` | `blueprint_name`、`variable_name`、`default_value` | 设置 Blueprint 成员变量默认值。（本地Python）通过 Blueprint CDO 属性写入并回编译。 |
| `add_blueprint_function` | `blueprint_name`、`function_name` | 为 Blueprint 新增函数图。（本地Python）当前重复创建会直接返回已有函数。 |
| `add_blueprint_macro` | `blueprint_name`、`macro_name` | 为 Blueprint 新增宏图。当前复用 `create_blueprint_graph(graph_type="macro")`。 |
| `delete_blueprint_function` | `blueprint_name`、`function_name` | 删除 Blueprint 中指定函数图。（本地Python）当前不会自动修复该函数的调用节点。 |
| `get_blueprint_compile_errors` | `blueprint_name` | 获取 Blueprint 当前编译错误与警告详情。（C++主实现）会触发一次静默编译并返回消息数组。 |
| `rename_blueprint_member` | `blueprint_name`、`old_name`、`new_name`、`member_type="auto"` | 重命名 Blueprint 成员。（Python+C++桥接）函数图/宏图/普通图走本地Python，变量改名走 C++ 桥接。 |
| `save_blueprint` | `blueprint_name` | 保存 Blueprint 资产。（本地Python） |
| `open_blueprint_editor` | `blueprint_name` | 打开 Blueprint 编辑器。（本地Python） |
| `reparent_blueprint` | `blueprint_name`、`new_parent_class`、`save=true` | 修改 Blueprint 父类。（本地Python）默认保存改动后的资产。 |

## 参数注意事项

### `add_component_to_blueprint`

- `location`、`rotation`、`scale` 在 Python 层要求是长度为 3 的列表。
- 传空列表时会回退到默认值：
  - `location` -> `[0.0, 0.0, 0.0]`
  - `rotation` -> `[0.0, 0.0, 0.0]`
  - `scale` -> `[1.0, 1.0, 1.0]`
- `parent_name` 非空时，会尝试把新组件挂到已有父组件下；找不到会直接报错。

### `remove_component_from_blueprint`

- 当前只允许删除该 Blueprint 自己声明的可删除组件。
- 默认场景根和当前根组件不允许通过此命令直接删除，避免破坏 Blueprint 组件树。

### `attach_component_in_blueprint`

- `component_name` 和 `parent_name` 都必须能解析到 Blueprint 当前组件树中的组件。
- 当前仅支持 `SceneComponent -> SceneComponent` 的挂接；非场景组件会直接报错。
- `socket_name` 非空时会同时写入挂接 Socket；`keep_world_transform=true` 时使用 `KEEP_WORLD` 规则。

### Blueprint 组件实现层

- `add_component_to_blueprint`
- `remove_component_from_blueprint`
- `attach_component_in_blueprint`
- `set_component_property`
- `set_static_mesh_properties`
- `set_physics_properties`
  以上六条命令已迁移到本地 Python，统一通过 `SubobjectDataSubsystem` 处理 Blueprint SCS 组件句柄、模板对象、删除与重新挂接。
- `cleanup_blueprint_for_reparent`
  当前仍保留为 `C++ 主实现`，因为它同时涉及 SCS 节点删除和 Graph 成员节点清理，当前继续沿用现有稳定实现。

### `set_pawn_properties`

Python 工具只会把“明确传入”的字段转成具体设置：

- `auto_possess_player` 非空时才会写入
- `use_controller_rotation_*` 和 `can_be_damaged` 只有在不为 `None` 时才会下发

也就是说，不传某个字段并不等于把它重置为 `false`。

### `set_blueprint_property`

- 优先按传入名查找属性；如果失败，会继续尝试常见的 `CamelCase -> snake_case` 转换。
- 对 `bUseControllerRotationYaw` 这类 UE 布尔成员，也会额外尝试去掉 `b` 前缀后的 Python 属性名。
- 当前已覆盖常见的布尔、整数、浮点、字符串、`Name`、`Vector`、`Rotator`、`LinearColor`、枚举和值为对象路径的引用类型。

### `set_blueprint_variable_default`

- 当前实现会先预编译 Blueprint，确保刚新增的成员变量已经同步到生成类默认对象，再优先通过 `unreal.get_default_object(...)` 解析真实 Blueprint CDO 进行属性写入。
- 写入成功后会再次编译 Blueprint，并在返回里带上 `resolved_property`、`precompile_api` 与 `compile_api`，便于确认实际落点和两次编译入口。
- `delete_blueprint_variable` 仍保留为 `C++ 主实现`，因为 UE5.7 官方 Python API 目前没有公开的“按变量名删除成员变量”接口。
- `remove_unused_blueprint_variables` 走 `本地Python`，因为 UE5.7 官方 Python 已公开 `BlueprintEditorLibrary.remove_unused_variables(...)`，适合直接收敛为脚本化清理入口。
- 当前返回以 `removed_count` 为主。
  - UE5.7 Python 对 `Blueprint.NewVariables` 仍是受保护属性，暂时不能稳定回传被删变量名列表。
- `add_blueprint_interface` 当前保留为 `C++ 主实现`，因为 UE5.7 官方 Python API 没有公开 `FBlueprintEditorUtils::ImplementNewInterface(...)` 的等价入口。

### `add_blueprint_interface`

- `interface_class` 支持三类常见输入：
  - 原生接口短名，例如 `GameplayTagAssetInterface`
  - 原生接口完整类路径，例如 `/Script/GameplayTags.GameplayTagAssetInterface`
  - Blueprint Interface 资产名、包路径或对象路径
- 如果目标 Blueprint 已经实现该接口，命令会返回成功并标记 `already_implemented=true`，不会重复添加。
- 当前实现会在接口添加后自动执行：
  - `ConformImplementedInterfaces`
  - `CompileBlueprint`
  - `SaveLoadedAsset`

### `add_blueprint_function` / `add_blueprint_macro` / `delete_blueprint_function`

- 当前实现直接复用 `BlueprintEditorLibrary.add_function_graph(...)`、既有 `create_blueprint_graph(graph_type="macro")` 与 `remove_function_graph(...)`。
- `add_blueprint_function` 如果目标函数已经存在，会返回成功但标记 `created=false`，避免重复创建。
- `add_blueprint_macro` 是宏图创建的便捷入口，避免调用侧手写 `graph_type="macro"`。
- `delete_blueprint_function` 只删除函数图本身；UE 官方接口不会自动替换或修复旧的调用节点。

### `get_blueprint_compile_errors`

- 当前实现保留为 `C++ 主实现`。
- 原因是 UE5.7 Python 仍拿不到 `FCompilerResultsLog`、`FTokenizedMessage` 与 `Blueprint.Status` 的完整可读接口，无法无损返回错误/警告消息明细。
- 该命令会执行一次静默编译，并返回：
  - `compile_status`
  - `has_errors`
  - `error_count`
  - `warning_count`
  - `message_count`
  - `errors`
  - `warnings`
  - `messages`
- `errors` / `warnings` / `messages` 中每条消息当前至少包含：
  - `index`
  - `severity`
  - `message`

### `rename_blueprint_member`

- `member_type` 当前支持：
  - `auto`
  - `variable`
  - `function`
  - `macro`
  - `graph`
- `variable`
  - 走 `Python + C++桥接`。
  - 原因是 UE5.7 Python 没有公开稳定的成员变量重命名 API，也拿不到 `NewVariables` 的完整元数据；变量实际改名继续使用 `FBlueprintEditorUtils::RenameMemberVariable(...)`。
- `function` / `macro` / `graph`
  - 走本地 Python。
  - 当前复用 `BlueprintEditorLibrary.find_graph(...)` 与 `rename_graph(...)`，完成后会立即回编译 Blueprint。
- 当前已知限制：
  - UE5.7 Python 对“函数图”和“普通图”的公开区分不完整，因此本地 Python 侧目前只对 `macro` 做强识别；`function` 与 `graph` 都会按“非宏图”路径处理。
  - 该命令当前只负责成员改名，不自动保存资产。

### `compile_blueprints`

- `blueprint_names` 必须是非空数组，数组元素支持资产名、包路径或对象路径，解析规则与 `blueprint_name` 一致。
- `stop_on_error=true` 时，遇到第一条失败结果就会终止后续编译。
- `save=true` 时，会在单个 Blueprint 编译成功后立即保存该资产；保存失败会记为该条失败结果。

## 返回说明

Blueprint 资产类工具大多数直接透传 Unreal 桥接结果：

- 成功：`{"status":"success","result":{...}}`
- 失败：`{"status":"error","error":"..."}`

其中较常见的成功结果包括：

- `create_blueprint`：新资产名称、路径、父类、Blueprint 类型等信息
- `create_child_blueprint`：子 Blueprint 名称、路径、父 Blueprint 信息与父类信息
- `add_component_to_blueprint`：组件名、类型与附加结果
- `remove_component_from_blueprint`：删除的组件摘要、删除数量与回编译结果
- `attach_component_in_blueprint`：目标组件、父组件、Socket 与回编译结果
- `compile_blueprint`：编译是否成功
- `compile_blueprints`：批量结果数组、成功/失败数量、是否启用 `stop_on_error` 与 `save`
- `cleanup_blueprint_for_reparent`：移除数量、编译与保存状态
- `set_blueprint_property`：原始属性名、实际写入属性名与写回后的属性值
- `set_pawn_properties`：`results` 中包含每个属性的单独执行结果
- `set_game_mode_default_pawn`：GameMode 名称、Pawn 名称、资产路径与编译结果
- `add_blueprint_interface`：接口类路径、是否新添加、是否已存在、编译与保存状态
- `remove_unused_blueprint_variables`：删除数量与回编译结果
- `set_blueprint_variable_default`：变量名、实际写入属性名、默认值与回编译结果
- `add_blueprint_function`：函数名、函数图路径、是否新建与回编译结果
- `add_blueprint_macro`：宏图名称、图路径与是否新建
- `delete_blueprint_function`：函数名、删除前函数图路径与回编译结果
- `get_blueprint_compile_errors`：编译状态、错误/警告数量与消息数组
- `rename_blueprint_member`：旧名称、新名称、成员类型、图路径变化与回编译结果
- `save_blueprint`：Blueprint 名称与资产路径
- `open_blueprint_editor`：Blueprint 名称与资产路径
- `reparent_blueprint`：旧父类、新父类、保存状态与实现层

## 调用示例

### 创建一个 Actor Blueprint

```json
{
  "tool": "create_blueprint",
  "args": {
    "name": "BP_TestActor",
    "parent_class": "Actor",
    "path": "/Game/Blueprints"
  }
}
```

### 创建一个 Editor Utility Object Blueprint

```json
{
  "tool": "create_blueprint",
  "args": {
    "name": "EUB_TestUtility",
    "parent_class": "EditorUtilityObject",
    "path": "/Game/EditorUtilities"
  }
}
```

### 基于现有 Blueprint 创建子 Blueprint

```json
{
  "tool": "create_child_blueprint",
  "args": {
    "name": "BP_TestActorChild",
    "parent_blueprint_name": "BP_TestActor"
  }
}
```

### `create_blueprint` 额外说明

- `parent_class` 既可以传类短名，例如 `Actor`、`EditorUtilityObject`，也可以传完整类路径，例如 `/Script/Engine.Actor`。
- `EditorUtilityWidget` 不属于 `create_blueprint` 的适用范围；这类资产请改用 `create_umg_widget_blueprint`。
- 已废弃父类会直接返回错误，避免 Unreal 弹出模态对话框导致 MCP 卡住。

### `create_child_blueprint`

- `parent_blueprint_name` 可以传 Blueprint 资产名、包路径或对象路径。
- `path` 为空时，会默认创建到父 Blueprint 所在目录。
- 当前实现会复用父 Blueprint 的生成类作为新资产父类。

### `reparent_blueprint`

- `new_parent_class` 支持类短名或完整类路径，解析规则与 `create_blueprint` 一致。
- `BlueprintEditorLibrary.reparent_blueprint(...)` 会编译 Blueprint，但不会自动保存，所以当前默认 `save=true`。
- 如果目标父类与当前父类相同，命令会返回成功并标记为无需变更。

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

### 调整 Blueprint 组件父子关系

```json
{
  "tool": "attach_component_in_blueprint",
  "args": {
    "blueprint_name": "BP_TestActor",
    "component_name": "Mesh",
    "parent_name": "Root",
    "socket_name": "",
    "keep_world_transform": false
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

### 修改 Blueprint 父类

```json
{
  "tool": "reparent_blueprint",
  "args": {
    "blueprint_name": "BP_TestActorChild",
    "new_parent_class": "Pawn",
    "save": true
  }
}
```

### 新增 Blueprint 函数

```json
{
  "tool": "add_blueprint_function",
  "args": {
    "blueprint_name": "BP_TestActor",
    "function_name": "HandleXRInput"
  }
}
```

### 删除 Blueprint 函数

```json
{
  "tool": "delete_blueprint_function",
  "args": {
    "blueprint_name": "BP_TestActor",
    "function_name": "HandleXRInput"
  }
}
```

### 获取 Blueprint 编译错误

```json
{
  "tool": "get_blueprint_compile_errors",
  "args": {
    "blueprint_name": "BP_TestActor"
  }
}
```

### 为 Blueprint 添加接口

```json
{
  "tool": "add_blueprint_interface",
  "args": {
    "blueprint_name": "BP_TestActor",
    "interface_class": "/Script/GameplayTags.GameplayTagAssetInterface"
  }
}
```

### 清理 Blueprint 未使用变量

```json
{
  "tool": "remove_unused_blueprint_variables",
  "args": {
    "blueprint_name": "BP_TestActor"
  }
}
```

### 重命名 Blueprint 成员

```json
{
  "tool": "rename_blueprint_member",
  "args": {
    "blueprint_name": "BP_TestActor",
    "old_name": "HandleXRInput",
    "new_name": "HandleXRInputV2",
    "member_type": "function"
  }
}
```

### 批量编译 Blueprint

```json
{
  "tool": "compile_blueprints",
  "args": {
    "blueprint_names": [
      "BP_TestActor",
      "/Game/Blueprints/BP_WorldUI.BP_WorldUI"
    ],
    "stop_on_error": false,
    "save": false
  }
}
```
