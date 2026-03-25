<div align="center">

# Unreal Engine 的 Model Context Protocol
<span style="color: #555555">unreal-mcp</span>

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.5%2B-orange)](https://www.unrealengine.com)
[![Python](https://img.shields.io/badge/Python-3.12%2B-yellow)](https://www.python.org)
[![Status](https://img.shields.io/badge/Status-Experimental-red)](https://github.com/chongdashu/unreal-mcp)

</div>

本项目通过 Model Context Protocol（MCP），让 Cursor、Windsurf、Claude Desktop 等 AI 助手客户端可以使用自然语言控制 Unreal Engine。

## ⚠️ 实验状态

本项目当前处于**实验阶段（EXPERIMENTAL）**。API、功能和实现细节都可能发生较大变化。欢迎测试与反馈，但请注意：

- 可能在无通知的情况下出现破坏性变更
- 功能可能不完整或不稳定
- 文档可能过时或缺失
- 当前不建议用于生产环境

## 🌟 概览

Unreal MCP 集成提供了通过自然语言控制 Unreal Engine 的完整工具能力：

| 类别 | 能力 |
|----------|-------------|
| **Actor 管理** | • 创建和删除 Actor（立方体、球体、灯光、相机等）<br>• 设置 Actor 变换（位置、旋转、缩放）<br>• 查询 Actor 属性并按名称查找 Actor<br>• 列出当前关卡中的全部 Actor |
| **Blueprint 开发** | • 创建带自定义组件的新 Blueprint 类<br>• 添加并配置组件（网格体、相机、灯光等）<br>• 设置组件属性和物理参数<br>• 编译 Blueprint 并生成 Blueprint Actor<br>• 创建玩家控制输入映射 |
| **Blueprint 节点图** | • 添加事件节点（BeginPlay、Tick 等）<br>• 创建函数调用节点并连接<br>• 添加自定义类型变量和默认值<br>• 创建组件引用和 Self 引用<br>• 查找并管理图中的节点 |
| **编辑器控制** | • 将视口聚焦到指定 Actor 或位置<br>• 控制视口相机朝向与距离 |

以上能力都可以通过 AI 助手的自然语言指令调用，便于自动化 Unreal Engine 工作流。

## 🧩 组件

### 示例项目（MCPGameProject）`MCPGameProject`
- 基于 Blank 项目，并已添加 UnrealMCP 插件。

### 插件（UnrealMCP）`MCPGameProject/Plugins/UnrealMCP`
- 用于 MCP 通信的原生 TCP 服务器
- 集成 Unreal Editor 子系统
- 实现 Actor 操作工具
- 处理命令执行与响应返回

### Python MCP 服务器 `Python/unreal_mcp_server.py`
- 实现在 `unreal_mcp_server.py`
- 管理到 C++ 插件的 TCP Socket 连接（端口 55557）
- 处理命令序列化与响应解析
- 提供错误处理与连接管理
- 从 `tools` 目录加载并注册工具模块
- 使用 FastMCP 库实现 Model Context Protocol

## 📂 目录结构

- **MCPGameProject/** - Unreal 示例项目
  - **Plugins/UnrealMCP/** - C++ 插件源码
    - **Source/UnrealMCP/** - 插件源代码
    - **UnrealMCP.uplugin** - 插件定义文件

- **Python/** - Python 服务器与工具
  - **tools/** - Actor、编辑器、Blueprint 操作工具模块
  - **scripts/** - 示例脚本与演示

- **Docs/** - 完整文档
  - 参见 [Docs/README.md](Docs/README.md) 获取文档索引

## 🚀 快速开始

### 前置条件
- Unreal Engine 5.5+
- Python 3.12+
- MCP 客户端（如 Claude Desktop、Cursor、Windsurf）

### 示例项目

为快速上手，你可以直接使用 `MCPGameProject` 中的启动项目。它是一个 UE 5.5 Blank Starter Project，并已配置好 `UnrealMCP.uplugin`。

1. **准备项目**
   - 右键你的 `.uproject` 文件
   - 生成 Visual Studio 项目文件
2. **构建项目（包含插件）**
   - 打开解决方案（`.sln`）
   - 目标选择 `Development Editor`
   - 执行 Build

### 插件
如果你希望在自己的现有项目中使用该插件：

1. **复制插件到你的项目**
   - 将 `MCPGameProject/Plugins/UnrealMCP` 复制到你的项目 `Plugins` 文件夹

2. **启用插件**
   - 打开 Edit > Plugins
   - 在 Editor 分类找到 “UnrealMCP”
   - 启用插件
   - 按提示重启编辑器

3. **构建插件**
   - 右键 `.uproject` 文件
   - 生成 Visual Studio 项目文件
   - 打开解决方案（`.sln`）
   - 按你的目标平台和输出设置进行构建

### Python 服务器设置

详细 Python 配置见 [Python/README.md](Python/README.md)，包括：
- Python 环境配置
- 运行 MCP 服务器
- 使用直连或服务器模式连接

### 配置 MCP 客户端

根据你的 MCP 客户端，使用如下 `mcp` 配置 JSON：

```json
{
  "mcpServers": {
    "unrealMCP": {
      "command": "uv",
      "args": [
        "--directory",
        "<path/to/the/folder/PYTHON>",
        "run",
        "unreal_mcp_server.py"
      ]
    }
  }
}
```

可参考仓库中的 `mcp.json` 示例。

### MCP 配置文件位置

不同 MCP 客户端的配置文件位置如下：

| MCP 客户端 | 配置文件位置 | 说明 |
|------------|------------------------------|-------|
| Claude Desktop | `~/.config/claude-desktop/mcp.json` | Windows: `%USERPROFILE%\.config\claude-desktop\mcp.json` |
| Cursor | `.cursor/mcp.json` | 位于项目根目录 |
| Windsurf | `~/.config/windsurf/mcp.json` | Windows: `%USERPROFILE%\.config\windsurf\mcp.json` |

各客户端使用相同的 JSON 格式。
将配置放到对应位置即可。


## License
MIT

## 问题反馈

如有问题，可以在 X/Twitter 联系我：[@chongdashu](https://www.x.com/chongdashu)
