# 元数据工具

## 概览

本页描述 UnrealMCP 的“自描述”能力，用于导出当前 Python 层已公开的工具清单与输入参数 Schema。

- Python 注册位置：`Python/tools/meta_tools.py`
- 元数据提取位置：`Python/tools/tool_metadata.py`
- 这些工具通过静态解析 `Python/tools/*_tools.py` 生成结果，不依赖 Unreal 编辑器连接状态。

## 工具列表

| 工具 | 关键参数 | 说明 |
| --- | --- | --- |
| `list_mcp_tools` | `group=""`、`name_contains=""`、`include_parameters=False` | 列出当前已注册的工具名称、来源模块、描述与参数摘要。 |
| `export_tool_schema` | `tool_name=""`、`group=""` | 导出一个或一组工具的输入 Schema，便于生成文档、校验参数或做外部桥接。 |

## 返回说明

### `list_mcp_tools`

成功时返回：

- `tool_count`：过滤后命中的工具数量
- `total_tool_count`：当前本地全部工具数量
- `tools`：工具摘要数组

每个工具摘要包含：

- `name`
- `group`
- `module`
- `source_file`
- `description`
- `parameter_count`
- `parameter_names`
- `parameters`：仅当 `include_parameters=true` 时返回

### `export_tool_schema`

成功时返回：

- `schema_version`
- `tool_count`
- `total_tool_count`
- `tools`

`tools[*].input_schema` 为 object schema，包含：

- `properties`
- `required`
- `additionalProperties`

## 参数注意事项

- `group` 使用工具文件名去掉 `_tools` 后的分组名，例如 `editor`、`asset`、`meta`。
- `tool_name` 是精确匹配，`name_contains` 是大小写不敏感的子串匹配。
- Schema 来源于 Python 工具函数签名与 docstring，因此如果以后改了参数定义或说明文字，这两个工具会自动反映最新本地状态。

## 调用示例

### 列出全部工具

```json
{
  "tool": "list_mcp_tools",
  "args": {}
}
```

### 列出 Blueprint 相关工具并附带参数

```json
{
  "tool": "list_mcp_tools",
  "args": {
    "group": "blueprint",
    "include_parameters": true
  }
}
```

### 导出单个工具的输入 Schema

```json
{
  "tool": "export_tool_schema",
  "args": {
    "tool_name": "spawn_actor"
  }
}
```
