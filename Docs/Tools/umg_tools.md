# UMG 工具

## 概览

本页描述 Widget Blueprint 的创建、控件添加、事件绑定、属性绑定和运行时挂载能力。

- Python 注册位置：`Python/tools/umg_tools.py`
- Unreal 命令处理位置：`Source/UnrealMCP/Private/Commands/UnrealMCPUMGCommands.cpp`

## 参数命名说明

为了兼容旧调用方式，Unreal 侧对部分字段做了兼容解析；但文档推荐统一使用以下命名：

- 目标 Widget Blueprint：`widget_name`
- Blueprint 内部控件名：
  - `text_block_name`
  - `button_name`
  - `widget_component_name`

对外新文档以 Python 工具签名为准，不再推荐直接依赖旧字段别名。

## 工具列表

| 工具 | 关键参数 | 说明 |
| --- | --- | --- |
| `create_umg_widget_blueprint` | `widget_name`、`parent_class="UserWidget"`、`path="/Game/UI"` | 创建 Widget Blueprint，并确保根节点为 CanvasPanel。 |
| `add_text_block_to_widget` | `widget_name`、`text_block_name`、`text=""`、`position=[0,0]`、`size=[200,50]`、`font_size=12`、`color=[1,1,1,1]` | 向 Widget Blueprint 添加 TextBlock。 |
| `add_button_to_widget` | `widget_name`、`button_name`、`text=""`、`position=[0,0]`、`size=[200,50]`、`font_size=12`、`color=[1,1,1,1]`、`background_color=[0.1,0.1,0.1,1.0]` | 向 Widget Blueprint 添加 Button，并自动创建内部 TextBlock。 |
| `bind_widget_event` | `widget_name`、`widget_component_name`、`event_name`、`function_name=""` | 为控件事件绑定 Blueprint 函数；若 `function_name` 为空，会自动生成函数名。 |
| `add_widget_to_viewport` | `widget_name`、`z_order=0` | 在运行中的 PIE/VR Preview 世界中创建 Widget 实例并加入视口。 |
| `set_text_block_binding` | `widget_name`、`text_block_name`、`binding_property`、`binding_type="Text"` | 为 TextBlock 建立属性绑定，必要时自动创建绑定变量。 |

## 参数注意事项

### `create_umg_widget_blueprint`

- Python 侧默认路径是 `/Game/UI`
- Unreal 侧如果没有收到路径，会退回 `/Game/Widgets`

由于 Python 工具总会把默认值传下去，所以对外可以认为默认路径就是 `/Game/UI`

### `add_widget_to_viewport`

该工具要求当前存在可用的运行时世界：

- PIE
- VR Preview

如果编辑器当前没有运行中的游戏世界，Unreal 侧会报错。

### `set_text_block_binding`

当前已支持的 `binding_type` 包括：

- `Text`
- `Visibility`
- `ColorAndOpacity`
- `ShadowColorAndOpacity`
- `ToolTipText`
- `IsEnabled`

## 返回说明

这些工具大多直接透传 Unreal 桥接结果：

- 成功：`{"status":"success","result":{...}}`
- 失败：`{"status":"error","error":"..."}`

常见成功结果字段包括：

- `create_umg_widget_blueprint`：`name`、`path`、`parent_class`
- `add_text_block_to_widget`：`blueprint_name`、`widget_name`、`text`
- `add_button_to_widget`：`blueprint_name`、`widget_name`、`text`
- `bind_widget_event`：`blueprint_name`、`widget_name`、`event_name`、`function_name`
- `add_widget_to_viewport`：`blueprint_name`、`class_path`、`instance_name`、`world_name`、`z_order`
- `set_text_block_binding`：`blueprint_name`、`widget_name`、`binding_type`、`binding_name`

## 调用示例

### 创建一个主菜单 Widget Blueprint

```json
{
  "tool": "create_umg_widget_blueprint",
  "args": {
    "widget_name": "WBP_MainMenu",
    "parent_class": "UserWidget",
    "path": "/Game/UI"
  }
}
```

### 向 Widget Blueprint 添加按钮

```json
{
  "tool": "add_button_to_widget",
  "args": {
    "widget_name": "WBP_MainMenu",
    "button_name": "StartButton",
    "text": "开始游戏",
    "position": [80, 120],
    "size": [260, 60],
    "font_size": 20
  }
}
```

### 在 PIE 中把 Widget 加入视口

```json
{
  "tool": "add_widget_to_viewport",
  "args": {
    "widget_name": "WBP_MainMenu",
    "z_order": 0
  }
}
```
