# 资产工具

## 概览

本页描述资产检索、元数据查询、依赖分析、摘要生成与资产保存相关工具。

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
| `get_blueprint_summary` | 任一资产定位字段 | 加载 Blueprint 资产并返回 Blueprint 专用摘要。 |

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

### `save_asset`

成功结果中包含：

- `saved`
- `was_dirty`
- `only_if_dirty`
- `save_attempted`

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
