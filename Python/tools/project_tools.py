"""Project tools for Unreal MCP."""

import logging
from typing import Any, Dict, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")


def _send_project_command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
    from unreal_mcp_server import get_unreal_connection

    unreal = get_unreal_connection()
    if not unreal:
        logger.error("连接 Unreal Engine 失败")
        return {"success": False, "message": "Failed to connect to Unreal Engine"}

    response = unreal.send_command(command_name, params)
    if not response:
        logger.error("Unreal Engine 未返回结果: %s", command_name)
        return {"success": False, "message": "No response from Unreal Engine"}

    return response

def register_project_tools(mcp: FastMCP):
    """Register project tools with the MCP server."""
    
    @mcp.tool()
    def create_input_mapping(
        ctx: Context,
        action_name: str,
        key: str,
        input_type: str = "Action",
        scale: float = 1.0,
        shift: bool = False,
        ctrl: bool = False,
        alt: bool = False,
        cmd: bool = False
    ) -> Dict[str, Any]:
        """
        Create an input mapping for the project.
        
        Args:
            action_name: Name of the input action
            key: Key to bind (SpaceBar, LeftMouseButton, etc.)
            input_type: Type of input mapping (Action or Axis)
            scale: Axis scale, only used when input_type is Axis
            shift: Whether Shift modifier is required for Action mappings
            ctrl: Whether Ctrl modifier is required for Action mappings
            alt: Whether Alt modifier is required for Action mappings
            cmd: Whether Cmd modifier is required for Action mappings
            
        Returns:
            Response indicating success or failure
        """
        try:
            params = {
                "action_name": action_name,
                "key": key,
                "input_type": input_type
            }

            if input_type.lower() == "axis":
                params["scale"] = scale
            else:
                params["shift"] = shift
                params["ctrl"] = ctrl
                params["alt"] = alt
                params["cmd"] = cmd

            logger.info("创建输入映射: %s -> %s", action_name, key)
            return _send_project_command("create_input_mapping", params)
        except Exception as e:
            error_msg = f"Error creating input mapping: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def create_input_axis_mapping(
        ctx: Context,
        mapping_name: str,
        key: str,
        scale: float = 1.0
    ) -> Dict[str, Any]:
        """
        Create an Axis input mapping for the project.

        Args:
            mapping_name: Name of the axis mapping
            key: Key to bind
            scale: Axis scale

        Returns:
            Response indicating success or failure
        """
        try:
            logger.info("创建轴向输入映射: %s -> %s", mapping_name, key)
            return _send_project_command(
                "create_input_axis_mapping",
                {
                    "mapping_name": mapping_name,
                    "key": key,
                    "scale": scale,
                },
            )
        except Exception as e:
            error_msg = f"Error creating axis input mapping: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def list_input_mappings(
        ctx: Context,
        input_type: str = "All",
        mapping_name: str = "",
        key: str = ""
    ) -> Dict[str, Any]:
        """
        List input mappings in the project.

        Args:
            input_type: Mapping type filter (All, Action, Axis)
            mapping_name: Optional mapping name filter
            key: Optional key filter

        Returns:
            Response containing matching input mappings
        """
        try:
            params: Dict[str, Any] = {"input_type": input_type}
            if mapping_name:
                params["mapping_name"] = mapping_name
            if key:
                params["key"] = key

            logger.info("列出输入映射: type=%s name=%s key=%s", input_type, mapping_name, key)
            return _send_project_command("list_input_mappings", params)
        except Exception as e:
            error_msg = f"Error listing input mappings: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def remove_input_mapping(
        ctx: Context,
        mapping_name: str,
        input_type: str = "Action",
        key: str = "",
        scale: Optional[float] = None,
        shift: Optional[bool] = None,
        ctrl: Optional[bool] = None,
        alt: Optional[bool] = None,
        cmd: Optional[bool] = None
    ) -> Dict[str, Any]:
        """
        Remove input mappings from the project.

        Args:
            mapping_name: Name of the mapping to remove
            input_type: Mapping type (Action or Axis)
            key: Optional key filter
            scale: Optional axis scale filter
            shift: Optional Action Shift modifier filter
            ctrl: Optional Action Ctrl modifier filter
            alt: Optional Action Alt modifier filter
            cmd: Optional Action Cmd modifier filter

        Returns:
            Response indicating success or failure
        """
        try:
            params: Dict[str, Any] = {
                "mapping_name": mapping_name,
                "input_type": input_type,
            }

            if key:
                params["key"] = key
            if scale is not None:
                params["scale"] = scale
            if shift is not None:
                params["shift"] = shift
            if ctrl is not None:
                params["ctrl"] = ctrl
            if alt is not None:
                params["alt"] = alt
            if cmd is not None:
                params["cmd"] = cmd

            logger.info("删除输入映射: type=%s name=%s key=%s", input_type, mapping_name, key)
            return _send_project_command("remove_input_mapping", params)
        except Exception as e:
            error_msg = f"Error removing input mapping: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def create_input_action_asset(
        ctx: Context,
        action_name: str,
        path: str = "/Game/Input",
        value_type: str = "Boolean",
        consume_input: bool = True,
        trigger_when_paused: bool = False,
        consume_legacy_keys: bool = False,
        accumulation_behavior: str = "TakeHighestAbsoluteValue"
    ) -> Dict[str, Any]:
        """
        Create an Enhanced Input Action asset.

        Args:
            action_name: Name of the Input Action asset
            path: Content Browser path for the asset
            value_type: Input value type (Boolean, Axis1D, Axis2D, Axis3D)
            consume_input: Whether to consume lower-priority mappings
            trigger_when_paused: Whether the action can trigger while paused
            consume_legacy_keys: Whether to consume legacy Action/Axis mappings
            accumulation_behavior: How multiple mappings are accumulated

        Returns:
            Response indicating success or failure
        """
        try:
            logger.info("创建 Enhanced Input Action: %s", action_name)
            return _send_project_command(
                "create_input_action_asset",
                {
                    "action_name": action_name,
                    "path": path,
                    "value_type": value_type,
                    "consume_input": consume_input,
                    "trigger_when_paused": trigger_when_paused,
                    "consume_legacy_keys": consume_legacy_keys,
                    "accumulation_behavior": accumulation_behavior,
                },
            )
        except Exception as e:
            error_msg = f"Error creating input action asset: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def create_input_mapping_context(
        ctx: Context,
        context_name: str,
        path: str = "/Game/Input",
        description: str = "",
        registration_tracking_mode: str = "Untracked"
    ) -> Dict[str, Any]:
        """
        Create an Enhanced Input Mapping Context asset.

        Args:
            context_name: Name of the Mapping Context asset
            path: Content Browser path for the asset
            description: Optional localized description text
            registration_tracking_mode: Registration tracking mode

        Returns:
            Response indicating success or failure
        """
        try:
            logger.info("创建 Input Mapping Context: %s", context_name)
            return _send_project_command(
                "create_input_mapping_context",
                {
                    "context_name": context_name,
                    "path": path,
                    "description": description,
                    "registration_tracking_mode": registration_tracking_mode,
                },
            )
        except Exception as e:
            error_msg = f"Error creating input mapping context: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_mapping_to_context(
        ctx: Context,
        mapping_context: str,
        input_action: str,
        key: str
    ) -> Dict[str, Any]:
        """
        Add a key mapping to an Enhanced Input Mapping Context.

        Args:
            mapping_context: Mapping Context asset path or unique asset name
            input_action: Input Action asset path or unique asset name
            key: Key to bind

        Returns:
            Response indicating success or failure
        """
        try:
            logger.info("往 Context 添加映射: %s <- %s (%s)", mapping_context, input_action, key)
            return _send_project_command(
                "add_mapping_to_context",
                {
                    "mapping_context": mapping_context,
                    "input_action": input_action,
                    "key": key,
                },
            )
        except Exception as e:
            error_msg = f"Error adding mapping to context: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def assign_mapping_context(
        ctx: Context,
        mapping_context: str,
        priority: int = 0,
        name: str = "",
        actor_path: str = "",
        player_index: int = 0,
        world_type: str = "auto",
        clear_existing: bool = False,
        ignore_all_pressed_keys_until_release: bool = True,
        force_immediately: bool = False,
        notify_user_settings: bool = False
    ) -> Dict[str, Any]:
        """
        Assign a Mapping Context to a running local player's Enhanced Input subsystem.

        Args:
            mapping_context: Mapping Context asset path or unique asset name
            priority: Mapping Context priority
            name: Optional Pawn or PlayerController name
            actor_path: Optional Pawn or PlayerController path
            player_index: Local player index when name/actor_path is omitted
            world_type: Target world (auto, editor, pie)
            clear_existing: Whether to clear existing mappings before assignment
            ignore_all_pressed_keys_until_release: Enhanced Input rebuild option
            force_immediately: Apply synchronously on the same frame
            notify_user_settings: Register the mapping with user settings

        Returns:
            Response indicating success or failure
        """
        try:
            params: Dict[str, Any] = {
                "mapping_context": mapping_context,
                "priority": priority,
                "player_index": player_index,
                "world_type": world_type,
                "clear_existing": clear_existing,
                "ignore_all_pressed_keys_until_release": ignore_all_pressed_keys_until_release,
                "force_immediately": force_immediately,
                "notify_user_settings": notify_user_settings,
            }
            if name:
                params["name"] = name
            if actor_path:
                params["actor_path"] = actor_path

            logger.info("分配 Mapping Context: %s", mapping_context)
            return _send_project_command("assign_mapping_context", params)
        except Exception as e:
            error_msg = f"Error assigning mapping context: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def get_project_setting(
        ctx: Context,
        settings_class: str,
        property_name: str
    ) -> Dict[str, Any]:
        """
        Read one property from a supported project settings object.

        Args:
            settings_class: Supported settings object name, such as GameMapsSettings or InputSettings
            property_name: Reflected property name on that settings object

        Returns:
            Response containing the resolved setting value
        """
        try:
            logger.info("读取项目设置: %s.%s", settings_class, property_name)
            return _send_project_command(
                "get_project_setting",
                {
                    "settings_class": settings_class,
                    "property_name": property_name,
                },
            )
        except Exception as e:
            error_msg = f"Error reading project setting: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_project_setting(
        ctx: Context,
        settings_class: str,
        property_name: str,
        value: Any
    ) -> Dict[str, Any]:
        """
        Write one property on a supported project settings object.

        Args:
            settings_class: Supported settings object name, such as GameMapsSettings or InputSettings
            property_name: Reflected property name on that settings object
            value: New property value

        Returns:
            Response containing the updated setting value
        """
        try:
            logger.info("写入项目设置: %s.%s", settings_class, property_name)
            return _send_project_command(
                "set_project_setting",
                {
                    "settings_class": settings_class,
                    "property_name": property_name,
                    "value": value,
                },
            )
        except Exception as e:
            error_msg = f"Error writing project setting: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_default_maps(
        ctx: Context,
        game_default_map: str = "",
        server_default_map: str = "",
        editor_startup_map: str = "",
        transition_map: str = "",
        local_map_options: str = ""
    ) -> Dict[str, Any]:
        """
        Set the project's default map related settings.

        Args:
            game_default_map: Default game map asset path
            server_default_map: Default dedicated server map asset path
            editor_startup_map: Map loaded when the editor starts
            transition_map: Transition map asset path
            local_map_options: Default URL options appended to map loads

        Returns:
            Response indicating success or failure
        """
        try:
            params: Dict[str, Any] = {}
            if game_default_map:
                params["game_default_map"] = game_default_map
            if server_default_map:
                params["server_default_map"] = server_default_map
            if editor_startup_map:
                params["editor_startup_map"] = editor_startup_map
            if transition_map:
                params["transition_map"] = transition_map
            if local_map_options:
                params["local_map_options"] = local_map_options

            logger.info("设置默认地图配置")
            return _send_project_command("set_default_maps", params)
        except Exception as e:
            error_msg = f"Error setting default maps: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_game_framework_defaults(
        ctx: Context,
        game_instance_class: str = "",
        game_mode_class: str = "",
        server_game_mode_class: str = "",
        target_game_mode: str = "",
        default_pawn_class: str = "",
        hud_class: str = "",
        player_controller_class: str = "",
        game_state_class: str = "",
        spectator_class: str = "",
        replay_spectator_player_controller_class: str = ""
    ) -> Dict[str, Any]:
        """
        Set project-wide Game Framework defaults under Maps & Modes.

        Args:
            game_instance_class: Default GameInstance class
            game_mode_class: Default GameMode class
            server_game_mode_class: Default dedicated server GameMode class
            target_game_mode: Blueprint GameMode asset to modify class defaults on
            default_pawn_class: Default Pawn class on the target GameMode
            hud_class: HUD class on the target GameMode
            player_controller_class: PlayerController class on the target GameMode
            game_state_class: GameState class on the target GameMode
            spectator_class: SpectatorPawn class on the target GameMode
            replay_spectator_player_controller_class: Replay spectator controller class on the target GameMode

        Returns:
            Response indicating success or failure
        """
        try:
            params: Dict[str, Any] = {}
            if game_instance_class:
                params["game_instance_class"] = game_instance_class
            if game_mode_class:
                params["game_mode_class"] = game_mode_class
            if server_game_mode_class:
                params["server_game_mode_class"] = server_game_mode_class
            if target_game_mode:
                params["target_game_mode"] = target_game_mode
            if default_pawn_class:
                params["default_pawn_class"] = default_pawn_class
            if hud_class:
                params["hud_class"] = hud_class
            if player_controller_class:
                params["player_controller_class"] = player_controller_class
            if game_state_class:
                params["game_state_class"] = game_state_class
            if spectator_class:
                params["spectator_class"] = spectator_class
            if replay_spectator_player_controller_class:
                params["replay_spectator_player_controller_class"] = replay_spectator_player_controller_class

            logger.info("设置 Game Framework 默认类")
            return _send_project_command("set_game_framework_defaults", params)
        except Exception as e:
            error_msg = f"Error setting game framework defaults: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    logger.info("项目工具注册完成")
