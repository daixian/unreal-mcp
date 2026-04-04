"""
Blueprint Tools for Unreal MCP.

This module provides tools for creating and manipulating Blueprint assets in Unreal Engine.
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")

def register_blueprint_tools(mcp: FastMCP):
    """Register Blueprint tools with the MCP server."""
    
    @mcp.tool()
    def create_blueprint(
        ctx: Context,
        name: str,
        parent_class: str,
        path: str = ""
    ) -> Dict[str, Any]:
        """Create a new Blueprint class."""
        # Import inside function to avoid circular imports
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            params = {
                "name": name,
                "parent_class": parent_class
            }
            if path:
                params["path"] = path

            response = unreal.send_command("create_blueprint", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Blueprint creation response: {response}")
            return response or {}
            
        except Exception as e:
            error_msg = f"Error creating blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def create_child_blueprint(
        ctx: Context,
        name: str,
        parent_blueprint_name: str,
        path: str = ""
    ) -> Dict[str, Any]:
        """Create a child Blueprint from an existing Blueprint asset."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "name": name,
                "parent_blueprint_name": parent_blueprint_name,
            }
            if path:
                params["path"] = path

            response = unreal.send_command("create_child_blueprint", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Create child blueprint response: {response}")
            return response or {}

        except Exception as e:
            error_msg = f"Error creating child blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_component_to_blueprint(
        ctx: Context,
        blueprint_name: str,
        component_type: str,
        component_name: str,
        parent_name: str = "",
        location: List[float] = [],
        rotation: List[float] = [],
        scale: List[float] = [],
        component_properties: Dict[str, Any] = {}
    ) -> Dict[str, Any]:
        """
        Add a component to a Blueprint.
        
        Args:
            blueprint_name: Name of the target Blueprint
            component_type: Type of component to add (use component class name without U prefix)
            component_name: Name for the new component
            location: [X, Y, Z] coordinates for component's position
            rotation: [Pitch, Yaw, Roll] values for component's rotation
            scale: [X, Y, Z] values for component's scale
            component_properties: Additional properties to set on the component
        
        Returns:
            Information about the added component
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            # Ensure all parameters are properly formatted
            params = {
                "blueprint_name": blueprint_name,
                "component_type": component_type,
                "component_name": component_name,
                "location": location or [0.0, 0.0, 0.0],
                "rotation": rotation or [0.0, 0.0, 0.0],
                "scale": scale or [1.0, 1.0, 1.0]
            }

            if parent_name:
                params["parent_name"] = parent_name
            
            # Add component_properties if provided
            if component_properties and len(component_properties) > 0:
                params["component_properties"] = component_properties
            
            # Validate location, rotation, and scale formats
            for param_name in ["location", "rotation", "scale"]:
                param_value = params[param_name]
                if not isinstance(param_value, list) or len(param_value) != 3:
                    logger.error(f"Invalid {param_name} format: {param_value}. Must be a list of 3 float values.")
                    return {"success": False, "message": f"Invalid {param_name} format. Must be a list of 3 float values."}
                # Ensure all values are float
                params[param_name] = [float(val) for val in param_value]
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            logger.info(f"Adding component to blueprint with params: {params}")
            response = unreal.send_command("add_component_to_blueprint", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Component addition response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding component to blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def remove_component_from_blueprint(
        ctx: Context,
        blueprint_name: str,
        component_name: str
    ) -> Dict[str, Any]:
        """Remove a component from a Blueprint."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("remove_component_from_blueprint", {
                "blueprint_name": blueprint_name,
                "component_name": component_name
            })

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Remove component from blueprint response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error removing component from blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def attach_component_in_blueprint(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        parent_name: str,
        socket_name: str = "",
        keep_world_transform: bool = False
    ) -> Dict[str, Any]:
        """Attach a Blueprint scene component to another component."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "parent_name": parent_name,
                "keep_world_transform": keep_world_transform
            }
            if socket_name:
                params["socket_name"] = socket_name

            response = unreal.send_command("attach_component_in_blueprint", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Attach component in blueprint response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error attaching component in blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def set_static_mesh_properties(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        static_mesh: str = "/Engine/BasicShapes/Cube.Cube",
        material: str = ""
    ) -> Dict[str, Any]:
        """
        Set static mesh properties on a StaticMeshComponent.
        
        Args:
            blueprint_name: Name of the target Blueprint
            component_name: Name of the StaticMeshComponent
            static_mesh: Path to the static mesh asset (e.g., "/Engine/BasicShapes/Cube.Cube")
            
        Returns:
            Response indicating success or failure
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "static_mesh": static_mesh
            }
            if material:
                params["material"] = material
            
            logger.info(f"Setting static mesh properties with params: {params}")
            response = unreal.send_command("set_static_mesh_properties", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Set static mesh properties response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting static mesh properties: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def set_component_property(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        property_name: str,
        property_value,
    ) -> Dict[str, Any]:
        """Set a property on a component in a Blueprint."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "property_name": property_name,
                "property_value": property_value
            }
            
            logger.info(f"Setting component property with params: {params}")
            response = unreal.send_command("set_component_property", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Set component property response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting component property: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def set_physics_properties(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        simulate_physics: bool = True,
        gravity_enabled: bool = True,
        mass: float = 1.0,
        linear_damping: float = 0.01,
        angular_damping: float = 0.0
    ) -> Dict[str, Any]:
        """Set physics properties on a component."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "simulate_physics": simulate_physics,
                "gravity_enabled": gravity_enabled,
                "mass": float(mass),
                "linear_damping": float(linear_damping),
                "angular_damping": float(angular_damping)
            }
            
            logger.info(f"Setting physics properties with params: {params}")
            response = unreal.send_command("set_physics_properties", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Set physics properties response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting physics properties: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def compile_blueprint(
        ctx: Context,
        blueprint_name: str
    ) -> Dict[str, Any]:
        """Compile a Blueprint."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "blueprint_name": blueprint_name
            }
            
            logger.info(f"Compiling blueprint: {blueprint_name}")
            response = unreal.send_command("compile_blueprint", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Compile blueprint response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error compiling blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def compile_blueprints(
        ctx: Context,
        blueprint_names: List[str],
        stop_on_error: bool = False,
        save: bool = False
    ) -> Dict[str, Any]:
        """Batch compile Blueprint assets."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_names": blueprint_names,
                "stop_on_error": stop_on_error,
                "save": save
            }

            logger.info(f"Batch compiling blueprints: {params}")
            response = unreal.send_command("compile_blueprints", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Compile blueprints response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error compiling blueprints: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def cleanup_blueprint_for_reparent(
        ctx: Context,
        blueprint_name: str,
        remove_components: Optional[List[str]] = None,
        remove_member_nodes: Optional[List[str]] = None,
        refresh_nodes: bool = True,
        compile: bool = True,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Remove stale component/member nodes from a Blueprint after reparenting.

        Args:
            blueprint_name: Name or path of the target Blueprint
            remove_components: Component names to remove from SCS and related variable nodes
            remove_member_nodes: Member variable nodes to remove from graphs
            refresh_nodes: Whether to refresh all Blueprint nodes after cleanup
            compile: Whether to compile the Blueprint after cleanup
            save: Whether to save the Blueprint asset after cleanup

        Returns:
            Cleanup result with removed component/node counts
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("连接 Unreal Engine 失败")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": blueprint_name,
                "remove_components": remove_components or [],
                "remove_member_nodes": remove_member_nodes or [],
                "refresh_nodes": refresh_nodes,
                "compile": compile,
                "save": save
            }

            logger.info(f"开始清理重设父类后的 Blueprint 残留节点: {blueprint_name}")
            response = unreal.send_command("cleanup_blueprint_for_reparent", params)

            if not response:
                logger.error("Unreal Engine 没有返回 cleanup_blueprint_for_reparent 结果")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"cleanup_blueprint_for_reparent 返回: {response}")
            return response

        except Exception as e:
            error_msg = f"Error cleaning Blueprint after reparent: {e}"
            logger.error(f"执行 cleanup_blueprint_for_reparent 出错: {error_msg}")
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_blueprint_property(
        ctx: Context,
        blueprint_name: str,
        property_name: str,
        property_value
    ) -> Dict[str, Any]:
        """
        Set a property on a Blueprint class default object.
        
        Args:
            blueprint_name: Name of the target Blueprint
            property_name: Name of the property to set
            property_value: Value to set the property to
            
        Returns:
            Response indicating success or failure
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "blueprint_name": blueprint_name,
                "property_name": property_name,
                "property_value": property_value
            }
            
            logger.info(f"Setting blueprint property with params: {params}")
            response = unreal.send_command("set_blueprint_property", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Set blueprint property response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting blueprint property: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_pawn_properties(
        ctx: Context,
        blueprint_name: str,
        auto_possess_player: str = "",
        use_controller_rotation_yaw: bool = None,
        use_controller_rotation_pitch: bool = None,
        use_controller_rotation_roll: bool = None,
        can_be_damaged: bool = None
    ) -> Dict[str, Any]:
        """
        Set common Pawn properties on a Blueprint.
        This is a utility function that sets multiple pawn-related properties at once.
        
        Args:
            blueprint_name: Name of the target Blueprint (must be a Pawn or Character)
            auto_possess_player: Auto possess player setting (None, "Disabled", "Player0", "Player1", etc.)
            use_controller_rotation_yaw: Whether the pawn should use the controller's yaw rotation
            use_controller_rotation_pitch: Whether the pawn should use the controller's pitch rotation
            use_controller_rotation_roll: Whether the pawn should use the controller's roll rotation
            can_be_damaged: Whether the pawn can be damaged
            
        Returns:
            Response indicating success or failure with detailed results for each property
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            # Define the properties to set
            properties = {}
            if auto_possess_player and auto_possess_player != "":
                properties["auto_possess_player"] = auto_possess_player
            
            # Only include boolean properties if they were explicitly set
            if use_controller_rotation_yaw is not None:
                properties["bUseControllerRotationYaw"] = use_controller_rotation_yaw
            if use_controller_rotation_pitch is not None:
                properties["bUseControllerRotationPitch"] = use_controller_rotation_pitch
            if use_controller_rotation_roll is not None:
                properties["bUseControllerRotationRoll"] = use_controller_rotation_roll
            if can_be_damaged is not None:
                properties["bCanBeDamaged"] = can_be_damaged
                
            if not properties:
                logger.warning("No properties specified to set")
                return {"success": True, "message": "No properties specified to set", "results": {}}
            
            # Set each property using the generic set_blueprint_property function
            results = {}
            overall_success = True
            
            for prop_name, prop_value in properties.items():
                params = {
                    "blueprint_name": blueprint_name,
                    "property_name": prop_name,
                    "property_value": prop_value
                }
                
                logger.info(f"Setting pawn property {prop_name} to {prop_value}")
                response = unreal.send_command("set_blueprint_property", params)
                
                if not response:
                    logger.error(f"No response from Unreal Engine for property {prop_name}")
                    results[prop_name] = {"success": False, "message": "No response from Unreal Engine"}
                    overall_success = False
                    continue
                
                results[prop_name] = response
                if not response.get("success", False):
                    overall_success = False
            
            return {
                "success": overall_success,
                "message": "Pawn properties set" if overall_success else "Some pawn properties failed to set",
                "results": results
            }
            
        except Exception as e:
            error_msg = f"Error setting pawn properties: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_game_mode_default_pawn(
        ctx: Context,
        game_mode_name: str,
        pawn_blueprint_name: str
    ) -> Dict[str, Any]:
        """Set the default pawn class for a GameMode Blueprint."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "game_mode_name": game_mode_name,
                "pawn_blueprint_name": pawn_blueprint_name
            }

            logger.info(f"Setting default pawn for GameMode '{game_mode_name}' to '{pawn_blueprint_name}'")
            response = unreal.send_command("set_game_mode_default_pawn", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Set game mode default pawn response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error setting game mode default pawn: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def delete_blueprint_variable(
        ctx: Context,
        blueprint_name: str,
        variable_name: str
    ) -> Dict[str, Any]:
        """Delete a variable from a Blueprint."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("delete_blueprint_variable", {
                "blueprint_name": blueprint_name,
                "variable_name": variable_name
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def remove_unused_blueprint_variables(
        ctx: Context,
        blueprint_name: str
    ) -> Dict[str, Any]:
        """Remove all unused member variables from a Blueprint."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("remove_unused_blueprint_variables", {
                "blueprint_name": blueprint_name
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def add_blueprint_interface(
        ctx: Context,
        blueprint_name: str,
        interface_class: str
    ) -> Dict[str, Any]:
        """Add an interface implementation to a Blueprint."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("add_blueprint_interface", {
                "blueprint_name": blueprint_name,
                "interface_class": interface_class,
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def set_blueprint_variable_default(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
        default_value
    ) -> Dict[str, Any]:
        """Set the default value of a Blueprint member variable."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("set_blueprint_variable_default", {
                "blueprint_name": blueprint_name,
                "variable_name": variable_name,
                "default_value": default_value
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def add_blueprint_function(
        ctx: Context,
        blueprint_name: str,
        function_name: str
    ) -> Dict[str, Any]:
        """Add a function graph to a Blueprint."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("add_blueprint_function", {
                "blueprint_name": blueprint_name,
                "function_name": function_name
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def add_blueprint_macro(
        ctx: Context,
        blueprint_name: str,
        macro_name: str
    ) -> Dict[str, Any]:
        """Add a macro graph to a Blueprint."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("create_blueprint_graph", {
                "blueprint_name": blueprint_name,
                "graph_name": macro_name,
                "graph_type": "macro"
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def delete_blueprint_function(
        ctx: Context,
        blueprint_name: str,
        function_name: str
    ) -> Dict[str, Any]:
        """Delete a function graph from a Blueprint."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("delete_blueprint_function", {
                "blueprint_name": blueprint_name,
                "function_name": function_name
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def get_blueprint_compile_errors(
        ctx: Context,
        blueprint_name: str
    ) -> Dict[str, Any]:
        """Get the current compile errors and warnings for a Blueprint asset."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("get_blueprint_compile_errors", {
                "blueprint_name": blueprint_name
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def rename_blueprint_member(
        ctx: Context,
        blueprint_name: str,
        old_name: str,
        new_name: str,
        member_type: str = "auto"
    ) -> Dict[str, Any]:
        """Rename a Blueprint member. Graph members use local Python; variables use a Python+C++ bridge."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("rename_blueprint_member", {
                "blueprint_name": blueprint_name,
                "old_name": old_name,
                "new_name": new_name,
                "member_type": member_type,
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def save_blueprint(ctx: Context, blueprint_name: str) -> Dict[str, Any]:
        """Save a Blueprint asset."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("save_blueprint", {
                "blueprint_name": blueprint_name
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def open_blueprint_editor(ctx: Context, blueprint_name: str) -> Dict[str, Any]:
        """Open the Blueprint editor for a Blueprint asset."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("open_blueprint_editor", {
                "blueprint_name": blueprint_name
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}

    @mcp.tool()
    def reparent_blueprint(
        ctx: Context,
        blueprint_name: str,
        new_parent_class: str,
        save: bool = True
    ) -> Dict[str, Any]:
        """Reparent a Blueprint asset to a new parent class."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            return unreal.send_command("reparent_blueprint", {
                "blueprint_name": blueprint_name,
                "new_parent_class": new_parent_class,
                "save": save
            }) or {}
        except Exception as e:
            return {"success": False, "message": str(e)}
    
    logger.info("Blueprint tools registered successfully") 
