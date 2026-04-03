# 项目工具

## 概览

本页描述项目级全局配置工具，当前重点覆盖传统输入映射、Enhanced Input，以及 `Maps & Modes` 常用项目配置。

- Python 注册位置：`Python/tools/project_tools.py`
- Unreal 命令处理位置：`Source/UnrealMCP/Private/Commands/UnrealMCPProjectCommands.cpp`

## 工具列表

| 工具 | 关键参数 | 说明 |
| --- | --- | --- |
| `create_input_mapping` | `action_name`、`key`、`input_type` | 在项目默认输入设置中创建传统输入映射。 |
| `create_input_axis_mapping` | `mapping_name`、`key`、`scale` | 创建传统 Axis Mapping。 |
| `list_input_mappings` | `input_type`、`mapping_name`、`key` | 查询传统输入映射。 |
| `remove_input_mapping` | `mapping_name`、`input_type` | 删除传统输入映射。 |
| `create_input_action_asset` | `action_name`、`path`、`value_type` | 创建 Enhanced Input `UInputAction` 资源。 |
| `create_input_mapping_context` | `context_name`、`path` | 创建 Enhanced Input `UInputMappingContext` 资源。 |
| `add_mapping_to_context` | `mapping_context`、`input_action`、`key` | 往 Mapping Context 中添加按键映射。 |
| `assign_mapping_context` | `mapping_context`、`priority`、`player_index/world_type` | 在运行中的本地玩家上分配 Mapping Context。 |
| `get_project_setting` | `settings_class`、`property_name` | 读取受支持项目设置对象上的单个属性。 |
| `set_project_setting` | `settings_class`、`property_name`、`value` | 写入受支持项目设置对象上的单个属性。 |
| `set_default_maps` | `game_default_map`、`editor_startup_map` | 设置项目默认地图相关配置。 |
| `set_game_framework_defaults` | `game_instance_class`、`game_mode_class` | 设置项目级 Game Framework 默认类，并可改写目标 GameMode Blueprint 的类默认值。 |

## 参数注意事项

### 传统输入映射

- `create_input_mapping` 支持 `input_type="Action|Axis"`。
- `Action` 模式下支持 `shift`、`ctrl`、`alt`、`cmd`。
- `Axis` 模式下支持 `scale`。

### Enhanced Input 资源

- `create_input_action_asset.value_type` 支持 `Boolean`、`Axis1D`、`Axis2D`、`Axis3D`。
- `create_input_action_asset.accumulation_behavior` 支持 `TakeHighestAbsoluteValue`、`Cumulative`。
- `create_input_mapping_context.registration_tracking_mode` 支持 `Untracked`、`CountRegistrations`。
- `mapping_context`、`input_action` 参数优先传完整资源路径；若只传名字，要求资源名在当前工程中唯一。

### 运行时分配

- `assign_mapping_context` 需要运行中的 `PIE` 或 `VR Preview` 世界，`world_type="pie"` 最稳妥。
- 如果不传 `name/actor_path`，会按 `player_index` 找本地 `PlayerController`。
- 如果传 `name/actor_path`，目标必须是 `Pawn` 或 `PlayerController`。

### 项目设置

- `get_project_setting` / `set_project_setting` 当前支持 `GameMapsSettings`、`InputSettings` 两类设置对象。
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

失败时统一返回：

```json
{
  "status": "error",
  "error": "..."
}
```
