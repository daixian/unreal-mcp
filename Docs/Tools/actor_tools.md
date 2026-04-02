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
| `get_actors_in_level` | `include_components=False`、`detailed_components=True`、`world_type="auto"` | 返回当前关卡 Actor 列表。Python 层会归一化为 `success`、`actors`、`actor_count` 等字段。 |
| `find_actors_by_name` | `pattern`、`world_type="auto"`、`include_components=False`、`detailed_components=True` | 按名称模式筛选 Actor。成功返回结构与 `get_actors_in_level` 类似，并附带 `pattern`。 |
| `spawn_actor` | `name`、`type`、`location=[0,0,0]`、`rotation=[0,0,0]` | 在编辑器 World 里生成原生 Actor。当前 Python 工具公开的是 `spawn_actor`，不是旧别名 `create_actor`。 |
| `delete_actor` | `name` | 删除指定名称的 Actor。成功时返回被删除 Actor 的摘要。 |
| `set_actor_transform` | `name`、`location=None`、`rotation=None`、`scale=None` | 修改 Actor 变换。只传需要修改的字段即可。 |
| `get_actor_properties` | `name` 或 `actor_path`、`include_components=True`、`detailed_components=True`、`world_type="auto"` | 获取单个 Actor 的详细属性，可按名称或完整路径查找。 |
| `get_actor_components` | `name` 或 `actor_path`、`detailed_components=True`、`world_type="auto"` | 获取单个 Actor 的组件列表。 |
| `get_scene_components` | `pattern=""`、`detailed_components=True`、`world_type="auto"` | 按场景范围汇总组件信息，可选按 Actor 名称模式过滤。 |
| `set_actor_property` | `name`、`property_name`、`property_value` | 通过反射设置 Actor 属性。常见布尔、数字、字符串、Name、枚举值由 Unreal 侧统一处理。 |
| `spawn_blueprint_actor` | `blueprint_name`、`actor_name`、`location=[0,0,0]`、`rotation=[0,0,0]` | 将 Blueprint 资产实例化到场景中。 |

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

## 返回说明

- `get_actors_in_level` / `find_actors_by_name`：
  - 成功时直接返回 `success`、`actors`、`actor_count`，并可能附带 `resolved_world_type`、`world_name`、`world_path`。
- 其余工具：
  - 成功时通常为 `{"status":"success","result":{...}}`
  - 失败时通常为 `{"status":"error","error":"..."}`

`get_actor_properties`、`get_actor_components`、`get_scene_components` 的结果中，Actor/组件对象由公共序列化逻辑生成，常见字段包括：

- Actor：`name`、`class`、`path`、`location`、`rotation`、`scale`、`tags`、`label`、`folder_path`
- 组件：`name`、`class`、`path`、`owner_name`、`component_tags`、`relative_location`、`relative_rotation`、`relative_scale`

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
  "tool": "spawn_actor",
  "args": {
    "name": "TestPointLight",
    "type": "PointLight",
    "location": [0, 0, 300],
    "rotation": [0, 0, 0]
  }
}
```
