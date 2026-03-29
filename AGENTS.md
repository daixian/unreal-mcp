# AGENTS.md

这是UE的mcp插件项目，仓库根目录本身就是插件目录，对应最终放置位置是 `<UE项目>/Plugins/UnrealMCP`。
`Source/UnrealMCP` 是 UE C++ 插件模块，`Python` 文件夹是独立的 python MCP 服务器，不参与 UE 模块编译。
`Docs` 是插件和工具说明文档，`Content` 是插件资源目录。
涉及 MCP 命令、参数、返回结构时，必须同时检查 C++ 命令处理和 `Python/tools` 中对应工具注册，不能只改一侧。

## AGENTS
- AGENTS 的所有非代码文本回复必须使用中文
- 每次执行任务前必须要重新读取本地文件，更新记忆，以本地文件实际代码状态为准
- 使用apply_patch修改文件..
- 回复中提供的项目中代码行链接要使用相对项目的路径：`[文件路径:行数](相对路径#L行数)` 例如：[xxx.shader:20](Assets/Shaders/xxx.shader#L20)
- 回复中提供的项目中文件也要使用相对项目的路径. 
  
## UE的源码参考路径
- UE5: "C:\Program Files\Epic Games\UE_5.7\Engine\Source"
- UE4: "C:\Program Files\Epic Games\UE_4.27\Engine\Source"

## 代码风格
- 日志全部改成“中文主体”，只保留函数名/变量名等为英文.
- 禁止在项目中使用anonymous-namespace
- 禁止写类似 `EnsureOpened()` 这样的逻辑.初始化必须在流程上可控的时候操作,而不是随便Ensure.
- C++代码注释风格见 `Docs\Standards\code-comment-style.md`
- 禁止添加旧类名或者旧函数名的兼容包装
