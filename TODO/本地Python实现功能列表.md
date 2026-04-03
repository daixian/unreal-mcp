# 本地Python实现功能列表

## 说明

- 本文基于 `Plugins/UnrealMCP/TODO/常用的UE功能列表.md` 当前内容整理。
- 这里的“本地 Python”指的是放在 `Plugins/UnrealMCP/Content/Python/` 下，由 Unreal 编辑器内 `PythonScriptPlugin` 执行的脚本，不是 `Plugins/UnrealMCP/Python/` 里的外部 MCP 服务端代码。
- 迁移原则按当前决策执行：原则上只要能用 Python 实现的功能，都优先改为 Python 实现；C++ 尽量只保留命令分发、参数校验、结果封装、少量引擎级桥接。
- `execute_unreal_python` 本身属于基础执行能力，不是要被迁移掉的目标，而是后续本地 Python 化的底层入口之一。

## 迁移分层

### A. 适合直接迁移为本地Python实现

- 特点：
  - 主要在编辑器内操作资产、蓝图、WidgetTree、Sequencer、材质图等对象模型。
  - 逻辑更偏反射式编辑、批处理、图编辑和资产重构。
  - Python API 足够覆盖，且改成脚本后通常更灵活、更短、更容易迭代。

## 1. 资产与 Content Browser

- `search_assets`
- `get_asset_metadata`
- `get_asset_dependencies`
- `get_asset_referencers`
- `get_asset_summary`
- `get_blueprint_summary`
- `save_asset`
- `make_directory`
- `duplicate_asset`
- `rename_asset`
- `move_asset`
- `delete_asset`
- `create_asset`
- `import_asset`
- `reimport_asset`
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
- `search_assets` 的通配符/正则增强

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

### B. 适合“Python 为主实现 + 薄 C++ 外壳保留”的功能

- 特点：
  - 底层执行逻辑可以放 Python。
  - 但仍建议保留 C++ 命令壳，负责参数校验、编辑器上下文解析、结果封装，避免工具契约直接退化成“随便传一段脚本”。

- `start_pie`
- `start_vr_preview`
- `stop_pie`
- `get_play_state`
- `start_live_coding`
- `compile_live_coding`
- `get_live_coding_state`
- `start_standalone_game`
- `start_pie` 的地图覆盖/玩家数量/网络模式/窗口模式增强

### C. 暂不建议优先迁到插件内本地Python的功能

- 特点：
  - 这些能力虽然理论上可以由 Python 间接调用外部命令或编辑配置文件实现，但它们更偏工程命令行、系统环境或外部工具链，不适合优先做成“编辑器内本地 Python 脚本”。
  - 更适合保留为外部流程、命令行工具，或后续单独设计专用执行层。

- `regenerate_project_files`
- `build_project`
- `cook_project`
- `package_project`
- `build_patch`
- `clean_project_artifacts`

## 首批迁移建议

- `UMG / UI`
  - 这部分最适合先迁到本地 Python，尤其是 WidgetTree 重构、Slot 布局、容器层级调整、动画和绑定这类逻辑。

- `Blueprint 图节点 / Graph 编辑`
  - 图编辑本身就很适合 Python 反射式操作，脚本化后也更容易做批量修改和回读校验。

- `资产与 Content Browser`
  - 这部分 API 成熟，Python 覆盖高，迁移风险较低。

- `材质 / 数据资产 / Sequencer`
  - 都是典型“编辑器对象模型操作”，很适合放到本地 Python 脚本。

## 目录建议

- 本地 Python 脚本建议放在：
  - `Plugins/UnrealMCP/Content/Python/unreal_mcp/`

- 建议按能力域拆分：
  - `Plugins/UnrealMCP/Content/Python/unreal_mcp/assets/`
  - `Plugins/UnrealMCP/Content/Python/unreal_mcp/blueprint/`
  - `Plugins/UnrealMCP/Content/Python/unreal_mcp/umg/`
  - `Plugins/UnrealMCP/Content/Python/unreal_mcp/material/`
  - `Plugins/UnrealMCP/Content/Python/unreal_mcp/common/`
