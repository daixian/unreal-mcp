"""
Blueprint Node Tools for Unreal MCP.

This module provides tools for manipulating Blueprint graph nodes and connections.
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")

def register_blueprint_node_tools(mcp: FastMCP):
    """Register Blueprint node manipulation tools with the MCP server."""
    
    @mcp.tool()
    def add_blueprint_event_node(
        ctx: Context,
        blueprint_name: str,
        event_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """
        Add an event node to a Blueprint's event graph.
        
        Args:
            blueprint_name: Name of the target Blueprint
            event_name: Name of the event. Use 'Receive' prefix for standard events:
                       - 'ReceiveBeginPlay' for Begin Play
                       - 'ReceiveTick' for Tick
                       - etc.
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Response containing the node ID and success status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            # Handle default value within the method body
            if node_position is None:
                node_position = [0, 0]
            
            params = {
                "blueprint_name": blueprint_name,
                "event_name": event_name,
                "node_position": node_position
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Adding event node '{event_name}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_event_node", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Event node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding event node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_blueprint_input_action_node(
        ctx: Context,
        blueprint_name: str,
        action_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """
        Add an input action event node to a Blueprint's event graph.
        
        Args:
            blueprint_name: Name of the target Blueprint
            action_name: Name of the input action to respond to
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Response containing the node ID and success status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            # Handle default value within the method body
            if node_position is None:
                node_position = [0, 0]
            
            params = {
                "blueprint_name": blueprint_name,
                "action_name": action_name,
                "node_position": node_position
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Adding input action node for '{action_name}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_input_action_node", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Input action node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding input action node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_blueprint_function_node(
        ctx: Context,
        blueprint_name: str,
        target: str,
        function_name: str,
        params = None,
        node_position = None
    ) -> Dict[str, Any]:
        """
        Add a function call node to a Blueprint's event graph.
        
        Args:
            blueprint_name: Name of the target Blueprint
            target: Target object for the function (component name or self)
            function_name: Name of the function to call
            params: Optional parameters to set on the function node
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Response containing the node ID and success status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            # Handle default values within the method body
            if params is None:
                params = {}
            if node_position is None:
                node_position = [0, 0]
            
            command_params = {
                "blueprint_name": blueprint_name,
                "target": target,
                "function_name": function_name,
                "params": params,
                "node_position": node_position
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Adding function node '{function_name}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_function_node", command_params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Function node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding function node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
            
    @mcp.tool()
    def connect_blueprint_nodes(
        ctx: Context,
        blueprint_name: str,
        source_node_id: str,
        source_pin: str,
        target_node_id: str,
        target_pin: str
    ) -> Dict[str, Any]:
        """
        Connect two nodes in a Blueprint's event graph.
        
        Args:
            blueprint_name: Name of the target Blueprint
            source_node_id: ID of the source node
            source_pin: Name of the output pin on the source node
            target_node_id: ID of the target node
            target_pin: Name of the input pin on the target node
            
        Returns:
            Response indicating success or failure
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            params = {
                "blueprint_name": blueprint_name,
                "source_node_id": source_node_id,
                "source_pin": source_pin,
                "target_node_id": target_node_id,
                "target_pin": target_pin
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Connecting nodes in blueprint '{blueprint_name}'")
            response = unreal.send_command("connect_blueprint_nodes", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Node connection response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error connecting nodes: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_blueprint_variable(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
        variable_type: str,
        is_exposed: bool = False
    ) -> Dict[str, Any]:
        """
        Add a variable to a Blueprint.
        
        Args:
            blueprint_name: Name of the target Blueprint
            variable_name: Name of the variable
            variable_type: Type of the variable (Boolean, Integer, Float, Vector, etc.)
            is_exposed: Whether to expose the variable to the editor
            
        Returns:
            Response indicating success or failure
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            params = {
                "blueprint_name": blueprint_name,
                "variable_name": variable_name,
                "variable_type": variable_type,
                "is_exposed": is_exposed
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Adding variable '{variable_name}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_variable", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Variable creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding variable: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_blueprint_get_self_component_reference(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """
        Add a node that gets a reference to a component owned by the current Blueprint.
        This creates a node similar to what you get when dragging a component from the Components panel.
        
        Args:
            blueprint_name: Name of the target Blueprint
            component_name: Name of the component to get a reference to
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Response containing the node ID and success status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            # Handle None case explicitly in the function
            if node_position is None:
                node_position = [0, 0]
            
            params = {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "node_position": node_position
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Adding self component reference node for '{component_name}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_get_self_component_reference", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Self component reference node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding self component reference node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_blueprint_self_reference(
        ctx: Context,
        blueprint_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """
        Add a 'Get Self' node to a Blueprint's event graph that returns a reference to this actor.
        
        Args:
            blueprint_name: Name of the target Blueprint
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Response containing the node ID and success status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            if node_position is None:
                node_position = [0, 0]
                
            params = {
                "blueprint_name": blueprint_name,
                "node_position": node_position
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Adding self reference node to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_self_reference", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Self reference node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding self reference node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def find_blueprint_nodes(
        ctx: Context,
        blueprint_name: str,
        node_type = None,
        event_type = None,
        include_details: bool = False
    ) -> Dict[str, Any]:
        """
        Find nodes in a Blueprint's event graph.
        
        Args:
            blueprint_name: Name of the target Blueprint
            node_type: Optional type of node to find (Event, Function, Variable, etc.)
            event_type: Optional specific event type to find (BeginPlay, Tick, etc.)
            
        Returns:
            Response containing array of found node IDs and success status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            params = {"blueprint_name": blueprint_name}
            if node_type:
                params["node_type"] = node_type
            if event_type:
                params["event_name"] = event_type
                params["event_type"] = event_type
            if include_details:
                params["include_details"] = True
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Finding nodes in blueprint '{blueprint_name}'")
            response = unreal.send_command("find_blueprint_nodes", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Node find response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error finding nodes: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def spawn_blueprint_node(
        ctx: Context,
        blueprint_name: str,
        node_kind: str = None,
        node_class: str = None,
        function_name: str = None,
        target: str = None,
        variable_name: str = None,
        action_name: str = None,
        component_name: str = None,
        event_name: str = None,
        class_name: str = None,
        class_path: str = None,
        widget_class: str = None,
        params = None,
        node_position = None
    ) -> Dict[str, Any]:
        """
        Spawn a Blueprint graph node using a generic node specification.

        Args:
            blueprint_name: Name of the target Blueprint
            node_kind: Optional semantic node kind, such as event/function_call/self_variable_get/branch/create_widget
            node_class: Optional graph node class name for reflective instantiation, such as K2Node_CreateWidget
            function_name: Optional function name for function call nodes
            target: Optional target class/component/self hint for function call nodes
            variable_name: Optional variable name for variable get/set nodes
            action_name: Optional action name for input action nodes
            component_name: Optional component name for self component reference nodes
            event_name: Optional event name for event nodes
            class_name: Optional class name used by construct-object style nodes
            class_path: Optional class object path used by construct-object style nodes
            widget_class: Optional widget class name/path shortcut for create widget nodes
            params: Optional input pin default map
            node_position: Optional [X, Y] position in the graph

        Returns:
            Response containing node creation result
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            command_params = {
                "blueprint_name": blueprint_name
            }
            if node_kind is not None:
                command_params["node_kind"] = node_kind
            if node_class is not None:
                command_params["node_class"] = node_class
            if function_name is not None:
                command_params["function_name"] = function_name
            if target is not None:
                command_params["target"] = target
            if variable_name is not None:
                command_params["variable_name"] = variable_name
            if action_name is not None:
                command_params["action_name"] = action_name
            if component_name is not None:
                command_params["component_name"] = component_name
            if event_name is not None:
                command_params["event_name"] = event_name
            if class_name is not None:
                command_params["class_name"] = class_name
            if class_path is not None:
                command_params["class_path"] = class_path
            if widget_class is not None:
                command_params["widget_class"] = widget_class
            if params is not None:
                command_params["params"] = params
            if node_position is None:
                node_position = [0, 0]
            command_params["node_position"] = node_position

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            logger.info(f"Spawning blueprint node in '{blueprint_name}' with kind='{node_kind}' class='{node_class}'")
            response = unreal.send_command("spawn_blueprint_node", command_params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Spawn blueprint node response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error spawning blueprint node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def describe_blueprint_node(
        ctx: Context,
        blueprint_name: str,
        node_id: str
    ) -> Dict[str, Any]:
        """
        Describe a single Blueprint graph node and its pins.

        Args:
            blueprint_name: Name of the target Blueprint
            node_id: GUID of the node to inspect

        Returns:
            Response containing node details and pin metadata
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            params = {
                "blueprint_name": blueprint_name,
                "node_id": node_id
            }

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            logger.info(f"Describing blueprint node '{node_id}' in '{blueprint_name}'")
            response = unreal.send_command("describe_blueprint_node", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Describe blueprint node response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error describing blueprint node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_blueprint_pin_default(
        ctx: Context,
        blueprint_name: str,
        node_id: str,
        pin_name: str,
        default_value = None,
        object_path: str = None,
        pin_direction: str = "input"
    ) -> Dict[str, Any]:
        """
        Set the default value of a Blueprint node pin.

        Args:
            blueprint_name: Name of the target Blueprint
            node_id: GUID of the node to edit
            pin_name: Name of the target pin
            default_value: Optional scalar default value (string/number/bool)
            object_path: Optional object/class path for object-like pins
            pin_direction: input or output, defaults to input

        Returns:
            Response containing updated pin data
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            params = {
                "blueprint_name": blueprint_name,
                "node_id": node_id,
                "pin_name": pin_name,
                "pin_direction": pin_direction
            }
            if default_value is not None:
                params["default_value"] = default_value
            if object_path is not None:
                params["object_path"] = object_path

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            logger.info(f"Setting default for pin '{pin_name}' on node '{node_id}' in '{blueprint_name}'")
            response = unreal.send_command("set_blueprint_pin_default", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Set blueprint pin default response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error setting blueprint pin default: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def delete_blueprint_node(
        ctx: Context,
        blueprint_name: str,
        node_id: str
    ) -> Dict[str, Any]:
        """
        Delete a single Blueprint graph node.

        Args:
            blueprint_name: Name of the target Blueprint
            node_id: GUID of the node to remove

        Returns:
            Response indicating whether the node was deleted
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            params = {
                "blueprint_name": blueprint_name,
                "node_id": node_id
            }

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            logger.info(f"Deleting blueprint node '{node_id}' in '{blueprint_name}'")
            response = unreal.send_command("delete_blueprint_node", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Delete blueprint node response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error deleting blueprint node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def setup_zspace_minimal_interaction(
        ctx: Context,
        blueprint_name: str = "BP_ZSpacePawn",
        ray_length: float = 2000.0
    ) -> Dict[str, Any]:
        """Build the minimal ZSpace interaction graph for a pawn Blueprint."""
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": blueprint_name,
                "ray_length": float(ray_length)
            }

            logger.info(f"Setting up zSpace minimal interaction for '{blueprint_name}'")
            response = unreal.send_command("setup_zspace_minimal_interaction", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Setup zSpace minimal interaction response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error setting up zSpace minimal interaction: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    logger.info("Blueprint node tools registered successfully")
