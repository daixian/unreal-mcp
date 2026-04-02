# UnrealMCP 工具文档

## 说明

本目录按“使用场景”组织文档，而不是完全按源码文件名组织。  
例如 `Python/tools/editor_tools.py` 中既有编辑器运行控制，也有 Actor/场景操作，因此这里拆成了两份文档来说明。

当前公开工具总数为 `59`。

## 返回约定

绝大多数工具的返回结果遵循以下约定：

- 成功时通常为：

```json
{
  "status": "success",
  "result": {
    "...": "..."
  }
}
```

- 失败时通常为：

```json
{
  "status": "error",
  "error": "..."
}
```

需要额外注意的情况：

- `get_actors_in_level` 与 `find_actors_by_name` 在 Python 层做了归一化，成功时直接返回 `success`、`actors`、`actor_count` 等字段。
- 个别 Python 工具在连接 Unreal 失败时，会直接返回 `{"success": false, "message": "..."}` 这一类兼容结构。

## 工具分组

- [Actor 与场景工具](actor_tools.md)：Actor 查询、生成、属性读取、组件读取与 Blueprint 实例化。
- [编辑器与运行控制工具](editor_tools.md)：关卡保存/加载、PIE、Live Coding、视口控制与截图。
- [Blueprint 资产工具](blueprint_tools.md)：Blueprint 资产创建、组件编辑、编译、Pawn/GameMode 配置。
- [Blueprint 图节点工具](node_tools.md)：事件节点、函数节点、变量节点、通用节点生成与 Pin 编辑。
- [资产工具](asset_tools.md)：资产搜索、元数据、依赖/引用、摘要与保存。
- [项目工具](project_tools.md)：项目输入映射等全局配置。
- [UMG 工具](umg_tools.md)：Widget Blueprint 创建、控件添加、事件绑定、视口挂载。

## 维护建议

- 如果工具文档和代码不一致，优先以代码为准。
- 修改 C++ 命令处理器后，不要只更新 Python 文档；需要同步检查工具注册、参数转发和文档说明。
- 如果新增一类独立能力，优先新增单独文档，而不是继续把所有内容堆到同一页。
