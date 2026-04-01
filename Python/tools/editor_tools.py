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

    logger.info("Editor tools registered successfully")
