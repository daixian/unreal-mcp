# 本地Python实现功能列表

## 说明

- 本文基于 `Plugins/UnrealMCP/TODO/常用的UE功能列表.md` 当前内容整理。
- 这里的“本地 Python”指的是放在 `Plugins/UnrealMCP/Content/Python/` 下，由 Unreal 编辑器内 `PythonScriptPlugin` 执行的脚本，不是 `Plugins/UnrealMCP/Python/` 里的外部 MCP 服务端代码。
- 当前结论已经调整：不再把“能用 Python 实现”当作默认迁移条件。
- 实现层选择原则应改为：优先选择最稳定、最短链路、最容易验证、最符合 UE 现有能力边界的实现层。
- `PythonScriptPlugin` 仍然是重要候选层，尤其适合编辑器脚本、资产批处理、反射式修改和工具性流程；但它不是默认主实现层。
- C++ 不再只被限定为“薄分发层”。凡是涉及生命周期、GC、编辑器状态机、PIE/VR 运行控制、复杂对象引用关系的能力，优先保留或回退到 C++ 主实现。
- `execute_unreal_python` 本身属于基础执行能力，不是要被迁移掉的目标，而是后续本地 Python 化的底层入口之一。

## 实现层决策规则

### 1. 优先使用本地 Python 的场景

- `PythonScriptPlugin` 已直接暴露稳定 API 或编辑器子系统。
- 逻辑主要是资产批处理、查询、整理、元数据读写、图结构批量编辑。
- 实现更偏脚本编排和反射式编辑，放在 Python 中明显更短、更易调试。
- 失败模式清晰，可通过日志、返回值、对象路径稳定验证。

### 2. 优先使用 C++ 的场景

- 功能涉及对象生命周期、GC、包卸载、编辑器缓存、事务系统。
- 功能涉及 PIE、VR Preview、Live Coding、进程启动、引擎运行状态切换。
- Python 侧虽然“勉强能调”，但需要绕很多包装层，行为不稳定或验证成本高。
- 需要更严格的类型约束、性能、事务控制、错误边界和长期可维护性。

### 3. 允许采用“Python 为主 + C++ 桥接”的场景

- 主要业务逻辑适合脚本化。
- 但需要少量 C++ 来补齐 Python 缺失 API、上下文获取、结果封装或引擎级桥接。
- 桥接层应保持小而明确，不把复杂业务同时写两份。

## 已完成迁移记录

### 2026-04-03 第一批：资产与 Content Browser

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/assets/asset_commands.py`：
  - `make_directory`
  - `duplicate_asset`
  - `save_asset`
  - `rename_asset`
  - `move_asset`
  - `delete_asset`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - Unreal 编辑器内具体资产操作逻辑改为本地 Python。

### 2026-04-03 第二批：资产查询与元数据编辑

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/assets/asset_commands.py`：
  - `search_assets`
  - `get_asset_metadata`
  - `set_asset_metadata`
  - `get_selected_assets`
  - `save_all_dirty_assets`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - Unreal 编辑器内查询、元数据写入与脏包保存逻辑改为本地 Python。
- 本轮补充增强：
  - `search_assets` 已支持 `query_mode="contains|wildcard|regex"`
  - 返回结果新增 `query_mode` 字段，便于调用侧确认实际匹配模式
- 本轮补充判断：
  - `get_asset_dependencies`
  - `get_asset_referencers`
- 暂不迁移原因：
  - 当前 UE5.7 的 Python `AssetRegistry.get_dependencies/get_referencers` 只能稳定返回 `Name` 列表，拿不到 C++ 版本当前已经暴露的 `category`、`relation_type`、`properties` 等详细依赖结构。
  - 在不降低现有返回契约的前提下，这两项当前更适合继续保留为 `C++ 主实现`。

### 2026-04-03 第三批：资产合并与 Content Browser 同步

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/assets/asset_commands.py`：
  - `consolidate_assets`
  - `replace_asset_references`
  - `sync_content_browser_to_assets`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责目标/来源资产解析、同类校验、去重和 Content Browser 同步。
- 本轮补充判断：
  - `consolidate_assets` / `replace_asset_references`
    - UE5.7 的 `EditorAssetSubsystem.consolidate_assets(...)` 已直接暴露给 Python，可保持当前删除来源资产与引用切换的返回契约。
  - `sync_content_browser_to_assets`
    - 可稳定使用 `EditorAssetLibrary.sync_browser_to_objects(...)`，无需额外 C++ 桥接。

### 2026-04-03 第四批：通用资产创建

- 已新增为 `Plugins/UnrealMCP/Content/Python/commands/assets/asset_commands.py` 本地 Python 主实现：
  - `create_asset`
- 当前实现形态：
  - C++ 侧新增命令入口，但只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责解析 `asset_class`、选择默认 `factory_class`、创建工厂、处理 `unique_name` 与 `parent_class`。
- 本轮补充判断：
  - `get_asset_summary`
  - `get_blueprint_summary`
- 暂不迁移原因：
  - UE5.7 Python 里 `Blueprint` / `WidgetBlueprint` 的 `GeneratedClass`、`ParentClass`、`NewVariables`、`FunctionGraphs`、`UbergraphPages`、`WidgetTree`、`Bindings`、`Animations` 等关键信息当前属于受保护属性，直接读取会失败。
  - 现有 Python API 只能拿到零散替代能力，例如 `BlueprintEditorLibrary.generated_class(...)`、`SubobjectDataSubsystem` 等，无法稳定复刻当前 C++ 已返回的完整结构化摘要契约。
  - 按“优先选择最稳定、最短链路、最容易验证”的规则，这两项当前应继续保留为 `C++ 主实现`，避免为了迁移而降级返回结构。

### 2026-04-03 第五批：批量资产改名与移动

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/assets/asset_commands.py`：
  - `batch_rename_assets`
  - `batch_move_assets`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责批量参数解析、源资产解析、目标路径拼装、逐项执行与结果汇总。
- 本轮补充判断：
  - 这两项本质上仍是标准编辑器资产路径操作，复用 `EditorAssetLibrary.rename_asset(...)` 即可。
  - 逻辑主要是批处理编排和结果聚合，失败模式清晰，适合继续落在本地 Python。

### 2026-04-03 第六批：资产导入与导出

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/assets/asset_commands.py`：
  - `import_asset`
  - `export_asset`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责源文件校验、`AssetImportTask` 构造、导出目录枚举与结果汇总。
- 本轮补充判断：
  - `import_asset`
    - UE5.7 Python 已稳定暴露 `AssetImportTask` 与 `AssetTools.import_asset_tasks(...)`，足以保留当前 `source_files / imported_object_paths / expected_object_paths` 这组返回结构。
    - 同步导入在 UE5.7 里仍有递归断言风险，因此继续统一走异步导入，与当前工具契约保持一致。
  - `export_asset`
    - UE5.7 Python 已暴露 `AssetTools.export_assets(...)`，导出目录差异也可用本地文件系统枚举稳定回读。
    - Python 当前没有 `ExportAssetsWithCleanFilename` 对应接口，因此 `clean_filenames` 先保留为兼容参数，实际文件名遵循当前导出器默认行为。

### 2026-04-03 第七批：资产重导入

- 已调整为 `Python + C++桥接`：
  - `reimport_asset`
- 当前实现形态：
  - 默认请求先走 `Plugins/UnrealMCP/Content/Python/commands/assets/asset_commands.py`，由本地 Python 负责资源解析、`preferred_reimport_file` 覆写、`ImportAssetParameters` 组织与 `scripted_reimport_asset_async(...)` 调用。
  - 当请求显式依赖 `ask_for_new_file_if_missing=true`、`show_notification=false`、`force_new_file=true`，或资源未暴露 Interchange 重导入能力时，C++ 侧自动回退到原有 `FReimportManager::ReimportAsync(...)` 路径。
- 本轮补充判断：
  - UE5.7 Python 已稳定暴露 `InterchangeManager.can_reimport(...)`、`get_asset_import_data(...)`、`scripted_reimport_asset_async(...)` 与 `AssetImportData.scripted_add_filename(...)`，足以覆盖常见异步重导入与 `preferred_reimport_file` 覆写工作流。
  - 但 `ask_for_new_file_if_missing`、`show_notification`、`force_new_file` 仍没有等价的 Python 参数，因此保持“小型 C++ 回退壳”更稳妥，也更符合“优先最稳定实现层”的规则。
- 暂不迁移原因：
  - `fixup_redirectors`
    - UE5.7 Python 当前没有暴露与 C++ `FixupReferencers(...)` 等价的稳定接口，也缺少可直接解析 `UObjectRedirector` 的同级能力。
    - 该命令仍涉及重定向器收集、引用修复与资产删除结果判断，当前更适合继续保留为 `C++ 主实现`。

### 2026-04-03 第八批：材质资产与实例参数

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/assets/asset_commands.py`：
  - `create_material`
  - `create_material_instance`
  - `get_material_parameters`
  - `set_material_instance_scalar_parameter`
  - `set_material_instance_vector_parameter`
  - `set_material_instance_texture_parameter`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责材质/材质实例创建、父材质绑定、参数枚举、参数回读与保存。
- 本轮补充判断：
  - UE5.7 Python 已稳定暴露 `MaterialFactoryNew`、`MaterialInstanceConstantFactoryNew`、`MaterialEditingLibrary.set_material_instance_parent(...)`、`get_*_parameter_names(...)`、`get_material_default_*_parameter_value(...)`、`get_material_instance_*_parameter_value(...)` 与对应 `set_*` 接口。
  - 因此材质资产创建和实例参数读写已经适合完全下沉到本地 Python，不再需要保留原来的 C++ 主实现。
- 当前已知差异：
  - UE5.7 Python 目前没有稳定暴露与 C++ `FMaterialParameterMetadata` 完全等价的 `group / description / sort_priority / dynamic_switch` 元数据读取路径。
  - 为了保持现有返回结构，本地 Python 实现仍保留这些字段，但当前统一返回空字符串、`0` 或 `false` 的保守值；`override` 则改为通过“当前实例值与父材质回读值比较”恢复常用语义。

### 2026-04-03 第九批：材质函数与渲染目标创建

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/assets/asset_commands.py`：
  - `create_material_function`
  - `create_render_target`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责材质函数工厂配置、渲染目标创建后的尺寸/格式/清屏色写入与保存。
- 本轮补充判断：
  - `create_material_function`
    - UE5.7 Python 已暴露 `MaterialFunctionFactoryNew.supported_class`，可以稳定切换 `MaterialFunction`、`MaterialFunctionMaterialLayer`、`MaterialFunctionMaterialLayerBlend` 三种资产类型。
    - 该命令本质上仍是标准资产创建和少量工厂属性设置，适合落在本地 Python。
  - `create_render_target`
    - UE5.7 Python 已暴露 `TextureRenderTarget2D` 的 `size_x`、`size_y`、`render_target_format`、`clear_color`、`auto_generate_mips` 等编辑器属性。
    - 创建后直接在资产对象上设置这些属性即可稳定回读，属于典型的编辑器资产脚本场景，适合继续走本地 Python。

### 2026-04-03 第十批：材质槽赋值

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/assets/asset_commands.py`：
  - `assign_material_to_actor`
  - `assign_material_to_component`
  - `replace_material_slot`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责 `world_type` 解析、Actor/组件查找、材质槽定位、材质赋值与返回结构拼装。
- 本轮补充判断：
  - 这 3 项本质上仍是编辑器对象查询 + `MeshComponent.set_material(...)` 的轻量写操作。
  - UE5.7 Python 已稳定暴露 `GameplayStatics.get_all_actors_of_class(...)`、`Actor.get_components_by_class(...)`、`MeshComponent.get_material_slot_names()`、`get_material()`、`set_material()`。
  - 行为边界清晰，失败模式可以稳定回收到 “Actor 不存在 / 组件不存在 / 槽位不存在 / 未指定 component_name” 这几类错误，适合下沉到本地 Python。

### 2026-04-03 第十一批：编辑器关卡与资产编辑器控制

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `load_level`
  - `save_current_level`
  - `open_asset_editor`
  - `close_asset_editor`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责 `LevelEditorSubsystem`、`AssetEditorSubsystem` 的调用、资源解析与返回结果组装。
- 本轮补充判断：
  - `load_level` / `save_current_level`
    - UE5.7 Python 已稳定暴露 `LevelEditorSubsystem.load_level(...)` 与 `save_current_level(...)`。
    - 这两项属于明确的编辑器脚本调用，不涉及复杂生命周期桥接，适合落在本地 Python。
  - `open_asset_editor` / `close_asset_editor`
    - UE5.7 Python 已稳定暴露 `AssetEditorSubsystem.open_editor_for_assets(...)` 与 `close_all_editors_for_asset(...)`。
    - 逻辑只包含资源解析、编辑器标签开关与结果回读，失败模式清晰，适合迁移到本地 Python。

### 2026-04-03 第十二批：编辑器选中查询

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `get_selected_actors`
  - `get_editor_selection`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责读取编辑器当前选中的 Actor / 资产，并按现有 MCP 契约组装基础 Actor、组件、资产与世界信息。
- 本轮补充判断：
- `get_selected_actors` / `get_editor_selection`
    - UE5.7 Python 已稳定暴露 `EditorActorSubsystem.get_selected_level_actors()` 与 `EditorUtilityLibrary.get_selected_asset_data()`。
    - 这两项本质上仍是编辑器查询与结果序列化，不涉及 PIE/VR 状态切换、事务系统或复杂生命周期管理，适合下沉到本地 Python。
  - `select_actor`
    - 暂继续保留为 `C++ 主实现`。
    - 原因是 UE5.7 Python 的 `set_actor_selection_state(...)` 没有覆盖当前工具契约里的 `notify`、`select_even_if_hidden` 等行为控制；强行纯 Python 会让现有参数语义退化。

### 2026-04-03 第十三批：灯光 Actor 创建与属性调整

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `create_light`
  - `set_light_properties`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责 `world_type` 解析、灯光 Actor 创建、LightComponent 查找、属性写入与结果序列化。
- 本轮补充判断：
  - `create_light`
    - UE5.7 Python 已稳定暴露 `EditorActorSubsystem.spawn_actor_from_class(...)`，足以覆盖 Point / Spot / Directional 三类灯光 Actor 的创建。
    - 创建后再通过 LightComponent 写入强度、颜色、衰减半径、锥角等属性，链路更短，也更容易验证。
  - `set_light_properties`
    - UE5.7 Python 已稳定暴露 `LightComponent.set_intensity(...)`、`set_light_color(...)` 与一组编辑器属性写接口。
    - 这项本质上是编辑器对象查找 + 轻量属性写入，不涉及复杂生命周期或运行态状态机，适合继续保留为 `本地Python 主实现`。

### 2026-04-03 第十四批：场景捕捉与 PostProcessVolume 调整

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `capture_scene_to_render_target`
  - `set_post_process_settings`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责 `SceneCapture2D` 查找/临时创建、RenderTarget 绑定、手动触发 `capture_scene()`，以及 `PostProcessVolume` 查找、Volume 参数更新与 `PostProcessSettings` 覆写。
- 本轮补充判断：
  - `capture_scene_to_render_target`
    - UE5.7 Python 已稳定暴露 `SceneCapture2D`、`SceneCaptureComponent2D.texture_target`、`capture_scene()` 与 `UnrealEditorSubsystem.get_level_viewport_camera_info()`。
    - 命令本质上是编辑器对象查找 + 短链路捕捉动作，不涉及复杂运行态控制；临时 `SceneCapture2D` 也可以在捕捉后立即回收，适合继续保留为 `本地Python 主实现`。
  - `set_post_process_settings`
    - UE5.7 Python 已稳定暴露 `PostProcessVolume.settings` 与 `PostProcessSettings` 的大批编辑器属性。
    - 该命令主要是反射式属性写入和 `override_<property>` 同步，失败边界清晰，属于典型编辑器脚本场景，适合落在本地 Python。

### 2026-04-03 第十五批：关卡 Actor 查询

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `get_actors_in_level`
  - `find_actors_by_name`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧新增 `ActorQueryHelper`，负责 `world_type` 解析、Actor 枚举、名称模式过滤、世界信息补充与结果序列化。
- 本轮补充判断：
  - `get_actors_in_level`
    - UE5.7 Python 已稳定暴露 `GameplayStatics.get_all_actors_of_class(...)`，可以直接枚举指定 World 下的全部 Actor。
    - 结果仍复用当前本地序列化器输出 Actor/组件结构，链路短，验证成本低，适合下沉到本地 Python。
  - `find_actors_by_name`
    - 当前工具契约只要求按 `Actor.get_name()` 做包含匹配，不依赖复杂索引或事务控制。
    - 该能力本质上是在 `get_actors_in_level` 的基础上做轻量过滤，适合与世界解析和序列化一起收敛到同一个本地 Python 帮助类中。

### 2026-04-03 第十六批：单 Actor 与场景组件查询

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `get_actor_properties`
  - `get_actor_components`
  - `get_scene_components`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧复用 `ActorQueryHelper` 统一处理 `world_type` 解析、`name/actor_path` 查找、Actor/组件序列化与场景范围组件统计。
- 本轮补充判断：
  - `get_actor_properties`
    - 这项本质上是对单个 Actor 复用既有序列化器输出，没有额外事务或生命周期控制。
    - UE5.7 Python 已稳定支持 `GameplayStatics.get_all_actors_of_class(...)` 与 Actor/Component 反射读取，适合下沉到本地 Python。
  - `get_actor_components`
    - 当前返回结构只是把单 Actor 序列化结果里的 `components` 单独提取并补上计数，属于查询拼装逻辑。
    - 和 `get_actor_properties` 共享同一套 Actor 解析逻辑后，维护成本更低。
  - `get_scene_components`
    - 该命令只是遍历目标 World 下的 Actor，按可选 `pattern` 过滤后累计组件数量。
    - 查询链路短、失败边界清晰，适合继续保留为 `本地Python 主实现`。

### 2026-04-03 第十七批：Actor 基础编辑

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `spawn_actor`
  - `delete_actor`
  - `set_actor_transform`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧新增 `ActorEditHelper`，负责编辑器 World 解析、Actor 类型映射、对象重命名、静态网格绑定、删除与变换写入。
- 本轮补充判断：
  - `spawn_actor`
    - UE5.7 Python 已稳定暴露 `EditorActorSubsystem.spawn_actor_from_class(...)`，并且生成后可直接调用 `rename(...)` 与 `set_actor_label(...)`，能够保留当前工具契约里的命名语义。
    - 该命令本质上仍是编辑器内对象生成和少量初始化逻辑，适合下沉到本地 Python。
  - `delete_actor`
    - 删除前只需序列化 Actor 摘要，随后通过 `EditorActorSubsystem.destroy_actor(...)` 回收即可。
    - 失败边界清晰，没有额外事务状态机依赖，适合继续保留为 `本地Python 主实现`。
  - `set_actor_transform`
    - 该命令只是对现有 Actor 执行位置、旋转、缩放的轻量写入，可直接复用本地 Python 的 Actor 查找与序列化逻辑。
    - 相比继续保留 C++，Python 实现更短，也更利于后续把 `spawn_actor`、`duplicate_actor`、`set_actors_transform` 收敛到同一编辑器帮助类。

### 2026-04-03 第十八批：Actor 复制与批量变换

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `duplicate_actor`
  - `set_actors_transform`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧继续复用 `ActorEditHelper`，统一处理 Actor 查找、复制、批量变换写入与返回序列化。
- 本轮补充判断：
  - `duplicate_actor`
    - UE5.7 Python 已稳定暴露 `EditorActorSubsystem.duplicate_actor(...)`，可以直接复用现有 `name/actor_path/world_type/offset` 契约。
    - 该命令仍是单个 Actor 的轻量编辑器操作，失败边界清晰，适合并入本地 Python。
  - `set_actors_transform`
    - 本质上只是对多个已解析 Actor 复用 `set_actor_transform` 的写入逻辑，并聚合结果列表。
    - 逻辑与当前本地 Python 的 Actor 解析/序列化体系完全一致，继续保留在同一个帮助类中维护成本更低。

### 2026-04-03 第十九批：编辑器视口聚焦

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `focus_viewport`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧新增 `ViewportCommandHelper`，负责解析目标 Actor / 坐标、读取当前视口朝向、计算相机落点并写回活动关卡视口。
- 本轮补充判断：
  - `focus_viewport`
    - UE5.7 Python 已稳定暴露 `UnrealEditorSubsystem.get_level_viewport_camera_info(...)`、`set_level_viewport_camera_info(...)`，以及 `LevelEditorSubsystem.editor_invalidate_viewports()`。
    - 该命令本质上只是编辑器视口相机的读写与轻量目标解析，不涉及 PIE/VR 状态切换、截图异步回调或复杂生命周期控制，适合下沉到本地 Python。
  - `take_screenshot`
    - 暂继续保留为 `C++ 主实现`。
    - 原因是当前稳定链路依赖活动视口 `ReadPixels(...)` 同步读回与 PNG 写盘；UE5.7 Python 虽然暴露了 `AutomationLibrary` 截图接口，但更偏自动化测试/异步回调，暂不适合直接替换现有同步文件输出契约。

### 2026-04-03 第二十批：Actor 属性写入与 Blueprint Actor 生成

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `set_actor_property`
  - `spawn_blueprint_actor`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧继续复用 `ActorEditHelper`，统一处理编辑器世界内 Actor 查找、属性类型转换、Blueprint 资产解析、实例生成与结果序列化。
- 本轮补充判断：
  - `set_actor_property`
    - 该命令本质上是对单个编辑器 Actor 做反射式属性写入，链路短，失败边界清晰。
    - UE5.7 Python 已稳定暴露 `get_editor_property(...)` / `set_editor_property(...)`，并且可以基于当前属性值推断常见布尔、数字、字符串、Name 与枚举的转换策略，适合下沉到本地 Python。
  - `spawn_blueprint_actor`
  - 该命令本质上仍是“解析 Blueprint 资产 -> 取 GeneratedClass -> 生成关卡实例 -> 命名/缩放初始化”的编辑器脚本流程。
  - UE5.7 Python 已稳定暴露 `EditorAssetLibrary.load_asset(...)`、Blueprint `generated_class` 与 `EditorActorSubsystem.spawn_actor_from_class(...)`，足以保留现有契约并补一个更稳的完整资产路径入口，因此适合继续保留为 `本地Python 主实现`。

### 2026-04-03 第二十一批：Actor 挂接与分离

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `attach_actor`
  - `detach_actor`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧继续复用 `ActorQueryHelper`，统一处理子/父 Actor 解析、挂接规则切换、分离规则切换与结果序列化。
- 本轮补充判断：
  - `attach_actor`
    - UE5.7 Python 已稳定暴露 `Actor.attach_to_actor(...)`，并直接接受 `AttachmentRule` 与 `socket_name`，足以覆盖当前 `keep_world_transform` 契约。
    - 该命令本质上是编辑器对象查找 + RootComponent 挂接，不涉及复杂生命周期或事务桥接，适合下沉到本地 Python。
  - `detach_actor`
    - UE5.7 Python 已稳定暴露 `Actor.detach_from_actor(...)` 与 `DetachmentRule`，可以直接复刻当前保留世界变换/相对变换两种分离语义。
    - 失败边界清晰，和现有 Actor 查找/结果拼装逻辑完全一致，适合继续保留为 `本地Python 主实现`。
- 本轮补充增强：
  - `Plugins/UnrealMCP/MCPServer/tools/editor_tools.py` 已同步补齐 `attach_actor` 的 `actor_path` / `parent_actor_path`，以及 `detach_actor` 的 `actor_path` 参数，避免工具层遗漏插件实际支持的字段。

### 2026-04-03 第二十二批：Actor 标签与文件夹路径

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `set_actor_tags`
  - `set_actor_folder_path`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧继续复用 `ActorEditHelper`，统一处理编辑器世界内 Actor 解析、标签数组覆写、文件夹路径写入与结果序列化。
- 本轮补充判断：
  - `set_actor_tags`
    - 该命令本质上是对编辑器 Actor 的 `Tags` 数组做一次覆盖写入，失败边界清晰，验证也可以直接从 Actor 摘要回读。
    - UE5.7 Python 已稳定暴露 `Actor.tags`，并支持 `unreal.Name(...)` 形式写入，适合下沉到本地 Python。
  - `set_actor_folder_path`
    - 该命令本质上是对单个 Actor 调用 `SetFolderPath(...)` 的轻量编辑器写操作，不涉及复杂事务或运行态状态机。
    - UE5.7 Python 已稳定暴露 `Actor.set_folder_path(...)` 与 `Actor.get_folder_path()`，可以保留当前工具契约并稳定回读结果，适合继续保留为 `本地Python 主实现`。

### 2026-04-03 第二十三批：Editor Utility 运行

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `run_editor_utility_widget`
  - `run_editor_utility_blueprint`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧新增 `EditorUtilityCommandHelper`，负责 Editor Utility 资产解析、类型校验、标签页注册、`Run` 入口触发与结果序列化。
- 本轮补充判断：
  - `run_editor_utility_widget`
    - UE5.7 Python 已稳定暴露 `EditorUtilitySubsystem.spawn_and_register_tab_and_get_id(...)` 与 `spawn_and_register_tab_with_id(...)`。
    - 该命令本质上是编辑器资产解析 + 标签页注册，失败边界清晰，适合下沉到本地 Python。
  - `run_editor_utility_blueprint`
    - UE5.7 Python 已稳定暴露 `EditorUtilitySubsystem.can_run(...)` 与 `try_run(...)`，足以覆盖当前 `Run` 入口校验与执行流程。
    - 该命令不涉及 PIE/VR 状态切换，也不依赖复杂生命周期桥接，适合继续保留为 `本地Python 主实现`。
- 本轮补充结论：
  - `get_viewport_camera`
    - 暂继续保留为 `C++ 主实现`。
    - 原因是 UE5.7 Python 虽已稳定暴露 `UnrealEditorSubsystem.get_level_viewport_camera_info(...)`，但当前公开契约还包含 `fov`、`view_mode`、`view_mode_index`、`is_perspective`、`is_realtime`、`viewport_type`、`viewport_size` 等活动视口细节；这些字段暂未发现同等稳定、低风险的 Python 读取路径。
    - 按“优先最稳定、最短链路、最容易验证”的规则，不应为了迁移而削弱现有返回结构。

### 2026-04-03 第二十四批：Actor 可见性与 Mobility

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `set_actor_visibility`
  - `set_actor_mobility`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧继续复用 `ActorEditHelper`，统一处理编辑器 Actor 解析、可见性状态写入、根组件 Mobility 写入与结果序列化。
- 本轮补充判断：
  - `set_actor_visibility`
    - 该命令本质上是对单个编辑器 Actor 做可见性状态切换，不涉及复杂生命周期或运行态状态机。
    - UE5.7 Python 已稳定暴露 `set_actor_hidden_in_game(...)` 与 `set_is_temporarily_hidden_in_editor(...)`，适合在本地 Python 内同时收敛运行时隐藏与编辑器临时隐藏语义。
  - `set_actor_mobility`
    - 该命令本质上是对根 `SceneComponent` 的 `mobility` 枚举做一次反射式写入。
    - UE5.7 Python 已稳定暴露 `SceneComponent` 的 `mobility` 编辑器属性与 `ComponentMobility` 枚举，失败边界清晰，适合继续保留为 `本地Python 主实现`。
  - `select_actor`
    - 仍继续保留为 `C++ 主实现`。
    - 原因不变：现有契约依赖 `notify`、`select_even_if_hidden` 语义，纯 Python 仍不能稳定等价覆盖。

### 2026-04-04 第二十八批：Actor 选中桥接

- 已调整为 `Python + C++桥接`：
  - `select_actor`
- 当前实现形态：
  - 本地 Python 侧新增选择前解析命令，统一复用现有 `ActorQueryHelper` 的 `world_type` 解析与 Actor 查找逻辑。
  - C++ 侧保留最终 `GEditor->SelectActor(...)` 调用，继续负责 `replace_selection`、`notify`、`select_even_if_hidden` 语义。
- 本轮补充判断：
  - `select_actor`
    - 该命令的“找谁”部分适合下沉到本地 Python，因为现有 Actor 查询和世界解析已经在 Python 侧稳定收敛。
    - 但真正的编辑器选择动作仍依赖 `GEditor->SelectActor(...)`，而 UE5.7 Python 暂未稳定覆盖同等参数语义，因此更适合做成 `Python + C++桥接`，而不是纯 Python 或继续全部留在 C++。

### 2026-04-03 第二十五批：Actor 多条件过滤查询

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `find_actors`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧继续复用 `ActorQueryHelper`，统一处理 `world_type` 解析、Actor 枚举、多条件过滤、排序与结果序列化。
- 本轮补充判断：
  - `find_actors`
    - 该命令本质上仍是编辑器世界中的只读查询，不涉及 PIE/VR 状态切换、事务系统或对象生命周期写操作。
    - UE5.7 Python 已稳定暴露 Actor 名称、类名、标签、路径、文件夹路径等查询入口，适合下沉到本地 Python。
    - DataLayer 过滤当前按 `Actor.get_data_layer_assets(...)` 可用性做受控支持；若当前 Python API 不可用，则直接报错，不伪装为“无结果”。
  - 返回结构继续沿用 `get_actors_in_level` / `find_actors_by_name` 的归一化列表格式，并新增 `filters` 回显实际应用的过滤条件与排序方式。

### 2026-04-04 第二十六批：按类路径生成 Actor

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `spawn_actor_from_class`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧继续复用 `ActorEditHelper`，统一处理编辑器世界解析、类路径解析、Actor 命名、防重名检查、实例生成与结果序列化。
- 本轮补充判断：
  - `spawn_actor_from_class`
    - 该命令本质上仍是编辑器内“解析类引用 -> 生成 Actor -> 回填名称/缩放”的轻量生成流程。
    - UE5.7 Python 已稳定暴露 `unreal.load_class(...)`、Blueprint `generated_class` 与 `EditorActorSubsystem.spawn_actor_from_class(...)`，足以覆盖原生类路径和 Blueprint 路径两种常见输入。
    - 由于 `EditorActorSubsystem` 当前稳定工作在编辑器世界，本轮先明确只支持 `editor` 世界，不为了扩展 `pie` 语义而引入不稳定桥接。

### 2026-04-04 第二十七批：WorldSettings 查询与写入

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `get_world_settings`
  - `set_world_settings`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧新增 `WorldSettingsCommandHelper`，统一处理编辑器世界解析、`WorldSettings` 属性读取、反射式类型转换与批量写入。
- 本轮补充判断：
  - `get_world_settings`
    - 该命令本质上是对当前编辑器 World 的只读反射查询。
    - UE5.7 Python 已稳定暴露 `UnrealEditorSubsystem.get_editor_world()` 与 `World.get_world_settings()`，适合直接下沉到本地 Python。
  - `set_world_settings`
    - 该命令主要是对 `WorldSettings` 做轻量属性写入，不涉及 PIE/VR 状态切换、GC 或复杂事务编排。
    - 按“优先最稳定、最短链路、最容易验证”的规则，更适合做成本地 Python 主实现。
- 当前约束：
  - 两条命令都只支持 `editor` 世界。
  - `set_world_settings` 当前稳定覆盖布尔、整数、浮点、字符串、`Name`、枚举、`Vector`、`Rotator`、`LinearColor`，以及通过对象路径解析的类/资源引用。

## 规划分层

### A. 优先评估为本地 Python 实现

- 特点：
  - 主要在编辑器内操作资产、蓝图、WidgetTree、Sequencer、材质图等对象模型。
  - 逻辑更偏反射式编辑、批处理、图编辑和资产重构。
  - 当 PythonScriptPlugin 已有稳定 API 时，脚本化通常更灵活、更短、更容易迭代。

## 1. 资产与 Content Browser

- `search_assets`
- `get_asset_metadata`
- `get_asset_dependencies`
- `get_asset_referencers`
- `get_asset_summary`
- `get_blueprint_summary`
- `create_asset`
- `import_asset`
- `export_asset`
- `fixup_redirectors`
- `consolidate_assets`
- `replace_asset_references`
- `set_asset_metadata`
- `sync_content_browser_to_assets`
- `get_selected_assets`
- `save_all_dirty_assets`
- `checkout_asset`
- `submit_asset`
- `revert_asset`
- `batch_rename_assets`
- `batch_move_assets`
- `search_assets` 的通配符/正则增强（已完成）

## 2. 关卡 / World / Actor

- `load_level`
- `save_current_level`
- `get_actors_in_level`
- `find_actors_by_name`
- `spawn_actor`
- `delete_actor`
- `set_actor_transform`
- `set_actor_property`（已完成）
- `get_actor_properties`
- `get_actor_components`
- `get_scene_components`
- `focus_viewport`
- `take_screenshot`
- `spawn_blueprint_actor`（已完成）
- `duplicate_actor`
- `select_actor`（已完成，Python+C++桥接）
- `get_selected_actors`
- `find_actors`（已完成）
- `set_actor_tags`（已完成）
- `set_actor_folder_path`（已完成）
- `set_actor_visibility`（已完成）
- `set_actor_mobility`（已完成）
- `attach_actor`（已完成）
- `detach_actor`（已完成）
- `add_component_to_actor`
- `remove_component_from_actor`
- `spawn_actor_from_class`（已完成）
- `set_actors_transform`
- `get_world_settings`（已完成）
- `set_world_settings`（已完成）
- `line_trace`
- `box_trace`
- `sphere_trace`
- `get_hit_result_under_cursor`
- `get_data_layers`
- `set_actor_data_layers`
- `load_world_partition_cells`
- `unload_world_partition_cells`
- `build_navigation`
- `build_lighting`
- `build_hlod`
- `spawn_actor` 的 `scale/class_path/world_type/template_actor` 增强
- `find_actors_by_name` 的类过滤/标签过滤/路径过滤/排序增强

## 3. Blueprint 资产 / 类 / 组件

- `create_blueprint`
- `add_component_to_blueprint`
- `set_component_property`
- `set_static_mesh_properties`
- `set_physics_properties`
- `compile_blueprint`
- `cleanup_blueprint_for_reparent`
- `set_blueprint_property`
- `set_pawn_properties`
- `set_game_mode_default_pawn`
- `reparent_blueprint`
- `create_child_blueprint`
- `rename_blueprint_member`
- `delete_blueprint_variable`
- `set_blueprint_variable_default`
- `add_blueprint_function`
- `delete_blueprint_function`
- `add_blueprint_macro`
- `add_blueprint_interface`
- `implement_blueprint_interface`
- `add_event_dispatcher`
- `add_timeline_to_blueprint`
- `remove_component_from_blueprint`
- `attach_component_in_blueprint`
- `set_inherited_component_override`
- `get_blueprint_compile_errors`
- `compile_blueprints`
- `save_blueprint`
- `open_blueprint_editor`
- `create_blueprint` 的路径/包名/父类路径增强
- `add_component_to_blueprint` 的附着父节点/Socket/事务回滚增强

## 4. Blueprint 图节点 / Graph 编辑

- `add_blueprint_event_node`
- `add_blueprint_input_action_node`
- `add_blueprint_function_node`
- `connect_blueprint_nodes`
- `add_blueprint_variable`
- `add_blueprint_get_self_component_reference`
- `add_blueprint_self_reference`
- `find_blueprint_nodes`
- `spawn_blueprint_node`
- `describe_blueprint_node`
- `set_blueprint_pin_default`
- `delete_blueprint_node`
- `disconnect_blueprint_nodes`
- `move_blueprint_node`
- `layout_blueprint_nodes`
- `add_blueprint_comment_node`
- `add_blueprint_reroute_node`
- `create_blueprint_graph`
- `delete_blueprint_graph`
- `add_branch_node`
- `add_sequence_node`
- `add_delay_node`
- `add_gate_node`
- `add_for_loop_node`
- `add_for_each_loop_node`
- `add_cast_node`
- `add_variable_get_node`
- `add_variable_set_node`
- `add_make_struct_node`
- `add_break_struct_node`
- `add_switch_on_enum_node`
- `add_select_node`
- `collapse_nodes_to_function`
- `collapse_nodes_to_macro`
- `duplicate_blueprint_subgraph`
- `validate_blueprint_graph`
- `spawn_blueprint_node` 的语义节点/自动连线/Wildcard Pin 增强
- `find_blueprint_nodes` 的函数名/Widget Graph/Pin 条件过滤增强

## 5. 项目设置 / 输入系统

- `create_input_mapping`
- `list_input_mappings`
- `remove_input_mapping`
- `create_input_axis_mapping`
- `create_input_action_asset`
- `create_input_mapping_context`
- `add_mapping_to_context`
- `assign_mapping_context`
- `get_project_setting`
- `set_project_setting`
- `set_default_maps`
- `set_game_framework_defaults`
- `enable_plugin`
- `disable_plugin`
- `add_module_dependency`
- `set_packaging_settings`

## 6. UMG / UI

- `create_umg_widget_blueprint`
- `add_text_block_to_widget`
- `add_button_to_widget`
- `bind_widget_event`
- `add_widget_to_viewport`
- `set_text_block_binding`
- `add_image_to_widget`
- `add_border_to_widget`
- `add_canvas_panel_to_widget`
- `add_horizontal_box_to_widget`
- `add_vertical_box_to_widget`
- `add_overlay_to_widget`
- `add_scroll_box_to_widget`
- `add_size_box_to_widget`
- `add_spacer_to_widget`
- `add_progress_bar_to_widget`
- `add_slider_to_widget`
- `add_check_box_to_widget`
- `add_editable_text_to_widget`
- `add_rich_text_to_widget`
- `add_multi_line_text_to_widget`
- `add_list_view_to_widget`
- `add_tile_view_to_widget`
- `add_tree_view_to_widget`
- `remove_widget_from_blueprint`
- `set_widget_slot_layout`
- `set_widget_visibility`
- `set_widget_style`
- `set_widget_brush`
- `create_widget_animation`
- `add_widget_animation_keyframe`
- `bind_widget_property`
- `add_named_slot_to_widget`
- `open_widget_blueprint_editor`
- `add_widget_to_viewport` 的 `owning_player/句柄返回/移除接口` 增强
- `get_blueprint_summary` 的 WidgetTree/Binding/Animation 摘要增强

## 7. 编辑器运行控制 / 调试

- `open_asset_editor`
- `close_asset_editor`
- `execute_console_command`
- `run_editor_utility_widget`（已完成）
- `run_editor_utility_blueprint`（已完成）
- `get_output_log`
- `clear_output_log`
- `get_message_log`
- `show_editor_notification`
- `set_viewport_mode`
- `get_viewport_camera`
- `take_highres_screenshot`
- `capture_viewport_sequence`
- `take_screenshot` 的分辨率/带 UI/透明背景/指定视口增强

## 8. 材质 / 渲染 / 灯光

- `create_material`
- `create_material_function`
- `create_render_target`
- `create_material_instance`
- `get_material_parameters`
- `set_material_instance_scalar_parameter`
- `set_material_instance_vector_parameter`
- `set_material_instance_texture_parameter`
- `assign_material_to_actor`
- `assign_material_to_component`
- `replace_material_slot`
- `add_material_expression`
- `connect_material_expressions`
- `layout_material_graph`
- `compile_material`
- `create_material_function`
- `create_render_target`
- `capture_scene_to_render_target`（已完成）
- `set_post_process_settings`（已完成）
- `create_light`（已完成）
- `set_light_properties`（已完成）

## 9. 骨骼 / 动画 / 角色

- `set_skeletal_mesh`
- `set_anim_instance_class`
- `create_animation_blueprint`
- `create_blend_space`
- `create_animation_montage`
- `add_socket_to_skeleton`
- `set_socket_transform`
- `retarget_animations`
- `set_character_movement_properties`
- `setup_third_person_camera`
- `setup_character_input_graph`

## 10. Sequencer / 电影化

- `create_level_sequence`
- `open_level_sequence`
- `add_actor_to_sequence`
- `add_sequence_track`
- `add_sequence_section`
- `add_sequence_key`
- `set_sequence_playback_range`
- `play_sequence`
- `pause_sequence`
- `stop_sequence`
- `render_movie_queue`

## 11. Niagara / VFX

- `create_niagara_system`
- `create_niagara_emitter`
- `spawn_niagara_system`
- `set_niagara_parameter`
- `attach_niagara_to_actor`
- `open_niagara_editor`

## 12. 音频

- `import_sound`
- `create_sound_cue`
- `create_meta_sound`
- `set_audio_component_properties`
- `spawn_audio_actor`
- `set_sound_attenuation`
- `set_sound_concurrency`
- `set_sound_submix`

## 13. 数据资产 / 表格 / 配置

- `create_data_asset`
- `create_primary_data_asset`
- `create_data_table`
- `get_data_table_rows`
- `set_data_table_row`
- `remove_data_table_row`
- `import_data_table`
- `export_data_table`
- `create_curve`
- `set_curve_keys`
- `get_config_value`
- `set_config_value`

## 14. Landscape / Foliage / PCG / 开放世界

- `create_landscape`
- `import_heightmap`
- `set_landscape_material`
- `paint_landscape_layer`
- `sculpt_landscape`
- `add_foliage_instances`
- `create_pcg_graph`
- `execute_pcg_graph`
- `set_world_partition_streaming`
- `create_data_layer`
- `set_data_layer_state`

## 15. AI / 导航 / 玩法系统

- `create_behavior_tree`
- `create_blackboard`
- `set_ai_controller_class`
- `find_nav_path`
- `rebuild_navigation`
- `create_nav_mesh_bounds_volume`
- `setup_ai_perception`
- `create_gameplay_tag`
- `create_gameplay_effect`
- `create_gameplay_ability`

## 16. 自动化 / 测试 / 协作

- `run_automation_test`
- `run_commandlet`
- `collect_editor_errors`
- `collect_runtime_issues`
- `get_source_control_status`
- `checkout_files`
- `submit_files`
- `revert_files`
- `analyze_asset_change_impact`
- `list_mcp_tools`
- `export_tool_schema`

### B. 适合“Python 为主 + C++ 明确桥接”的功能

- 特点：
  - 底层执行逻辑可以放 Python。
  - 但仍建议保留 C++ 命令壳，负责参数校验、编辑器上下文解析、结果封装，避免工具契约直接退化成“随便传一段脚本”。
  - 当 Python 缺少某个关键引擎能力时，再通过极小的 C++ 桥接补齐，不强求纯 Python。

- `start_pie`
- `start_vr_preview`
- `stop_pie`
- `get_play_state`
- `start_live_coding`
- `compile_live_coding`
- `get_live_coding_state`
- `start_standalone_game`
- `reimport_asset`
- `start_pie` 的地图覆盖/玩家数量/网络模式/窗口模式增强

### C. 优先保留或实现为 C++ 的功能

- 特点：
  - 这些能力要么更偏工程命令行、系统环境或外部工具链，要么更偏引擎运行态控制。
  - 即使 PythonScriptPlugin“可以间接做到”，也不代表应该以 Python 作为主实现。
  - 更适合保留为 C++、外部流程、命令行工具，或后续单独设计专用执行层。

- `regenerate_project_files`
- `build_project`
- `cook_project`
- `package_project`
- `build_patch`
- `clean_project_artifacts`

## 首批迁移建议

- `资产与 Content Browser`
  - 这部分 API 成熟，适合继续逐项评估。
  - 但涉及删除、卸载、引用清理、重定向器和包状态的能力，需要单独验证，不应直接假设 Python 足够稳定。

- `UMG / UI`
  - WidgetTree、Slot 布局、层级构造、批量绑定等仍然适合优先评估本地 Python。
  - 但涉及编辑器持有对象、预览实例、复杂引用链时，需要保留回退到 C++ 的空间。

- `Blueprint 图节点 / Graph 编辑`
  - 图编辑仍是适合脚本化的一类能力。
  - 但凡是编译、重建、重实例化、父类变更等会触发较强编辑器状态变化的能力，不默认承诺纯 Python。

- `材质 / 数据资产 / Sequencer`
  - 这些仍是本地 Python 的重点候选域。
  - 具体是否迁移，仍以 API 覆盖和验证稳定性为准，而不是只看“理论上能不能调到”。

## 目录建议

- 本地 Python 脚本建议放在：
  - `Plugins/UnrealMCP/Content/Python/commands/`

- 建议按能力域拆分：
  - `Plugins/UnrealMCP/Content/Python/commands/assets/`
  - `Plugins/UnrealMCP/Content/Python/commands/blueprint/`
  - `Plugins/UnrealMCP/Content/Python/commands/umg/`
  - `Plugins/UnrealMCP/Content/Python/commands/material/`
  - `Plugins/UnrealMCP/Content/Python/commands/common/`
