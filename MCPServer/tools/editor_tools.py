"""
Editor Tools for Unreal MCP.

This module provides tools for controlling the Unreal Editor viewport and other editor functionality.
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")

def register_editor_tools(mcp: FastMCP):
    """Register editor tools with the MCP server."""

    def _build_actor_list_result(
        response: Dict[str, Any],
        fallback_message: str
    ) -> Dict[str, Any]:
        """Normalize actor list responses to a stable object shape."""
        result = response.get("result", response)
        actors = result.get("actors", [])

        if not isinstance(actors, list):
            logger.warning(f"Unexpected actor list format: {response}")
            return {
                "success": False,
                "message": fallback_message,
                "actors": [],
                "actor_count": 0,
            }

        normalized_result = {
            "success": True,
            "actors": actors,
            "actor_count": len(actors),
        }

        for field_name in ("resolved_world_type", "world_type", "world_name", "world_path"):
            if field_name in result:
                normalized_result[field_name] = result[field_name]

        return normalized_result
    
    @mcp.tool()
    def get_actors_in_level(
        ctx: Context,
        include_components: bool = False,
        detailed_components: bool = True,
        world_type: str = "auto"
    ) -> Dict[str, Any]:
        """Get actors in the current level.
        
        Args:
            include_components: Whether to include each actor's component list
            detailed_components: Whether to include detailed component fields
            world_type: Target world to query: auto/editor/pie
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.warning("Failed to connect to Unreal Engine")
                return {
                    "success": False,
                    "message": "Failed to connect to Unreal Engine",
                    "actors": [],
                    "actor_count": 0,
                }

            response = unreal.send_command("get_actors_in_level", {
                "include_components": include_components,
                "detailed_components": detailed_components,
                "world_type": world_type
            })
            
            if not response:
                logger.warning("No response from Unreal Engine")
                return {
                    "success": False,
                    "message": "No response from Unreal Engine",
                    "actors": [],
                    "actor_count": 0,
                }

            normalized_result = _build_actor_list_result(
                response,
                "Unexpected response format while getting actors in level",
            )
            logger.info(
                "Get actors in level completed, actor_count=%s",
                normalized_result.get("actor_count", 0),
            )
            return normalized_result
            
        except Exception as e:
            logger.error(f"Error getting actors: {e}")
            return {
                "success": False,
                "message": str(e),
                "actors": [],
                "actor_count": 0,
            }

    @mcp.tool()
    def find_actors_by_name(
        ctx: Context,
        pattern: str,
        world_type: str = "auto",
        include_components: bool = False,
        detailed_components: bool = True
    ) -> Dict[str, Any]:
        """Find actors by name pattern.
        
        Args:
            pattern: Name pattern to match
            world_type: Target world to query: auto/editor/pie
            include_components: Whether to include each actor's component list
            detailed_components: Whether to include detailed component fields
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.warning("Failed to connect to Unreal Engine")
                return {
                    "success": False,
                    "message": "Failed to connect to Unreal Engine",
                    "actors": [],
                    "actor_count": 0,
                }
                
            response = unreal.send_command("find_actors_by_name", {
                "pattern": pattern,
                "world_type": world_type,
                "include_components": include_components,
                "detailed_components": detailed_components
            })
            
            if not response:
                return {
                    "success": False,
                    "message": "No response from Unreal Engine",
                    "actors": [],
                    "actor_count": 0,
                }

            normalized_result = _build_actor_list_result(
                response,
                "Unexpected response format while finding actors by name",
            )
            normalized_result["pattern"] = pattern
            return normalized_result
            
        except Exception as e:
            logger.error(f"Error finding actors: {e}")
            return {
                "success": False,
                "message": str(e),
                "pattern": pattern,
                "actors": [],
                "actor_count": 0,
            }
    
    @mcp.tool()
    def spawn_actor(
        ctx: Context,
        name: str,
        type: str,
        location: List[float] = [0.0, 0.0, 0.0],
        rotation: List[float] = [0.0, 0.0, 0.0]
    ) -> Dict[str, Any]:
        """Create a new actor in the current level.
        
        Args:
            ctx: The MCP context
            name: The name to give the new actor (must be unique)
            type: The type of actor to create (e.g. StaticMeshActor, PointLight)
            location: The [x, y, z] world location to spawn at
            rotation: The [pitch, yaw, roll] rotation in degrees
            
        Returns:
            Dict containing the created actor's properties
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            # Ensure all parameters are properly formatted
            params = {
                "name": name,
                "type": type.strip(),
                "location": location,
                "rotation": rotation
            }
            
            # Validate location and rotation formats
            for param_name in ["location", "rotation"]:
                param_value = params[param_name]
                if not isinstance(param_value, list) or len(param_value) != 3:
                    logger.error(f"Invalid {param_name} format: {param_value}. Must be a list of 3 float values.")
                    return {"success": False, "message": f"Invalid {param_name} format. Must be a list of 3 float values."}
                # Ensure all values are float
                params[param_name] = [float(val) for val in param_value]
            
            logger.info(f"Creating actor '{name}' of type '{type}' with params: {params}")
            response = unreal.send_command("spawn_actor", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            # Log the complete response for debugging
            logger.info(f"Actor creation response: {response}")
            
            # Handle error responses correctly
            if response.get("status") == "error":
                error_message = response.get("error", "Unknown error")
                logger.error(f"Error creating actor: {error_message}")
                return {"success": False, "message": error_message}
            
            return response
            
        except Exception as e:
            error_msg = f"Error creating actor: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def delete_actor(ctx: Context, name: str) -> Dict[str, Any]:
        """Delete an actor by name."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            response = unreal.send_command("delete_actor", {
                "name": name
            })
            return response or {}
            
        except Exception as e:
            logger.error(f"Error deleting actor: {e}")
            return {}
    
    @mcp.tool()
    def set_actor_transform(
        ctx: Context,
        name: str,
        location: List[float]  = None,
        rotation: List[float]  = None,
        scale: List[float] = None
    ) -> Dict[str, Any]:
        """Set the transform of an actor."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            params = {"name": name}
            if location is not None:
                params["location"] = location
            if rotation is not None:
                params["rotation"] = rotation
            if scale is not None:
                params["scale"] = scale
                
            response = unreal.send_command("set_actor_transform", params)
            return response or {}
            
        except Exception as e:
            logger.error(f"Error setting transform: {e}")
            return {}
    
    @mcp.tool()
    def get_actor_properties(
        ctx: Context,
        name: str = "",
        actor_path: str = "",
        include_components: bool = True,
        detailed_components: bool = True,
        world_type: str = "auto"
    ) -> Dict[str, Any]:
        """Get all properties of an actor.
        
        Args:
            name: Name of actor (used when actor_path is not provided)
            actor_path: Full actor path for exact lookup
            include_components: Whether to include component details
            detailed_components: Whether to include detailed component fields
            world_type: Target world to query: auto/editor/pie
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            if not name and not actor_path:
                return {"success": False, "message": "Either 'name' or 'actor_path' is required"}

            params = {
                "include_components": include_components,
                "detailed_components": detailed_components,
                "world_type": world_type
            }
            if actor_path:
                params["actor_path"] = actor_path
            else:
                params["name"] = name

            response = unreal.send_command("get_actor_properties", params)
            return response or {}
            
        except Exception as e:
            logger.error(f"Error getting properties: {e}")
            return {}

    @mcp.tool()
    def get_actor_components(
        ctx: Context,
        name: str = "",
        actor_path: str = "",
        detailed_components: bool = True,
        world_type: str = "auto"
    ) -> Dict[str, Any]:
        """Get all components for a single actor.
        
        Args:
            name: Actor name (used when actor_path is not provided)
            actor_path: Full actor path for exact lookup
            detailed_components: Whether to include detailed component fields
            world_type: Target world to query: auto/editor/pie
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            if not name and not actor_path:
                return {"success": False, "message": "Either 'name' or 'actor_path' is required"}

            params = {
                "detailed_components": detailed_components,
                "world_type": world_type
            }
            if actor_path:
                params["actor_path"] = actor_path
            else:
                params["name"] = name

            response = unreal.send_command("get_actor_components", params)
            return response or {}

        except Exception as e:
            logger.error(f"Error getting actor components: {e}")
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def get_scene_components(
        ctx: Context,
        pattern: str = "",
        detailed_components: bool = True,
        world_type: str = "auto"
    ) -> Dict[str, Any]:
        """Get components for all actors in current scene.
        
        Args:
            pattern: Optional actor name filter
            detailed_components: Whether to include detailed component fields
            world_type: Target world to query: auto/editor/pie
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "detailed_components": detailed_components,
                "world_type": world_type
            }
            if pattern:
                params["pattern"] = pattern

            response = unreal.send_command("get_scene_components", params)
            return response or {}

        except Exception as e:
            logger.error(f"Error getting scene components: {e}")
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def start_pie(ctx: Context, simulate: bool = False) -> Dict[str, Any]:
        """Start Play-In-Editor (PIE) session.
        
        Args:
            simulate: If true, start in Simulate-In-Editor mode
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("start_pie", {
                "simulate": simulate
            })
            return response or {}

        except Exception as e:
            logger.error(f"Error starting PIE: {e}")
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def start_vr_preview(ctx: Context) -> Dict[str, Any]:
        """Start a VR Preview session."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("start_vr_preview", {})
            return response or {}

        except Exception as e:
            logger.error(f"Error starting VR Preview: {e}")
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def start_standalone_game(
        ctx: Context,
        map_override: str = "",
        additional_command_line_parameters: str = ""
    ) -> Dict[str, Any]:
        """Start Standalone Game in a new local process.

        Args:
            map_override: Optional map asset path to override the launched map
            additional_command_line_parameters: Extra command line appended to the standalone process
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params: Dict[str, Any] = {}
            if map_override:
                params["map_override"] = map_override
            if additional_command_line_parameters:
                params["additional_command_line_parameters"] = additional_command_line_parameters

            response = unreal.send_command("start_standalone_game", params)
            return response or {}

        except Exception as e:
            logger.error(f"Error starting Standalone Game: {e}")
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def stop_pie(ctx: Context) -> Dict[str, Any]:
        """Stop the current Play-In-Editor (PIE) session."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("stop_pie", {})
            return response or {}

        except Exception as e:
            logger.error(f"Error stopping PIE: {e}")
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def get_play_state(ctx: Context) -> Dict[str, Any]:
        """Get current Play-In-Editor (PIE) running state."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("get_play_state", {})
            return response or {}

        except Exception as e:
            logger.error(f"Error getting play state: {e}")
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def make_directory(ctx: Context, directory_path: str) -> Dict[str, Any]:
        """Create a directory in the Content Browser."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("make_directory", {
                "directory_path": directory_path
            })
            return response or {}

        except Exception as e:
            logger.error(f"Error making directory: {e}")
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def duplicate_asset(
        ctx: Context,
        source_asset_path: str,
        destination_asset_path: str,
        overwrite: bool = False
    ) -> Dict[str, Any]:
        """Duplicate an asset to another path."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("duplicate_asset", {
                "source_asset_path": source_asset_path,
                "destination_asset_path": destination_asset_path,
                "overwrite": overwrite
            })
            return response or {}

        except Exception as e:
            logger.error(f"Error duplicating asset: {e}")
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def load_level(ctx: Context, level_path: str) -> Dict[str, Any]:
        """Load a level by content path."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("load_level", {
                "level_path": level_path
            })
            return response or {}

        except Exception as e:
            logger.error(f"Error loading level: {e}")
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def save_current_level(ctx: Context) -> Dict[str, Any]:
        """Save the current level."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("save_current_level", {})
            return response or {}

        except Exception as e:
            logger.error(f"Error saving current level: {e}")
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def start_live_coding(ctx: Context, show_console: bool = True) -> Dict[str, Any]:
        """Enable Live Coding for the current editor session.
        
        Args:
            show_console: Whether to show the Live Coding console window
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("连接 Unreal Engine 失败")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("start_live_coding", {
                "show_console": show_console
            })
            return response or {}

        except Exception as e:
            logger.error(f"启动 Live Coding 失败: {e}")
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def compile_live_coding(
        ctx: Context,
        wait_for_completion: bool = False,
        show_console: bool = True
    ) -> Dict[str, Any]:
        """Trigger a Live Coding compile.
        
        Args:
            wait_for_completion: Whether to wait for compile completion before returning
            show_console: Whether to show the Live Coding console window
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("连接 Unreal Engine 失败")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("compile_live_coding", {
                "wait_for_completion": wait_for_completion,
                "show_console": show_console
            })
            return response or {}

        except Exception as e:
            logger.error(f"触发 Live Coding 编译失败: {e}")
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def get_live_coding_state(ctx: Context) -> Dict[str, Any]:
        """Get current Live Coding session state."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("连接 Unreal Engine 失败")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("get_live_coding_state", {})
            return response or {}

        except Exception as e:
            logger.error(f"获取 Live Coding 状态失败: {e}")
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def set_actor_property(
        ctx: Context,
        name: str,
        property_name: str,
        property_value,
    ) -> Dict[str, Any]:
        """
        Set a property on an actor.
        
        Args:
            name: Name of the actor
            property_name: Name of the property to set
            property_value: Value to set the property to
            
        Returns:
            Dict containing response from Unreal with operation status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            response = unreal.send_command("set_actor_property", {
                "name": name,
                "property_name": property_name,
                "property_value": property_value
            })
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Set actor property response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting actor property: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def focus_viewport(
        ctx: Context,
        target: str = None,
        location: List[float] = None,
        distance: float = 1000.0,
        orientation: List[float] = None
    ) -> Dict[str, Any]:
        """
        Focus the viewport on a specific actor or location.
        
        Args:
            target: Name of the actor to focus on (if provided, location is ignored)
            location: [X, Y, Z] coordinates to focus on (used if target is None)
            distance: Distance from the target/location
            orientation: Optional [Pitch, Yaw, Roll] for the viewport camera
            
        Returns:
            Response from Unreal Engine
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            params = {}
            if target:
                params["target"] = target
            elif location:
                params["location"] = location
            
            if distance:
                params["distance"] = distance
                
            if orientation:
                params["orientation"] = orientation
                
            response = unreal.send_command("focus_viewport", params)
            return response or {}
            
        except Exception as e:
            logger.error(f"Error focusing viewport: {e}")
            return {"status": "error", "message": str(e)}

    @mcp.tool()
    def take_screenshot(ctx: Context, filepath: str) -> Dict[str, Any]:
        """Capture a screenshot of the active viewport."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("take_screenshot", {
                "filepath": filepath
            })
            return response or {}

        except Exception as e:
            logger.error(f"Error taking screenshot: {e}")
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def take_highres_screenshot(
        ctx: Context,
        filepath: str,
        resolution: Optional[List[int]] = None,
        resolution_multiplier: float = 1.0,
        capture_hdr: bool = False
    ) -> Dict[str, Any]:
        """Capture a high-resolution screenshot of the active viewport.

        Args:
            filepath: Output file path. If no extension is provided, Unreal picks png/exr by hdr mode
            resolution: Optional [Width, Height] target resolution
            resolution_multiplier: Multiplier used when resolution is omitted
            capture_hdr: Whether to capture HDR output
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params: Dict[str, Any] = {
                "filepath": filepath,
                "resolution_multiplier": resolution_multiplier,
                "capture_hdr": capture_hdr,
            }
            if resolution:
                params["resolution"] = resolution

            response = unreal.send_command("take_highres_screenshot", params)
            return response or {}

        except Exception as e:
            logger.error(f"Error taking high-resolution screenshot: {e}")
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def capture_viewport_sequence(
        ctx: Context,
        output_dir: str,
        frame_count: int,
        interval_seconds: float = 0.0,
        base_filename: str = "ViewportSequence",
        use_high_res: bool = False,
        resolution: Optional[List[int]] = None,
        resolution_multiplier: float = 1.0,
        show_ui: bool = False,
        capture_hdr: bool = False
    ) -> Dict[str, Any]:
        """Capture a sequence of viewport frames to disk.

        Args:
            output_dir: Output directory for the frame sequence
            frame_count: Number of frames to capture
            interval_seconds: Delay between frame requests
            base_filename: Common filename prefix for output frames
            use_high_res: Whether to use the high-resolution screenshot path for each frame
            resolution: Optional [Width, Height] target resolution
            resolution_multiplier: Multiplier used when resolution is omitted
            show_ui: Whether standard screenshots should include editor UI
            capture_hdr: Whether to write HDR frames
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params: Dict[str, Any] = {
                "output_dir": output_dir,
                "frame_count": frame_count,
                "interval_seconds": interval_seconds,
                "base_filename": base_filename,
                "use_high_res": use_high_res,
                "resolution_multiplier": resolution_multiplier,
                "show_ui": show_ui,
                "capture_hdr": capture_hdr,
            }
            if resolution:
                params["resolution"] = resolution

            response = unreal.send_command("capture_viewport_sequence", params)
            return response or {}

        except Exception as e:
            logger.error(f"Error capturing viewport sequence: {e}")
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def spawn_blueprint_actor(
        ctx: Context,
        blueprint_name: str,
        actor_name: str,
        location: List[float] = [0.0, 0.0, 0.0],
        rotation: List[float] = [0.0, 0.0, 0.0]
    ) -> Dict[str, Any]:
        """Spawn an actor from a Blueprint.
        
        Args:
            ctx: The MCP context
            blueprint_name: Name of the Blueprint to spawn from
            actor_name: Name to give the spawned actor
            location: The [x, y, z] world location to spawn at
            rotation: The [pitch, yaw, roll] rotation in degrees
            
        Returns:
            Dict containing the spawned actor's properties
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            # Ensure all parameters are properly formatted
            params = {
                "blueprint_name": blueprint_name,
                "actor_name": actor_name,
                "location": location or [0.0, 0.0, 0.0],
                "rotation": rotation or [0.0, 0.0, 0.0]
            }
            
            # Validate location and rotation formats
            for param_name in ["location", "rotation"]:
                param_value = params[param_name]
                if not isinstance(param_value, list) or len(param_value) != 3:
                    logger.error(f"Invalid {param_name} format: {param_value}. Must be a list of 3 float values.")
                    return {"success": False, "message": f"Invalid {param_name} format. Must be a list of 3 float values."}
                # Ensure all values are float
                params[param_name] = [float(val) for val in param_value]
            
            logger.info(f"Spawning blueprint actor with params: {params}")
            response = unreal.send_command("spawn_blueprint_actor", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Spawn blueprint actor response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error spawning blueprint actor: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def duplicate_actor(
        ctx: Context,
        name: str = "",
        actor_path: str = "",
        world_type: str = "auto",
        offset: Optional[List[float]] = None
    ) -> Dict[str, Any]:
        """Duplicate a single actor."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params: Dict[str, Any] = {"world_type": world_type}
            if name:
                params["name"] = name
            if actor_path:
                params["actor_path"] = actor_path
            if offset is not None:
                params["offset"] = offset

            return unreal.send_command("duplicate_actor", params) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def select_actor(
        ctx: Context,
        name: str = "",
        actor_path: str = "",
        world_type: str = "auto",
        replace_selection: bool = False,
        notify: bool = True,
        select_even_if_hidden: bool = True
    ) -> Dict[str, Any]:
        """Select an actor in the editor."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params: Dict[str, Any] = {
                "world_type": world_type,
                "replace_selection": replace_selection,
                "notify": notify,
                "select_even_if_hidden": select_even_if_hidden
            }
            if name:
                params["name"] = name
            if actor_path:
                params["actor_path"] = actor_path

            return unreal.send_command("select_actor", params) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def get_selected_actors(
        ctx: Context,
        include_components: bool = False,
        detailed_components: bool = True
    ) -> Dict[str, Any]:
        """Get actors currently selected in the editor."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("get_selected_actors", {
                "include_components": include_components,
                "detailed_components": detailed_components
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def get_editor_selection(
        ctx: Context,
        include_components: bool = False,
        detailed_components: bool = True,
        include_tags: bool = False
    ) -> Dict[str, Any]:
        """Get the current editor selection including both actors and assets.

        Args:
            include_components: Whether selected actor payloads should include component lists
            detailed_components: Whether actor components should include detailed fields
            include_tags: Whether selected asset payloads should include tag metadata
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("get_editor_selection", {
                "include_components": include_components,
                "detailed_components": detailed_components,
                "include_tags": include_tags
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def attach_actor(
        ctx: Context,
        name: str,
        parent_name: str,
        socket_name: str = "",
        keep_world_transform: bool = True,
        world_type: str = "auto"
    ) -> Dict[str, Any]:
        """Attach one actor to another actor."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("attach_actor", {
                "name": name,
                "parent_name": parent_name,
                "socket_name": socket_name,
                "keep_world_transform": keep_world_transform,
                "world_type": world_type
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def detach_actor(
        ctx: Context,
        name: str,
        keep_world_transform: bool = True,
        world_type: str = "auto"
    ) -> Dict[str, Any]:
        """Detach an actor from its parent actor."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("detach_actor", {
                "name": name,
                "keep_world_transform": keep_world_transform,
                "world_type": world_type
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def set_actors_transform(
        ctx: Context,
        actor_names: List[str],
        location: Optional[List[float]] = None,
        rotation: Optional[List[float]] = None,
        scale: Optional[List[float]] = None,
        world_type: str = "auto"
    ) -> Dict[str, Any]:
        """Set the same transform values on multiple actors."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params: Dict[str, Any] = {
                "actor_names": actor_names,
                "world_type": world_type
            }
            if location is not None:
                params["location"] = location
            if rotation is not None:
                params["rotation"] = rotation
            if scale is not None:
                params["scale"] = scale

            return unreal.send_command("set_actors_transform", params) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def open_asset_editor(ctx: Context, asset_path: str) -> Dict[str, Any]:
        """Open the editor for any asset by content path."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("open_asset_editor", {
                "asset_path": asset_path
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def close_asset_editor(ctx: Context, asset_path: str) -> Dict[str, Any]:
        """Close the editor tab for any asset by content path."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("close_asset_editor", {
                "asset_path": asset_path
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def execute_console_command(
        ctx: Context,
        command: str,
        world_type: str = "auto"
    ) -> Dict[str, Any]:
        """Execute an Unreal console command."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("execute_console_command", {
                "command": command,
                "world_type": world_type
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def execute_unreal_python(
        ctx: Context,
        command: str,
        execution_mode: str = "Auto",
        file_execution_scope: str = "Private",
        unattended: bool = False
    ) -> Dict[str, Any]:
        """Execute Unreal Python using the PythonScriptPlugin.

        Args:
            command: Literal Python code, or a Python file path with optional arguments.
            execution_mode: "Auto", "ExecuteFile", "ExecuteStatement", or "EvaluateStatement".
                "Auto" will choose "ExecuteFile" for multi-line/file-like input, otherwise "ExecuteStatement".
            file_execution_scope: Scope used when executing files or multi-line scripts in ExecuteFile mode.
            unattended: Whether to suppress certain UI while the script runs.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("execute_unreal_python", {
                "command": command,
                "execution_mode": execution_mode,
                "file_execution_scope": file_execution_scope,
                "unattended": unattended
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def run_editor_utility_widget(
        ctx: Context,
        asset_path: str,
        tab_id: str = ""
    ) -> Dict[str, Any]:
        """Run an Editor Utility Widget asset and open its tab.

        Args:
            asset_path: Content path of the Editor Utility Widget Blueprint
            tab_id: Optional explicit tab id used for registration/spawn
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params: Dict[str, Any] = {"asset_path": asset_path}
            if tab_id:
                params["tab_id"] = tab_id

            return unreal.send_command("run_editor_utility_widget", params) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def run_editor_utility_blueprint(
        ctx: Context,
        asset_path: str
    ) -> Dict[str, Any]:
        """Run an Editor Utility Blueprint asset via its Run entry."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("run_editor_utility_blueprint", {
                "asset_path": asset_path
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def set_viewport_mode(
        ctx: Context,
        view_mode: str,
        apply_to_all: bool = False
    ) -> Dict[str, Any]:
        """Set the active level viewport rendering mode.

        Args:
            view_mode: View mode name or numeric index, such as Lit, Unlit, Wireframe
            apply_to_all: Whether to apply the mode to all level viewports
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("set_viewport_mode", {
                "view_mode": view_mode,
                "apply_to_all": apply_to_all
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def get_viewport_camera(ctx: Context) -> Dict[str, Any]:
        """Get camera information for the active level viewport."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("get_viewport_camera", {}) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def get_output_log(
        ctx: Context,
        limit: int = 100,
        category: str = "",
        verbosity: str = "",
        contains: str = ""
    ) -> Dict[str, Any]:
        """Get captured Unreal output log entries."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params: Dict[str, Any] = {"limit": limit}
            if category:
                params["category"] = category
            if verbosity:
                params["verbosity"] = verbosity
            if contains:
                params["contains"] = contains

            return unreal.send_command("get_output_log", params) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def clear_output_log(ctx: Context) -> Dict[str, Any]:
        """Clear the captured Unreal output log buffer."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("clear_output_log", {}) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def get_message_log(
        ctx: Context,
        log_name: str = "PIE",
        limit: int = 100
    ) -> Dict[str, Any]:
        """Read messages from a named Unreal Message Log listing."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("get_message_log", {
                "log_name": log_name,
                "limit": limit
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def show_editor_notification(
        ctx: Context,
        message: str,
        severity: str = "info",
        expire_duration: float = 5.0,
        fire_and_forget: bool = True
    ) -> Dict[str, Any]:
        """Show a transient notification in the Unreal Editor."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("show_editor_notification", {
                "message": message,
                "severity": severity,
                "expire_duration": expire_duration,
                "fire_and_forget": fire_and_forget
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    logger.info("Editor tools registered successfully")
