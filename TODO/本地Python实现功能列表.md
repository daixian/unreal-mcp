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
- `set_actor_property`
- `get_actor_properties`
- `get_actor_components`
- `get_scene_components`
- `focus_viewport`
- `take_screenshot`
- `spawn_blueprint_actor`
- `duplicate_actor`
- `select_actor`
- `get_selected_actors`
- `find_actors`
- `set_actor_tags`
- `set_actor_folder_path`
- `set_actor_visibility`
- `set_actor_mobility`
- `attach_actor`
- `detach_actor`
- `add_component_to_actor`
- `remove_component_from_actor`
- `spawn_actor_from_class`
- `set_actors_transform`
- `get_world_settings`
- `set_world_settings`
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
- `run_editor_utility_widget`
- `run_editor_utility_blueprint`
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
- `capture_scene_to_render_target`
- `set_post_process_settings`
- `create_light`
- `set_light_properties`

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
