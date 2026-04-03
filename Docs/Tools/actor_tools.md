# Actor 与场景工具

## 概览

本页描述与关卡内 Actor、场景对象和场景组件直接相关的工具。

- Python 注册位置：`Python/tools/editor_tools.py`
- Unreal 命令处理位置：`Source/UnrealMCP/Private/Commands/UnrealMCPEditorCommands.cpp`

## World 选择

部分查询工具支持 `world_type` 参数，可选值如下：

- `auto`：自动选择当前最合适的 World。
- `editor`：编辑器 World。
- `pie`：Play In Editor World。

## 工具列表

| 工具 | 关键参数 | 说明 |
| --- | --- | --- |
| `get_actors_in_level` | `include_components=False`、`detailed_components=True`、`world_type="auto"` | 返回当前关卡 Actor 列表（本地Python）。Python 层会归一化为 `success`、`actors`、`actor_count` 等字段。 |
| `find_actors_by_name` | `pattern`、`world_type="auto"`、`include_components=False`、`detailed_components=True` | 按名称模式筛选 Actor（本地Python）。成功返回结构与 `get_actors_in_level` 类似，并附带 `pattern`。 |
| `find_actors` | 可选 `name_pattern`、`class_name`、`folder_path`、`path_contains`、`tag` / `tags=[]`、`data_layer`、`sort_by="name"`、`sort_desc=False`、`world_type="auto"` | 按类、标签、文件夹、路径、DataLayer 等条件组合筛选 Actor（本地Python）。成功返回结构与 `get_actors_in_level` 类似，并附带 `filters`。 |
| `spawn_actor` | `name`、`type`、`location=[0,0,0]`、`rotation=[0,0,0]` | 在编辑器 World 里生成原生 Actor（本地Python）。当前 Python 工具公开的是 `spawn_actor`，不是旧别名 `create_actor`。 |
| `spawn_actor_from_class` | `actor_name`、`class_path`、可选 `location=[0,0,0]`、`rotation=[0,0,0]`、`scale=[1,1,1]`、`world_type="editor"` | 按原生类路径或 Blueprint 生成类路径在关卡中生成 Actor（本地Python）。当前只支持 `editor` 世界。 |
| `create_light` | `actor_name`、`light_type`、可选 `location`、`rotation`、`intensity`、`light_color` 等 | 创建 Point / Spot / Directional Light Actor，并可在创建时直接写入常用灯光属性（本地Python）。 |
| `delete_actor` | `name` | 删除指定名称的 Actor（本地Python）。成功时返回被删除 Actor 的摘要。 |
| `duplicate_actor` | `name` 或 `actor_path`、可选 `world_type`、`offset=[0,0,0]` | 复制单个 Actor，并可附加世界坐标偏移（本地Python）。 |
| `select_actor` | `name` 或 `actor_path`、可选 `world_type="auto"`、`replace_selection=False`、`notify=True`、`select_even_if_hidden=True` | 选中指定 Actor（Python+C++桥接）。本地 Python 负责解析目标 Actor 与世界，C++ 保留最终选择调用，确保现有选择语义不退化。 |
| `set_actor_transform` | `name`、`location=None`、`rotation=None`、`scale=None` | 修改 Actor 变换（本地Python）。只传需要修改的字段即可。 |
| `set_actors_transform` | `actor_names`、可选 `world_type`、`location`、`rotation`、`scale` | 对多个 Actor 批量写入同一组变换值（本地Python）。 |
| `set_actor_tags` | `name` 或 `actor_path`、`tags=[]` | 覆盖写入单个 Actor 的标签数组（本地Python）。空数组表示清空全部标签。 |
| `set_actor_folder_path` | `name` 或 `actor_path`、`folder_path=""` | 设置单个 Actor 的 Scene Outliner 文件夹路径（本地Python）。传空字符串可清空文件夹。 |
| `set_actor_visibility` | `name` 或 `actor_path`、`visible=True` | 设置单个 Actor 的可见性/隐藏状态（本地Python）。当前会同时同步编辑器临时隐藏与运行时 `hidden_in_game`。 |
| `set_actor_mobility` | `name` 或 `actor_path`、`mobility="Static|Stationary|Movable"` | 设置单个 Actor 根 `SceneComponent` 的 Mobility（本地Python）。 |
| `set_light_properties` | `name` 或 `actor_path`、可选 `world_type` 与一组灯光属性 | 修改已有灯光 Actor 的强度、颜色、阴影、移动性、Spot 锥角等常用属性（本地Python）。 |
| `attach_actor` | `name` 或 `actor_path`、`parent_name` 或 `parent_actor_path`、可选 `socket_name`、`keep_world_transform=True`、`world_type="auto"` | 将子 Actor 挂接到父 Actor（本地Python）。支持按名称或完整 Actor 路径解析双方对象。 |
| `detach_actor` | `name` 或 `actor_path`、可选 `keep_world_transform=True`、`world_type="auto"` | 将 Actor 从父对象分离（本地Python）。 |
| `set_post_process_settings` | `name` 或 `actor_path`、可选 `world_type`、`blend_radius`、`blend_weight`、`priority`、`enabled`、`unbound`、`settings={...}` | 修改 `PostProcessVolume` 的常用 Volume 参数，并批量写入 `PostProcessSettings` 字段（本地Python）。 |
| `get_actor_properties` | `name` 或 `actor_path`、`include_components=True`、`detailed_components=True`、`world_type="auto"` | 获取单个 Actor 的详细属性，可按名称或完整路径查找（本地Python）。 |
| `get_actor_components` | `name` 或 `actor_path`、`detailed_components=True`、`world_type="auto"` | 获取单个 Actor 的组件列表（本地Python）。 |
| `get_scene_components` | `pattern=""`、`detailed_components=True`、`world_type="auto"` | 按场景范围汇总组件信息，可选按 Actor 名称模式过滤（本地Python）。 |
| `get_world_settings` | `world_type="editor"`、可选 `property_names=[]` | 读取当前编辑器 World 的 `WorldSettings` 常用属性或指定属性（本地Python）。 |
| `set_world_settings` | `settings={...}`、`world_type="editor"` | 批量写入当前编辑器 World 的 `WorldSettings` 属性（本地Python）。 |
| `set_actor_property` | `name`、`property_name`、`property_value` | 通过反射设置 Actor 属性（本地Python）。当前已覆盖常见布尔、数字、字符串、Name 与枚举值。 |
| `spawn_blueprint_actor` | `blueprint_name`、`actor_name`、`location=[0,0,0]`、`rotation=[0,0,0]`、`scale=[1,1,1]` | 将 Blueprint 资产实例化到场景中（本地Python）。默认兼容旧的 `/Game/Blueprints/<name>` 查找方式，也支持直接传完整资产路径。 |

## 类型与限制

`spawn_actor` 当前对外稳定支持的 `type` 值来自 Unreal 侧显式分支：

- `StaticMeshActor`
- `PointLight`
- `SpotLight`
- `DirectionalLight`
- `CameraActor`

补充说明：

- Unreal 侧 `HandleSpawnActor` 实际还支持 `scale` 与 `static_mesh` 参数，但当前 Python MCP 工具没有把这两个参数公开出来。
- 如果后续需要对外支持这两个字段，必须同时修改 Python 工具签名与文档。
- `spawn_actor_from_class` 当前支持两类 `class_path` 输入：
  - 原生类路径，例如 `/Script/Engine.StaticMeshActor`
  - Blueprint 资产路径或生成类路径，例如 `/Game/Blueprints/BP_Test.BP_Test` 或 `/Game/Blueprints/BP_Test.BP_Test_C`
- `spawn_actor_from_class` 会先尝试按类路径加载；若失败，再回退把路径当作 Blueprint 资产并解析其 `generated_class`。

`create_light` / `set_light_properties` 当前支持的 `light_type`：

- `PointLight`
- `SpotLight`
- `DirectionalLight`

当前支持的常用灯光参数包括：

- `intensity`
- `light_color=[R,G,B]` 或 `[R,G,B,A]`
- `attenuation_radius`
- `source_radius`
- `source_length`
- `indirect_lighting_intensity`
- `cast_shadows`
- `affects_world`
- `mobility="Static|Stationary|Movable"`
- `temperature`
- `use_temperature`
- `cast_volumetric_shadow`
- `volumetric_scattering_intensity`
- Spot Light 额外支持 `inner_cone_angle`、`outer_cone_angle`

`set_post_process_settings` 当前支持两类输入：

- Volume 级字段：
  - `blend_radius`
  - `blend_weight`
  - `priority`
  - `enabled`
  - `unbound`
- `settings` 字典：
  - key 直接对应 `PostProcessSettings` 属性名，例如 `bloom_intensity`、`vignette_intensity`、`auto_exposure_min_brightness`
  - 工具会自动把对应的 `override_<property>` 设为 `true`
  - 当前已稳定覆盖标量、布尔、枚举字符串、`LinearColor` 和 `Vector`

`get_world_settings` / `set_world_settings` 当前约束：

- 只支持 `editor` 世界，不支持 `pie`
- `get_world_settings`
  - 不传 `property_names` 时，会返回一组稳定的常用字段，例如：
    - `kill_z`
    - `global_gravity_set`
    - `global_gravity_z`
    - `enable_world_bounds_checks`
    - `enable_navigation_system`
    - `enable_ai_system`
    - `default_game_mode`
    - `world_to_meters`
- `set_world_settings`
  - `settings` 必须是对象，key 为 `WorldSettings` 的反射属性名
  - 当前已稳定覆盖布尔、整数、浮点、字符串、`Name`、枚举、`Vector`、`Rotator`、`LinearColor`
  - 当属性值是类/对象引用时，支持传 Unreal 对象路径字符串，例如 `/Script/Engine.GameModeBase`

## 返回说明

- `get_actors_in_level` / `find_actors_by_name` / `find_actors`：
  - 成功时直接返回 `success`、`actors`、`actor_count`，并可能附带 `resolved_world_type`、`world_name`、`world_path`。
- `find_actors`：
  - 额外返回 `filters`，用于回显实际应用的过滤条件与排序方式。
- 其余工具：
  - 成功时通常为 `{"status":"success","result":{...}}`
  - 失败时通常为 `{"status":"error","error":"..."}`

`get_actor_properties`、`get_actor_components`、`get_scene_components` 的结果中，Actor/组件对象由公共序列化逻辑生成，常见字段包括：

- Actor：`name`、`class`、`path`、`location`、`rotation`、`scale`、`tags`、`label`、`folder_path`
- 组件：`name`、`class`、`path`、`owner_name`、`component_tags`、`relative_location`、`relative_rotation`、`relative_scale`
- `duplicate_actor`：
  - 返回复制后 Actor 摘要，并补充 `source_actor`
- `select_actor`：
  - 返回被选中 Actor 摘要，并补充 `selected=true`
- `get_world_settings`：
  - 返回 `world_settings_name`、`world_settings_path`、`world_settings_class`、`properties`、`missing_properties`
- `set_world_settings`：
  - 返回 `world_settings_name`、`world_settings_path`、`updated_properties`、`updated_property_count`
- `attach_actor`：
  - 返回 `child_actor`、`child_actor_path`、`parent_actor`、`parent_actor_path`、`socket_name`
- `detach_actor`：
  - 返回 `actor`、`actor_path`
- `set_actor_tags`：
  - 返回 `actor`、`actor_path`、`tags`、`tag_count`
- `set_actor_folder_path`：
  - 返回 `actor`、`actor_path`、`folder_path`
- `set_actor_visibility`：
  - 返回 `actor`、`actor_path`、`visible`、`hidden_in_game`、`editor_temporarily_hidden`
- `set_actor_mobility`：
  - 返回 `actor`、`actor_path`、`mobility`、`root_component`、`root_component_path`
- `set_actors_transform`：
  - 返回 `actors`、`actor_count`，每项为写入后的 Actor 摘要
- `spawn_actor_from_class`：
  - 返回 `requested_name`、`class_path`、`resolved_class_path`、`resolved_class_name`

## 调用示例

### 获取编辑器 World 中全部 Actor

```json
{
  "tool": "get_actors_in_level",
  "args": {
    "world_type": "editor",
    "include_components": false
  }
}
```

### 生成一个点光源

```json
{
  "tool": "create_light",
  "args": {
    "actor_name": "TestPointLight",
    "light_type": "PointLight",
    "location": [0, 0, 300],
    "intensity": 6500,
    "light_color": [1.0, 0.95, 0.82, 1.0],
    "attenuation_radius": 1800,
    "mobility": "Movable"
  }
}
```

### 按类路径生成一个 StaticMeshActor

```json
{
  "tool": "spawn_actor_from_class",
  "args": {
    "actor_name": "SM_FromClass",
    "class_path": "/Script/Engine.StaticMeshActor",
    "location": [0, 200, 120],
    "scale": [1.2, 1.2, 1.2]
  }
}
```

### 按类、标签和文件夹联合筛选 Actor

```json
{
  "tool": "find_actors",
  "args": {
    "world_type": "editor",
    "class_name": "StaticMeshActor",
    "tags": ["Gameplay", "Interactable"],
    "folder_path": "Gameplay/Interactables",
    "sort_by": "label"
  }
}
```

### 复制一个 Actor 并向 X 轴偏移

```json
{
  "tool": "duplicate_actor",
  "args": {
    "name": "SM_TestCube",
    "world_type": "editor",
    "offset": [150, 0, 0]
  }
}
```

### 选中一个 Actor 并替换当前选择集

```json
{
  "tool": "select_actor",
  "args": {
    "name": "SM_TestCube_A",
    "world_type": "editor",
    "replace_selection": true
  }
}
```

### 批量写入多个 Actor 的缩放

```json
{
  "tool": "set_actors_transform",
  "args": {
    "actor_names": ["SM_TestCube_A", "SM_TestCube_B"],
    "world_type": "editor",
    "scale": [1.5, 1.5, 1.5]
  }
}
```

### 读取当前 WorldSettings 的常用字段

```json
{
  "tool": "get_world_settings",
  "args": {
    "world_type": "editor"
  }
}
```

### 调整当前 World 的重力和世界边界检测

```json
{
  "tool": "set_world_settings",
  "args": {
    "world_type": "editor",
    "settings": {
      "global_gravity_set": true,
      "global_gravity_z": -980.0,
      "enable_world_bounds_checks": false
    }
  }
}
```

### 将一个 Actor 挂到另一个 Actor

```json
{
  "tool": "attach_actor",
  "args": {
    "name": "SM_TestCube_A",
    "parent_name": "SM_TestCube_B",
    "keep_world_transform": true
  }
}
```

### 将 Actor 从父对象分离

```json
{
  "tool": "detach_actor",
  "args": {
    "name": "SM_TestCube_A",
    "keep_world_transform": true
  }
}
```

### 覆盖 Actor 标签

```json
{
  "tool": "set_actor_tags",
  "args": {
    "name": "SM_TestCube_A",
    "tags": ["Gameplay", "Interactable"]
  }
}
```

### 设置 Actor 文件夹路径

```json
{
  "tool": "set_actor_folder_path",
  "args": {
    "name": "SM_TestCube_A",
    "folder_path": "Gameplay/Interactables"
  }
}
```

### 设置 Actor 可见性

```json
{
  "tool": "set_actor_visibility",
  "args": {
    "name": "SM_TestCube_A",
    "visible": false
  }
}
```

### 设置 Actor Mobility

```json
{
  "tool": "set_actor_mobility",
  "args": {
    "name": "SM_TestCube_A",
    "mobility": "Movable"
  }
}
```

### 调整一个 Spot Light 的锥角和阴影参数

```json
{
  "tool": "set_light_properties",
  "args": {
    "name": "TestSpotLight",
    "intensity": 12000,
    "inner_cone_angle": 18,
    "outer_cone_angle": 36,
    "cast_shadows": true,
    "cast_volumetric_shadow": true
  }
}
```

### 修改 PostProcessVolume 的 Bloom 与 Vignette

```json
{
  "tool": "set_post_process_settings",
  "args": {
    "name": "PPV_CodexTest",
    "blend_weight": 1.0,
    "priority": 10,
    "unbound": true,
    "settings": {
      "bloom_intensity": 2.5,
      "vignette_intensity": 0.4
    }
  }
}
```
