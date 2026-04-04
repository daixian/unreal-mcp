# 项目工具

## 概览

本页描述项目级全局配置工具，当前重点覆盖传统输入映射、Enhanced Input，以及 `Maps & Modes` 常用项目配置。

- 传统输入映射 `create_input_mapping`、`create_input_axis_mapping`、`list_input_mappings`、`remove_input_mapping` 当前已迁到 `本地Python` 主实现。
- Enhanced Input 资源命令 `create_input_action_asset`、`create_input_mapping_context`、`add_mapping_to_context` 当前已迁到 `本地Python` 主实现。
- 项目设置读取命令 `get_project_setting` 当前已迁到 `本地Python` 主实现。
- 项目设置写入命令 `set_project_setting` 当前采用 `Python + C++桥接`：`InputSettings` 走本地 Python，`GameMapsSettings` 继续保留 C++。

- Python 注册位置：`Python/tools/project_tools.py`
- Unreal 命令处理位置：`Source/UnrealMCP/Private/Commands/UnrealMCPProjectCommands.cpp`

## 工具列表

| 工具 | 关键参数 | 说明 |
| --- | --- | --- |
| `create_input_mapping` | `action_name`、`key`、`input_type` | 在项目默认输入设置中创建传统输入映射（本地Python）。 |
| `create_input_axis_mapping` | `mapping_name`、`key`、`scale` | 创建传统 Axis Mapping（本地Python）。 |
| `list_input_mappings` | `input_type`、`mapping_name`、`key` | 查询传统输入映射（本地Python）。 |
| `remove_input_mapping` | `mapping_name`、`input_type` | 删除传统输入映射（本地Python）。 |
| `create_input_action_asset` | `action_name`、`path`、`value_type` | 创建 Enhanced Input `UInputAction` 资源（本地Python）。 |
| `create_input_mapping_context` | `context_name`、`path` | 创建 Enhanced Input `UInputMappingContext` 资源（本地Python）。 |
| `add_mapping_to_context` | `mapping_context`、`input_action`、`key` | 往 Mapping Context 中添加按键映射（本地Python）。 |
| `assign_mapping_context` | `mapping_context`、`priority`、`player_index/world_type` | 在运行中的本地玩家上分配 Mapping Context（本地Python）。 |
| `get_project_setting` | `settings_class`、`property_name` | 读取受支持项目设置对象上的单个属性（本地Python）。 |
| `set_project_setting` | `settings_class`、`property_name`、`value` | 写入受支持项目设置对象上的单个属性；`InputSettings` 走本地Python，`GameMapsSettings` 走 C++。 |
| `set_default_maps` | `game_default_map`、`editor_startup_map` | 设置项目默认地图相关配置。 |
| `set_game_framework_defaults` | `game_instance_class`、`game_mode_class` | 设置项目级 Game Framework 默认类，并可改写目标 GameMode Blueprint 的类默认值。 |

## 参数注意事项

### 传统输入映射

- `create_input_mapping` 支持 `input_type="Action|Axis"`。
- `Action` 模式下支持 `shift`、`ctrl`、`alt`、`cmd`。
- `Axis` 模式下支持 `scale`。
- 这 4 条命令当前都直接通过 UE5.7 Python `InputSettings` 接口持久化，调用后会执行 `force_rebuild_keymaps()` 与 `save_key_mappings()`。
- `key` 会先经过 `Key.import_text(...)` 和 `InputLibrary.key_is_valid(...)` 校验；非法按键名会直接报错，不会写脏配置。

### Enhanced Input 资源

- `create_input_action_asset.value_type` 支持 `Boolean`、`Axis1D`、`Axis2D`、`Axis3D`。
- `create_input_action_asset.accumulation_behavior` 支持 `TakeHighestAbsoluteValue`、`Cumulative`。
- `create_input_mapping_context.registration_tracking_mode` 支持 `Untracked`、`CountRegistrations`。
- 这 3 条命令当前都直接通过 UE5.7 Python 的 `AssetTools.create_asset(...)`、资源属性写入与 `EditorAssetLibrary.save_asset(...)` 持久化。
- `mapping_context`、`input_action` 参数优先传完整资源路径；若只传名字，要求资源名在当前工程中唯一。

### 运行时分配

- `assign_mapping_context` 需要运行中的 `PIE` 或 `VR Preview` 世界，`world_type="pie"` 最稳妥。
- 如果不传 `name/actor_path`，会按 `player_index` 找本地 `PlayerController`。
- 如果传 `name/actor_path`，目标必须是 `Pawn` 或 `PlayerController`。
- 当前本地 Python 实现会优先按 `player_index` 解析 `LocalPlayer` 与 `EnhancedInputLocalPlayerSubsystem`；即使当前帧还没有可见的 `PlayerController`，也可以继续分配 Mapping Context。

### 项目设置

- `get_project_setting` / `set_project_setting` 当前支持 `GameMapsSettings`、`InputSettings` 两类设置对象。
- `get_project_setting` 当前在本地 Python 中按 `get_editor_property(...)` 读取，并显式序列化常见基础类型、`SoftObjectPath` 与 UObject 路径。
- `set_project_setting` 当前对 `InputSettings` 直接使用 UE5.7 Python `set_editor_property(...) + force_rebuild_keymaps() + save_key_mappings()` 持久化；对 `GameMapsSettings` 继续保留 C++ 写入与 `SaveConfig()`。
- `set_default_maps` 传入的地图路径必须是存在的 `/Game/...` 资源路径，可传包路径或对象路径。
- `set_game_framework_defaults` 中：
  - `game_instance_class`、`game_mode_class`、`server_game_mode_class` 会写入 `GameMapsSettings` 配置。
  - `default_pawn_class`、`hud_class`、`player_controller_class`、`game_state_class`、`spectator_class`、`replay_spectator_player_controller_class` 会修改目标 `GameMode Blueprint` 的类默认值。
  - 若要修改上述 GameMode 默认类，目标 GameMode 必须是 Blueprint 资源，不能只是原生 C++ 类。

## 返回说明

成功结果通常包含：

- 资源创建类工具：资源名、`asset_path`
- Context 编辑类工具：`mapping_count`
- 运行时分配类工具：`controller_name`、`world_path`、`resolved_world_type`
- 项目设置读写类工具：`settings_class`、`property_name`、`value`、`exported_value`
- 传统输入映射类工具：`mapping_name`、`input_type`，并在查询结果中返回 `action_mappings` / `axis_mappings`

其中 `get_project_setting` / `set_project_setting` 成功结果还会额外返回：

- `property_cpp_type`
- `config_file`
- `implementation`

失败时统一返回：

```json
{
  "status": "error",
  "error": "..."
}
```
