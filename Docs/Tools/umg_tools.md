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
| `create_umg_widget_blueprint` | `widget_name`、`parent_class="UserWidget"`、`path="/Game/UI"` | 创建 Widget Blueprint，并确保根节点为 CanvasPanel。（本地Python） |
| `add_text_block_to_widget` | `widget_name`、`text_block_name`、`text=""`、`position=[0,0]`、`size=[200,50]`、`font_size=12`、`color=[1,1,1,1]` | 向 Widget Blueprint 添加 TextBlock。（本地Python） |
| `add_button_to_widget` | `widget_name`、`button_name`、`text=""`、`position=[0,0]`、`size=[200,50]`、`font_size=12`、`color=[1,1,1,1]`、`background_color=[0.1,0.1,0.1,1.0]` | 向 Widget Blueprint 添加 Button，并自动创建内部 TextBlock。（本地Python） |
| `add_image_to_widget` | `widget_name`、`image_name`、`position=[0,0]`、`size=[200,200]`、`color=[1,1,1,1]` | 向 Widget Blueprint 添加 Image。（本地Python） |
| `add_border_to_widget` | `widget_name`、`border_name`、`position=[0,0]`、`size=[200,100]`、`brush_color=[1,1,1,1]`、`content_color=[1,1,1,1]` | 向 Widget Blueprint 添加 Border。（本地Python） |
| `add_canvas_panel_to_widget` | `widget_name`、`canvas_panel_name`、`position=[0,0]`、`size=[400,300]` | 向 Widget Blueprint 添加 CanvasPanel。（本地Python） |
| `add_horizontal_box_to_widget` | `widget_name`、`horizontal_box_name`、`position=[0,0]`、`size=[300,100]` | 向 Widget Blueprint 添加 HorizontalBox。（本地Python） |
| `add_vertical_box_to_widget` | `widget_name`、`vertical_box_name`、`position=[0,0]`、`size=[300,100]` | 向 Widget Blueprint 添加 VerticalBox。（本地Python） |
| `add_overlay_to_widget` | `widget_name`、`overlay_name`、`position=[0,0]`、`size=[300,100]` | 向 Widget Blueprint 添加 Overlay。（本地Python） |
| `add_scroll_box_to_widget` | `widget_name`、`scroll_box_name`、`position=[0,0]`、`size=[300,200]` | 向 Widget Blueprint 添加 ScrollBox。（本地Python） |
| `add_size_box_to_widget` | `widget_name`、`size_box_name`、`position=[0,0]`、`size=[200,100]`、`width_override`、`height_override` | 向 Widget Blueprint 添加 SizeBox。（本地Python） |
| `add_spacer_to_widget` | `widget_name`、`spacer_name`、`position=[0,0]`、`size=[32,32]` | 向 Widget Blueprint 添加 Spacer。（本地Python） |
| `add_progress_bar_to_widget` | `widget_name`、`progress_bar_name`、`position=[0,0]`、`size=[300,30]`、`percent=0.5`、`fill_color=[1,1,1,1]` | 向 Widget Blueprint 添加 ProgressBar。（本地Python） |
| `add_slider_to_widget` | `widget_name`、`slider_name`、`position=[0,0]`、`size=[300,40]`、`value=0.0` | 向 Widget Blueprint 添加 Slider。（本地Python） |
| `add_check_box_to_widget` | `widget_name`、`check_box_name`、`position=[0,0]`、`size=[48,48]`、`is_checked=false` | 向 Widget Blueprint 添加 CheckBox。（本地Python） |
| `add_editable_text_to_widget` | `widget_name`、`editable_text_name`、`text=""`、`hint_text=""`、`position=[0,0]`、`size=[300,40]`、`is_read_only=false` | 向 Widget Blueprint 添加 EditableText。（本地Python） |
| `add_rich_text_to_widget` | `widget_name`、`rich_text_name`、`text=""`、`position=[0,0]`、`size=[300,80]`、`color=[1,1,1,1]` | 向 Widget Blueprint 添加 RichText。（本地Python） |
| `add_multi_line_text_to_widget` | `widget_name`、`multi_line_text_name`、`text=""`、`hint_text=""`、`position=[0,0]`、`size=[300,120]`、`is_read_only=false` | 向 Widget Blueprint 添加 MultiLineEditableTextBox。（本地Python） |
| `add_named_slot_to_widget` | `widget_name`、`named_slot_name`、`position=[0,0]`、`size=[200,100]` | 向 Widget Blueprint 添加 NamedSlot。（本地Python） |
| `add_list_view_to_widget` | `widget_name`、`list_view_name`、`position=[0,0]`、`size=[300,200]` | 向 Widget Blueprint 添加 ListView。（本地Python） |
| `add_tile_view_to_widget` | `widget_name`、`tile_view_name`、`position=[0,0]`、`size=[300,200]` | 向 Widget Blueprint 添加 TileView。（本地Python） |
| `add_tree_view_to_widget` | `widget_name`、`tree_view_name`、`position=[0,0]`、`size=[300,200]` | 向 Widget Blueprint 添加 TreeView。（本地Python） |
| `bind_widget_event` | `widget_name`、`widget_component_name`、`event_name`、`function_name=""` | 为控件事件绑定 Blueprint 函数；若 `function_name` 为空，会自动生成函数名。（C++主实现） |
| `add_widget_to_viewport` | `widget_name`、`z_order=0`、`instance_name=""`、`player_index` | 在运行中的 PIE/VR Preview 世界中创建 Widget 实例并加入视口；当传入 `player_index` 时会绑定到对应本地玩家。（本地Python） |
| `remove_widget_from_viewport` | `instance_path=""`、`instance_name=""`、`widget_name=""` | 从运行中的 PIE/VR Preview 世界移除先前创建的 Widget 实例；优先传 `instance_path`。（本地Python） |
| `set_text_block_binding` | `widget_name`、`text_block_name`、`binding_property`、`binding_type="Text"` | 为 TextBlock 建立属性绑定，必要时自动创建绑定变量。（C++主实现） |
| `bind_widget_property` | `widget_name`、`child_widget_name`、`binding_property`、`binding_type="Text"` | 为任意支持该属性的控件建立属性绑定，必要时自动创建绑定变量。（C++主实现） |
| `remove_widget_from_blueprint` | `widget_name`、`child_widget_name` | 从 Widget Blueprint 中删除指定子控件。（本地Python） |
| `set_widget_slot_layout` | `widget_name`、`child_widget_name`、`position=[0,0]`、`size=[100,100]`、`alignment=[0,0]`、`anchors=[0,0,0,0]`、`offsets`、`auto_size=false`、`z_order=0` | 设置目标控件的 CanvasPanelSlot 布局参数。（本地Python） |
| `set_widget_visibility` | `widget_name`、`child_widget_name`、`visibility` | 设置目标控件可见性，支持 `Visible`、`Collapsed`、`Hidden`、`HitTestInvisible`、`SelfHitTestInvisible`。（本地Python） |
| `set_widget_style` | `widget_name`、`child_widget_name`、`style` | 设置控件稳定可写的样式字段；当前支持 `Button`、`ProgressBar`、`CheckBox`、`Border`、`Image`。（本地Python） |
| `set_widget_brush` | `widget_name`、`child_widget_name`、`brush_asset_path=""`、`texture_asset_path=""`、`material_asset_path=""`、`tint_color`、`image_size`、`match_size=true` | 设置控件 Brush 资源和基础显示参数；当前支持 `Image`、`Border`。（本地Python） |
| `create_widget_animation` | `widget_name`、`animation_name=""`、`start_time=0.0`、`end_time=1.0`、`display_rate=20` | 在 Widget Blueprint 中创建 Widget 动画；当 `animation_name` 为空时自动生成唯一名称。（C++主实现） |
| `add_widget_animation_keyframe` | `widget_name`、`animation_name`、`target_widget_name`、`property_name`、`time`、`value`、`interpolation="cubic"` | 给 Widget 动画写入关键帧；当前支持 `render_opacity` 与常用 `render_transform` 通道。（C++主实现） |
| `open_widget_blueprint_editor` | `widget_name` | 打开 Widget Blueprint 编辑器。（本地Python） |

## 参数注意事项

### `create_umg_widget_blueprint`

- Python 侧默认路径是 `/Game/UI`
- Unreal 侧如果没有收到路径，会退回 `/Game/Widgets`

由于 Python 工具总会把默认值传下去，所以对外可以认为默认路径就是 `/Game/UI`

### 本地 Python 实现层

- `create_umg_widget_blueprint`
- `add_text_block_to_widget`
- `add_button_to_widget`
- `add_image_to_widget`
- `add_border_to_widget`
- `add_canvas_panel_to_widget`
- `add_horizontal_box_to_widget`
- `add_vertical_box_to_widget`
- `add_overlay_to_widget`
- `add_scroll_box_to_widget`
- `add_size_box_to_widget`
- `add_spacer_to_widget`
- `add_progress_bar_to_widget`
- `add_slider_to_widget`
- `add_check_box_to_widget`
- `add_editable_text_to_widget`
- `add_rich_text_to_widget`
- `add_multi_line_text_to_widget`
- `add_named_slot_to_widget`
- `add_list_view_to_widget`
- `add_tile_view_to_widget`
- `add_tree_view_to_widget`
- `remove_widget_from_blueprint`
- `set_widget_slot_layout`
- `set_widget_visibility`
- `set_widget_style`
- `set_widget_brush`
- `add_widget_to_viewport`
- `remove_widget_from_viewport`
- `open_widget_blueprint_editor`
  以上 30 条命令已迁移到 `Plugins/UnrealMCP/Content/Python/commands/umg/umg_commands.py`。
- 当前实现依赖 UE5.7 Python 暴露的：
  - `AssetToolsHelpers.create_asset(...)`
  - `EditorUtilityLibrary.cast_to_widget_blueprint(...)`
  - `EditorUtilityLibrary.add_source_widget(...)`
  - `EditorUtilityLibrary.find_source_widget_by_name(...)`
  - `BlueprintEditorLibrary.compile_blueprint(...)`
- 当前以下命令继续保留为现有 C++ 实现：
  - `bind_widget_event`
  - `set_text_block_binding`
  - `bind_widget_property`
  - `create_widget_animation`
  - `add_widget_animation_keyframe`

### `create_widget_animation`

- `animation_name` 为空时会自动生成唯一名称，默认基名为 `NewAnimation`。
- `end_time` 必须大于 `start_time`。
- `display_rate` 必须大于 `0`，当前按整数帧率写入 `MovieScene.DisplayRate`。
- 该命令当前保留为 C++ 主实现，原因是 `WidgetBlueprint.Animations` 在 UE5.7 Python 中仍是受保护属性，无法稳定直接追加新动画对象。

### `add_widget_animation_keyframe`

- `property_name` 当前稳定支持：
  - `render_opacity`
  - `render_transform.translation`
  - `render_transform.translation_x`
  - `render_transform.translation_y`
  - `render_transform.scale`
  - `render_transform.scale_x`
  - `render_transform.scale_y`
  - `render_transform.shear`
  - `render_transform.shear_x`
  - `render_transform.shear_y`
  - `render_transform.rotation`
- 标量属性的 `value` 必须是数字；`translation / scale / shear` 的 `value` 必须是长度为 `2` 的数组。
- `interpolation` 当前支持 `cubic`、`linear`、`constant`。
- 该命令当前保留为 C++ 主实现，原因是关键帧写入除了轨道/Section 之外，还要稳定维护 `WidgetAnimationBinding` 与 `MovieScene Possessable`，这部分在 UE5.7 Python 暴露仍不完整。

### `set_widget_style`

- `style` 必须是对象，当前支持以下控件和字段：
  - `Button`
    - `normal`、`hovered`、`pressed`、`disabled`
    - `normal_foreground`、`hovered_foreground`、`pressed_foreground`、`disabled_foreground`
    - `normal_padding`、`pressed_padding`
  - `ProgressBar`
    - `background_image`、`fill_image`、`marquee_image`
    - `enable_fill_animation`
  - `CheckBox`
    - `background_image`、`background_hovered_image`、`background_pressed_image`
    - `unchecked_image`、`unchecked_hovered_image`、`unchecked_pressed_image`
    - `checked_image`、`checked_hovered_image`、`checked_pressed_image`
    - `undetermined_image`、`undetermined_hovered_image`、`undetermined_pressed_image`
    - `foreground_color`、`border_background_color`
    - `checked_foreground`、`checked_hovered_foreground`、`checked_pressed_foreground`
    - `hovered_foreground`、`pressed_foreground`、`undetermined_foreground`
    - `padding`、`check_box_type`
  - `Border`
    - `brush`、`brush_color`、`content_color`、`padding`
  - `Image`
    - `brush`、`color_and_opacity`
- 其中 Brush 对象当前支持：
  - `brush_asset_path`
  - `texture_asset_path`
  - `material_asset_path`
  - `tint_color`
  - `image_size`
  - `draw_as`
  - `margin`
- `draw_as` 当前支持：
  - `NoDrawType`
  - `Image`
  - `Box`
  - `Border`
  - `RoundedBox`
- `check_box_type` 当前支持：
  - `CheckBox`
  - `ToggleButton`

### `add_widget_to_viewport`

该工具要求当前存在可用的运行时世界：

- PIE
- VR Preview

如果编辑器当前没有运行中的游戏世界，Unreal 侧会报错。

### `remove_widget_from_viewport`

- 该工具同样要求当前存在可用的运行时世界：
  - PIE
  - VR Preview
- 推荐直接使用 `add_widget_to_viewport` 返回的 `instance_path`
- 只有在拿不到 `instance_path` 时，才回退使用 `instance_name`
- 若按 `instance_name` 匹配到多个实例，会直接报错并要求改传 `instance_path`

### `set_widget_brush`

- 当前只支持以下控件：
  - `Image`
  - `Border`
- 三种资源参数互斥：
  - `brush_asset_path`
  - `texture_asset_path`
  - `material_asset_path`
- `tint_color` 可单独使用，用于只改 Tint 不换资源。
- `image_size` 当前只对 `Image` 生效；`Border` 如需更完整样式编辑，后续请走 `set_widget_style`。

### `set_text_block_binding`

当前已支持的 `binding_type` 包括：

- `Text`
- `Visibility`
- `ColorAndOpacity`
- `ShadowColorAndOpacity`
- `ToolTipText`
- `IsEnabled`

### `bind_widget_property`

- `binding_type` 与 `set_text_block_binding` 共用同一组受支持类型：
  - `Text`
  - `Visibility`
  - `ColorAndOpacity`
  - `ShadowColorAndOpacity`
  - `ToolTipText`
  - `IsEnabled`
- 与 `set_text_block_binding` 的区别：
  - `set_text_block_binding` 继续兼容旧接口命名
  - `bind_widget_property` 不再强制目标控件必须是 `TextBlock`
- 但目标控件类必须真实存在对应属性，否则会返回错误。
- 当前返回会额外带上：
  - `widget_class`
  - `implementation="cpp"`

## 返回说明

这些工具大多直接透传 Unreal 桥接结果：

- 成功：`{"status":"success","result":{...}}`
- 失败：`{"status":"error","error":"..."}`

常见成功结果字段包括：

- `create_umg_widget_blueprint`：`name`、`path`、`parent_class`
- `add_text_block_to_widget`：`blueprint_name`、`widget_name`、`text`
- `add_button_to_widget`：`blueprint_name`、`widget_name`、`text`
- `add_image_to_widget` / `add_border_to_widget` / `add_canvas_panel_to_widget` / `add_horizontal_box_to_widget` / `add_vertical_box_to_widget` / `add_overlay_to_widget` / `add_scroll_box_to_widget` / `add_size_box_to_widget` / `add_spacer_to_widget`：`blueprint_name`、`widget_name`、`widget_type`
- `add_progress_bar_to_widget`：`blueprint_name`、`widget_name`、`widget_type`、`percent`
- `add_slider_to_widget`：`blueprint_name`、`widget_name`、`widget_type`、`value`
- `add_check_box_to_widget`：`blueprint_name`、`widget_name`、`widget_type`、`is_checked`
- `add_editable_text_to_widget`：`blueprint_name`、`widget_name`、`widget_type`、`text`、`is_read_only`
- `add_rich_text_to_widget`：`blueprint_name`、`widget_name`、`widget_type`、`text`
- `add_multi_line_text_to_widget`：`blueprint_name`、`widget_name`、`widget_type`、`text`、`hint_text`、`is_read_only`
- `add_named_slot_to_widget`：`blueprint_name`、`widget_name`、`widget_type`
- `add_list_view_to_widget` / `add_tile_view_to_widget` / `add_tree_view_to_widget`：`blueprint_name`、`widget_name`、`widget_type`
- `remove_widget_from_blueprint`：`blueprint_name`、`widget_name`、`removed`
- `set_widget_slot_layout`：`blueprint_name`、`widget_name`、`position`、`size`、`alignment`、`anchors`、`offsets`、`auto_size`、`z_order`
- `set_widget_visibility`：`blueprint_name`、`widget_name`、`visibility`
- `set_widget_style`：`blueprint_name`、`widget_name`、`widget_class`、`updated_fields`
- `set_widget_brush`：`blueprint_name`、`widget_name`、`widget_class`、`resource_type`、`resource_path`、`tint_color`、`image_size`
- `create_widget_animation`：`blueprint_name`、`asset_path`、`animation_name`、`movie_scene_name`、`start_time`、`end_time`、`display_rate`
- `add_widget_animation_keyframe`：`blueprint_name`、`animation_name`、`target_widget_name`、`property_name`、`time`、`frame_number`、`track_class`、`written_channels`
- `bind_widget_event`：`blueprint_name`、`widget_name`、`event_name`、`function_name`
- `add_widget_to_viewport`：`blueprint_name`、`asset_path`、`class_path`、`instance_name`、`instance_path`、`world_name`、`z_order`、`screen_mode`、`player_index`、`owning_player_name`、`owning_player_path`
- `remove_widget_from_viewport`：`instance_name`、`instance_path`、`widget_class`、`world_name`、`was_in_viewport`、`removed`、`implementation`
- `set_text_block_binding`：`blueprint_name`、`widget_name`、`binding_type`、`binding_name`
- `bind_widget_property`：`blueprint_name`、`widget_name`、`widget_class`、`binding_type`、`binding_name`、`implementation`

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
