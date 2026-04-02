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
| `load_level` | `level_path` | 按内容路径加载关卡。 |
| `save_current_level` | 无 | 保存当前编辑器关卡。 |
| `start_pie` | `simulate=False` | 请求启动 PIE。返回 `already_playing`、`play_world_available` 等状态字段。 |
| `start_vr_preview` | 无 | 请求启动 VR Preview。返回 `request_queued`、`is_vr_preview` 等状态字段。 |
| `stop_pie` | 无 | 请求结束当前 PIE。 |
| `get_play_state` | 无 | 查询当前编辑器是否处于 `pie` / `vr_preview` / `queued` / `stopped`。 |
| `start_live_coding` | `show_console=True` | 启用当前编辑器会话的 Live Coding。 |
| `compile_live_coding` | `wait_for_completion=False`、`show_console=True` | 触发 Live Coding 编译。 |
| `get_live_coding_state` | 无 | 查询 Live Coding 当前状态。 |
| `focus_viewport` | `target=None`、`location=None`、`distance=1000.0`、`orientation=None` | 聚焦到指定 Actor 或坐标。`target` 与 `location` 至少要传一个。 |
| `take_screenshot` | `filepath` | 对当前激活视口截图并写入文件。若路径不带 `.png`，Unreal 侧会自动补齐。 |

## 参数注意事项

### `take_screenshot`

当前对外公开参数只有：

- `filepath`

旧文档中的 `filename`、`show_ui`、`resolution` 并不是当前 MCP 工具真实支持的参数，不应再继续使用。

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
- `get_play_state`：`is_playing`、`is_play_session_queued`、`is_vr_preview`、`play_mode`
- `start_live_coding`：`show_console` 以及 Unreal 侧当前 Live Coding 状态字段
- `compile_live_coding`：`compile_result`、`wait_for_completion`、`show_console`
- `take_screenshot`：`filepath`

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
