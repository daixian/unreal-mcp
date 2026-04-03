# 资产工具

## 概览

本页描述资产检索、元数据查询、元数据写入、导入导出、重导入、重定向器修复、引用替换 / 合并、材质资产操作与资产保存相关工具。

- Python 注册位置：`Python/tools/asset_tools.py`
- Unreal 命令处理位置：`Source/UnrealMCP/Private/Commands/UnrealMCPAssetCommands.cpp`

## 资产定位规则

除 `search_assets` 之外，其余资产工具都支持以下任一定位字段：

- `asset_path`
- `object_path`
- `asset_name`
- `name`

至少需要提供其中一个。  
Unreal 侧会优先按路径/对象路径解析，必要时再退回到按名称搜索。

## 工具列表

| 工具 | 关键参数 | 说明 |
| --- | --- | --- |
| `search_assets` | `path="/Game"`、`query=""`、`class_name=""`、`recursive_paths=True`、`include_tags=False`、`limit=50` | 按路径、关键字和类名搜索资产。 |
| `get_asset_metadata` | 任一资产定位字段 | 返回基础身份信息、标签、依赖和引用者。 |
| `get_asset_dependencies` | 任一资产定位字段 | 返回详细依赖列表，包含依赖类别、关系类型和属性。 |
| `get_asset_referencers` | 任一资产定位字段 | 返回详细引用者列表。 |
| `get_asset_summary` | 任一资产定位字段 | 加载资产并按类型生成结构化摘要。 |
| `save_asset` | 任一资产定位字段、`only_if_dirty=False` | 保存指定资产；可选择只在 Dirty 时保存。 |
| `import_asset` | `destination_path`、`filename=""`、`source_files=[]`、`destination_name=""`、`async_import=True` | 从外部文件导入资产。为避免 UE5.7 Interchange 同步导入断言，当前统一按异步导入执行。 |
| `export_asset` | `export_path`、`asset_path=""`、`asset_paths=[]`、`clean_filenames=True` | 将一个或多个资产导出到外部目录。 |
| `reimport_asset` | 任一资产定位字段或 `asset_paths=[]`、可选 `source_file_index` 等 | 触发已有资产重导入；当前按异步重导入执行。 |
| `fixup_redirectors` | `directory_path=""`、`directory_paths=[]`、`asset_path=""`、`asset_paths=[]` | 修复指定路径下的 Redirector；若未找到 Redirector，会返回幂等成功结果。 |
| `get_blueprint_summary` | 任一资产定位字段 | 加载 Blueprint 资产并返回 Blueprint 专用摘要。 |
| `create_material` | `name`、`path="/Game/Materials"` | 创建基础 `Material` 资产。 |
| `create_material_instance` | `name`、`parent_material`、`path="/Game/Materials"` | 创建 `MaterialInstanceConstant` 资产。 |
| `get_material_parameters` | 任一资产定位字段 | 读取材质或材质实例的 Scalar/Vector/Texture/Static Switch 参数。 |
| `set_material_instance_scalar_parameter` | 任一资产定位字段、`parameter_name`、`value` | 设置材质实例的 Scalar 参数。 |
| `set_material_instance_vector_parameter` | 任一资产定位字段、`parameter_name`、`value=[R,G,B,A]` | 设置材质实例的 Vector 参数。 |
| `set_material_instance_texture_parameter` | 任一资产定位字段、`parameter_name`、`texture_asset_path` | 设置材质实例的 Texture 参数。 |
| `assign_material_to_actor` | `material_asset_path`、`name=""`/`actor_path=""`、可选 `slot_index=0` / `slot_name=""` | 给目标 Actor 上所有匹配材质槽的 `MeshComponent` 赋材质。 |
| `assign_material_to_component` | `material_asset_path`、`component_name`、`name=""`/`actor_path=""`、可选 `slot_index=0` / `slot_name=""` | 给目标 Actor 的指定 `MeshComponent` 赋材质。 |
| `replace_material_slot` | `material_asset_path`、`name=""`/`actor_path=""`、可选 `component_name`、必填 `slot_index` 或 `slot_name` | 精确替换某个材质槽，未指定 `component_name` 时要求 Actor 只有一个 `MeshComponent`。 |
| `add_material_expression` | 任一基础材质定位字段、`expression_class`、可选 `node_position=[X,Y]`、`selected_asset_path`、`property_values={}` | 在材质图里创建一个表达式节点，并可附带基础属性覆写。 |
| `connect_material_expressions` | 任一基础材质定位字段、`from_expression_*`、可选 `from_output_name`、以及 `to_expression_*` 或 `material_property` 二选一 | 连接两个表达式，或把表达式输出接到材质属性。 |
| `layout_material_graph` | 任一基础材质定位字段 | 自动整理材质图里全部表达式节点布局。 |
| `compile_material` | 任一基础材质定位字段 | 触发材质重新编译并保存。 |
| `set_asset_metadata` | 任一资产定位字段、`metadata={}`、`remove_metadata_keys=[]`、`clear_existing=False` | 写入或移除资产元数据标签。 |
| `consolidate_assets` | 目标资产定位字段（`target_*`）、`source_assets=[]` | 将多个重复资产合并到目标资产，并删除来源资产。 |
| `replace_asset_references` | 替换目标资产定位字段（`replacement_*`）、`assets_to_replace=[]` | 使用 UE 的 Replace References 工作流替换引用，并删除来源资产。 |

## 结果说明

### `search_assets`

成功结果中包含：

- `path`
- `query`
- `class_name`
- `total_matches`
- `returned_count`
- `assets`

每个资产对象至少包含：

- `asset_name`
- `asset_class`
- `asset_class_path`
- `asset_path`
- `package_name`
- `package_path`
- `object_path`

如果 `include_tags=True`，还会包含：

- `tags`
- `tag_count`

### `get_asset_metadata`

成功结果在资产身份信息基础上，额外包含：

- `tags`
- `dependencies`
- `referencers`

### `get_asset_dependencies` / `get_asset_referencers`

详细依赖项中常见字段包括：

- `asset_id`
- `package_name`
- `object_name`
- `category`
- `relation_type`
- `properties`

### `get_asset_summary`

Unreal 侧会根据实际资产类型生成不同摘要，当前已经显式支持：

- `Blueprint`
- `WidgetBlueprint`
- `StaticMesh`
- `DataTable`
- `World`
- `MaterialInstance`
- 其他类型统一归为 `Generic`

### `get_blueprint_summary`

成功结果包含 Blueprint 专用结构，例如：

- `parent_class`
- `generated_class`
- `components`
- `variables`
- `ubergraph_pages`
- `function_graphs`
- `total_node_count`
- `default_values`
- `dependencies`

如果是 `WidgetBlueprint`，还会额外包含：

- `widget_tree`
- `bindings`
- `animations`

### `create_material` / `create_material_instance`

成功结果中常见字段：

- `asset_name`
- `asset_path`
- `package_path`
- `asset_class`

其中 `create_material_instance` 还会额外返回：

- `parent_material`

说明：

- 两个工具默认都在 `/Game/Materials` 下创建资源。
- `create_material_instance` 当前要求父资源必须是 `Material` 或 `MaterialInstance`。

### `get_material_parameters`

成功结果中常见字段：

- `summary_kind`
- `is_material_instance`
- `parent_material`
- `child_instances`
- `scalar_parameters`
- `vector_parameters`
- `texture_parameters`
- `static_switch_parameters`

每个参数对象都会带上：

- `name`
- `association`
- `association_index`
- `group`
- `description`
- `source_asset_path`
- `override`

不同类型还会额外带上：

- Scalar: `value`
- Vector: `value=[R,G,B,A]`
- Texture: `texture_name`、`texture_path`
- Static Switch: `value`、`dynamic`

### `set_material_instance_scalar_parameter` / `set_material_instance_vector_parameter` / `set_material_instance_texture_parameter`

成功结果中常见字段：

- `asset_name`
- `asset_path`
- `parameter_name`

不同类型还会额外带上：

- Scalar: `value`
- Vector: `value=[R,G,B,A]`
- Texture: `texture_name`、`texture_path`

说明：

- 这 3 个写入工具当前只接受 `MaterialInstanceConstant`。
- 每次写入后都会自动更新实例并保存资产。

### `assign_material_to_actor` / `assign_material_to_component` / `replace_material_slot`

成功结果中常见字段：

- `actor_name`
- `actor_path`
- `world_type`
- `component_name`
- `component_path`
- `slot_index`
- `slot_name`
- `previous_material_name`
- `previous_material_path`
- `material_name`
- `material_path`

其中 `assign_material_to_actor` 还会额外返回：

- `updated_components`
- `skipped_components`
- `updated_component_count`
- `skipped_component_count`

说明：

- 这 3 个工具都支持 `name` 或 `actor_path` 定位 Actor，并支持 `world_type="auto|editor|pie"`。
- `assign_material_to_actor` 会遍历目标 Actor 上全部 `MeshComponent`，对能解析到目标槽位的组件批量赋材质。
- `assign_material_to_component` 用于精确修改单个 `component_name`。
- `replace_material_slot` 要求显式指定 `slot_index` 或 `slot_name`，避免误把默认槽位 0 当成精确替换。
- 在 `editor` 世界下会标记 Actor/组件包为 Dirty；在 `pie` 世界下只修改运行时实例。

### `add_material_expression` / `connect_material_expressions` / `layout_material_graph` / `compile_material`

成功结果中常见字段：

- `material_name`
- `material_path`
- `expression_name`
- `expression_path`
- `expression_class`
- `expression_class_path`
- `expression_index`
- `node_pos_x`
- `node_pos_y`
- `input_names`
- `output_names`
- `expression_count`

其中：

- `add_material_expression` 还会额外返回 `selected_asset_path`、`resolved_expression_class_path`、`applied_properties`
- `connect_material_expressions` 会返回：
  - `connection_kind="expression|material_property"`
  - `from_expression`
  - `from_output_name`
  - `to_expression` 或 `material_property`

说明：

- 这 4 个工具当前都只接受基础 `Material`，不接受 `MaterialInstanceConstant`。
- `expression_class` 支持短类名，例如 `Constant`、`MaterialExpressionConstant3Vector`，也支持完整类路径。
- `property_values` 会优先按常见基础属性写入；目前已经覆盖数字、布尔、枚举、对象引用以及 `Vector` / `Vector2D` / `LinearColor`。
- `connect_material_expressions` 允许两种目标：
  - 连接到另一个表达式的输入 pin
  - 连接到材质属性，例如 `base_color`、`roughness`、`normal`、`emissive_color`
- `layout_material_graph` 会返回整理后的表达式摘要列表，方便后续脚本继续连线。
- `compile_material` 会触发 `MaterialEditingLibrary::RecompileMaterial`，随后保存材质资产。

### `save_asset`

成功结果中包含：

- `saved`
- `was_dirty`
- `only_if_dirty`
- `save_attempted`

### `set_asset_metadata`

成功结果中常见字段：

- `updated_keys`
- `removed_keys`
- `updated_count`
- `removed_count`
- `metadata_count`
- `metadata`
- `saved`

说明：

- `metadata` 的值最终都会按字符串写入 UE 元数据系统；布尔、数字、数组、对象会先转成字符串。
- `clear_existing=True` 会先清空当前已存在的全部元数据，再应用 `metadata` 和 `remove_metadata_keys`。
- 该工具修改的是 Loaded Asset Metadata，不是 `AssetRegistry` 的内建标签字段。

### `consolidate_assets` / `replace_asset_references`

成功结果中常见字段：

- `operation_kind`
- `deleted_source_assets`
- `source_asset_count`
- `deleted_asset_count`
- `source_assets`

说明：

- 这两个工具底层都走 UE 编辑器原生的 Consolidate / Replace References 工作流。
- 当前实现都会删除来源资产，并把引用切换到目标资产；因此返回里固定带 `deleted_source_assets=true`。
- `consolidate_assets` 使用 `target_asset_path|target_object_path|target_asset_name|target_name` 定位目标资产。
- `replace_asset_references` 使用 `replacement_asset_path|replacement_object_path|replacement_asset_name|replacement_name` 定位替换目标资产。
- `source_assets` / `assets_to_replace` 中的资产必须与目标资产同类，否则命令会失败。

### `import_asset`

成功结果中常见字段：

- `destination_path`
- `destination_name`
- `requested_async_import`
- `async_import`
- `import_completed`
- `has_async_results`
- `source_files`
- `imported_object_paths`
- `expected_object_paths`

说明：

- UE5.7 下通过 MCP 命令同步走 Interchange 导入会触发 TaskGraph 递归断言，因此该工具当前统一转为异步导入。
- 当 `import_completed=false` 且 `expected_object_paths` 非空时，调用方应随后用 `search_assets` 或其他查询工具轮询确认资产出现。

### `export_asset`

成功结果中常见字段：

- `export_path`
- `clean_filenames`
- `assets`
- `new_files`
- `new_file_count`

### `reimport_asset`

成功结果中常见字段：

- `assets`
- `asset_count`
- `ask_for_new_file_if_missing`
- `show_notification`
- `force_new_file`
- `automated`
- `force_show_dialog`
- `source_file_index`
- `preferred_reimport_file`

每个资产项中常见字段：

- `can_reimport`
- `reimport_started`
- `reimport_completed`
- `reimport_result`
- `source_files`

说明：

- 当前实现统一走异步重导入，避免 UE5.7 下同步 Interchange 重导入断言崩溃。
- 常见 `reimport_result` 为 `pending` 或 `completed`；`pending` 时可继续轮询其他只读命令确认编辑器仍然存活，等待后台导入结束。

### `fixup_redirectors`

成功结果中常见字段：

- `redirectors`
- `redirector_count`
- `remaining_redirector_count`
- `directory_paths`
- `recursive_paths`
- `checkout_dialog_prompt`
- `fixup_mode`
- `message`

说明：

- 当目标路径下没有 Redirector 时，该工具会返回 `success=true` 与 `redirector_count=0`，便于自动化任务幂等执行。

## 调用示例

### 搜索 `/Game/UI` 下的 WidgetBlueprint

```json
{
  "tool": "search_assets",
  "args": {
    "path": "/Game/UI",
    "class_name": "WidgetBlueprint",
    "recursive_paths": true,
    "limit": 20
  }
}
```

### 获取某个 Blueprint 的结构摘要

```json
{
  "tool": "get_blueprint_summary",
  "args": {
    "asset_path": "/Game/UI/WBP_MainMenu"
  }
}
```

### 导入一个外部 PNG，再轮询资产是否出现

```json
{
  "tool": "import_asset",
  "args": {
    "filename": "X:/work/conan2/veidon/UE5/XREmpty/Saved/MCPTemp/CodexImportTexture.png",
    "destination_path": "/Game/MCPTemp",
    "destination_name": "T_CodexImportTexture_Test",
    "automated": true,
    "save": true,
    "async_import": true
  }
}
```

### 导出一个 Texture2D 到外部目录

```json
{
  "tool": "export_asset",
  "args": {
    "asset_path": "/Game/MCPTemp/T_CodexImportTexture_Test",
    "export_path": "X:/work/conan2/veidon/UE5/XREmpty/Saved/MCPExport",
    "clean_filenames": true
  }
}
```

### 给资产写元数据

```json
{
  "tool": "set_asset_metadata",
  "args": {
    "asset_path": "/Game/MCPTemp/MI_CodexTemp",
    "metadata": {
      "Owner": "Codex",
      "Reviewed": true,
      "Priority": 3
    },
    "remove_metadata_keys": ["DeprecatedTag"],
    "save_asset": true
  }
}
```

### 合并重复资产

```json
{
  "tool": "consolidate_assets",
  "args": {
    "target_asset_path": "/Game/MCPTemp/M_Master",
    "source_assets": [
      "/Game/MCPTemp/M_Master_CopyA",
      "/Game/MCPTemp/M_Master_CopyB"
    ]
  }
}
```

### 批量 Replace References

```json
{
  "tool": "replace_asset_references",
  "args": {
    "replacement_asset_path": "/Game/MCPTemp/T_Final",
    "assets_to_replace": [
      "/Game/MCPTemp/T_OldA",
      "/Game/MCPTemp/T_OldB"
    ]
  }
}
```

### 创建一个基础材质和材质实例

```json
{
  "tool": "create_material",
  "args": {
    "name": "M_CodexTemp",
    "path": "/Game/MCPTemp"
  }
}
```

```json
{
  "tool": "create_material_instance",
  "args": {
    "name": "MI_CodexTemp",
    "path": "/Game/MCPTemp",
    "parent_material": "/Game/MCPTemp/M_CodexTemp"
  }
}
```

### 读取并修改材质实例参数

```json
{
  "tool": "get_material_parameters",
  "args": {
    "asset_path": "/Game/MCPTemp/MI_CodexTemp"
  }
}
```

```json
{
  "tool": "set_material_instance_vector_parameter",
  "args": {
    "asset_path": "/Game/MCPTemp/MI_CodexTemp",
    "parameter_name": "Tint",
    "value": [1.0, 0.3, 0.2, 1.0]
  }
}
```

### 给 Actor 或组件赋材质

```json
{
  "tool": "assign_material_to_actor",
  "args": {
    "name": "SM_TestActor",
    "material_asset_path": "/Game/MCPTemp/MI_CodexTemp",
    "slot_index": 0,
    "world_type": "editor"
  }
}
```

```json
{
  "tool": "assign_material_to_component",
  "args": {
    "name": "SM_TestActor",
    "component_name": "StaticMeshComponent0",
    "material_asset_path": "/Game/MCPTemp/MI_CodexTemp",
    "slot_name": "Element0"
  }
}
```

```json
{
  "tool": "replace_material_slot",
  "args": {
    "name": "SM_TestActor",
    "component_name": "StaticMeshComponent0",
    "slot_name": "Element0",
    "material_asset_path": "/Engine/EngineMaterials/DefaultMaterial"
  }
}
```

### 在材质图中添加表达式、连线、整理并编译

```json
{
  "tool": "add_material_expression",
  "args": {
    "asset_path": "/Game/MCPTemp/M_CodexGraph_Example",
    "expression_class": "Constant3Vector",
    "node_position": [-600, -120],
    "property_values": {
      "Constant": [0.1, 0.6, 0.9, 1.0]
    }
  }
}
```

```json
{
  "tool": "connect_material_expressions",
  "args": {
    "asset_path": "/Game/MCPTemp/M_CodexGraph_Example",
    "from_expression_name": "MaterialExpressionConstant3Vector_0",
    "material_property": "base_color"
  }
}
```

```json
{
  "tool": "layout_material_graph",
  "args": {
    "asset_path": "/Game/MCPTemp/M_CodexGraph_Example"
  }
}
```

```json
{
  "tool": "compile_material",
  "args": {
    "asset_path": "/Game/MCPTemp/M_CodexGraph_Example"
  }
}
```
