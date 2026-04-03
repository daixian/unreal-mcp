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
        graph_name: str = None,
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
            if graph_name:
                params["graph_name"] = graph_name
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
        graph_name: str = None,
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
        struct_name: str = None,
        struct_path: str = None,
        enum_name: str = None,
        enum_path: str = None,
        widget_class: str = None,
        duration: float = None,
        then_pin_count: int = None,
        option_pin_count: int = None,
        pure: bool = None,
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
            struct_name: Optional struct name used by make/break struct nodes
            struct_path: Optional struct path used by make/break struct nodes
            enum_name: Optional enum name used by switch/select nodes
            enum_path: Optional enum path used by switch/select nodes
            widget_class: Optional widget class name/path shortcut for create widget nodes
            then_pin_count: Optional then pin count for sequence nodes
            option_pin_count: Optional option count for select nodes
            pure: Optional purity flag for supported nodes such as dynamic cast
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
            if graph_name is not None:
                command_params["graph_name"] = graph_name
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
            if struct_name is not None:
                command_params["struct_name"] = struct_name
            if struct_path is not None:
                command_params["struct_path"] = struct_path
            if enum_name is not None:
                command_params["enum_name"] = enum_name
            if enum_path is not None:
                command_params["enum_path"] = enum_path
            if widget_class is not None:
                command_params["widget_class"] = widget_class
            if duration is not None:
                command_params["duration"] = duration
            if then_pin_count is not None:
                command_params["then_pin_count"] = then_pin_count
            if option_pin_count is not None:
                command_params["option_pin_count"] = option_pin_count
            if pure is not None:
                command_params["pure"] = pure
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
    def disconnect_blueprint_nodes(
        ctx: Context,
        blueprint_name: str,
        source_node_id: str,
        source_pin: str,
        target_node_id: str = None,
        target_pin: str = None,
        graph_name: str = None
    ) -> Dict[str, Any]:
        """Disconnect one or all links from a Blueprint node pin."""
        from unreal_mcp_server import get_unreal_connection

        try:
            params = {
                "blueprint_name": blueprint_name,
                "source_node_id": source_node_id,
                "source_pin": source_pin
            }
            if target_node_id is not None:
                params["target_node_id"] = target_node_id
            if target_pin is not None:
                params["target_pin"] = target_pin
            if graph_name is not None:
                params["graph_name"] = graph_name

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("disconnect_blueprint_nodes", params)
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            return response

        except Exception as e:
            error_msg = f"Error disconnecting blueprint nodes: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def move_blueprint_node(
        ctx: Context,
        blueprint_name: str,
        node_id: str,
        node_position = None,
        node_delta = None,
        graph_name: str = None
    ) -> Dict[str, Any]:
        """Move a Blueprint node to an absolute position or by delta."""
        from unreal_mcp_server import get_unreal_connection

        try:
            params = {
                "blueprint_name": blueprint_name,
                "node_id": node_id
            }
            if node_position is not None:
                params["node_position"] = node_position
            if node_delta is not None:
                params["node_delta"] = node_delta
            if graph_name is not None:
                params["graph_name"] = graph_name

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("move_blueprint_node", params)
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            return response

        except Exception as e:
            error_msg = f"Error moving blueprint node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def layout_blueprint_nodes(
        ctx: Context,
        blueprint_name: str,
        node_ids = None,
        graph_name: str = None,
        layout: str = "horizontal",
        origin = None,
        spacing_x: float = 240.0,
        spacing_y: float = 120.0,
        columns: int = 4
    ) -> Dict[str, Any]:
        """Layout Blueprint nodes in horizontal, vertical, or grid mode."""
        from unreal_mcp_server import get_unreal_connection

        try:
            params = {
                "blueprint_name": blueprint_name,
                "layout": layout,
                "spacing_x": spacing_x,
                "spacing_y": spacing_y,
                "columns": columns
            }
            if node_ids is not None:
                params["node_ids"] = node_ids
            if graph_name is not None:
                params["graph_name"] = graph_name
            if origin is not None:
                params["origin"] = origin

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("layout_blueprint_nodes", params)
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            return response

        except Exception as e:
            error_msg = f"Error laying out blueprint nodes: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_blueprint_comment_node(
        ctx: Context,
        blueprint_name: str,
        text: str = "Comment",
        node_position = None,
        size = None,
        color = None,
        graph_name: str = None
    ) -> Dict[str, Any]:
        """Add a comment node to a Blueprint graph."""
        from unreal_mcp_server import get_unreal_connection

        try:
            params = {
                "blueprint_name": blueprint_name,
                "text": text,
                "node_position": node_position or [0, 0]
            }
            if size is not None:
                params["size"] = size
            if color is not None:
                params["color"] = color
            if graph_name is not None:
                params["graph_name"] = graph_name

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("add_blueprint_comment_node", params)
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            return response

        except Exception as e:
            error_msg = f"Error adding blueprint comment node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_blueprint_reroute_node(
        ctx: Context,
        blueprint_name: str,
        node_position = None,
        source_node_id: str = None,
        source_pin: str = None,
        target_node_id: str = None,
        target_pin: str = None,
        graph_name: str = None
    ) -> Dict[str, Any]:
        """Add a reroute node to a Blueprint graph."""
        from unreal_mcp_server import get_unreal_connection

        try:
            params = {
                "blueprint_name": blueprint_name,
                "node_position": node_position or [0, 0]
            }
            if source_node_id is not None:
                params["source_node_id"] = source_node_id
            if source_pin is not None:
                params["source_pin"] = source_pin
            if target_node_id is not None:
                params["target_node_id"] = target_node_id
            if target_pin is not None:
                params["target_pin"] = target_pin
            if graph_name is not None:
                params["graph_name"] = graph_name

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("add_blueprint_reroute_node", params)
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            return response

        except Exception as e:
            error_msg = f"Error adding blueprint reroute node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def create_blueprint_graph(
        ctx: Context,
        blueprint_name: str,
        graph_name: str,
        graph_type: str = "graph"
    ) -> Dict[str, Any]:
        """Create a graph, function graph, or macro graph on a Blueprint."""
        from unreal_mcp_server import get_unreal_connection

        try:
            params = {
                "blueprint_name": blueprint_name,
                "graph_name": graph_name,
                "graph_type": graph_type
            }

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("create_blueprint_graph", params)
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            return response

        except Exception as e:
            error_msg = f"Error creating blueprint graph: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def delete_blueprint_graph(
        ctx: Context,
        blueprint_name: str,
        graph_name: str
    ) -> Dict[str, Any]:
        """Delete a graph from a Blueprint."""
        from unreal_mcp_server import get_unreal_connection

        try:
            params = {
                "blueprint_name": blueprint_name,
                "graph_name": graph_name
            }

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("delete_blueprint_graph", params)
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            return response

        except Exception as e:
            error_msg = f"Error deleting blueprint graph: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def duplicate_blueprint_subgraph(
        ctx: Context,
        blueprint_name: str,
        node_ids,
        graph_name: str = None,
        target_graph_name: str = None,
        position_offset = None
    ) -> Dict[str, Any]:
        """Duplicate a selected Blueprint subgraph."""
        from unreal_mcp_server import get_unreal_connection

        try:
            params = {
                "blueprint_name": blueprint_name,
                "node_ids": node_ids
            }
            if graph_name is not None:
                params["graph_name"] = graph_name
            if target_graph_name is not None:
                params["target_graph_name"] = target_graph_name
            if position_offset is not None:
                params["position_offset"] = position_offset

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("duplicate_blueprint_subgraph", params)
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            return response

        except Exception as e:
            error_msg = f"Error duplicating blueprint subgraph: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def collapse_nodes_to_function(
        ctx: Context,
        blueprint_name: str,
        node_ids,
        graph_name: str = None,
        new_graph_name: str = None
    ) -> Dict[str, Any]:
        """Collapse selected Blueprint nodes into a new function graph."""
        from unreal_mcp_server import get_unreal_connection

        try:
            params = {
                "blueprint_name": blueprint_name,
                "node_ids": node_ids
            }
            if graph_name is not None:
                params["graph_name"] = graph_name
            if new_graph_name is not None:
                params["new_graph_name"] = new_graph_name

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("collapse_nodes_to_function", params)
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            return response

        except Exception as e:
            error_msg = f"Error collapsing nodes to function: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def collapse_nodes_to_macro(
        ctx: Context,
        blueprint_name: str,
        node_ids,
        graph_name: str = None,
        new_graph_name: str = None
    ) -> Dict[str, Any]:
        """Collapse selected Blueprint nodes into a new macro graph."""
        from unreal_mcp_server import get_unreal_connection

        try:
            params = {
                "blueprint_name": blueprint_name,
                "node_ids": node_ids
            }
            if graph_name is not None:
                params["graph_name"] = graph_name
            if new_graph_name is not None:
                params["new_graph_name"] = new_graph_name

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("collapse_nodes_to_macro", params)
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            return response

        except Exception as e:
            error_msg = f"Error collapsing nodes to macro: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def validate_blueprint_graph(
        ctx: Context,
        blueprint_name: str,
        graph_name: str = None
    ) -> Dict[str, Any]:
        """Validate that a Blueprint graph can compile."""
        from unreal_mcp_server import get_unreal_connection

        try:
            params = {
                "blueprint_name": blueprint_name
            }
            if graph_name is not None:
                params["graph_name"] = graph_name

            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("validate_blueprint_graph", params)
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            return response

        except Exception as e:
            error_msg = f"Error validating blueprint graph: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_branch_node(
        ctx: Context,
        blueprint_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """Add a Branch node to a Blueprint graph."""
        del ctx
        return spawn_blueprint_node(
            None,
            blueprint_name=blueprint_name,
            node_kind="branch",
            node_position=node_position or [0, 0]
        )

    @mcp.tool()
    def add_variable_get_node(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """Add a self variable get node to a Blueprint graph."""
        del ctx
        return spawn_blueprint_node(
            None,
            blueprint_name=blueprint_name,
            node_kind="self_variable_get",
            variable_name=variable_name,
            node_position=node_position or [0, 0]
        )

    @mcp.tool()
    def add_variable_set_node(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """Add a self variable set node to a Blueprint graph."""
        del ctx
        return spawn_blueprint_node(
            None,
            blueprint_name=blueprint_name,
            node_kind="self_variable_set",
            variable_name=variable_name,
            node_position=node_position or [0, 0]
        )

    @mcp.tool()
    def add_sequence_node(
        ctx: Context,
        blueprint_name: str,
        node_position = None,
        then_pin_count: int = 2
    ) -> Dict[str, Any]:
        """Add a Sequence node to a Blueprint graph."""
        del ctx
        return spawn_blueprint_node(
            None,
            blueprint_name=blueprint_name,
            node_kind="sequence",
            then_pin_count=then_pin_count,
            node_position=node_position or [0, 0]
        )

    @mcp.tool()
    def add_delay_node(
        ctx: Context,
        blueprint_name: str,
        node_position = None,
        duration: float = 0.2
    ) -> Dict[str, Any]:
        """Add a Delay node to a Blueprint graph."""
        del ctx
        return spawn_blueprint_node(
            None,
            blueprint_name=blueprint_name,
            node_kind="delay",
            duration=duration,
            node_position=node_position or [0, 0]
        )

    @mcp.tool()
    def add_gate_node(
        ctx: Context,
        blueprint_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """Add a Gate node to a Blueprint graph."""
        del ctx
        return spawn_blueprint_node(
            None,
            blueprint_name=blueprint_name,
            node_kind="gate",
            node_position=node_position or [0, 0]
        )

    @mcp.tool()
    def add_for_loop_node(
        ctx: Context,
        blueprint_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """Add a For Loop node to a Blueprint graph."""
        del ctx
        return spawn_blueprint_node(
            None,
            blueprint_name=blueprint_name,
            node_kind="for_loop",
            node_position=node_position or [0, 0]
        )

    @mcp.tool()
    def add_for_each_loop_node(
        ctx: Context,
        blueprint_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """Add a For Each Loop node to a Blueprint graph."""
        del ctx
        return spawn_blueprint_node(
            None,
            blueprint_name=blueprint_name,
            node_kind="for_each_loop",
            node_position=node_position or [0, 0]
        )

    @mcp.tool()
    def add_cast_node(
        ctx: Context,
        blueprint_name: str,
        class_name: str,
        node_position = None,
        pure: bool = False
    ) -> Dict[str, Any]:
        """Add a Dynamic Cast node to a Blueprint graph."""
        del ctx
        return spawn_blueprint_node(
            None,
            blueprint_name=blueprint_name,
            node_kind="cast",
            class_name=class_name,
            pure=pure,
            node_position=node_position or [0, 0]
        )

    @mcp.tool()
    def add_make_struct_node(
        ctx: Context,
        blueprint_name: str,
        struct_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """Add a Make Struct node to a Blueprint graph."""
        del ctx
        return spawn_blueprint_node(
            None,
            blueprint_name=blueprint_name,
            node_kind="make_struct",
            struct_name=struct_name,
            node_position=node_position or [0, 0]
        )

    @mcp.tool()
    def add_break_struct_node(
        ctx: Context,
        blueprint_name: str,
        struct_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """Add a Break Struct node to a Blueprint graph."""
        del ctx
        return spawn_blueprint_node(
            None,
            blueprint_name=blueprint_name,
            node_kind="break_struct",
            struct_name=struct_name,
            node_position=node_position or [0, 0]
        )

    @mcp.tool()
    def add_switch_on_enum_node(
        ctx: Context,
        blueprint_name: str,
        enum_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """Add a Switch on Enum node to a Blueprint graph."""
        del ctx
        return spawn_blueprint_node(
            None,
            blueprint_name=blueprint_name,
            node_kind="switch_on_enum",
            enum_name=enum_name,
            node_position=node_position or [0, 0]
        )

    @mcp.tool()
    def add_select_node(
        ctx: Context,
        blueprint_name: str,
        node_position = None,
        enum_name: str = "",
        option_pin_count: int = 2
    ) -> Dict[str, Any]:
        """Add a Select node to a Blueprint graph."""
        del ctx
        return spawn_blueprint_node(
            None,
            blueprint_name=blueprint_name,
            node_kind="select",
            enum_name=enum_name or None,
            option_pin_count=option_pin_count,
            node_position=node_position or [0, 0]
        )

    logger.info("Blueprint node tools registered successfully")
