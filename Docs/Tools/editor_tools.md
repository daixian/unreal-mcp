# 编辑器与运行控制工具

## 概览

本页描述编辑器级控制能力，包括关卡读写、PIE、Live Coding、视口控制与截图。

- Python 注册位置：`Python/tools/editor_tools.py`
- Unreal 命令处理位置：`Source/UnrealMCP/Private/Commands/UnrealMCPEditorCommands.cpp`

## 工具列表

| 工具 | 关键参数 | 说明 |
| --- | --- | --- |
| `make_directory` | `directory_path` | 在 Content Browser 中创建目录。路径通常应以 `/Game` 开头。 |
| `duplicate_asset` | `source_asset_path`、`destination_asset_path`、`overwrite=False` | 复制一个资产到新路径。 |
| `load_level` | `level_path` | 按内容路径加载关卡（本地Python）。 |
| `save_current_level` | 无 | 保存当前编辑器关卡（本地Python）。 |
| `start_pie` | `simulate=False` | 请求启动 PIE。返回 `already_playing`、`play_world_available` 等状态字段。 |
| `start_vr_preview` | 无 | 请求启动 VR Preview。返回 `request_queued`、`is_vr_preview` 等状态字段。 |
| `start_standalone_game` | `map_override=""`、`additional_command_line_parameters=""` | 请求以本机新进程启动 Standalone Game。 |
| `stop_pie` | 无 | 请求结束当前 PIE。 |
| `get_play_state` | 无 | 查询当前编辑器是否处于 `pie` / `vr_preview` / `standalone_game` / `queued` / `stopped`。 |
| `start_live_coding` | `show_console=True` | 启用当前编辑器会话的 Live Coding。 |
| `compile_live_coding` | `wait_for_completion=False`、`show_console=True` | 触发 Live Coding 编译。 |
| `get_live_coding_state` | 无 | 查询 Live Coding 当前状态。 |
| `get_selected_actors` | `include_components=False`、`detailed_components=True` | 读取当前编辑器中选中的 Actor 列表（本地Python）。 |
| `get_editor_selection` | `include_components=False`、`detailed_components=True`、`include_tags=False` | 读取当前编辑器中同时选中的 Actor 与资产（本地Python）。 |
| `get_data_layers` | `world_type="editor"` | 读取当前编辑器世界中的 Data Layer 实例列表（本地Python）。 |
| `create_data_layer` | `data_layer_name`、`destination_path="/Game"`、`parent_data_layer=""`、`world_data_layers_path=""`、`world_type="editor"` | 创建 DataLayerAsset 并在当前编辑器世界生成对应实例（本地Python）。 |
| `set_actor_data_layers` | `data_layers`、`name=""`、`actor_path=""`、`world_type="editor"` | 设置指定编辑器 Actor 的 Data Layer 归属（本地Python）。 |
| `set_data_layer_state` | `data_layer=""`、`is_visible=None`、`is_loaded_in_editor=None`、`initial_runtime_state=""`、`world_type="editor"` | 设置指定编辑器 Data Layer 的可见性、加载态和初始运行态（本地Python）。 |
| `focus_viewport` | `target=None`、`location=None`、`distance=1000.0`、`orientation=None` | 聚焦到指定 Actor 或坐标（本地Python）。`target` 与 `location` 至少要传一个。 |
| `line_trace` | `start`、`end`、`trace_channel="visibility"`、`world_type="auto"` | 执行单次线性碰撞检测，返回统一序列化的 `HitResult`（本地Python）。 |
| `box_trace` | `start`、`end`、`half_size`、`orientation=[0,0,0]`、`trace_channel="visibility"` | 执行单次 Box Sweep 碰撞检测，返回统一序列化的 `HitResult`（本地Python）。 |
| `sphere_trace` | `start`、`end`、`radius`、`trace_channel="visibility"`、`world_type="auto"` | 执行单次 Sphere Sweep 碰撞检测，返回统一序列化的 `HitResult`（本地Python）。 |
| `get_hit_result_under_cursor` | `player_index=0`、`trace_channel="visibility"`、`world_type="auto"` | 在 PIE/VR Preview 中按当前鼠标位置做一次命中查询，返回鼠标坐标、射线与统一序列化的 `HitResult`（本地Python）。 |
| `take_screenshot` | `filepath`、`resolution=None`、`show_ui=False`、`transparent_background=False`、`viewport_index=None` | 对关卡视口截图并写入文件；支持同步 PNG 输出、可选分辨率、透明 Alpha，以及活动视口的 UI 截图。 |
| `take_highres_screenshot` | `filepath`、`resolution=None`、`resolution_multiplier=1.0`、`capture_hdr=False` | 生成一张高分辨率截图；默认 PNG 路径会同步落盘，HDR 仍走异步写盘。 |
| `capture_viewport_sequence` | `output_dir`、`frame_count`、`interval_seconds=0.0`、`base_filename="ViewportSequence"` | 按序列帧方式捕获当前活动视口，可选高分辨率模式。 |
| `capture_scene_to_render_target` | `render_target_asset_path`、可选 `name/actor_path`、`location`、`rotation`、`capture_source` | 使用现有或临时 `SceneCapture2D` 把场景捕捉到 `TextureRenderTarget2D`（本地Python）。 |
| `open_asset_editor` | `asset_path` | 打开任意资产编辑器标签（本地Python）。 |
| `close_asset_editor` | `asset_path` | 关闭指定资产的编辑器标签（本地Python）。 |
| `execute_console_command` | `command`、`world_type="auto"` | 在目标世界执行 Unreal 控制台命令（本地Python）。 |
| `run_editor_utility_widget` | `asset_path`、`tab_id=""` | 运行 Editor Utility Widget，并打开对应编辑器标签页。 |
| `run_editor_utility_blueprint` | `asset_path` | 运行 Editor Utility Blueprint 的 `Run` 入口。 |
| `set_viewport_mode` | `view_mode`、`apply_to_all=False` | 设置活动关卡视口或全部关卡视口的显示模式（本地Python）。 |
| `get_viewport_camera` | 无 | 读取当前活动关卡视口的相机位置、旋转与视图模式（本地Python；当前 `fov` 等视口细节会显式返回空值）。 |

## 参数注意事项

### `line_trace` / `box_trace` / `sphere_trace` / `get_hit_result_under_cursor`

当前这 4 个工具共用以下参数约定：

- `world_type` 仅支持 `auto`、`editor`、`pie`
- `trace_channel` 当前支持 `TraceTypeQuery1/TraceTypeQuery2` 的各种大小写/下划线写法，并额外兼容项目默认别名 `visibility -> TraceTypeQuery1`、`camera -> TraceTypeQuery2`
- `actors_to_ignore` 接收 Actor 名称、Label 或完整路径数组；如果其中某项解析不到，会直接返回错误，不会静默忽略
- `draw_debug_type` 支持 `none`、`for_one_frame`、`for_duration`、`persistent`

返回结构说明：

- `has_hit` 表示是否命中阻挡结果
- `hit_result` 会统一输出 `blocking_hit`、`initial_overlap`、`time`、`distance`、`location`、`impact_point`、`normal`、`impact_normal`
- 如果命中了 Actor / Component / PhysicalMaterial，还会额外返回 `hit_actor`、`hit_component`、`phys_mat_name`、`phys_mat_path`

`get_hit_result_under_cursor` 额外约束：

- 当前只支持 `PIE / VR Preview` 世界；如果 `world_type=auto` 且当前没有运行世界，会直接返回错误
- 工具会先读取 `PlayerController.get_mouse_position()` 与 `deproject_mouse_position_to_world()`，再按 `trace_distance` 做一次本地 Python 线性检测
- 未显式传 `trace_distance` 时，会优先复用 `PlayerController.hit_result_trace_distance`，否则回退到 `100000.0`

### `get_data_layers`

当前对外公开参数只有：

- `world_type`

说明：

- 当前只支持 `editor` 或 `auto`；如果传 `auto`，内部也会固定查询编辑器世界，不会切到 PIE。
- 返回每个 Data Layer 的基础信息，包括：
  - `name`
  - `path`
  - `short_name`
  - `full_name`
  - `type`
  - `is_runtime`
  - `is_visible`
  - `is_effective_visible`
  - `is_initially_visible`
  - `initial_runtime_state`
- 如果 Data Layer 绑定了资产，还会额外返回 `asset_name` 与 `asset_path`。

### `create_data_layer`

当前对外公开参数：

- `data_layer_name`
- `destination_path`
- `parent_data_layer`
- `world_data_layers_path`
- `world_type`

说明：

- 当前只支持 `editor` 或 `auto`；如果传 `auto`，内部也会固定写入编辑器世界，不会切到 PIE。
- 命令会先创建一个 `DataLayerAsset`，再通过 `DataLayerEditorSubsystem.create_data_layer_instance(...)` 在当前世界生成实例。
- `data_layer_name` 既是资产名，也是新实例最终使用的短名来源；当前不支持私有 Data Layer 或 External Data Layer。
- `destination_path` 必须是 Unreal 内容目录，例如 `/Game/CodexAutomation`；如果目录不存在，命令会先创建目录。
- `parent_data_layer` 可选，解析规则与 `set_actor_data_layers` 相同；如果提供，会在实例创建后尝试设置父层级。
- `world_data_layers_path` 可选，用于显式指定目标 `WorldDataLayers` Actor；未提供时默认使用当前编辑器世界自带的 `WorldDataLayers`。

### `set_actor_data_layers`

当前对外公开参数：

- `data_layers`
- `name`
- `actor_path`
- `world_type`

说明：

- 当前只支持 `editor` 或 `auto`；如果传 `auto`，内部也会固定写入编辑器世界，不会切到 PIE。
- `name` 与 `actor_path` 至少要提供一个。
- `data_layers` 是字符串数组，支持以下常见标识：
  - Data Layer 实例的 `name`
  - Data Layer 实例的 `path`
  - `short_name`
  - `full_name`
  - 绑定资产的 `asset_name`
  - 绑定资产的 `asset_path`
- 命令语义是“覆盖当前归属”：
  - 传空数组会清空该 Actor 的全部 Data Layer
  - 传非空数组时，会先移除旧归属，再写入目标列表
- 如果某个标识匹配到多个 Data Layer，命令会直接报错，不会静默选择其中一个。

### `set_data_layer_state`

当前对外公开参数：

- `data_layer`
- `name`
- `path`
- `short_name`
- `full_name`
- `asset_name`
- `asset_path`
- `is_visible`
- `is_loaded_in_editor`
- `initial_runtime_state`
- `world_type`

说明：

- 当前只支持 `editor` 或 `auto`；如果传 `auto`，内部也会固定写入编辑器世界，不会切到 PIE。
- Data Layer 标识支持与 `set_actor_data_layers` 相同的 6 组解析键；至少需要提供其中一个。
- 至少需要提供一个状态字段：
  - `is_visible`
  - `is_loaded_in_editor`
  - `initial_runtime_state`
- `initial_runtime_state` 当前支持：
  - `Activated`
  - `Loaded`
  - `Unloaded`
  - 也兼容 `DataLayerRuntimeState.ACTIVATED` / `EDataLayerRuntimeState::Loaded` 这类带前缀写法
- 返回值会同时带上：
  - `previous_state`
  - `data_layer`
  - `updated_fields`
  - 以及底层 `set_loaded_in_editor_result` 布尔结果（如果调用了该子路径）

### `take_screenshot`

当前对外公开参数只有：

- `filepath`
- `resolution`
- `show_ui`
- `transparent_background`
- `viewport_index`

说明：

- `resolution` 传 `[宽, 高]` 时，会基于目标关卡视口像素同步缩放并写出 PNG。
- `show_ui=true` 时，当前只支持活动关卡视口，且不支持同时传 `resolution` 或 `transparent_background`；该路径会返回 `request_queued=true`，文件可能在响应后异步落盘。
- `transparent_background=true` 时，工具会把输出 PNG 的 Alpha 写成透明；当前只支持 `show_ui=false` 的同步截图路径。
- `viewport_index` 按当前有效关卡视口列表重新编号；不传时优先使用活动视口。

### `take_highres_screenshot`

当前对外公开参数：

- `filepath`
- `resolution`
- `resolution_multiplier`
- `capture_hdr`

说明：

- `resolution` 传 `[宽, 高]` 时会直接指定目标分辨率。
- 不传 `resolution` 时，Unreal 会以当前活动视口尺寸乘以 `resolution_multiplier` 生成高分辨率截图。
- 默认 `capture_hdr=False` 时，工具会直接读取当前活动视口像素、按目标分辨率缩放并同步写出 PNG。
- `capture_hdr=True` 时仍走 Unreal 原生高分辨率截图链路，文件可能在响应返回后异步落盘。

### `capture_viewport_sequence`

当前对外公开参数：

- `output_dir`
- `frame_count`
- `interval_seconds`
- `base_filename`
- `use_high_res`
- `resolution`
- `resolution_multiplier`
- `show_ui`
- `capture_hdr`

说明：

- 该命令一次只允许存在一个活动序列捕获任务。
- `planned_filepaths` 会返回预期写出的所有帧文件路径，便于外部轮询落盘完成情况。
- 当 `use_high_res=True` 时，每一帧都走高分辨率截图路径；否则走普通截图路径，`show_ui` 仅在普通截图模式下生效。

### `capture_scene_to_render_target`

当前对外公开参数：

- `render_target_asset_path`
- `name`
- `actor_path`
- `world_type`
- `location`
- `rotation`
- `capture_source`
- `fov_angle`
- `post_process_blend_weight`
- `capture_every_frame`
- `capture_on_movement`
- `primitive_render_mode`

说明：

- `render_target_asset_path` 必须指向一个已存在的 `TextureRenderTarget2D`。
- 传 `name` 或 `actor_path` 时，会复用现有 `SceneCapture2D` Actor。
- 未传 `name/actor_path` 时，工具会基于当前视口相机或显式 `location/rotation` 临时生成一个 `SceneCapture2D`，捕捉后自动销毁。
- `capture_source` 当前支持 Unreal 枚举名，例如 `SCS_FINAL_COLOR_LDR`、`SCS_FINAL_COLOR_HDR`、`SCS_BASE_COLOR`、`SCS_SCENE_DEPTH`。
- 当前命令只支持 `editor` 世界，不建议用于 PIE/VR 运行态。

### `compile_live_coding`

当 `wait_for_completion=True` 时：

- Python 侧会把响应等待时间提升到较长超时。
- 如果 Unreal 在超时时间内没有返回，Python 侧会补做一次状态查询，并在错误结果中附加：
  - `timed_out_waiting_for_response`
  - `live_coding_state`

## 返回说明

这些工具大多直接透传 Unreal 桥接结果：

- 成功：`{"status":"success","result":{...}}`
- 失败：`{"status":"error","error":"..."}`

其中比较常见的成功结果字段包括：

- `start_pie`：`already_playing`、`play_world_available`、`message`
- `start_vr_preview`：`already_playing`、`request_queued`、`is_vr_preview`
- `start_standalone_game`：`request_queued`、`play_session_destination`、`is_standalone_game`
- `get_play_state`：`is_playing`、`is_play_session_queued`、`is_vr_preview`、`is_standalone_game`、`play_session_destination`、`play_mode`
- `get_selected_actors`：`actors`、`actor_count`
- `get_editor_selection`：`actors`、`assets`、`actor_count`、`asset_count`、`selection_count`
- `get_data_layers`：`data_layers`、`data_layer_count`、`resolved_world_type`、`world_path`、`implementation`
- `create_data_layer`：`created_asset`、`data_layer`、`parent_data_layer`、`set_parent_result`、`implementation`
- `set_actor_data_layers`：`actor_name`、`requested_data_layers`、`resolved_data_layers`、`added_data_layers`、`removed_data_layers`、`final_data_layers`
- `set_data_layer_state`：`requested_data_layer`、`previous_state`、`data_layer`、`updated_fields`、`implementation`
- `start_live_coding`：`show_console` 以及 Unreal 侧当前 Live Coding 状态字段
- `compile_live_coding`：`compile_result`、`wait_for_completion`、`show_console`
- `take_screenshot`：`filepath`、`show_ui`、`transparent_background`、`viewport_index`，同步路径下还会返回 `resolution`
- `take_highres_screenshot`：`request_queued`、`written`、`filepath`、`resolution`、`resolution_multiplier`
- `capture_viewport_sequence`：`request_queued`、`sequence_id`、`planned_filepaths`、`frame_count`
- `capture_scene_to_render_target`：`render_target_asset_path`、`capture_actor_path`、`capture_component_path`、`used_temporary_actor`
- `execute_console_command`：`command`、`resolved_world_type`、`implementation`
- `get_hit_result_under_cursor`：`mouse_position`、`ray_origin`、`ray_direction`、`trace_distance`、`player_controller_name`、`hit_result`
- `run_editor_utility_widget`：`tab_id`、`widget_name`、`widget_class`、`implementation`
- `run_editor_utility_blueprint`：`generated_class`、`implementation`
- `set_viewport_mode`：`view_mode`、`view_mode_index`、`updated_viewport_count`、`implementation`
- `get_viewport_camera`：`location`、`rotation`、`view_mode`、`view_mode_index`、`viewport_config_key`，以及当前会显式留空的 `fov`、`viewport_type`

## 调用示例

### 启动 PIE

```json
{
  "tool": "start_pie",
  "args": {
    "simulate": false
  }
}
```

### 启动 Standalone Game

```json
{
  "tool": "start_standalone_game",
  "args": {
    "map_override": "/Game/MainMap",
    "additional_command_line_parameters": "-ResX=1280 -ResY=720"
  }
}
```

### 读取当前编辑器选择

```json
{
  "tool": "get_editor_selection",
  "args": {
    "include_components": false,
    "include_tags": true
  }
}
```

### 聚焦到指定 Actor

```json
{
  "tool": "focus_viewport",
  "args": {
    "target": "PlayerStart",
    "distance": 600,
    "orientation": [0, 180, 0]
  }
}
```

### 保存一张视口截图

```json
{
  "tool": "take_screenshot",
  "args": {
    "filepath": "C:/Temp/ue_view.png",
    "resolution": [1600, 900],
    "transparent_background": true,
    "viewport_index": 0
  }
}
```

### 请求一张高分辨率截图

```json
{
  "tool": "take_highres_screenshot",
  "args": {
    "filepath": "C:/Temp/ue_highres.png",
    "resolution": [2560, 1440]
  }
}
```

### 捕获 5 帧视口序列

```json
{
  "tool": "capture_viewport_sequence",
  "args": {
    "output_dir": "C:/Temp/ViewportSequence",
    "frame_count": 5,
    "interval_seconds": 0.1,
    "base_filename": "VRPreview",
    "use_high_res": true,
    "resolution_multiplier": 1.5
  }
}
```

### 把场景捕捉到已有 RenderTarget

```json
{
  "tool": "capture_scene_to_render_target",
  "args": {
    "render_target_asset_path": "/Game/MCPTemp/RT_CodexCapture",
    "capture_source": "SCS_FINAL_COLOR_LDR",
    "fov_angle": 70.0
  }
}
```

### 运行 Editor Utility Widget

```json
{
  "tool": "run_editor_utility_widget",
  "args": {
    "asset_path": "/Game/Editor/EUW_DebugPanel"
  }
}
```

### 执行控制台命令

```json
{
  "tool": "execute_console_command",
  "args": {
    "command": "stat fps",
    "world_type": "editor"
  }
}
```

### 切换视口到 Unlit

```json
{
  "tool": "set_viewport_mode",
  "args": {
    "view_mode": "Unlit",
    "apply_to_all": false
  }
}
```

### 读取当前视口摄像机

```json
{
  "tool": "get_viewport_camera",
  "args": {}
}
```
