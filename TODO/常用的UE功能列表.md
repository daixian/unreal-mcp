# 常用的UE功能列表

## 说明

- 本文基于当前仓库本地代码状态整理，范围限定为 `UnrealMCP` 插件应该覆盖的 UE 常用能力。
- 当前正式暴露的 MCP 工具共 `226` 个，来源于 `Python/tools` 的 7 组工具注册；C++ 侧还有 `create_actor` 兼容别名，但不建议当作正式工具名继续扩展。
- 标记说明：
  - `- [x]` 当前已实现，且 Python 工具注册与 C++ 命令分发都已存在。
  - `- [ ]` 当前未实现，建议补齐。
  - `- [ ] 建议增强` 表示已有基础能力，但还不够覆盖常见工作流。

## 1. 资产与 Content Browser

- [x] 资产搜索：`search_assets`（本地Python）
- [x] 资产搜索增强：`search_assets` 支持 `query_mode="contains|wildcard|regex"`（本地Python）
- [x] 查询资产元数据：`get_asset_metadata`（本地Python）
- [x] 查询资产依赖：`get_asset_dependencies`
- [x] 查询资产引用者：`get_asset_referencers`
- [x] 获取资产摘要：`get_asset_summary`
- [x] 获取 Blueprint 摘要：`get_blueprint_summary`
- [x] 保存资产：`save_asset`（本地Python）
- [x] 创建目录：`make_directory`（本地Python）
- [x] 复制资产：`duplicate_asset`（本地Python）
- [x] 重命名资产：`rename_asset`（本地Python）
- [x] 移动资产：`move_asset`（本地Python）
- [x] 删除资产：`delete_asset`（本地Python）
- [x] 创建通用资产：`create_asset`（本地Python）
- [x] 导入外部资源：`import_asset`（本地Python）
- [x] 重新导入资源：`reimport_asset`（Python+C++桥接）
- [x] 导出资源：`export_asset`（本地Python）
- [x] 修复重定向器：`fixup_redirectors`
- [x] 合并重复资产：`consolidate_assets`（本地Python）
- [x] 批量替换引用：`replace_asset_references`（本地Python）
- [x] 设置资产标签和元数据：`set_asset_metadata`（本地Python）
- [x] 同步 Content Browser 选中状态：`sync_content_browser_to_assets`（本地Python）
- [x] 获取当前选中资产：`get_selected_assets`（本地Python）
- [x] 保存全部脏资产：`save_all_dirty_assets`（本地Python）
- [x] 资产签出：`checkout_asset`（本地Python）
- [x] 资产提交/还原：`submit_asset`、`revert_asset`（本地Python）
- [x] 查询源码控制状态：`get_source_control_status`（本地Python）
- [x] 批量重命名与批量移动：`batch_rename_assets`、`batch_move_assets`（本地Python）

## 2. 关卡 / World / Actor

- [x] 加载关卡：`load_level`（本地Python）
- [x] 保存当前关卡：`save_current_level`（本地Python）
- [x] 列出关卡 Actor：`get_actors_in_level`（本地Python）
- [x] 按名称查找 Actor：`find_actors_by_name`（本地Python）
- [x] `find_actors_by_name` 已支持类过滤、标签过滤、路径过滤与排序增强（本地Python）
- [x] 生成原生 Actor：`spawn_actor`（本地Python）
- [x] 删除 Actor：`delete_actor`（本地Python）
- [x] 设置 Actor Transform：`set_actor_transform`（本地Python）
- [x] 设置 Actor 属性：`set_actor_property`（本地Python）
- [x] 获取 Actor 属性：`get_actor_properties`（本地Python）
- [x] 获取单个 Actor 组件：`get_actor_components`（本地Python）
- [x] 获取场景组件总览：`get_scene_components`（本地Python）
- [x] 聚焦视口：`focus_viewport`（本地Python）
- [x] 视口截图：`take_screenshot`（C++主实现）
- [x] 生成 Blueprint Actor：`spawn_blueprint_actor`（本地Python）
- [x] `spawn_blueprint_actor` 已支持任意目录 Blueprint 的唯一短名解析（本地Python）
- [x] 复制 Actor：`duplicate_actor`（本地Python）
- [x] 选中 Actor：`select_actor`（Python+C++桥接）
- [x] 获取当前选中 Actor：`get_selected_actors`（本地Python）
- [x] 按类、Tag、Folder、Layer 过滤 Actor：`find_actors`（本地Python）
- [x] 设置 Actor 标签：`set_actor_tags`（本地Python）
- [x] 设置 Actor Folder：`set_actor_folder_path`（本地Python）
- [x] 设置 Actor 可见性/隐藏状态：`set_actor_visibility`（本地Python）
- [x] 设置 Actor Mobility：`set_actor_mobility`（本地Python）
- [x] Attach / Detach Actor：`attach_actor`、`detach_actor`（本地Python）
- [x] 在关卡实例上添加组件：`add_component_to_actor`（本地Python）
- [x] 在关卡实例上移除组件：`remove_component_from_actor`（本地Python）
- [x] 在关卡中按类路径生成对象：`spawn_actor_from_class`（本地Python）
- [x] 批量设置 Transform：`set_actors_transform`（本地Python）
- [x] 获取 World Settings：`get_world_settings`（本地Python）
- [x] 设置 World Settings：`set_world_settings`（本地Python）
- [x] 执行 LineTrace / BoxTrace / SphereTrace：`line_trace`、`box_trace`、`sphere_trace`（本地Python）
- [x] 获取鼠标命中结果：`get_hit_result_under_cursor`（本地Python，当前仅支持 PIE/VR Preview）
- [x] 读取数据层列表：`get_data_layers`（本地Python）
- [x] 创建 Data Layer：`create_data_layer`（本地Python）
- [x] 设置 Actor 数据层：`set_actor_data_layers`（本地Python）
- [x] 设置 Data Layer 状态：`set_data_layer_state`（本地Python）
- [ ] 操作 World Partition：`load_world_partition_cells`、`unload_world_partition_cells`
- [ ] 构建导航、光照、HLOD：`build_navigation`、`build_lighting`、`build_hlod`
- [x] `spawn_actor` 已支持 `class_path`、`world_type`、`template_actor` 增强（当前 `world_type` 仅支持 `editor`）

## 3. Blueprint 资产 / 类 / 组件

- [x] 创建 Blueprint：`create_blueprint`（本地Python）
- [x] 给 Blueprint 添加组件：`add_component_to_blueprint`（本地Python）
- [x] 设置组件属性：`set_component_property`（本地Python）
- [x] 设置静态网格：`set_static_mesh_properties`（本地Python）
- [x] 设置物理参数：`set_physics_properties`（本地Python）
- [x] 编译 Blueprint：`compile_blueprint`（本地Python）
- [x] 批量编译 Blueprint：`compile_blueprints`（本地Python）
- [x] 清理重设父类后的残留节点：`cleanup_blueprint_for_reparent`（C++主实现）
- [x] 设置 Blueprint 默认属性：`set_blueprint_property`（本地Python）
- [x] 批量设置 Pawn 常用属性：`set_pawn_properties`（本地Python）
- [x] 设置 GameMode 默认 Pawn：`set_game_mode_default_pawn`（本地Python）
- [x] Blueprint 改父类：`reparent_blueprint`（本地Python）
- [x] 创建子 Blueprint：`create_child_blueprint`（本地Python）
- [x] 重命名 Blueprint 成员：`rename_blueprint_member`（Python+C++桥接）
- [x] 删除 Blueprint 变量：`delete_blueprint_variable`（C++主实现）
- [x] 清理未使用 Blueprint 变量：`remove_unused_blueprint_variables`（本地Python）
- [x] 设置 Blueprint 变量默认值：`set_blueprint_variable_default`（本地Python，现已支持刚新增变量的预编译刷新）
- [x] 添加 Blueprint 函数：`add_blueprint_function`（本地Python）
- [x] 删除 Blueprint 函数：`delete_blueprint_function`（本地Python）
- [x] 添加 Macro：`add_blueprint_macro`
- [x] 添加 Interface：`add_blueprint_interface`（C++主实现）
- [ ] 实现 Interface：`implement_blueprint_interface`
- [ ] 添加 Event Dispatcher：`add_event_dispatcher`
- [ ] 添加 Timeline：`add_timeline_to_blueprint`
- [x] 删除组件：`remove_component_from_blueprint`（本地Python）
- [x] 设置组件父子关系和 Socket：`attach_component_in_blueprint`（本地Python）
- [ ] 设置继承组件覆写：`set_inherited_component_override`
- [x] 获取 Blueprint 编译错误：`get_blueprint_compile_errors`（C++主实现）
- [x] 保存 Blueprint：`save_blueprint`（本地Python）
- [x] 打开 Blueprint 编辑器：`open_blueprint_editor`（本地Python）
- [x] `create_blueprint` 已支持显式路径与父类完整类路径，并会拦截废弃父类/错误类型
- [ ] 建议增强：`add_component_to_blueprint` 增加事务回滚信息

## 4. Blueprint 图节点 / Graph 编辑

- [x] 添加事件节点：`add_blueprint_event_node`
- [x] 添加输入动作事件：`add_blueprint_input_action_node`
- [x] 添加函数调用节点：`add_blueprint_function_node`
- [x] 连接节点：`connect_blueprint_nodes`
- [x] 添加 Blueprint 变量：`add_blueprint_variable`（本地Python）
- [x] 添加组件引用节点：`add_blueprint_get_self_component_reference`
- [x] 添加 Self 引用节点：`add_blueprint_self_reference`
- [x] 查找节点：`find_blueprint_nodes`
- [x] 通用节点生成：`spawn_blueprint_node`
- [x] 读取节点详情：`describe_blueprint_node`
- [x] 设置 Pin 默认值：`set_blueprint_pin_default`
- [x] 删除节点：`delete_blueprint_node`
- [x] 断开节点连接：`disconnect_blueprint_nodes`
- [x] 移动节点：`move_blueprint_node`
- [x] 批量移动和对齐节点：`layout_blueprint_nodes`
- [x] 添加注释节点：`add_blueprint_comment_node`
- [x] 添加 Reroute 节点：`add_blueprint_reroute_node`
- [x] 新建 Graph / Function Graph / Macro Graph：`create_blueprint_graph`（Python+C++桥接）
- [x] 删除 Graph：`delete_blueprint_graph`（本地Python）
- [x] 创建 Branch 节点：`add_branch_node`
- [x] 创建 Sequence 节点：`add_sequence_node`
- [x] 创建 Delay 节点：`add_delay_node`
- [x] 创建 Gate 节点：`add_gate_node`
- [x] 创建 Loop 节点：`add_for_loop_node`、`add_for_each_loop_node`
- [x] 创建 Cast 节点：`add_cast_node`
- [x] 创建变量 Get / Set 节点：`add_variable_get_node`、`add_variable_set_node`
- [x] 创建 Struct Make / Break 节点：`add_make_struct_node`、`add_break_struct_node`
- [x] 创建 Enum Switch / Select 节点：`add_switch_on_enum_node`、`add_select_node`
- [x] 折叠为函数/宏：`collapse_nodes_to_function`、`collapse_nodes_to_macro`
- [x] 复制节点子图：`duplicate_blueprint_subgraph`
- [x] 校验图可编译性：`validate_blueprint_graph`
- [ ] 建议增强：`spawn_blueprint_node` 增加更多语义节点类型、自动连线、Wildcard Pin 支持
- [ ] 建议增强：`find_blueprint_nodes` 增加按函数名、Widget Graph、Pin 条件过滤

## 5. 项目设置 / 输入系统

- [x] 创建传统输入映射：`create_input_mapping`（本地Python）
- [x] 列出输入映射：`list_input_mappings`（本地Python）
- [x] 删除输入映射：`remove_input_mapping`（本地Python）
- [x] 创建 Axis Mapping：`create_input_axis_mapping`（本地Python）
- [x] 创建 Enhanced Input Action：`create_input_action_asset`（本地Python）
- [x] 创建 Input Mapping Context：`create_input_mapping_context`（本地Python）
- [x] 往 Context 中添加按键映射：`add_mapping_to_context`（本地Python）
- [x] 给 Pawn / Controller 分配 Mapping Context：`assign_mapping_context`（本地Python）
- [x] 读取项目设置：`get_project_setting`（本地Python）
- [x] 写入项目设置：`set_project_setting`（InputSettings=本地Python，GameMapsSettings=C++）
- [x] 设置默认地图：`set_default_maps`
- [x] 设置 GameInstance / GameMode / HUD / PlayerController：`set_game_framework_defaults`
- [ ] 启用 / 禁用插件：`enable_plugin`、`disable_plugin`
- [ ] 增加模块依赖：`add_module_dependency`
- [ ] 重新生成项目文件：`regenerate_project_files`
- [ ] 编辑打包设置：`set_packaging_settings`

## 6. UMG / UI

- [x] 创建 Widget Blueprint：`create_umg_widget_blueprint`（本地Python）
- [x] 添加 TextBlock：`add_text_block_to_widget`（本地Python）
- [x] 添加 Button：`add_button_to_widget`（本地Python）
- [x] 绑定 Widget 事件：`bind_widget_event`（C++主实现）
- [x] 添加到 Viewport：`add_widget_to_viewport`（本地Python）
- [x] 从 Viewport 移除运行时实例：`remove_widget_from_viewport`（本地Python）
- [x] 设置 TextBlock 绑定：`set_text_block_binding`（C++主实现）
- [x] 添加 Image：`add_image_to_widget`（本地Python）
- [x] 添加 Border：`add_border_to_widget`（本地Python）
- [x] 添加 CanvasPanel：`add_canvas_panel_to_widget`（本地Python）
- [x] 添加 HorizontalBox：`add_horizontal_box_to_widget`（本地Python）
- [x] 添加 VerticalBox：`add_vertical_box_to_widget`（本地Python）
- [x] 添加 Overlay：`add_overlay_to_widget`（本地Python）
- [x] 添加 ScrollBox：`add_scroll_box_to_widget`（本地Python）
- [x] 添加 SizeBox：`add_size_box_to_widget`（本地Python）
- [x] 添加 Spacer：`add_spacer_to_widget`（本地Python）
- [x] 添加 ProgressBar：`add_progress_bar_to_widget`（本地Python）
- [x] 添加 Slider：`add_slider_to_widget`（本地Python）
- [x] 添加 CheckBox：`add_check_box_to_widget`（本地Python）
- [x] 添加 EditableText：`add_editable_text_to_widget`（本地Python）
- [x] 添加 RichText：`add_rich_text_to_widget`（本地Python）
- [x] 添加 MultiLineText：`add_multi_line_text_to_widget`（本地Python）
- [x] 创建 Named Slot：`add_named_slot_to_widget`（本地Python）
- [x] 添加 ListView / TileView / TreeView：`add_list_view_to_widget`、`add_tile_view_to_widget`、`add_tree_view_to_widget`（本地Python）
- [x] 删除 Widget：`remove_widget_from_blueprint`（本地Python）
- [x] 设置 Slot 布局：`set_widget_slot_layout`（本地Python）
- [x] 设置 Widget 可见性：`set_widget_visibility`（本地Python）
- [x] 设置 Widget 样式：`set_widget_style`（本地Python）
- [x] 设置 Brush / Texture / Material：`set_widget_brush`（本地Python）
- [x] 创建 Widget 动画：`create_widget_animation`（C++主实现）
- [x] 添加动画关键帧：`add_widget_animation_keyframe`（C++主实现）
- [x] 绑定任意属性：`bind_widget_property`（C++主实现）
- [x] 打开 Widget Blueprint 编辑器：`open_widget_blueprint_editor`（本地Python）
- [x] `add_widget_to_viewport` 已支持 `player_index` 形式的 `owning_player` 绑定与实例句柄返回（本地Python）
- [ ] 建议增强：`get_blueprint_summary` 输出更完整的 WidgetTree、Binding、Animation 摘要

## 7. 编辑器运行控制 / PIE / 调试

- [x] 启动 PIE：`start_pie`
- [x] 启动 VR Preview：`start_vr_preview`
- [x] 停止 PIE：`stop_pie`
- [x] 获取 PIE 状态：`get_play_state`
- [x] 启动 Live Coding：`start_live_coding`
- [x] 触发 Live Coding 编译：`compile_live_coding`
- [x] 获取 Live Coding 状态：`get_live_coding_state`
- [x] 启动 Standalone Game：`start_standalone_game`
- [x] 获取编辑器当前选择：`get_editor_selection`（本地Python）
- [x] 打开任意资产编辑器：`open_asset_editor`（本地Python）
- [x] 关闭资产编辑器标签：`close_asset_editor`（本地Python）
- [x] 执行控制台命令：`execute_console_command`（本地Python）
- [x] 执行 Unreal Python：`execute_unreal_python`
- [x] 运行 Editor Utility Widget / Blueprint：`run_editor_utility_widget`、`run_editor_utility_blueprint`（本地Python）
- [x] 读取 Output Log：`get_output_log`
- [x] 清空 Output Log：`clear_output_log`
- [x] 读取 Message Log：`get_message_log`
- [x] 发送编辑器通知：`show_editor_notification`
- [x] 同步视口显示模式：`set_viewport_mode`（本地Python）
- [x] 获取视口摄像机信息：`get_viewport_camera`（本地Python）
- [x] 录制高分辨率截图或序列帧：`take_highres_screenshot`、`capture_viewport_sequence`
- [ ] 建议增强：`start_pie` 增加地图覆盖、玩家数量、网络模式、窗口模式
- [x] `take_screenshot` 已支持 `resolution`、`show_ui`、`transparent_background`、`viewport_index` 增强（C++主实现）

## 8. 材质 / 渲染 / 灯光

- [x] 创建材质：`create_material`（本地Python）
- [x] 创建 Material Function：`create_material_function`（本地Python）
- [x] 创建 Render Target：`create_render_target`（本地Python）
- [x] 创建材质实例：`create_material_instance`（本地Python）
- [x] 读取材质参数：`get_material_parameters`（本地Python）
- [x] 设置材质实例参数：`set_material_instance_scalar_parameter`、`set_material_instance_vector_parameter`、`set_material_instance_texture_parameter`（本地Python）
- [x] 给 Actor / 组件赋材质：`assign_material_to_actor`、`assign_material_to_component`（本地Python）
- [x] 替换指定材质槽：`replace_material_slot`（本地Python）
- [x] 添加材质表达式节点：`add_material_expression`（本地Python）
- [x] 连接材质表达式：`connect_material_expressions`（本地Python）
- [x] 自动整理材质图：`layout_material_graph`（本地Python）
- [x] 编译材质：`compile_material`（本地Python）
- [x] 场景捕捉到 Render Target：`capture_scene_to_render_target`（本地Python）
- [x] 设置 PostProcessVolume 参数：`set_post_process_settings`（本地Python）
- [x] 创建和调整灯光 Actor：`create_light`、`set_light_properties`（本地Python）

## 9. 骨骼 / 动画 / 角色

- [ ] 设置 SkeletalMesh：`set_skeletal_mesh`
- [ ] 设置 AnimInstance：`set_anim_instance_class`
- [ ] 创建 Animation Blueprint：`create_animation_blueprint`
- [ ] 创建 BlendSpace：`create_blend_space`
- [ ] 创建 Montage：`create_animation_montage`
- [ ] 添加或修改 Socket：`add_socket_to_skeleton`、`set_socket_transform`
- [ ] Retarget 动画：`retarget_animations`
- [ ] 配置 CharacterMovement：`set_character_movement_properties`
- [ ] 配置 CameraBoom / FollowCamera：`setup_third_person_camera`
- [ ] 生成基础角色控制蓝图：`setup_character_input_graph`

## 10. Sequencer / 电影化

- [ ] 创建 Level Sequence：`create_level_sequence`
- [ ] 打开 Level Sequence：`open_level_sequence`
- [ ] 往 Sequencer 添加 Actor 绑定：`add_actor_to_sequence`
- [ ] 添加 Track / Section：`add_sequence_track`、`add_sequence_section`
- [ ] 添加关键帧：`add_sequence_key`
- [ ] 设置播放范围：`set_sequence_playback_range`
- [ ] 控制 Sequencer 播放：`play_sequence`、`pause_sequence`、`stop_sequence`
- [ ] 渲染 Movie Render Queue：`render_movie_queue`

## 11. Niagara / VFX

- [ ] 创建 Niagara System：`create_niagara_system`
- [ ] 创建 Niagara Emitter：`create_niagara_emitter`
- [ ] 在关卡中生成 Niagara Actor / Component：`spawn_niagara_system`
- [ ] 设置 Niagara 参数：`set_niagara_parameter`
- [ ] 绑定 Niagara 到 Actor：`attach_niagara_to_actor`
- [ ] 打开 Niagara 编辑器：`open_niagara_editor`

## 12. 音频

- [ ] 导入音频资源：`import_sound`
- [ ] 创建 SoundCue：`create_sound_cue`
- [ ] 创建 MetaSound：`create_meta_sound`
- [ ] 设置 AudioComponent：`set_audio_component_properties`
- [ ] 在关卡中放置声音：`spawn_audio_actor`
- [ ] 设置衰减、并发、子混音：`set_sound_attenuation`、`set_sound_concurrency`、`set_sound_submix`

## 13. 数据资产 / 表格 / 配置

- [x] 创建 DataAsset：`create_data_asset`（本地Python）
- [x] 创建 PrimaryDataAsset：`create_primary_data_asset`（本地Python）
- [x] 创建 DataTable：`create_data_table`（本地Python）
- [x] 读取 DataTable 行：`get_data_table_rows`（本地Python）
- [x] 写入 DataTable 行：`set_data_table_row`（本地Python）
- [x] 删除 DataTable 行：`remove_data_table_row`（本地Python）
- [x] 导入 / 导出 CSV、JSON：`import_data_table`、`export_data_table`（本地Python）
- [x] 创建 Curve 资产：`create_curve`（本地Python）
- [ ] 设置 Curve / CurveTable：`set_curve_keys`
- [ ] 读取和修改 Config：`get_config_value`、`set_config_value`

## 14. Landscape / Foliage / PCG / 开放世界

- [ ] 创建 Landscape：`create_landscape`
- [ ] 导入高度图：`import_heightmap`
- [ ] 设置 Landscape 材质：`set_landscape_material`
- [ ] 涂层与地形编辑：`paint_landscape_layer`、`sculpt_landscape`
- [ ] 放置 Foliage：`add_foliage_instances`
- [ ] 创建 PCG Graph：`create_pcg_graph`
- [ ] 运行 PCG：`execute_pcg_graph`
- [ ] 设置 World Partition Streaming：`set_world_partition_streaming`
- [x] 管理 Data Layer：`create_data_layer`（本地Python）

## 15. AI / 导航 / 玩法系统

- [ ] 创建 Behavior Tree：`create_behavior_tree`
- [ ] 创建 Blackboard：`create_blackboard`
- [ ] 配置 AIController：`set_ai_controller_class`
- [ ] 获取导航路径：`find_nav_path`
- [ ] 触发导航重建：`rebuild_navigation`
- [ ] 放置 NavMeshBoundsVolume：`create_nav_mesh_bounds_volume`
- [ ] 配置感知系统：`setup_ai_perception`
- [ ] 创建 GameplayTag：`create_gameplay_tag`
- [ ] 配置 AbilitySystem 常用资产：`create_gameplay_effect`、`create_gameplay_ability`

## 16. 自动化 / 测试 / 打包 / 工程协作

- [ ] 运行 Automation Test：`run_automation_test`
- [ ] 执行命令行 Commandlet：`run_commandlet`
- [ ] 收集编译与运行错误：`collect_editor_errors`
- [ ] 收集 Crash / Ensure / Warning 摘要：`collect_runtime_issues`
- [ ] 构建项目：`build_project`
- [ ] Cook 内容：`cook_project`
- [ ] 打包项目：`package_project`
- [ ] 生成补丁：`build_patch`
- [ ] 清理 DerivedDataCache / Intermediate：`clean_project_artifacts`
- [ ] 签出 / 提交 / 回滚文件：`checkout_files`、`submit_files`、`revert_files`
- [ ] 获取改动文件依赖影响：`analyze_asset_change_impact`
- [x] 导出当前 MCP 工具清单：`list_mcp_tools`
- [x] 导出命令 Schema：`export_tool_schema`

## 建议优先级

### P0：应该尽快补齐

- 资产改名、移动、删除、导入、重导入。
- 选中 Actor / 资产、复制 Actor、Attach / Detach、批量设置 Transform。
- Blueprint 改父类、删除变量、设置变量默认值、获取编译错误。
- 常用 Blueprint 流程节点与变量 Get/Set 节点。
- UMG 常见容器和控件：`Image`、`CanvasPanel`、`HorizontalBox`、`VerticalBox`、`ScrollBox`、`ProgressBar`、`EditableText`。
- 编辑器控制：执行控制台命令、读取 Output Log、打开资产编辑器、保存全部脏资产。

### P1：常见项目很快会需要

- Enhanced Input 全套能力。
- 材质实例参数修改与赋材质。
- Sequencer 基础编辑。
- 数据资产、DataTable、配置读写。
- Landscape / Foliage / PCG 基础能力。

### P2：中大型项目会需要

- 动画、Niagara、音频、AI、打包、源码控制。
- World Partition、Data Layer、Movie Render Queue。
- 自动化测试、命令行 Commandlet、问题收集与诊断工具。
