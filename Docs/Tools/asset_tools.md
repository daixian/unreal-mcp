# 资产工具

## 概览

本页描述资产检索、元数据查询、导入导出、重导入、重定向器修复、材质资产操作与资产保存相关工具。

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

### `save_asset`

成功结果中包含：

- `saved`
- `was_dirty`
- `only_if_dirty`
- `save_attempted`

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
