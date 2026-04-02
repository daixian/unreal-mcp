# UnrealMCP 文档

## 文档目标

本文档用于说明 `UnrealMCP` 插件当前已经暴露的 MCP 能力，以及这些能力在仓库中的维护方式。

- 文档默认描述“对外 MCP 工具接口”，以 `Python/tools` 中注册的工具为准。
- 如果工具的参数、返回结构或行为依赖 Unreal 侧实现，则同时以 `Source/UnrealMCP/Private/Commands` 中对应命令处理器为准。
- 兼容别名只在必要时说明，不作为推荐接口写法。例如 Unreal 侧兼容 `create_actor`，但对外应使用 `spawn_actor`。

## 当前覆盖范围

当前公开 MCP 工具共 `59` 个，按使用场景可以分为以下 7 组：

- Actor 与场景工具：10 个
- 编辑器与运行控制工具：13 个
- Blueprint 资产工具：10 个
- Blueprint 图节点工具：12 个
- 资产工具：7 个
- 项目工具：1 个
- UMG 工具：6 个

## 文档目录

- [工具总览](Tools/README.md)：所有工具分组、返回约定和文档索引。
- [Python 侧服务说明](../Python/README.md)：Python MCP 服务的安装、调试与开发说明。

## 维护约定

更新文档时请遵守以下原则：

1. 涉及工具名、参数、返回结构时，同时检查 `Python/tools/*.py` 与 `Source/UnrealMCP/Private/Commands/*.cpp`。
2. 文档优先描述最终对外可调用的参数，而不是 Unreal 侧的内部兼容字段。
3. 若 Python 层对返回结果做了归一化处理，文档以“最终返回给 MCP 客户端的结果”描述为准。
4. 新增工具时，同时更新 [Tools/README.md](Tools/README.md) 与对应分组文档。
