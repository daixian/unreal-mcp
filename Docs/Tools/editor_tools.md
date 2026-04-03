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
| `focus_viewport` | `target=None`、`location=None`、`distance=1000.0`、`orientation=None` | 聚焦到指定 Actor 或坐标（本地Python）。`target` 与 `location` 至少要传一个。 |
| `take_screenshot` | `filepath` | 对当前激活视口截图并写入文件。若路径不带 `.png`，Unreal 侧会自动补齐。 |
| `take_highres_screenshot` | `filepath`、`resolution=None`、`resolution_multiplier=1.0`、`capture_hdr=False` | 生成一张高分辨率截图；默认 PNG 路径会同步落盘，HDR 仍走异步写盘。 |
| `capture_viewport_sequence` | `output_dir`、`frame_count`、`interval_seconds=0.0`、`base_filename="ViewportSequence"` | 按序列帧方式捕获当前活动视口，可选高分辨率模式。 |
| `capture_scene_to_render_target` | `render_target_asset_path`、可选 `name/actor_path`、`location`、`rotation`、`capture_source` | 使用现有或临时 `SceneCapture2D` 把场景捕捉到 `TextureRenderTarget2D`（本地Python）。 |
| `open_asset_editor` | `asset_path` | 打开任意资产编辑器标签（本地Python）。 |
| `close_asset_editor` | `asset_path` | 关闭指定资产的编辑器标签（本地Python）。 |
| `run_editor_utility_widget` | `asset_path`、`tab_id=""` | 运行 Editor Utility Widget，并打开对应编辑器标签页。 |
| `run_editor_utility_blueprint` | `asset_path` | 运行 Editor Utility Blueprint 的 `Run` 入口。 |
| `set_viewport_mode` | `view_mode`、`apply_to_all=False` | 设置活动关卡视口或全部关卡视口的显示模式。 |
| `get_viewport_camera` | 无 | 读取当前活动关卡视口的相机位置、旋转、FOV 与视图模式。 |

## 参数注意事项

### `take_screenshot`

当前对外公开参数只有：

- `filepath`

旧文档中的 `filename`、`show_ui`、`resolution` 并不是当前 MCP 工具真实支持的参数，不应再继续使用。

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
- `start_live_coding`：`show_console` 以及 Unreal 侧当前 Live Coding 状态字段
- `compile_live_coding`：`compile_result`、`wait_for_completion`、`show_console`
- `take_screenshot`：`filepath`
- `take_highres_screenshot`：`request_queued`、`written`、`filepath`、`resolution`、`resolution_multiplier`
- `capture_viewport_sequence`：`request_queued`、`sequence_id`、`planned_filepaths`、`frame_count`
- `capture_scene_to_render_target`：`render_target_asset_path`、`capture_actor_path`、`capture_component_path`、`used_temporary_actor`
- `run_editor_utility_widget`：`tab_id`、`widget_name`、`widget_class`、`implementation`
- `run_editor_utility_blueprint`：`generated_class`、`implementation`
- `set_viewport_mode`：`view_mode`、`view_mode_index`、`updated_viewport_count`
- `get_viewport_camera`：`location`、`rotation`、`fov`、`view_mode`、`viewport_type`

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
    "filepath": "C:/Temp/ue_view.png"
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
