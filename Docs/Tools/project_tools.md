# 项目工具

## 概览

本页描述项目级全局配置工具。当前对外公开能力只有输入映射创建。

- Python 注册位置：`Python/tools/project_tools.py`
- Unreal 命令处理位置：`Source/UnrealMCP/Private/Commands/UnrealMCPProjectCommands.cpp`

## 工具列表

| 工具 | 关键参数 | 说明 |
| --- | --- | --- |
| `create_input_mapping` | `action_name`、`key`、`input_type="Action"` | 在项目默认输入设置中创建输入映射，支持 `Action` 和 `Axis`。 |

## 参数注意事项

### `input_type`

可选值：

- `Action`
- `Axis`

当前 Python MCP 工具公开参数只有：

- `action_name`
- `key`
- `input_type`

Unreal 侧实现还支持以下扩展字段，但 Python 工具尚未公开：

- `scale`：Axis 映射缩放
- `shift`
- `ctrl`
- `alt`
- `cmd`

如果要把这些参数开放给 MCP 客户端，需要同步修改 Python 工具签名、文档和测试。

## 返回说明

成功结果中包含：

- `action_name`
- `key`
- `input_type`

失败时通常返回：

```json
{
  "status": "error",
  "error": "..."
}
```

## 调用示例

### 创建一个动作映射

```json
{
  "tool": "create_input_mapping",
  "args": {
    "action_name": "Jump",
    "key": "SpaceBar",
    "input_type": "Action"
  }
}
```

### 创建一个轴映射

```json
{
  "tool": "create_input_mapping",
  "args": {
    "action_name": "MoveForward",
    "key": "W",
    "input_type": "Axis"
  }
}
```
