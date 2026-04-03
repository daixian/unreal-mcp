# UnrealMCP

UnrealMCP 是一个面向 Unreal Editor 的 MCP 插件方案，用来把 Unreal Engine 编辑器能力暴露给支持 Model Context Protocol 的 AI 客户端。

当前仓库已经不再沿用早期“单一插件 + 附带脚本”的组织方式，而是明确拆分为三层：

- UE C++ 插件层：运行在 Unreal Editor 内，负责命令执行、桥接服务与主路由。
- 插件内 Python 命令层：位于 `Content/Python/commands`，负责在 Unreal Python 环境里实现部分本地命令逻辑。
- 外部 Python MCP 层：独立运行在 `MCPServer/`，负责 MCP 协议接入、工具注册、请求转发与返回归一化。

仓库根目录本身就是插件目录，最终放置位置就是：

```text
<你的UE项目>/Plugins/UnrealMCP
```

## 当前架构

### 1. UE 插件层

位置：

- `Source/UnrealMCP`

职责：

- 在 Unreal Editor 内启动桥接子系统
- 监听本地 TCP 连接，当前默认端口为 `55557`
- 将 MCP 请求路由到各类 Unreal 命令处理器
- 调用编辑器、资产、Blueprint、UMG、项目设置等 Unreal API

这一层是“真正执行 Unreal 操作”的地方。当前主要命令分发入口在：

- `Source/UnrealMCP/Private/UnrealMCPBridge.cpp`

### 2. 插件内 Python 命令层

位置：

- `Content/Python/commands`
- `Content/Python/commands/common/command_bridge.py`

职责：

- 作为插件自带的 Unreal Python 包随插件一起分发
- 在编辑器内直接调用 `unreal` Python API
- 承载部分本地命令实现，而不是全部都放在 C++ 中
- 通过本地命令桥把模块名、函数名、参数 JSON 分发到具体处理器
- 便于把更适合 Python 的编辑器逻辑从 C++ 命令层中抽离

这一层运行在 Unreal Editor 内部的 Python 环境里，属于插件本体的一部分，不是独立 MCP Server。

### 3. 外部 Python MCP 层

位置：

- `MCPServer/unreal_mcp_server.py`
- `MCPServer/tools/*.py`

职责：

- 作为标准 MCP Server 运行，对外使用 `stdio` 与 MCP 客户端通信
- 维护到 Unreal 插件 TCP 服务的连接
- 将 MCP 工具参数转换为 Unreal 侧命令
- 对返回结果做适度归一化
- 注册对外可见的工具与工具元数据

这一层是“协议适配和工具暴露”的地方，不参与 UE 模块编译，也不直接属于 Unreal 插件运行时模块。

## 仓库结构

```text
UnrealMCP/
├─Content/                     # 插件资源
│  └─Python/
│     └─commands/              # 插件内本地 Python 命令层
├─Docs/                        # 工具文档与维护说明
├─MCPServer/                   # 独立 Python MCP 服务
│  ├─scripts/                  # 直连调试脚本
│  ├─tools/                    # MCP 工具注册
│  ├─pyproject.toml
│  └─unreal_mcp_server.py
├─Source/UnrealMCP/            # UE C++ 插件模块
├─STD/                         # 项目规范文档
├─UnrealMCP.uplugin
└─README.md
```

## 适用范围

当前插件是 `Editor` 类型插件，主要服务于编辑器自动化和 AI 工作流，不是运行时插件。

已启用的关键依赖见 `UnrealMCP.uplugin`：

- `EditorScriptingUtilities`
- `EnhancedInput`
- `PythonScriptPlugin`

## 安装

### 安装到 UE 项目

把仓库放到目标项目的插件目录下：

```bash
cd <你的UE项目目录>
git clone <repo-url> Plugins/UnrealMCP
```

或者作为子模块：

```bash
git submodule add <repo-url> Plugins/UnrealMCP
git submodule update --init --recursive
```

然后：

1. 重新生成 `.uproject` 对应的工程文件。
2. 编译 `<你的项目名>Editor`。
3. 启动 Unreal Editor。
4. 在插件列表中确认 `UnrealMCP` 已启用。

## 运行方式

使用 UnrealMCP 时，实际涉及三层协作：

1. Unreal Editor 中的 `UnrealMCP` C++ 插件
2. 插件内 `Content/Python/commands` 本地 Python 命令层
3. `MCPServer/` 下的外部 Python MCP 服务

对 MCP 客户端来说，通常需要显式启动的是第 1 和第 3 部分；第 2 部分随插件一起被 Unreal Python 环境加载和调用。

### 启动 Unreal 侧

先打开包含本插件的 UE 项目。插件加载后，会在编辑器侧提供本地 TCP 桥接服务；当命令被设计为本地 Python 实现时，也会通过插件内的 `Content/Python/commands` 包进入 Unreal Python 处理逻辑。

### 启动 Python MCP 服务

进入 `MCPServer/`，准备独立 Python 环境：

```bash
cd Plugins/UnrealMCP/MCPServer
uv venv
uv pip install -e .
```

然后启动服务：

```bash
uv run unreal_mcp_server.py
```

该服务对 MCP 客户端走 `stdio`，对 Unreal Editor 走本地 TCP。

## MCP 客户端配置示例

以下示例以 `uv` 启动 `MCPServer`：

```json
{
  "mcpServers": {
    "unrealMCP": {
      "command": "uv",
      "args": [
        "--directory",
        "X:/Path/To/YourProject/Plugins/UnrealMCP/MCPServer",
        "run",
        "unreal_mcp_server.py"
      ]
    }
  }
}
```

启动前请确认：

- Unreal Editor 已经打开目标项目
- `UnrealMCP` 插件已经成功加载
- Python 依赖已经在 `MCPServer/` 中安装完成

## 目前的能力范围

当前公开能力集中在 Unreal Editor 自动化，主要包括：

- Actor 与场景管理
- 编辑器控制与运行控制
- Blueprint 资产创建与修改
- Blueprint 图节点创建、连接、布局与检查
- UMG Widget Blueprint 编辑
- 资产检索、导入导出、材质与元数据操作
- 项目输入与部分项目设置管理
- 元工具，如工具列表和 schema 导出

详细工具说明见：

- [Docs/README.md](Docs/README.md)
- [Docs/Tools/README.md](Docs/Tools/README.md)
- [MCPServer/README.md](MCPServer/README.md)

## 开发约定

这个仓库现在最重要的约束，不是“只改一侧能跑”，而是“对外接口要在各层保持一致”。

凡是涉及以下内容的改动：

- MCP 工具名
- 参数名
- 参数默认值
- 返回字段
- 错误结构

都必须按实际调用链检查并更新相关层：

1. Unreal C++ 命令处理
2. `Content/Python/commands` 中对应本地 Python 命令实现
3. `MCPServer/tools` 中对应工具注册与封装

不能只改其中一层，然后假设其他层会自动兼容。

建议的开发入口如下：

- UE 命令路由与桥接：`Source/UnrealMCP/Private/UnrealMCPBridge.cpp`
- 各类 Unreal 命令实现：`Source/UnrealMCP/Private/Commands/`
- 插件内本地 Python 命令：`Content/Python/commands/`
- Python MCP Server 入口：`MCPServer/unreal_mcp_server.py`
- Python 工具注册：`MCPServer/tools/`
- 对外工具文档：`Docs/Tools/`

## 调试

Python 服务日志默认写入：

```text
MCPServer/unreal_mcp.log
```

如果 MCP 客户端没有拿到预期结果，优先检查：

1. Unreal Editor 是否已经打开且插件已加载
2. `MCPServer` 是否已成功启动
3. Python 层日志是否存在连接失败、参数错误或返回解析错误
4. UE 侧命令是否已经在 C++ 中实现并接入路由
5. 插件内 `Content/Python/commands` 是否存在对应本地实现
6. Python MCP 工具是否已注册到 MCP Server

## 版本说明

当前仓库已明显偏向 UE5 工作流，实际开发时请以本地代码和当前分支状态为准，不再把 README 视为旧项目结构的兼容说明。

## License

MIT
