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

### 2026-04-04 第五十六批：资产签出

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/assets/asset_commands.py`：
  - `checkout_asset`
- 当前实现形态：
  - C++ 侧新增命令入口，但只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责资产解析、源码控制签出调用与结果组装。
- 本轮补充判断：
  - `checkout_asset`
    - UE5.7 Python 已稳定暴露 `EditorAssetLibrary.checkout_asset(...)`。
    - 该命令本质上是标准编辑器资产脚本调用，失败边界清晰，适合直接落到本地 Python。

### 2026-04-04 第五十七批：资产提交、还原与源码控制状态查询

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/assets/asset_commands.py`：
  - `submit_asset`
  - `revert_asset`
  - `get_source_control_status`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责资产解析、`SourceControl` 调用、Provider 状态回读与结果组装。
- 本轮补充判断：
  - `submit_asset`
    - UE5.7 Python 已稳定暴露 `SourceControl.check_in_file(...)`，支持直接以资产路径提交，并可附带 `description`、`silent`、`keep_checked_out` 参数。
    - 提交前先保存资产的流程也适合脚本化，链路短，失败边界清晰。
  - `revert_asset`
    - UE5.7 Python 已稳定暴露 `SourceControl.revert_file(...)`，可直接以资产路径执行还原。
    - 该命令本质上仍是标准源码控制操作，不涉及额外 GC 或编辑器状态机切换，适合落在本地 Python。
  - `get_source_control_status`
    - UE5.7 Python 已稳定暴露 `SourceControl.query_file_state(...)` 与 `SourceControlState` 结构体字段。
    - 返回当前 Provider 可用性、最近错误和状态布尔集，足以支持自动化工作流判断是否可提交/可还原。

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

### 2026-04-04 第二十九批：控制台命令执行

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `execute_console_command`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧新增 `EditorRuntimeCommandHelper`，统一复用现有 `world_type` 解析后，通过 `unreal.SystemLibrary.execute_console_command(...)` 执行控制台命令。
- 本轮补充判断：
  - `execute_console_command`
    - 该命令本质上是“解析目标世界 + 执行一条命令”的轻量脚本入口，不涉及 PIE/VR 状态切换、GC 或复杂事务。
    - UE5.7 Python 已稳定暴露 `SystemLibrary.execute_console_command(...)`，且可直接接受编辑器世界对象作为 `world_context_object`。
    - 返回契约只要求命令回显与世界信息，不依赖 C++ 独有的复杂状态，因此适合继续收敛为 `本地Python 主实现`。

### 2026-04-04 第三十批：关卡实例组件增删

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `add_component_to_actor`
  - `remove_component_from_actor`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧复用 `ActorEditHelper`，通过 `SubobjectDataSubsystem` 统一处理实例组件句柄收集、组件创建、组件删除、附着与返回结构组装。
- 本轮补充判断：
  - `add_component_to_actor`
    - UE5.7 Python 已稳定暴露 `SubobjectDataSubsystem.add_new_subobject(...)`、`AddNewSubobjectParams` 与实例级 `k2_gather_subobject_data_for_instance(...)`。
    - 该命令本质上仍是编辑器内实例组件脚本化编辑，不涉及 PIE/VR 状态切换、GC 或复杂事务图，因此适合下沉到本地 Python。
  - `remove_component_from_actor`
    - UE5.7 Python 已稳定暴露 `k2_delete_subobject_from_instance(...)`，可以按实例组件句柄直接删除。
    - 删除逻辑的失败边界也比较明确，可稳定约束为“未找到组件”“不允许删除根组件”“引擎删除失败”几类错误，适合继续保留为 `本地Python 主实现`。

### 2026-04-04 第三十一批：Blueprint 资产创建与编辑器打开/保存

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/blueprint/blueprint_commands.py`：
  - `create_blueprint`
  - `save_blueprint`
  - `open_blueprint_editor`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧新增 `BlueprintCommandDispatcher` 与 `BlueprintAssetHelper`，负责父类解析、Blueprint 资产解析、创建、保存与编辑器打开。
- 本轮补充判断：
  - `create_blueprint`
    - UE5.7 Python 已稳定暴露 `BlueprintEditorLibrary.create_blueprint_asset_with_parent(...)`，并且实测可覆盖普通 Actor Blueprint、`BlueprintFunctionLibrary` 与 `EditorUtilityObject` 三类父类。
    - 该命令本质上仍是编辑器资产创建，不涉及 PIE/VR 状态切换或复杂运行态生命周期，适合继续保留为 `本地Python 主实现`。
  - `save_blueprint` / `open_blueprint_editor`
    - UE5.7 Python 已稳定暴露 `EditorAssetLibrary.save_loaded_asset(...)` 与 `AssetEditorSubsystem.open_editor_for_assets(...)`。
    - 两条命令都属于明确的编辑器资产操作，失败边界清晰，适合下沉到本地 Python。

### 2026-04-04 第三十二批：Blueprint 编译与默认对象属性编辑

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/blueprint/blueprint_commands.py`：
  - `compile_blueprint`
  - `set_blueprint_property`
  - `set_pawn_properties`
  - `set_game_mode_default_pawn`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧新增 `BlueprintPropertyHelper`，统一处理 Blueprint 编译入口探测、CDO 属性名解析、属性值类型转换、Pawn 常用属性批量写入与 GameMode 默认 Pawn 设置。
- 本轮补充判断：
  - `compile_blueprint`
    - 该命令本质上仍是编辑器资产编译，不涉及 PIE/VR 状态切换或复杂运行态桥接。
    - UE5.7 Python 已暴露 Blueprint 编译入口，适合直接收敛为 `本地Python 主实现`。
  - `set_blueprint_property`
    - 该命令主要是对 Blueprint CDO 做反射式属性写入，失败边界明确，适合落在本地 Python。
    - 为兼容当前工具调用，Python 侧补了 `CamelCase` 与常见 `snake_case` 属性名解析。
  - `set_pawn_properties`
    - 该命令只是对 Pawn/Character CDO 的一组常用字段做批量写入，本质上仍是 CDO 属性编排。
    - UE5.7 Python 已能稳定处理布尔、枚举和值为对象路径的引用类型，因此适合继续下沉到本地 Python。
  - `set_game_mode_default_pawn`
    - 该命令本质上是把 `DefaultPawnClass` 指向另一个 Blueprint 的生成类，并在修改后重新编译 GameMode Blueprint。
    - 逻辑清晰、验证口径稳定，适合作为 `本地Python 主实现`。

### 2026-04-04 第三十三批：Blueprint 变量默认值设置

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/blueprint/blueprint_commands.py`：
  - `set_blueprint_variable_default`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧复用 Blueprint CDO 属性写入与编译链路，当前会先预编译刷新生成类，并优先通过 `unreal.get_default_object(...)` 解析真实 Blueprint CDO，再执行默认值写入与回编译结果拼装。
- 本轮补充判断：
  - `set_blueprint_variable_default`
    - UE5.7 Python 虽然没有直接暴露 `FBPVariableDescription.DefaultValue` 的编辑接口，但 Blueprint 变量在编译后会落到生成类默认对象属性上。
    - 对这类“已有成员变量默认值调整”场景，直接复用本地 Python 的“预编译刷新生成类 + CDO 反射写入 + 回编译”链路更短、更容易验证，适合作为 `本地Python 主实现`。
  - `delete_blueprint_variable`
    - 暂继续保留为 `C++ 主实现`。
    - 原因是 UE5.7 官方 Python `BlueprintEditorLibrary` 没有公开“按变量名删除成员变量”的等价接口；只有 `remove_unused_variables(...)` 这类受限能力，不足以覆盖当前 MCP 契约。

### 2026-04-04 第三十四批：Blueprint 组件编辑命令迁移到本地Python

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/blueprint/blueprint_commands.py`：
  - `add_component_to_blueprint`
  - `set_component_property`
  - `set_static_mesh_properties`
  - `set_physics_properties`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧新增 `BlueprintComponentHelper`，统一通过 `SubobjectDataSubsystem` 收集 Blueprint SCS 句柄、创建组件、定位模板对象、写入属性并在需要时回编译。
- 本轮补充判断：
  - `add_component_to_blueprint`
    - UE5.7 Python 已稳定暴露 `SubobjectDataSubsystem.k2_gather_subobject_data_for_blueprint(...)`、`AddNewSubobjectParams`、`add_new_subobject(...)` 与 `rename_subobject(...)`。
    - 这项能力本质上仍是 Blueprint 编辑器内的 SCS 组件脚本化编辑，失败边界清晰，适合下沉到 `本地Python 主实现`。
  - `set_component_property`
    - 该命令主要是对 Blueprint 组件模板对象做反射式属性写入，可直接复用现有 Blueprint 属性解析与类型转换逻辑。
    - 相比继续保留大段 C++ 特判调试代码，本地 Python 版本更短、更容易维护，也更利于后续统一组件属性写入链路。
  - `set_static_mesh_properties`
    - UE5.7 Python 已稳定暴露 `StaticMeshComponent.set_static_mesh(...)`、`set_material(...)` 与编辑器资源加载接口。
    - 该命令本质上仍是标准模板组件资源绑定，适合继续保留为 `本地Python 主实现`。
  - `set_physics_properties`
    - UE5.7 Python 已稳定暴露 `PrimitiveComponent.set_simulate_physics(...)`、`set_enable_gravity(...)`、`set_mass_override_in_kg(...)`、`set_linear_damping(...)`、`set_angular_damping(...)`。
    - 逻辑主要是几个常见物理字段的批量写入，不涉及运行态 PIE/VR 状态切换，适合下沉到本地 Python。
  - `cleanup_blueprint_for_reparent`
    - 暂继续保留为 `C++ 主实现`。
    - 原因是它同时涉及 SCS 节点删除、Graph 成员节点清理、全图刷新与回编译；当前已有 C++ 实现稳定，暂不为了迁移而重写整条链路。

### 2026-04-04 第三十五批：Blueprint 组件删除与重新挂接迁移到本地Python

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/blueprint/blueprint_commands.py`：
  - `remove_component_from_blueprint`
  - `attach_component_in_blueprint`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧继续复用 `BlueprintComponentHelper`，统一通过 `SubobjectDataSubsystem` 处理组件句柄查找、删除、重新挂接、Socket 写入与回编译。
- 本轮补充判断：
  - `remove_component_from_blueprint`
    - UE5.7 Python 已稳定暴露 `SubobjectDataSubsystem.delete_subobject(...)` 与 `SubobjectDataBlueprintFunctionLibrary.can_delete(...)`。
    - 该命令本质上是 Blueprint SCS 中单组件删除与结果序列化，失败边界明确，适合下沉到 `本地Python 主实现`。
  - `attach_component_in_blueprint`
    - UE5.7 Python 已稳定暴露 `SubobjectDataSubsystem.reparent_subobject(...)`、`ReparentSubobjectParams` 与 `SceneComponent.attach_to_component(...)`。
    - 重新挂接属于 Blueprint 编辑器内短链路的组件树调整，配合 `can_reparent(...)`、循环父子检查与回编译即可稳定验证，适合落在本地 Python。
- 当前约束：
  - `remove_component_from_blueprint` 当前不允许删除根组件。
  - `attach_component_in_blueprint` 当前只支持 `SceneComponent -> SceneComponent`，并要求目标组件不能挂到自己的子节点下。

### 2026-04-04 第三十六批：Blueprint 改父类与子蓝图创建迁移到本地Python

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/blueprint/blueprint_commands.py`：
  - `reparent_blueprint`
  - `create_child_blueprint`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧继续复用 `BlueprintAssetHelper` 与 `BlueprintEditorLibrary`，统一处理父 Blueprint 解析、生成类提取、父类重定向、资产保存与结果拼装。
- 本轮补充判断：
  - `reparent_blueprint`
    - UE5.7 Python 已稳定暴露 `BlueprintEditorLibrary.reparent_blueprint(...)`，并且内部已经负责节点刷新与回编译。
    - 该命令本质上是 Blueprint 资产级结构调整，不涉及 PIE/VR 运行控制，也不需要额外的引擎级生命周期桥接，适合下沉到 `本地Python 主实现`。
  - `create_child_blueprint`
    - 该命令本质上是“解析父 Blueprint -> 取生成类 -> 调用 `create_blueprint_asset_with_parent(...)` 创建新资产”的短链路编辑器脚本流程。
    - UE5.7 Python 已稳定暴露 Blueprint 生成类访问与资产创建接口，失败边界清晰，适合继续保留为 `本地Python 主实现`。
- 当前约束：
  - `create_child_blueprint` 当前默认创建到父 Blueprint 同目录；如需其他目录，可显式传 `path`。
  - `reparent_blueprint` 当前默认 `save=true`，因为 `BlueprintEditorLibrary.reparent_blueprint(...)` 会编译但不会自动保存资产。

### 2026-04-04 第三十七批：Blueprint 函数图增删迁移到本地Python

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/blueprint/blueprint_commands.py`：
  - `add_blueprint_function`
  - `delete_blueprint_function`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧新增 `BlueprintFunctionHelper`，统一通过 `BlueprintEditorLibrary` 处理函数图查找、创建、删除与回编译。
- 本轮补充判断：
  - `add_blueprint_function`
    - UE5.7 Python 已稳定暴露 `BlueprintEditorLibrary.add_function_graph(...)`。
    - 该命令本质上是 Blueprint 资产内新增函数图的短链路编辑器脚本流程，失败边界清晰，适合下沉到 `本地Python 主实现`。
  - `delete_blueprint_function`
    - UE5.7 Python 已稳定暴露 `BlueprintEditorLibrary.remove_function_graph(...)`。
    - 该命令本质上仍是函数图级别的结构调整，不涉及 PIE/VR 运行控制，也不需要额外的引擎级桥接，适合继续保留为 `本地Python 主实现`。
- 当前约束：
  - `add_blueprint_function` 当前遇到同名函数时会返回成功并标记 `created=false`，不会重复创建。
  - `delete_blueprint_function` 当前沿用 UE 官方语义，只删除函数图本身，不自动修复旧的调用节点。

### 2026-04-04 第三十八批：Blueprint 批量编译迁移到本地Python

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/blueprint/blueprint_commands.py`：
  - `compile_blueprints`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧在 `BlueprintCommandDispatcher` 中复用现有 `BlueprintAssetHelper` 与 `BlueprintPropertyHelper`，统一处理 Blueprint 解析、逐条编译、可选保存与结果汇总。
- 本轮补充判断：
  - `compile_blueprints`
    - 该命令本质上是对现有 `compile_blueprint` 的批量编排，不涉及额外引擎生命周期控制。
    - UE5.7 Python 已稳定暴露单 Blueprint 编译入口，因此批量调度、错误汇总和可选保存更适合落在 `本地Python 主实现`。
  - `get_blueprint_compile_errors`
    - 暂不迁移。
    - 原因是 UE5.7 Python 下 `Blueprint.Status` 仍是受保护属性，`CurrentMessageLog/PreCompileLog` 也没有稳定可读入口；在拿不到可靠错误明细前，不应为了迁移而输出不完整契约。

### 2026-04-04 第三十九批：Blueprint 成员重命名能力补齐

- 已新增：
  - `rename_blueprint_member`
- 当前实现形态：
  - `function` / `macro` / `graph` 走 `Plugins/UnrealMCP/Content/Python/commands/blueprint/blueprint_commands.py` 本地 Python 主实现。
  - `variable` 走 `Python + C++桥接`；本地 Python 继续负责图成员，C++ 仅保留 `FBlueprintEditorUtils::RenameMemberVariable(...)` 这条最小缺口补齐路径。
- 本轮补充判断：
  - `rename_blueprint_member`
    - Blueprint 图成员重命名本质上是标准编辑器图资产改名流程，UE5.7 Python 已稳定暴露 `BlueprintEditorLibrary.find_graph(...)` 与 `rename_graph(...)`，因此函数图、宏图和普通图适合直接落在 `本地Python`。
    - 但成员变量改名在 UE5.7 Python 下没有公开稳定的 rename API，`Blueprint.NewVariables` 也无法直接读取；如果强行纯 Python，会在类型保持、引用替换和默认值迁移上退化。
    - 按“优先最稳定、最短链路、最容易验证”的规则，这条能力当前更适合做成 `Python + C++桥接`，由 C++ 只补变量改名缺口，其余成员仍走本地 Python。

### 2026-04-04 第四十批：UMG 基础命令迁移到本地Python

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/umg/umg_commands.py`：
  - `create_umg_widget_blueprint`
  - `add_text_block_to_widget`
  - `add_button_to_widget`
  - `open_widget_blueprint_editor`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧新增 `UMGAssetHelper`、`UMGWidgetHelper` 与 `UMGCommandDispatcher`，统一处理 Widget Blueprint 解析、资源创建、RootCanvas 补齐、控件添加、布局写入与编辑器打开。
- 本轮补充判断：
  - UE5.7 Python 虽然不能直接读写 `WidgetBlueprint.WidgetTree` 属性，但已经稳定暴露：
    - `EditorUtilityLibrary.cast_to_widget_blueprint(...)`
    - `EditorUtilityLibrary.add_source_widget(...)`
    - `EditorUtilityLibrary.find_source_widget_by_name(...)`
  - 再结合 `AssetToolsHelpers.create_asset(...)` 与 `BlueprintEditorLibrary.compile_blueprint(...)`，已经足以覆盖基础 Widget Blueprint 创建和简单控件树编辑。
  - 按“优先最稳定、最短链路、最容易验证”的规则，这 4 条命令已经适合下沉到本地 Python。

### 2026-04-04 第四十一批：UMG 常用控件补充迁移到本地Python

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/umg/umg_commands.py`：
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
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧新增统一的 RootCanvas 创建、CanvasSlot 布局写入、常用控件属性初始化与编译保存流程。
- 本轮补充判断：
  - 这 15 条命令本质上仍是 Widget Blueprint 内的控件树追加和少量属性写入，不涉及运行态世界、GC、事务回滚或复杂编辑器状态机。
  - UE5.7 Python 已稳定暴露 `EditorUtilityLibrary.add_source_widget(...)`、各类 `Widget` 的基础 setter，以及 `BlueprintEditorLibrary.compile_blueprint(...)`。
  - 因此更适合作为 `本地Python 主实现`，而不是继续保留大段重复的 C++ 控件创建代码。

### 2026-04-04 第四十二批：UMG 控件树编辑迁移到本地Python

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/umg/umg_commands.py`：
  - `remove_widget_from_blueprint`
  - `set_widget_slot_layout`
  - `set_widget_visibility`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧继续复用 `UMGAssetHelper`、`UMGWidgetHelper`，统一处理控件查找、父子关系删除、CanvasSlot 布局写入与可见性设置。
- 本轮补充判断：
  - 这 3 条命令本质上仍是 Widget Blueprint 源控件树上的轻量编辑，不涉及运行时世界、GC 或复杂状态机。
  - UE5.7 Python 已稳定暴露 `EditorUtilityLibrary.find_source_widget_by_name(...)`、`Widget.get_parent()`、`PanelWidget.remove_child(...)`、`Widget.set_visibility(...)` 和 `CanvasPanelSlot` 常用 setter。
  - 因此继续保留为 `本地Python 主实现` 更符合“最短链路、最容易验证”的实现层规则。

### 2026-04-04 第四十三批：UMG 运行时挂载迁移到本地Python

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/umg/umg_commands.py`：
  - `add_widget_to_viewport`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责 Widget Blueprint 解析、`GeneratedClass` 获取、PIE/Game 世界解析、Widget 实例创建与 `AddToViewport(...)` 调用。
- 本轮补充判断：
  - `add_widget_to_viewport`
    - 该命令虽然作用在运行中的 PIE/VR Preview 世界，但逻辑本身仍是短链路的“拿到 GameWorld -> 创建 UserWidget -> AddToViewport”流程。
    - UE5.7 Python 已稳定暴露 `UnrealEditorSubsystem.get_game_world()`、`WidgetBlueprint.generated_class()`、`unreal.new_object(...)` 与 `UserWidget.add_to_viewport()`，实测闭环可行，因此适合下沉到 `本地Python 主实现`。
  - `bind_widget_event` / `set_text_block_binding`
    - 这两项依赖 `FDelegateEditorBinding`、`FEditorPropertyPath`、Bound Event 节点等编辑器内部绑定结构。
    - 当前 UE5.7 Python 没有稳定暴露这些关键类型和 API，不适合为了迁移而降级契约，因此继续保留为 `C++ 主实现`。

### 2026-04-04 第四十四批：UMG Named Slot 迁移到本地Python

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/umg/umg_commands.py`：
  - `add_named_slot_to_widget`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧继续复用 `UMGWidgetHelper`，统一处理根 Canvas 创建、`NamedSlot` 控件追加、布局写入、编译与保存。
- 本轮补充判断：
  - `add_named_slot_to_widget`
    - 该命令本质上仍是 Widget Blueprint 源控件树上的轻量追加操作，不涉及运行态世界、GC 或复杂编辑器状态机。
    - UE5.7 已直接提供 `UNamedSlot`，而本地 Python 现有链路也已稳定覆盖 `EditorUtilityLibrary.add_source_widget(...)` 与编译保存。
    - 因此它更适合作为 `本地Python 主实现`，不需要继续保留额外的 C++ 业务逻辑。

### 2026-04-04 第四十五批：UMG Brush 编辑迁移到本地Python

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/umg/umg_commands.py`：
  - `set_widget_brush`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧复用 `UMGWidgetHelper`，统一处理目标控件查找、Brush 资源加载、Tint/尺寸写入与编译保存。
- 本轮补充判断：
  - `set_widget_brush`
    - 这项能力本质上仍是 Widget Blueprint 源控件树上的轻量属性编辑，不涉及运行态世界、GC 或复杂编辑器状态机。
    - UE5.7 Python 已稳定暴露 `Image.set_brush_from_texture/material/asset(...)`、`Image.set_brush_tint_color(...)`、`Image.set_brush_size(...)`、`Border.set_brush_from_texture/material/asset(...)` 与 `Border.set_brush_color(...)`。
    - 因此 `Image` / `Border` 两类控件适合作为 `本地Python 主实现`。
  - `set_widget_style`
  - `Button`、`ProgressBar`、`CheckBox` 等控件的 `widget_style` 仍然依赖更复杂的 Slate 样式结构体读写。
  - 当前更适合保持为后续独立条目，不和 `set_widget_brush` 混在同一轮里扩张参数面。

### 2026-04-04 第四十六批：UMG 视图控件迁移到本地Python

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/umg/umg_commands.py`：
  - `add_list_view_to_widget`
  - `add_tile_view_to_widget`
  - `add_tree_view_to_widget`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧继续复用 `UMGWidgetHelper` 与 `_create_widget_on_root_canvas(...)`，统一处理根 Canvas 创建、视图控件追加、布局写入与编译保存。
- 本轮补充判断：
  - 这 3 条命令本质上仍是 Widget Blueprint 源控件树上的轻量控件追加，不涉及运行时世界、GC 或复杂编辑器状态机。
  - UE5.7 Python 已直接暴露 `ListView`、`TileView`、`TreeView` 控件类型，并且当前本地 Python 链路已稳定覆盖 `EditorUtilityLibrary.add_source_widget(...)`、布局回写与编译保存。
  - 因此它们更适合作为 `本地Python 主实现`，不需要继续保留额外的 C++ 业务逻辑。

### 2026-04-04 第四十七批：UMG 样式结构体迁移到本地Python

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/umg/umg_commands.py`：
  - `set_widget_style`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧复用 `UMGWidgetHelper`，统一处理目标控件查找、Slate 样式结构体读改写、编译与保存。
- 本轮补充判断：
  - `set_widget_style`
    - 该命令本质上仍是 Widget Blueprint 源控件树上的轻量样式编辑，不涉及运行态世界、GC 或复杂编辑器状态机。
    - UE5.7 Python 已稳定暴露 `ButtonStyle`、`ProgressBarStyle`、`CheckBoxStyle`、`SlateBrush`，并支持通过 `set_editor_property(...)` 回写结构体字段。
    - 当前先收敛支持 `Button`、`ProgressBar`、`CheckBox`、`Border`、`Image` 的稳定字段，不在这一轮混入动画、时序或更深的 Slate 细节。
  - 当前约束：
    - Brush 子对象当前支持 `brush_asset_path`、`texture_asset_path`、`material_asset_path`、`tint_color`、`image_size`、`draw_as`、`margin`。
    - `image_size` 在 UE5.7 Python 下实际对应 `DeprecateSlateVector2D`，需要按 `x/y` 字段写回，不能直接塞 `Vector2D`。

### 2026-04-04 第四十八批：Widget 动画创建保留为 C++主实现

- 已新增：
  - `create_widget_animation`
- 当前实现形态：
  - `C++主实现`
  - 外部 MCP 工具注册位于 `Plugins/UnrealMCP/MCPServer/tools/umg_tools.py`
  - Unreal 侧命令处理位于 `Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPUMGCommands.cpp`
- 本轮补充判断：
  - `create_widget_animation`
    - 该命令需要向 `WidgetBlueprint->Animations` 追加新 `UWidgetAnimation`，同时创建配套 `UMovieScene` 并注册 Blueprint 变量。
    - UE5.7 Python 虽然已经暴露 `WidgetAnimation`、`MovieSceneTrack`、`MovieSceneSection` 等后续编辑对象，但 `WidgetBlueprint.Animations` 仍是受保护属性，无法稳定读写。
    - 按“优先最稳定、最短链路、最容易验证”的规则，这条命令当前更适合保留为 `C++主实现`，后续若 Python 暴露面补齐，再评估是否把“创建动画对象”迁到本地 Python。

### 2026-04-04 第四十九批：Widget 动画关键帧保留为 C++主实现

- 已新增：
  - `add_widget_animation_keyframe`
- 当前实现形态：
  - `C++主实现`
  - 外部 MCP 工具注册位于 `Plugins/UnrealMCP/MCPServer/tools/umg_tools.py`
  - Unreal 侧命令处理位于 `Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPUMGCommands.cpp`
- 本轮补充判断：
  - `add_widget_animation_keyframe`
    - 该命令除了向 `MovieScene` 轨道和 Section 写入关键帧，还要稳定维护 `WidgetAnimationBinding` 与 `Possessable` 的绑定关系。
    - UE5.7 Python 虽然已经暴露 `MovieSceneFloatTrack`、`MovieScene2DTransformTrack` 等轨道对象，但 `WidgetAnimationBinding` 仍未稳定暴露，无法无损复刻当前引擎编辑器里的绑定链路。
    - 按“优先最稳定、最短链路、最容易验证”的规则，这条命令当前更适合保留为 `C++主实现`，而不是为了追求纯 Python 化去降级契约或绕过绑定关系。
- 当前支持范围：
  - `render_opacity`
  - `render_transform.translation / translation_x / translation_y`
  - `render_transform.scale / scale_x / scale_y`
  - `render_transform.shear / shear_x / shear_y`
  - `render_transform.rotation`

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

### 2026-04-04 第二十八批：视口显示模式切换

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `set_viewport_mode`
- 当前实现形态：
  - C++ 侧只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧复用 `ViewportCommandHelper`，统一处理视图模式字符串/索引解析、活动视口或全部关卡视口切换与结果回包。
- 本轮补充判断：
  - `set_viewport_mode`
    - 这项命令本质上仍是编辑器视口上的轻量模式切换，不涉及 PIE/VR 状态机、事务系统或复杂生命周期控制。
    - UE5.7 Python 已稳定暴露 `AutomationLibrary.set_editor_active_viewport_view_mode(...)`、`set_editor_viewport_view_mode(...)` 与 `get_editor_active_viewport_view_mode(...)`。
    - 因此它更适合作为 `本地Python 主实现`，不需要继续保留重复的 C++ 视口枚举与设置逻辑。

### 2026-04-04 第七十五批：get_viewport_camera 迁移到本地Python

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `get_viewport_camera`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPEditorCommands.cpp` 对这条命令只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧复用 `ViewportCommandHelper`，负责活动关卡视口相机位姿读取、`ViewModeIndex` 查询，以及结果序列化。
- 本轮补充判断：
  - `get_viewport_camera`
    - 这条命令本质上仍是编辑器活动视口的只读查询，不涉及 PIE/VR 状态切换、事务系统或复杂生命周期控制。
    - UE5.7 Python 已稳定暴露 `UnrealEditorSubsystem.get_level_viewport_camera_info(...)` 与 `AutomationLibrary.get_editor_active_viewport_view_mode()`，足以覆盖最核心的相机位姿与视图模式回包。
    - 当前对 `fov`、`is_perspective`、`is_realtime`、`viewport_type`、`viewport_size` 仍未发现同等稳定的 Python 读取接口，因此本轮统一显式回传 `null`、空字符串或空数组，而不是继续保留一条纯 C++ 专用查询路径。
    - 按“优先最稳定、最短链路、最容易验证”的规则，这条命令现在可接受收敛到 `本地Python 主实现`；缺失字段已经在返回层显式标注，便于调用侧识别当前边界。

### 2026-04-04 第五十批：spawn_actor 开放 scale 参数

- 已增强：
  - `spawn_actor`
- 当前实现形态：
  - Unreal 侧主实现保持 `本地Python` 不变。
  - 外部 MCP 工具层 `Plugins/UnrealMCP/MCPServer/tools/editor_tools.py` 现已正式公开 `scale=[1,1,1]` 参数，不再把 Unreal 侧已支持的缩放能力藏在未暴露参数后面。
- 本轮补充判断：
  - `spawn_actor`
    - 本地 Python 的 `ActorEditHelper.spawn_actor(...)` 之前已经稳定支持 `scale`，而 MCP 工具层缺少这个参数只是包装层缺口，不是引擎能力缺口。
    - 这类“Unreal 侧已支持、工具层未公开”的差异应优先补齐，因为链路短、风险低，也符合自动化任务里“持续清理清单而不是机械迁移”的要求。

### 2026-04-04 第五十一批：新增 Blueprint Macro 便捷工具

- 已新增：
  - `add_blueprint_macro`
- 当前实现形态：
  - 外部 MCP 工具注册位于 `Plugins/UnrealMCP/MCPServer/tools/blueprint_tools.py`
  - Unreal 侧复用现有 `create_blueprint_graph(graph_type="macro")` 实现，不新增重复命令分支。
- 本轮补充判断：
  - `add_blueprint_macro`
    - 该能力本质上是现有 `create_blueprint_graph` 的常用语义封装，底层实现已经稳定支持 `macro graph`。
    - 单独暴露工具后，调用方不必再记忆 `graph_type="macro"` 这一隐式约定，更符合 TODO 中“补齐常用 UE 功能入口”的目标。

### 2026-04-04 第五十二批：spawn_actor 补齐 class_path / world_type / template_actor

- 已增强：
  - `spawn_actor`
- 当前实现形态：
  - Unreal 侧主实现保持 `本地Python`
  - 外部 MCP 工具注册位于 `Plugins/UnrealMCP/MCPServer/tools/editor_tools.py`
- 本轮补充判断：
  - `spawn_actor`
    - 这次不是把 C++ 主实现迁到 Python，而是继续扩展已经稳定的本地 Python 主实现，补齐 TODO 里长期缺的 `class_path`、`world_type`、`template_actor` 三个入口。
    - `class_path` 直接复用现有 `spawn_actor_from_class` 的类解析能力，链路短、验证成本低。
    - `template_actor` 本质上是“先解析现有 Actor，再走编辑器 duplication”，也适合放在 `ActorEditHelper` 中统一处理。
    - `world_type` 当前正式公开，但考虑到底层仍依赖 `EditorActorSubsystem`，本轮明确只支持 `editor` 世界；如果自动解析到 `pie`，则直接报错，不伪装为成功。

### 2026-04-04 第五十三批：take_screenshot 补齐视口参数增强

- 已增强：
  - `take_screenshot`
- 当前实现形态：
  - 继续保留为 `C++主实现`
  - 外部 MCP 工具注册位于 `Plugins/UnrealMCP/MCPServer/tools/editor_tools.py`
- 本轮补充判断：
  - `take_screenshot`
    - 这次不是把 C++ 主实现迁到本地 Python，而是继续按实现层规则补齐现有 C++ 主实现的能力面。
    - 同步 PNG 输出、指定关卡视口、像素缩放和透明 Alpha 都仍然依赖 `FViewport::ReadPixels(...) + ImageResize + PNGCompressImageArray(...)` 这条稳定链路，继续保留在 C++ 最短也最稳。
    - `show_ui=true` 仍需走 `FScreenshotRequest::RequestScreenshot(...)` 的异步请求路径，因此本轮保留“只支持活动视口、返回 request_queued”的约束，而不是为了追求参数对称把同步契约做坏。
    - 结论是：`take_screenshot` 当前更适合继续保持 `C++主实现`，并在此基础上补齐 `resolution`、`show_ui`、`transparent_background`、`viewport_index` 四个实用入口。

### 2026-04-04 第五十四批：spawn_blueprint_actor 路由收敛

- 已整理：
  - `spawn_blueprint_actor`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPEditorCommands.cpp` 继续保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - Blueprint 命令域中遗留的旧 C++ 直连实现已删除，避免同一命令在两个命令域中出现不一致的实现源。
- 本轮补充判断：
  - `spawn_blueprint_actor`
    - 该命令在功能层面早已归入 `Editor` 域，并由 `commands.editor.editor_commands` 中的 `ActorEditHelper.spawn_blueprint_actor(...)` 负责本地 Python 主实现。
    - 本轮不是新增迁移，而是把残留的重复 C++ 入口清理掉，保证桥接层、源码结构和 TODO 文档三者一致，避免后续维护时误判实现层。

### 2026-04-04 第五十五批：add_blueprint_variable 迁移到本地 Python

- 已迁移：
  - `add_blueprint_variable`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPBlueprintCommands.cpp` 只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧由 `commands.blueprint.blueprint_commands` 中的 `BlueprintMemberHelper.add_blueprint_variable(...)` 负责变量类型解析、变量创建与 `is_exposed` 写入。
- 本轮补充判断：
  - `add_blueprint_variable`
    - 该命令本质上是 Blueprint 资产上的成员变量编辑，不依赖图节点状态机，更适合放在 Blueprint 资产域而不是继续停留在 Node 命令域。
    - UE5.7 Python 已稳定暴露 `BlueprintEditorLibrary.add_member_variable(...)`、`get_basic_type_by_name(...)`、`get_struct_type(...)` 与 `set_blueprint_variable_instance_editable(...)`，足以覆盖当前工具契约中的 `Boolean / Integer / Float / String / Name / Text / Vector`。
    - 迁移后 C++ 侧不再维护重复的变量类型分支判断，Blueprint 变量创建逻辑统一回收到本地 Python，后续继续扩展变量类型时也更短链路。

### 2026-04-04 第五十八批：add_blueprint_interface 保留为 C++主实现

- 已新增：
  - `add_blueprint_interface`
- 当前实现形态：
  - 继续保留为 `C++主实现`
  - 外部 MCP 工具注册位于 `Plugins/UnrealMCP/MCPServer/tools/blueprint_tools.py`
- 本轮补充判断：
  - 这条命令底层依赖 `FBlueprintEditorUtils::ImplementNewInterface(...)` 与 `ConformImplementedInterfaces(...)`。
  - UE5.7 官方 Python API 当前没有公开等价入口，也拿不到同级的接口补图流程，因此不适合为了“纯 Python 化”去降级能力面。
  - 按“优先最稳定、最短链路、最容易验证”的规则，这条命令当前更适合保留为 `C++主实现`，同时对外补齐正式 MCP 工具入口。

### 2026-04-04 第五十九批：材质图编辑命令迁移到本地Python

- 已迁移到 `Plugins/UnrealMCP/Content/Python/commands/assets/asset_commands.py`：
  - `add_material_expression`
  - `connect_material_expressions`
  - `layout_material_graph`
  - `compile_material`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPAssetCommands.cpp` 只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责基础 `Material` 解析、表达式类解析、节点坐标、属性覆写、表达式连线、材质属性连线、整理布局与重新编译保存。
- 本轮补充判断：
  - 这 4 条命令的核心全部落在 `MaterialEditingLibrary` 上，属于典型的编辑器资产图编辑脚本，链路短、验证边界清晰。
  - UE5.7 Python 已稳定暴露 `create_material_expression_ex(...)`、`connect_material_expressions(...)`、`connect_material_property(...)`、`layout_material_expressions(...)` 与 `recompile_material(...)`。
  - 因此继续保留重复的 C++ 图编辑实现没有必要，统一收敛到本地 Python 后，材质资产域的实现层也更一致。

### 2026-04-04 第六十一批：Blueprint 编译错误读取保留为 C++主实现

- 已新增：
  - `get_blueprint_compile_errors`
- 当前实现形态：
  - `C++主实现`
  - 外部 MCP 工具注册位于 `Plugins/UnrealMCP/MCPServer/tools/blueprint_tools.py`
  - Unreal 侧命令处理位于 `Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPBlueprintCommands.cpp`
- 本轮补充判断：
  - `get_blueprint_compile_errors`
    - 该命令需要稳定读取 `FCompilerResultsLog`、`FTokenizedMessage` 与 `Blueprint.Status`，并把错误/警告消息数组直接序列化给 MCP。
    - UE5.7 官方 Python 目前虽然能调用 `compile_blueprint(...)`，但仍拿不到同级的内部编译日志对象，也无法无损回读 Blueprint 编译状态与消息明细。
  - 按“优先最稳定、最短链路、最容易验证”的规则，这条命令当前更适合保留为 `C++主实现`，而不是为了追求 Python 化去降级返回内容。

### 2026-04-04 第六十二批：spawn_blueprint_actor 补齐任意目录短名解析

- 已增强：
  - `spawn_blueprint_actor`
- 当前实现形态：
  - Unreal 侧主实现继续保持 `本地Python`
  - 实现位于 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`
- 本轮补充判断：
  - `spawn_blueprint_actor`
    - 该命令之前虽然已经迁到本地 Python，但短名解析仍只兼容 `/Game/Blueprints/<Name>`，导致放在其他目录的 Blueprint 必须手写完整路径。
    - 这类问题属于“Python 主实现已成立，但解析层能力面还没跟上”，更适合继续在本地 Python 中补齐，而不是回退到 C++。
    - 本轮改为先尝试旧候选路径，再回退到 AssetRegistry 全项目扫描；若短名唯一，则允许直接实例化；若同名 Blueprint 有多个，则明确要求调用侧改传完整资产路径，避免隐式命中错误资源。

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
- `get_source_control_status`
- `batch_rename_assets`
- `batch_move_assets`
- `search_assets` 的通配符/正则增强（已完成）

## 2. 关卡 / World / Actor

- `load_level`
- `save_current_level`
- `get_actors_in_level`
- `find_actors_by_name`
- `find_actors_by_name` 的类过滤/标签过滤/路径过滤/排序增强（已完成）
- `spawn_actor`
- `delete_actor`
- `set_actor_transform`
- `set_actor_property`（已完成）
- `get_actor_properties`
- `get_actor_components`
- `get_scene_components`
- `focus_viewport`
- `set_viewport_mode`（已完成）
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
- `add_component_to_actor`（已完成）
- `remove_component_from_actor`（已完成）
- `spawn_actor_from_class`（已完成）
- `set_actors_transform`
- `get_world_settings`（已完成）
- `set_world_settings`（已完成）
- `line_trace`
- `box_trace`
- `sphere_trace`
- `get_hit_result_under_cursor`（已完成；当前仅支持 PIE/VR Preview）
- `get_data_layers`（已完成）
- `set_actor_data_layers`（已完成）
- `load_world_partition_cells`
- `unload_world_partition_cells`
- `build_navigation`
- `build_lighting`
- `build_hlod`
- `spawn_actor` 的 `scale/class_path/world_type/template_actor` 增强（已完成；当前 `world_type` 仅支持 `editor`）

## 3. Blueprint 资产 / 类 / 组件

- `create_blueprint`（已完成）
- `add_component_to_blueprint`（已完成）
- `set_component_property`（已完成）
- `set_static_mesh_properties`（已完成）
- `set_physics_properties`（已完成）
- `compile_blueprint`（已完成）
- `cleanup_blueprint_for_reparent`
- `set_blueprint_property`（已完成）
- `set_pawn_properties`（已完成）
- `set_game_mode_default_pawn`（已完成）
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
- `remove_component_from_blueprint`（已完成）
- `attach_component_in_blueprint`（已完成）
- `set_inherited_component_override`
- `get_blueprint_compile_errors`（已完成，C++主实现）
- `compile_blueprints`
- `save_blueprint`（已完成）
- `open_blueprint_editor`（已完成）
- `create_blueprint` 的路径/包名/父类路径增强（已由本地Python 主实现补齐路径与完整类路径解析，并保留 `EditorUtilityWidget` 拦截）
- `add_component_to_blueprint` 的事务回滚增强（已支持 `parent_name`；组件重挂接与 `socket_name` 已由 `attach_component_in_blueprint` 覆盖）

## 4. Blueprint 图节点 / Graph 编辑

- `add_blueprint_event_node`
- `add_blueprint_input_action_node`
- `add_blueprint_function_node`
- `connect_blueprint_nodes`
- `add_blueprint_variable`（已完成）
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

- `create_input_mapping`（已完成）
- `list_input_mappings`（已完成）
- `remove_input_mapping`（已完成）
- `create_input_axis_mapping`（已完成）
- `create_input_action_asset`（已完成）
- `create_input_mapping_context`（已完成）
- `add_mapping_to_context`（已完成）
- `assign_mapping_context`（已完成）
- `get_project_setting`（已完成）
- `set_project_setting`（InputSettings 已完成；GameMapsSettings 保持 C++）
- `set_default_maps`
- `set_game_framework_defaults`
- `enable_plugin`
- `disable_plugin`
- `add_module_dependency`
- `set_packaging_settings`

### 2026-04-04 第七十批：传统输入映射迁移到本地Python

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/project/project_commands.py`：
  - `create_input_mapping`
  - `create_input_axis_mapping`
  - `list_input_mappings`
  - `remove_input_mapping`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPProjectCommands.cpp` 对上述 4 条命令只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责按键解析、Action/Axis 映射增删查、过滤与 `InputSettings.save_key_mappings()` 持久化。
- 本轮补充判断：
  - 传统输入映射命令本质上是 `InputSettings` 默认配置的脚本化编辑，不涉及 PIE/VR 运行态切换，也不需要复杂事务控制。
  - UE5.7 Python 已稳定暴露 `InputSettings.add_action_mapping(...)`、`add_axis_mapping(...)`、`remove_action_mapping(...)`、`remove_axis_mapping(...)`、`force_rebuild_keymaps()`、`save_key_mappings()`。
  - `Key` 本身虽然没有构造时校验，但可通过 `Key.import_text(...) + InputLibrary.key_is_valid(...)` 稳定完成按键名验证，因此这 4 条命令适合直接落到 `本地Python`。
  - 相比之下，`GameMapsSettings.save_config()` 当前并未暴露到 Python，所以本轮没有顺手硬迁 `set_default_maps` / `set_project_setting`，避免引入半持久化实现。

### 2026-04-04 第七十一批：Enhanced Input 资源命令迁移到本地Python

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/project/project_commands.py`：
  - `create_input_action_asset`
  - `create_input_mapping_context`
  - `add_mapping_to_context`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPProjectCommands.cpp` 对上述 3 条命令只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责资源目录校验、Enhanced Input 资源创建、枚举解析、Context 资源查找、按键映射写入与保存。
- 本轮补充判断：
  - 这 3 条命令本质上仍是编辑器资产脚本，不涉及 PIE/VR 运行态切换、事务回滚或复杂生命周期管理。
  - UE5.7 Python 已稳定暴露 `AssetTools.create_asset(...)`、`InputAction`、`InputMappingContext`、`InputMappingContext.map_key(...)` 与 `EditorAssetLibrary.save_asset(...)`。
  - `mapping_context` / `input_action` 的短名解析也可以通过资产注册表做唯一匹配，失败边界清晰，适合继续收敛为 `本地Python`。

### 2026-04-04 第七十二批：assign_mapping_context 迁移到本地Python

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/project/project_commands.py`：
  - `assign_mapping_context`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPProjectCommands.cpp` 对这条命令只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧新增 `RuntimePlayerResolver`，负责运行世界解析、目标 Actor 查找、`LocalPlayer` / `EnhancedInputLocalPlayerSubsystem` 解析，以及 `ModifyContextOptions` 组装。
- 本轮补充判断：
  - 这条命令确实属于运行态能力，但 UE5.7 Python 里可以稳定枚举 `EnhancedInputLocalPlayerSubsystem` 实例，并通过 `get_typed_outer(unreal.LocalPlayer)` 建立与当前 `PIE/VR Preview` 世界的对应关系。
  - 因此即使某些时刻 `GameplayStatics.get_player_controller(world, player_index)` 还拿不到控制器，也仍然可以按 `LocalPlayer` + `EnhancedInputLocalPlayerSubsystem` 继续完成 Mapping Context 分配。
  - 目标是 `Pawn` / `PlayerController` 时仍保留原有约束；如果传入 `Pawn`，会先解析其控制器，再校验本地玩家归属。

### 2026-04-04 第七十三批：get_project_setting 迁移到本地Python

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/project/project_commands.py`：
  - `get_project_setting`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPProjectCommands.cpp` 对这条命令只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧新增 `ProjectSettingsCommandHelper`，负责受支持设置对象解析、属性读取、值序列化与配置文件路径返回。
- 本轮补充判断：
  - `get_project_setting`
    - 这条命令是纯只读配置查询，不涉及 `SaveConfig()`、事务系统、PIE/VR 状态切换或复杂生命周期管理。
    - UE5.7 Python 已稳定暴露 `GameMapsSettings.get_game_maps_settings()`、`InputSettings.get_input_settings()` 与 `get_editor_property(...)`，足以覆盖当前支持的两类设置对象。
    - `SoftObjectPath` 之类复杂值在 Python 中虽然不会自动转成 JSON 基础类型，但可通过 `export_text()` 稳定导出字符串，因此这条命令适合下沉到 `本地Python`。
  - 当前边界：
    - `property_cpp_type` 由本地 Python 按常见值类型显式推断，不再依赖 C++ 反射直接回传底层 `FProperty::GetCPPType()`。
    - `set_default_maps`、`set_game_framework_defaults` 仍保留为更稳妥的非本地 Python 实现，因为写入链路依然依赖稳定持久化和更严格的类型导入。

### 2026-04-04 第七十四批：set_project_setting 收敛为 InputSettings 本地Python主路径

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/project/project_commands.py`：
  - `set_project_setting` 的 `InputSettings` 写入路径
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPProjectCommands.cpp` 先解析设置对象；若命中 `InputSettings`，则转发到 `ExecuteLocalPythonCommand(...)`，否则继续保留现有 C++ 写入链路。
  - 本地 Python 侧复用 `ProjectSettingsCommandHelper`，负责属性读取、按当前属性类型做值转换、`set_editor_property(...)` 写入，以及 `force_rebuild_keymaps() + save_key_mappings()` 持久化。
- 本轮补充判断：
  - `InputSettings`
    - 现场探针确认 UE5.7 Python 虽未暴露 `save_config()`，但 `InputSettings.save_key_mappings()` 会把普通配置属性一并落到 `DefaultInput.ini`，因此适合收敛到本地 Python。
    - 这条路径仍属于标准编辑器配置脚本，不涉及 PIE/VR 状态切换或复杂生命周期管理。
  - `GameMapsSettings`
    - `GameMapsSettings.save_config()` 仍未暴露到 Python，不能为迁移而退化为“只改内存不落盘”。
    - 因此本轮明确采用 `Python + C++桥接`，只迁稳定的 `InputSettings`，其余继续保留 C++。

## 6. UMG / UI

- `create_umg_widget_blueprint`
- `add_text_block_to_widget`
- `add_button_to_widget`
- `bind_widget_event`
- `add_widget_to_viewport`
- `remove_widget_from_viewport`
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
- `set_widget_style`（已完成）
- `set_widget_brush`（已完成）
- `create_widget_animation`
- `add_widget_animation_keyframe`
- `bind_widget_property`
- `add_named_slot_to_widget`
- `open_widget_blueprint_editor`
- `get_blueprint_summary` 的 WidgetTree/Binding/Animation 摘要增强

## 7. 编辑器运行控制 / 调试

- `open_asset_editor`
- `close_asset_editor`
- `execute_console_command`（已完成）
- `run_editor_utility_widget`（已完成）
- `run_editor_utility_blueprint`（已完成）
- `get_output_log`
- `clear_output_log`
- `get_message_log`
- `show_editor_notification`
- `set_viewport_mode`
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
- `add_material_expression`（已完成）
- `connect_material_expressions`（已完成）
- `layout_material_graph`（已完成）
- `compile_material`（已完成）
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
- `create_data_table`（已完成）
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
- `create_data_layer`（已完成）
- `set_data_layer_state`（已完成）

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

### 2026-04-04 第六十批：Trace 命令补齐到本地Python

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `line_trace`
  - `box_trace`
  - `sphere_trace`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPEditorCommands.cpp` 只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责世界解析、TraceTypeQuery 归一化、忽略 Actor 解析、`SystemLibrary.*_trace_single(...)` 调用，以及 `HitResult.to_tuple()` 的稳定序列化。
- 本轮补充判断：
  - 这 3 条命令本质上是标准编辑器/PIE 世界中的查询型碰撞检测，不涉及包生命周期、事务系统或运行态状态切换。
  - UE5.7 Python 已稳定暴露 `SystemLibrary.line_trace_single(...)`、`box_trace_single(...)`、`sphere_trace_single(...)`，链路短，验证成本也低。
  - `HitResult` 的字段在 Python 里大多是受保护属性，直接 `get_editor_property(...)` 不稳定；因此本轮采用 `to_tuple()` 固定次序反序列化，保持实现层在 Python 的同时避免回退到 C++ 取值。

### 2026-04-04 第六十三批：add_widget_to_viewport 补齐 owning_player 与实例句柄返回

- 已增强：
  - `add_widget_to_viewport`
- 当前实现形态：
  - Unreal 侧主实现继续保持 `本地Python`
  - 实现位于 `Plugins/UnrealMCP/Content/Python/commands/umg/umg_commands.py`
  - 外部 MCP 工具参数扩展位于 `Plugins/UnrealMCP/MCPServer/tools/umg_tools.py`
- 本轮补充判断：
  - 这条命令本质上仍是运行中的 PIE/VR Preview 世界里创建 `UserWidget` 并挂到屏幕，主要差异只是是否需要显式绑定本地玩家。
  - UE5.7 Python 已稳定暴露 `UserWidget.set_owning_player(...)`、`add_to_player_screen(...)`、`add_to_viewport(...)` 与 `GameplayStatics.get_player_controller(...)`，足以在不引入额外 C++ 桥接的前提下补齐 `owning_player` 绑定能力。
  - 因此本轮继续在本地 Python 主实现上增强，而不是回退到 C++；同时返回 `instance_path`、`screen_mode`、`owning_player_name`、`owning_player_path`，把“实例句柄返回”收敛到当前可稳定验证的对象路径层级。

### 2026-04-04 第六十四批：bind_widget_property 补齐为 C++主实现

- 已新增：
  - `bind_widget_property`
- 当前实现形态：
  - `C++主实现`
  - Unreal 侧命令处理位于 `Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPUMGCommands.cpp`
  - 外部 MCP 工具注册位于 `Plugins/UnrealMCP/MCPServer/tools/umg_tools.py`
- 本轮补充判断：
  - `bind_widget_property`
    - 现有 `set_text_block_binding` 已经具备 `FDelegateEditorBinding` 写入、缺失变量自动创建、编译保存这条稳定主链路，缺的是正式的通用命令入口，而不是另起一套 Python 绑定系统。
    - UE5.7 Python 当前没有公开与 `FDelegateEditorBinding` / `WidgetBlueprint->Bindings` 等价且稳定的编辑 API；如果强行改成本地 Python，只能退回到不完整或不可验证的实现。
    - 按“优先最稳定、最短链路、最容易验证”的规则，这条命令当前更适合直接补成 `C++主实现`，并让旧的 `set_text_block_binding` 复用同一处理逻辑。
  - 本轮收敛后的能力边界：
  - `bind_widget_property` 仍只支持当前 `UnrealMCPUMGResolveBindingConfig(...)` 已知的 `binding_type`
  - 但不再强制目标控件必须是 `TextBlock`
  - 只有当目标控件类真实存在对应属性时才允许绑定，避免“命令名更通用、实际 silently 失败”的隐患

### 2026-04-04 第六十五批：add_widget_to_viewport 补齐移除接口

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/umg/umg_commands.py`：
  - `remove_widget_from_viewport`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPUMGCommands.cpp` 只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责运行时世界解析、Widget 实例查找、同名歧义检查与 `remove_from_parent()` 调用。
- 本轮补充判断：
  - `remove_widget_from_viewport`
    - 这条命令与 `add_widget_to_viewport` 同属运行时 `UserWidget` 实例管理，当前实例句柄已经能由 `add_widget_to_viewport` 稳定返回 `instance_path`。
    - UE5.7 Python 已稳定暴露 `find_object(...)`、`load_object(...)`、`ObjectIterator(UserWidget)` 与 `UserWidget.remove_from_parent()`，足以在不引入额外 C++ 桥接的前提下补齐移除闭环。
    - 因此本轮继续保持 `本地Python 主实现`，并把“优先按 `instance_path` 精确删除、`instance_name` 只作为回退”的约束固化到接口层，避免多实例误删。

### 2026-04-04 第六十六批：get_hit_result_under_cursor 迁移到本地Python

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `get_hit_result_under_cursor`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPEditorCommands.cpp` 只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧复用现有 `TraceCommandHelper`，负责 `PlayerController` 解析、鼠标位置读取、反投影射线生成与统一 `HitResult` 序列化。
- 本轮补充判断：
  - `get_hit_result_under_cursor`
    - UE5.7 Python 已稳定暴露 `PlayerController.get_mouse_position()`、`deproject_mouse_position_to_world()` 与 `hit_result_trace_distance`，足以在 PIE/VR Preview 运行世界内完成鼠标命中查询。
    - `UnrealEditorSubsystem` 当前只暴露视口相机信息，没有编辑器鼠标位置接口，因此这条命令不适合承诺 `editor` 世界支持。
    - 按“优先最稳定、最短链路、最容易验证”的规则，本轮把它收敛为 `本地Python 主实现`，但明确当前能力边界为“仅支持 PIE/VR Preview 世界”。

### 2026-04-04 第六十七批：delete_blueprint_graph 迁移到本地Python

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/blueprint/blueprint_commands.py`：
  - `delete_blueprint_graph`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPBlueprintNodeCommands.cpp` 只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责图查找、图类型识别、最后一个主图保护与 `BlueprintEditorLibrary.remove_graph(...)` 调用。
- 本轮补充判断：
  - `delete_blueprint_graph`
    - UE5.7 Python 已稳定暴露 `BlueprintEditorLibrary.find_graph(...)` 与 `remove_graph(...)`，足以覆盖现有删除图主链路。
    - 这条命令本质上属于 Blueprint 图结构编辑，不涉及 PIE/VR 运行控制、GC 或进程状态切换，适合下沉到本地 Python。
    - 为避免行为退化，本轮保留了“最后一个主图不可删除”的保护，并额外回传 `graph_path` 与 `implementation=local_python`，便于自动化回读验证。

### 2026-04-04 第六十八批：get_data_layers 迁移到本地Python

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `get_data_layers`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPEditorCommands.cpp` 只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧通过 `DataLayerEditorSubsystem.get_all_data_layers()` 枚举当前编辑器世界中的 Data Layer 实例，并序列化基础可见性/运行态元数据。
- 本轮补充判断：
  - `get_data_layers`
    - UE5.7 Python 已稳定暴露 `DataLayerEditorSubsystem`、`DataLayerInstance.get_data_layer_short_name()`、`get_data_layer_full_name()`、`is_visible()`、`is_runtime()` 等正式接口。
    - 这条命令本质上是编辑器查询，不涉及 PIE/VR 状态切换、GC 或事务写入，适合直接落到 `本地Python`。
- 当前能力边界先收敛为“只读当前编辑器世界里的 Data Layer 列表”；`set_actor_data_layers` 之类写操作后续再单独补。

### 2026-04-04 第七十六批：create_blueprint_graph 收敛为 function 本地Python主路径

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/blueprint/blueprint_commands.py`：
  - `create_blueprint_graph` 的 `graph_type="function"` 路径
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPBlueprintNodeCommands.cpp` 先解析 `graph_type`。
  - 当 `graph_type="function"` 时，转发到本地 Python；
  - `graph_type="graph"` / `graph_type="macro"` 继续保留现有 C++ 创建逻辑。
- 本轮补充判断：
  - `function`
    - UE5.7 Python 已稳定暴露 `BlueprintEditorLibrary.add_function_graph(...)`，这条路径本质上仍是 Blueprint 图结构编辑，适合收敛到本地 Python。
    - 返回结果已补齐为与现有 C++ 创建图命令一致的 `graph` 描述结构，便于调用侧继续复用。
  - `graph` / `macro`
    - 结合前面探针记录，UE5.7 Python 当前仍没有稳定暴露 `AddUbergraphPage(...)` / `AddMacroGraph(...)` 的等价入口。
    - 因此这条命令当前更适合收敛为 `Python + C++桥接`，而不是为了追求纯 Python 化去降级图类型覆盖面。

### 2026-04-04 第六十九批：set_actor_data_layers 迁移到本地Python

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `set_actor_data_layers`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPEditorCommands.cpp` 只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧通过 `DataLayerEditorSubsystem` 负责 Actor 解析、Data Layer 标识解析、旧归属清理、目标归属写入和结果校验。
- 本轮补充判断：
  - `set_actor_data_layers`
    - UE5.7 Python 已稳定暴露 `DataLayerEditorSubsystem.add_actor_to_data_layers(...)`、`remove_actor_from_all_data_layers(...)`、`get_all_data_layers()` 等正式接口。
  - 这条命令本质上是编辑器世界里的对象归属写入，链路短、失败边界清晰，并且可以通过 `Actor.get_data_layer_assets()` 做结果校验。
  - 因此本轮把它直接收敛为 `本地Python 主实现`，不再额外保留 C++ 业务逻辑。

### 2026-04-04 第七十七批：set_data_layer_state 补齐到本地Python

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `set_data_layer_state`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPEditorCommands.cpp` 只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧通过 `DataLayerCommandHelper` 负责 Data Layer 标识解析、状态写入与结果回读。
- 本轮补充判断：
  - `set_data_layer_state`
    - UE5.7 Python 已稳定暴露 `DataLayerEditorSubsystem.set_data_layer_visibility(...)`、`set_data_layer_is_loaded_in_editor(...)`、`set_data_layer_initial_runtime_state(...)`。
    - 这条命令本质上仍是编辑器世界里的轻量状态写入，不涉及 PIE/VR 状态切换、GC 或复杂事务控制，适合直接落到 `本地Python`。
    - 当前命令先收敛到 3 个稳定字段：`is_visible`、`is_loaded_in_editor`、`initial_runtime_state`；Data Layer 标识解析规则与 `set_actor_data_layers` 保持一致。

## 首批迁移建议

- `资产与 Content Browser`
  - 这部分 API 成熟，适合继续逐项评估。
  - 但涉及删除、卸载、引用清理、重定向器和包状态的能力，需要单独验证，不应直接假设 Python 足够稳定。

### 2026-04-04 第七十八批：remove_unused_blueprint_variables 补齐到本地Python

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/blueprint/blueprint_commands.py`：
  - `remove_unused_blueprint_variables`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPBlueprintCommands.cpp` 只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧通过 `BlueprintEditorLibrary.remove_unused_variables(...)` 负责清理未被任何 Blueprint 图引用的成员变量，并回传被删变量名列表。
- 本轮补充判断：
  - `remove_unused_blueprint_variables`
    - UE5.7 Python 已正式暴露 `BlueprintEditorLibrary.remove_unused_variables(...)`，可直接返回删除数量。
    - 这条命令本质上是 Blueprint 资产清理，不涉及 PIE/VR 运行控制、进程状态切换或复杂引擎生命周期，适合直接落到 `本地Python`。
    - UE5.7 Python 对 `Blueprint.NewVariables` 仍是受保护属性，因此当前稳定返回 `removed_count`，不额外承诺被删变量名列表。

- `UMG / UI`
  - WidgetTree、Slot 布局、层级构造、批量绑定等仍然适合优先评估本地 Python。
  - 但涉及编辑器持有对象、预览实例、复杂引用链时，需要保留回退到 C++ 的空间。

- `Blueprint 图节点 / Graph 编辑`
  - 图编辑仍是适合脚本化的一类能力。
  - 但凡是编译、重建、重实例化、父类变更等会触发较强编辑器状态变化的能力，不默认承诺纯 Python。

- `材质 / 数据资产 / Sequencer`
  - 这些仍是本地 Python 的重点候选域。
  - 具体是否迁移，仍以 API 覆盖和验证稳定性为准，而不是只看“理论上能不能调到”。

### 2026-04-04 第七十九批：create_curve 补齐到本地Python

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/assets/asset_commands.py`：
  - `create_curve`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPAssetCommands.cpp` 只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责 `curve_type` 归一化、工厂映射与标准资产创建结果封装。
- 本轮补充判断：
  - `create_curve`
    - UE5.7 Python 已稳定暴露 `CurveFloatFactory`、`CurveVectorFactory`、`CurveLinearColorFactory`，且现有 `AssetTools.create_asset(...)` 已能稳定创建这 3 类曲线资产。
    - 这条命令本质上仍是标准工厂创建，不涉及 PIE/VR 状态切换、GC、包卸载或复杂事务控制，适合直接落到 `本地Python`。
    - 为了避免把数据资产域继续折叠成“全都走 create_asset”，本轮补了显式 `create_curve` 命令，并把 `curve_type` 收敛为 `CurveFloat`、`CurveVector`、`CurveLinearColor` 三类稳定入口。

## 目录建议

- 本地 Python 脚本建议放在：
  - `Plugins/UnrealMCP/Content/Python/commands/`

- 建议按能力域拆分：
  - `Plugins/UnrealMCP/Content/Python/commands/assets/`
  - `Plugins/UnrealMCP/Content/Python/commands/blueprint/`
  - `Plugins/UnrealMCP/Content/Python/commands/umg/`
  - `Plugins/UnrealMCP/Content/Python/commands/material/`
  - `Plugins/UnrealMCP/Content/Python/commands/common/`

### 2026-04-04 第八十批：create_data_table 补齐到本地Python

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/assets/asset_commands.py`：
  - `create_data_table`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPAssetCommands.cpp` 只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责行结构解析、`DataTableFactory.Struct` 写入、标准资产创建与结果封装。
- 本轮补充判断：
  - `create_data_table`
    - UE5.7 Python 已稳定暴露 `DataTableFactory`，而且工厂的 `Struct` 属性可以直接由脚本写入，适合落在 `本地Python`。
    - 这条命令本质上仍是标准资产工厂创建，不涉及 PIE/VR 状态切换、GC、包卸载或复杂事务控制，链路短且失败边界清晰。
    - `row_struct` 当前同时支持 UE 原生结构体短名和用户自定义结构体对象路径，足以覆盖常见 DataTable 创建工作流。

### 2026-04-04 第八十一批：create_data_layer 补齐到本地Python

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/editor/editor_commands.py`：
  - `create_data_layer`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPEditorCommands.cpp` 只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧负责 `DataLayerAsset` 创建、`DataLayerCreationParameters` 组织、`DataLayerEditorSubsystem.create_data_layer_instance(...)` 调用与结果封装。
- 本轮补充判断：
  - `create_data_layer`
    - UE5.7 Python 已稳定暴露 `DataLayerFactory`、`DataLayerCreationParameters` 与 `DataLayerEditorSubsystem.create_data_layer_instance(...)`。
    - 这条命令本质上是“标准资产创建 + 编辑器 Data Layer 实例创建”的短链路组合，不涉及 PIE/VR 运行控制、GC、包卸载或复杂事务系统，适合直接落到 `本地Python`。
    - 当前先收敛为“常规 DataLayerAsset + 对应实例”这条稳定主路径；`private` 或 `ExternalDataLayer` 暂不承诺，避免把 UE5.7 Python 中未稳定验证的边界一并暴露出来。

### 2026-04-04 第八十二批：create_data_asset / create_primary_data_asset 补齐到本地Python

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/assets/asset_commands.py`：
  - `create_data_asset`
  - `create_primary_data_asset`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPAssetCommands.cpp` 只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧统一复用 `DataAssetFactory`，负责目标类解析、继承关系校验、工厂 `DataAssetClass` 写入与标准资产创建结果封装。
- 本轮补充判断：
  - `create_data_asset`
    - UE5.7 的 `DataAssetFactory` 已稳定暴露 `DataAssetClass` 属性，且工厂创建本身就是标准编辑器资产创建链路，适合直接落到 `本地Python`。
    - 目标类当前支持原生 `DataAsset` 短类名、完整类路径，以及继承自 `DataAsset` 的蓝图类路径。
  - `create_primary_data_asset`
    - 这条命令与普通 `DataAsset` 的创建主链路完全一致，差异只是“目标类必须继承自 `PrimaryDataAsset`”这一层契约校验。
    - 因此本轮没有拆成另一套实现，而是在同一条本地 Python 工厂链路上加了更严格的类约束，避免接口表面相似、落地行为却分叉。

### 2026-04-04 第八十三批：DataTable 行读写删补齐到本地Python

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/assets/asset_commands.py`：
  - `get_data_table_rows`
  - `set_data_table_row`
  - `remove_data_table_row`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPAssetCommands.cpp` 只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧通过 `DataTableCommandHelper` 负责 `DataTable` 资产解析、整表 JSON 导出/回填、单行合并与删除结果回读。
- 本轮补充判断：
  - `get_data_table_rows`
    - UE5.7 Python 已稳定暴露 `DataTable.export_to_json_string()` 与 `get_row_names()`，足以把整表行数据序列化为稳定 JSON 数组。
    - 这条命令本质上是纯查询，不涉及 PIE/VR 状态切换、GC、包卸载或复杂事务控制，适合直接落到 `本地Python`。
  - `set_data_table_row`
    - UE5.7 Python 虽然没有单独的“设置一行” API，但已稳定暴露 `fill_from_json_string()`。
    - 当前实现改为“先导出整表 JSON，再按 `row_name` 合并单行，最后整表回填”，链路短且验证成本低，仍适合保持 `本地Python` 主实现。
  - `remove_data_table_row`
    - UE5.7 Python 已稳定暴露 `DataTableFunctionLibrary.remove_data_table_row(...)`，可以直接删除单行并回读剩余行列表。
    - 该操作本质上仍是标准编辑器资产修改，不涉及运行态控制，适合继续留在 `本地Python`。

### 2026-04-04 第八十四批：DataTable 文件导入导出补齐到本地Python

- 已新增到 `Plugins/UnrealMCP/Content/Python/commands/assets/asset_commands.py`：
  - `import_data_table`
  - `export_data_table`
- 当前实现形态：
  - `Source/UnrealMCP/Private/Commands/UnrealMCPAssetCommands.cpp` 只保留 `ExecuteLocalPythonCommand(...)` 薄分发。
  - 本地 Python 侧复用 `DataTableCommandHelper`，负责文件路径规范化、CSV/JSON 格式推断、整表导入导出与结果回读。
- 本轮补充判断：
  - `import_data_table`
    - UE5.7 Python 已稳定暴露 `DataTable.fill_from_csv_file(...)`、`fill_from_json_file(...)`，同时 `DataTableFunctionLibrary` 也有同名文件导入入口，适合直接下沉到 `本地Python`。
    - 该命令本质上是编辑器资产整表覆盖写入，不涉及 PIE/VR 状态切换、GC、包卸载或复杂事务控制，链路短且验证成本低。
  - `export_data_table`
    - UE5.7 Python 已稳定暴露 `DataTable.export_to_csv_file(...)`、`export_to_json_file(...)`。
    - 这条命令本质上是查询与文件导出，不涉及运行态控制，适合继续保持 `本地Python` 主实现。
