<div align="center">

# Unreal Engine 的 Model Context Protocol
<span style="color: #555555">unreal-mcp</span>

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.5%2B-orange)](https://www.unrealengine.com)
[![Python](https://img.shields.io/badge/Python-3.10%2B-yellow)](https://www.python.org)
[![Status](https://img.shields.io/badge/Status-Experimental-red)](https://github.com/chongdashu/unreal-mcp)

</div>

`unreal-mcp` 是一个 Unreal Engine 编辑器插件仓库。它通过 Model Context Protocol（MCP），让 Cursor、Windsurf、Claude Desktop 等 AI 助手客户端可以使用自然语言控制 Unreal Engine。

仓库根目录本身就是插件目录。使用时，直接把本仓库克隆到目标 UE 项目的 `Plugins/UnrealMCP` 下即可，不再额外附带示例 `.uproject` 工程。

```bash
cd <你的UE项目目录>
git clone git@github.com:daixian/unreal-mcp.git Plugins/UnrealMCP

# 或添加为 Git 子模块
git submodule add git@github.com:daixian/unreal-mcp.git Plugins/UnrealMCP
git submodule update --init --recursive
```

## ⚠️ 实验状态

本项目当前处于**实验阶段（EXPERIMENTAL）**。API、功能和实现细节都可能发生较大变化。欢迎测试与反馈，但请注意：

- 可能在无通知的情况下出现破坏性变更
- 功能可能不完整或不稳定
- 文档可能过时或缺失
- 当前不建议用于生产环境

## 🌟 概览

Unreal MCP 当前提供以下几类能力：

| 类别 | 能力 |
|----------|-------------|
| **Actor 管理** | • 创建和删除 Actor（立方体、球体、灯光、相机等）<br>• 设置 Actor 变换（位置、旋转、缩放）<br>• 查询 Actor 属性并按名称查找 Actor<br>• 列出当前关卡中的全部 Actor |
| **Blueprint 开发** | • 创建带自定义组件的新 Blueprint 类<br>• 添加并配置组件（网格体、相机、灯光等）<br>• 设置组件属性和物理参数<br>• 编译 Blueprint 并生成 Blueprint Actor<br>• 创建玩家控制输入映射 |
| **Blueprint 节点图** | • 添加事件节点（BeginPlay、Tick 等）<br>• 创建函数调用节点并连接<br>• 添加自定义类型变量和默认值<br>• 创建组件引用和 Self 引用<br>• 查找并管理图中的节点 |
| **编辑器控制** | • 将视口聚焦到指定 Actor 或位置<br>• 控制视口相机朝向与距离 |

以上能力都可以通过 AI 助手的自然语言指令调用，便于自动化 Unreal Engine 工作流。

## 🧩 组件

### Unreal 插件 `Source/UnrealMCP`
- 提供用于 MCP 通信的原生 TCP 服务器
- 集成 Unreal Editor 子系统
- 实现 Actor 操作工具
- 处理命令执行与响应返回

### Python MCP 服务器 `Python/unreal_mcp_server.py`
- 管理到 C++ 插件的 TCP Socket 连接（端口 55557）
- 处理命令序列化与响应解析
- 提供错误处理与连接管理
- 从 `tools` 目录加载并注册工具模块
- 使用 FastMCP 库实现 Model Context Protocol

## 📂 目录结构

```text
UnrealMCP/
├─Content/                  # 插件内容资源
├─Docs/                     # 使用说明与工具文档
├─Python/                   # MCP Python 服务端与测试脚本
│  ├─tools/                 # Python MCP 工具模块
│  └─scripts/               # 直连测试脚本
├─Source/UnrealMCP/         # C++ 插件源码
├─README.md
└─UnrealMCP.uplugin
```

## 🚀 快速开始

### 前置条件
- Unreal Engine 5.5+
- Python 3.10+
- MCP 客户端（如 Claude Desktop、Cursor、Windsurf）

### 安装到现有 UE 项目

1. 在你的 UE 项目根目录下创建 `Plugins` 文件夹（如果还没有）。
2. 将本仓库克隆到 `Plugins/UnrealMCP`，或添加为 Git 子模块：

   ```bash
   git clone <repo-url> Plugins/UnrealMCP

   # 或
   git submodule add <repo-url> Plugins/UnrealMCP
   git submodule update --init --recursive
   ```

3. 右键你的 `.uproject` 文件，执行“生成 Visual Studio 项目文件”。
4. 打开解决方案，构建 `<你的项目名>Editor` 的 `Development Editor` 配置。
5. 启动编辑器，在 `Edit > Plugins` 中确认 `UnrealMCP` 已启用；如果提示重启，则按提示重启编辑器。

> 说明：`UnrealMCP` 当前是 Editor 类型插件，主要用于编辑器内的 MCP 交互流程，不作为游戏运行时模块参与打包。

### Python 服务器设置

`Python/` 目录随插件仓库一起分发，但不参与 UE 模块编译。需要单独准备 Python 环境：

```bash
cd Plugins/UnrealMCP/Python
uv venv
uv pip install -e .
```

详细说明见 [Python/README.md](Python/README.md)。

### 配置 MCP 客户端

将 MCP 客户端配置为运行插件目录下的 Python 服务端。下面是通用示例：

```json
{
  "mcpServers": {
    "unrealMCP": {
      "command": "uv",
      "args": [
        "--directory",
        "X:/Path/To/YourProject/Plugins/UnrealMCP/Python",
        "run",
        "unreal_mcp_server.py"
      ]
    }
  }
}
```

启动前请确保 Unreal Editor 已打开，并且加载的是包含该插件的目标项目。

### MCP 配置文件位置

不同 MCP 客户端的配置文件位置如下：

| MCP 客户端 | 配置文件位置 | 说明 |
|------------|------------------------------|-------|
| Claude Desktop | `~/.config/claude-desktop/mcp.json` | Windows: `%USERPROFILE%\.config\claude-desktop\mcp.json` |
| Cursor | `.cursor/mcp.json` | 位于项目根目录 |
| Windsurf | `~/.config/windsurf/mcp.json` | Windows: `%USERPROFILE%\.config\windsurf\mcp.json` |

各客户端使用相同的 JSON 格式。
将配置放到对应位置即可。

## 📘 文档

- [Docs/README.md](Docs/README.md)
- [Python/README.md](Python/README.md)

## 🔧 开发入口

- C++ 插件实现位于 `Source/UnrealMCP`
- Python MCP 工具位于 `Python/tools`
- 工具说明位于 `Docs/Tools`

## License
MIT

## 问题反馈

如有问题，可以在 X/Twitter 联系我：[@chongdashu](https://www.x.com/chongdashu)
