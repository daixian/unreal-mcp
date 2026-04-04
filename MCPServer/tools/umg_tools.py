"""
UMG Tools for Unreal MCP.

This module provides tools for creating and manipulating UMG Widget Blueprints in Unreal Engine.
"""

import logging
from typing import Dict, List, Any
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")

def register_umg_tools(mcp: FastMCP):
    """Register UMG tools with the MCP server."""

    @mcp.tool()
    def create_umg_widget_blueprint(
        ctx: Context,
        widget_name: str,
        parent_class: str = "UserWidget",
        path: str = "/Game/UI"
    ) -> Dict[str, Any]:
        """
        Create a new UMG Widget Blueprint.
        
        Args:
            widget_name: Name of the widget blueprint to create
            parent_class: Parent class for the widget (default: UserWidget)
            path: Content browser path where the widget should be created
            
        Returns:
            Dict containing success status and widget path
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "name": widget_name,
                "parent_class": parent_class,
                "path": path
            }
            
            logger.info(f"Creating UMG Widget Blueprint with params: {params}")
            response = unreal.send_command("create_umg_widget_blueprint", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Create UMG Widget Blueprint response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error creating UMG Widget Blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_text_block_to_widget(
        ctx: Context,
        widget_name: str,
        text_block_name: str,
        text: str = "",
        position: List[float] = [0.0, 0.0],
        size: List[float] = [200.0, 50.0],
        font_size: int = 12,
        color: List[float] = [1.0, 1.0, 1.0, 1.0]
    ) -> Dict[str, Any]:
        """
        Add a Text Block widget to a UMG Widget Blueprint.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            text_block_name: Name to give the new Text Block
            text: Initial text content
            position: [X, Y] position in the canvas panel
            size: [Width, Height] of the text block
            font_size: Font size in points
            color: [R, G, B, A] color values (0.0 to 1.0)
            
        Returns:
            Dict containing success status and text block properties
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "blueprint_name": widget_name,
                "widget_name": text_block_name,
                "text": text,
                "position": position,
                "size": size,
                "font_size": font_size,
                "color": color
            }
            
            logger.info(f"Adding Text Block to widget with params: {params}")
            response = unreal.send_command("add_text_block_to_widget", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Add Text Block response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding Text Block to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_button_to_widget(
        ctx: Context,
        widget_name: str,
        button_name: str,
        text: str = "",
        position: List[float] = [0.0, 0.0],
        size: List[float] = [200.0, 50.0],
        font_size: int = 12,
        color: List[float] = [1.0, 1.0, 1.0, 1.0],
        background_color: List[float] = [0.1, 0.1, 0.1, 1.0]
    ) -> Dict[str, Any]:
        """
        Add a Button widget to a UMG Widget Blueprint.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            button_name: Name to give the new Button
            text: Text to display on the button
            position: [X, Y] position in the canvas panel
            size: [Width, Height] of the button
            font_size: Font size for button text
            color: [R, G, B, A] text color values (0.0 to 1.0)
            background_color: [R, G, B, A] button background color values (0.0 to 1.0)
            
        Returns:
            Dict containing success status and button properties
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "blueprint_name": widget_name,
                "widget_name": button_name,
                "text": text,
                "position": position,
                "size": size,
                "font_size": font_size,
                "color": color,
                "background_color": background_color
            }
            
            logger.info(f"Adding Button to widget with params: {params}")
            response = unreal.send_command("add_button_to_widget", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Add Button response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding Button to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def bind_widget_event(
        ctx: Context,
        widget_name: str,
        widget_component_name: str,
        event_name: str,
        function_name: str = ""
    ) -> Dict[str, Any]:
        """
        Bind an event on a widget component to a function.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            widget_component_name: Name of the widget component (button, etc.)
            event_name: Name of the event to bind (OnClicked, etc.)
            function_name: Name of the function to create/bind to (defaults to f"{widget_component_name}_{event_name}")
            
        Returns:
            Dict containing success status and binding information
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            # If no function name provided, create one from component and event names
            if not function_name:
                function_name = f"{widget_component_name}_{event_name}"
            
            params = {
                "blueprint_name": widget_name,
                "widget_name": widget_component_name,
                "event_name": event_name,
                "function_name": function_name
            }
            
            logger.info(f"Binding widget event with params: {params}")
            response = unreal.send_command("bind_widget_event", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Bind widget event response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error binding widget event: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_widget_to_viewport(
        ctx: Context,
        widget_name: str,
        z_order: int = 0,
        instance_name: str = "",
        player_index: int | None = None,
    ) -> Dict[str, Any]:
        """
        Add a Widget Blueprint instance to the viewport.
        
        Args:
            widget_name: Name of the Widget Blueprint to add
            z_order: Z-order for the widget (higher numbers appear on top)
            instance_name: Optional explicit object name for the created widget instance
            player_index: Optional local player index; when set, the widget is added to that player's screen
            
        Returns:
            Dict containing success status and widget instance information
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "blueprint_name": widget_name,
                "z_order": z_order,
                "instance_name": instance_name,
            }
            if player_index is not None:
                params["player_index"] = player_index
            
            logger.info(f"Adding widget to viewport with params: {params}")
            response = unreal.send_command("add_widget_to_viewport", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Add widget to viewport response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding widget to viewport: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def remove_widget_from_viewport(
        ctx: Context,
        instance_path: str = "",
        instance_name: str = "",
        widget_name: str = "",
    ) -> Dict[str, Any]:
        """
        Remove a previously created Widget instance from the viewport.

        Args:
            instance_path: Preferred runtime object path returned by add_widget_to_viewport
            instance_name: Optional runtime object name; only use when instance_path is unavailable
            widget_name: Optional Widget Blueprint name used to narrow matching when using instance_name

        Returns:
            Dict containing success status and removed widget instance information
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "instance_path": instance_path,
                "instance_name": instance_name,
                "blueprint_name": widget_name,
            }

            logger.info(f"Removing widget from viewport with params: {params}")
            response = unreal.send_command("remove_widget_from_viewport", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Remove widget from viewport response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error removing widget from viewport: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_text_block_binding(
        ctx: Context,
        widget_name: str,
        text_block_name: str,
        binding_property: str,
        binding_type: str = "Text"
    ) -> Dict[str, Any]:
        """
        Set up a property binding for a Text Block widget.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            text_block_name: Name of the Text Block to bind
            binding_property: Name of the property to bind to
            binding_type: Type of binding (Text, Visibility, ColorAndOpacity, ShadowColorAndOpacity, ToolTipText, IsEnabled)
            
        Returns:
            Dict containing success status and binding information
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "blueprint_name": widget_name,
                "widget_name": text_block_name,
                "binding_name": binding_property,
                "binding_type": binding_type
            }
            
            logger.info(f"Setting text block binding with params: {params}")
            response = unreal.send_command("set_text_block_binding", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Set text block binding response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting text block binding: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def bind_widget_property(
        ctx: Context,
        widget_name: str,
        child_widget_name: str,
        binding_property: str,
        binding_type: str = "Text"
    ) -> Dict[str, Any]:
        """
        Bind a supported widget property to a Blueprint member variable.

        Args:
            widget_name: Name of the target Widget Blueprint
            child_widget_name: Name of the child widget to bind
            binding_property: Name of the Blueprint member variable used as binding source
            binding_type: Supported property type alias (Text, Visibility, ColorAndOpacity, ShadowColorAndOpacity, ToolTipText, IsEnabled)

        Returns:
            Dict containing success status and binding information
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": child_widget_name,
                "binding_name": binding_property,
                "binding_type": binding_type
            }

            logger.info(f"Binding widget property with params: {params}")
            response = unreal.send_command("bind_widget_property", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Bind widget property response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error binding widget property: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_image_to_widget(
        ctx: Context,
        widget_name: str,
        image_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [200.0, 200.0],
        color: List[float] = [1.0, 1.0, 1.0, 1.0]
    ) -> Dict[str, Any]:
        """
        Add an Image widget to a UMG Widget Blueprint.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            image_name: Name to give the new Image
            position: [X, Y] position in the canvas panel
            size: [Width, Height] of the image
            color: [R, G, B, A] tint color values (0.0 to 1.0)
        
        Returns:
            Dict containing success status and image properties
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": image_name,
                "position": position,
                "size": size,
                "color": color
            }

            logger.info(f"Adding Image to widget with params: {params}")
            response = unreal.send_command("add_image_to_widget", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add Image response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding Image to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_border_to_widget(
        ctx: Context,
        widget_name: str,
        border_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [200.0, 100.0],
        brush_color: List[float] = [1.0, 1.0, 1.0, 1.0],
        content_color: List[float] = [1.0, 1.0, 1.0, 1.0]
    ) -> Dict[str, Any]:
        """
        Add a Border widget to a UMG Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            border_name: Name to give the new Border
            position: [X, Y] position in the root canvas
            size: [Width, Height] of the border
            brush_color: [R, G, B, A] border brush color values
            content_color: [R, G, B, A] border content tint values

        Returns:
            Dict containing success status and widget properties
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": border_name,
                "position": position,
                "size": size,
                "brush_color": brush_color,
                "content_color": content_color
            }

            logger.info(f"Adding Border to widget with params: {params}")
            response = unreal.send_command("add_border_to_widget", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add Border response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding Border to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_canvas_panel_to_widget(
        ctx: Context,
        widget_name: str,
        canvas_panel_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [400.0, 300.0]
    ) -> Dict[str, Any]:
        """
        Add a Canvas Panel widget to a UMG Widget Blueprint.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            canvas_panel_name: Name to give the new Canvas Panel
            position: [X, Y] position in the root canvas
            size: [Width, Height] of the canvas panel
        
        Returns:
            Dict containing success status and widget properties
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": canvas_panel_name,
                "position": position,
                "size": size
            }

            logger.info(f"Adding Canvas Panel to widget with params: {params}")
            response = unreal.send_command("add_canvas_panel_to_widget", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add Canvas Panel response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding Canvas Panel to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_horizontal_box_to_widget(
        ctx: Context,
        widget_name: str,
        horizontal_box_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [300.0, 100.0]
    ) -> Dict[str, Any]:
        """
        Add a Horizontal Box widget to a UMG Widget Blueprint.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            horizontal_box_name: Name to give the new Horizontal Box
            position: [X, Y] position in the root canvas
            size: [Width, Height] of the box
        
        Returns:
            Dict containing success status and widget properties
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": horizontal_box_name,
                "position": position,
                "size": size
            }

            logger.info(f"Adding Horizontal Box to widget with params: {params}")
            response = unreal.send_command("add_horizontal_box_to_widget", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add Horizontal Box response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding Horizontal Box to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_vertical_box_to_widget(
        ctx: Context,
        widget_name: str,
        vertical_box_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [300.0, 100.0]
    ) -> Dict[str, Any]:
        """
        Add a Vertical Box widget to a UMG Widget Blueprint.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            vertical_box_name: Name to give the new Vertical Box
            position: [X, Y] position in the root canvas
            size: [Width, Height] of the box
        
        Returns:
            Dict containing success status and widget properties
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": vertical_box_name,
                "position": position,
                "size": size
            }

            logger.info(f"Adding Vertical Box to widget with params: {params}")
            response = unreal.send_command("add_vertical_box_to_widget", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add Vertical Box response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding Vertical Box to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_overlay_to_widget(
        ctx: Context,
        widget_name: str,
        overlay_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [300.0, 100.0]
    ) -> Dict[str, Any]:
        """
        Add an Overlay widget to a UMG Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            overlay_name: Name to give the new Overlay
            position: [X, Y] position in the root canvas
            size: [Width, Height] of the overlay

        Returns:
            Dict containing success status and widget properties
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": overlay_name,
                "position": position,
                "size": size
            }

            logger.info(f"Adding Overlay to widget with params: {params}")
            response = unreal.send_command("add_overlay_to_widget", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add Overlay response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding Overlay to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_scroll_box_to_widget(
        ctx: Context,
        widget_name: str,
        scroll_box_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [300.0, 200.0]
    ) -> Dict[str, Any]:
        """
        Add a Scroll Box widget to a UMG Widget Blueprint.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            scroll_box_name: Name to give the new Scroll Box
            position: [X, Y] position in the root canvas
            size: [Width, Height] of the scroll box
        
        Returns:
            Dict containing success status and widget properties
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": scroll_box_name,
                "position": position,
                "size": size
            }

            logger.info(f"Adding Scroll Box to widget with params: {params}")
            response = unreal.send_command("add_scroll_box_to_widget", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add Scroll Box response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding Scroll Box to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_size_box_to_widget(
        ctx: Context,
        widget_name: str,
        size_box_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [200.0, 100.0],
        width_override: float = 0.0,
        height_override: float = 0.0
    ) -> Dict[str, Any]:
        """
        Add a Size Box widget to a UMG Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            size_box_name: Name to give the new Size Box
            position: [X, Y] position in the root canvas
            size: [Width, Height] of the size box
            width_override: Width override applied to the Size Box
            height_override: Height override applied to the Size Box

        Returns:
            Dict containing success status and widget properties
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": size_box_name,
                "position": position,
                "size": size
            }
            if width_override > 0.0:
                params["width_override"] = width_override
            if height_override > 0.0:
                params["height_override"] = height_override

            logger.info(f"Adding Size Box to widget with params: {params}")
            response = unreal.send_command("add_size_box_to_widget", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add Size Box response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding Size Box to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_spacer_to_widget(
        ctx: Context,
        widget_name: str,
        spacer_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [32.0, 32.0]
    ) -> Dict[str, Any]:
        """
        Add a Spacer widget to a UMG Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            spacer_name: Name to give the new Spacer
            position: [X, Y] position in the root canvas
            size: [Width, Height] of the spacer

        Returns:
            Dict containing success status and widget properties
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": spacer_name,
                "position": position,
                "size": size
            }

            logger.info(f"Adding Spacer to widget with params: {params}")
            response = unreal.send_command("add_spacer_to_widget", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add Spacer response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding Spacer to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_progress_bar_to_widget(
        ctx: Context,
        widget_name: str,
        progress_bar_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [300.0, 30.0],
        percent: float = 0.5,
        fill_color: List[float] = [1.0, 1.0, 1.0, 1.0]
    ) -> Dict[str, Any]:
        """
        Add a Progress Bar widget to a UMG Widget Blueprint.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            progress_bar_name: Name to give the new Progress Bar
            position: [X, Y] position in the root canvas
            size: [Width, Height] of the progress bar
            percent: Initial percent value from 0.0 to 1.0
            fill_color: [R, G, B, A] fill color values (0.0 to 1.0)
        
        Returns:
            Dict containing success status and widget properties
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": progress_bar_name,
                "position": position,
                "size": size,
                "percent": percent,
                "fill_color": fill_color
            }

            logger.info(f"Adding Progress Bar to widget with params: {params}")
            response = unreal.send_command("add_progress_bar_to_widget", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add Progress Bar response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding Progress Bar to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_slider_to_widget(
        ctx: Context,
        widget_name: str,
        slider_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [300.0, 40.0],
        value: float = 0.0
    ) -> Dict[str, Any]:
        """
        Add a Slider widget to a UMG Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            slider_name: Name to give the new Slider
            position: [X, Y] position in the root canvas
            size: [Width, Height] of the slider
            value: Initial slider value from 0.0 to 1.0

        Returns:
            Dict containing success status and widget properties
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": slider_name,
                "position": position,
                "size": size,
                "value": value
            }

            logger.info(f"Adding Slider to widget with params: {params}")
            response = unreal.send_command("add_slider_to_widget", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add Slider response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding Slider to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_check_box_to_widget(
        ctx: Context,
        widget_name: str,
        check_box_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [48.0, 48.0],
        is_checked: bool = False
    ) -> Dict[str, Any]:
        """
        Add a Check Box widget to a UMG Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            check_box_name: Name to give the new Check Box
            position: [X, Y] position in the root canvas
            size: [Width, Height] of the check box
            is_checked: Whether the check box starts checked

        Returns:
            Dict containing success status and widget properties
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": check_box_name,
                "position": position,
                "size": size,
                "is_checked": is_checked
            }

            logger.info(f"Adding Check Box to widget with params: {params}")
            response = unreal.send_command("add_check_box_to_widget", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add Check Box response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding Check Box to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_editable_text_to_widget(
        ctx: Context,
        widget_name: str,
        editable_text_name: str,
        text: str = "",
        hint_text: str = "",
        position: List[float] = [0.0, 0.0],
        size: List[float] = [300.0, 40.0],
        color: List[float] = [1.0, 1.0, 1.0, 1.0],
        is_read_only: bool = False
    ) -> Dict[str, Any]:
        """
        Add an Editable Text widget to a UMG Widget Blueprint.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            editable_text_name: Name to give the new Editable Text
            text: Initial text content
            hint_text: Placeholder text shown when empty
            position: [X, Y] position in the root canvas
            size: [Width, Height] of the editable text
            color: [R, G, B, A] text color values (0.0 to 1.0)
            is_read_only: Whether the widget should be read only
        
        Returns:
            Dict containing success status and widget properties
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": editable_text_name,
                "text": text,
                "hint_text": hint_text,
                "position": position,
                "size": size,
                "color": color,
                "is_read_only": is_read_only
            }

            logger.info(f"Adding Editable Text to widget with params: {params}")
            response = unreal.send_command("add_editable_text_to_widget", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add Editable Text response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding Editable Text to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_rich_text_to_widget(
        ctx: Context,
        widget_name: str,
        rich_text_name: str,
        text: str = "",
        position: List[float] = [0.0, 0.0],
        size: List[float] = [300.0, 80.0],
        color: List[float] = [1.0, 1.0, 1.0, 1.0]
    ) -> Dict[str, Any]:
        """
        Add a Rich Text widget to a UMG Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            rich_text_name: Name to give the new Rich Text widget
            text: Initial rich text content
            position: [X, Y] position in the root canvas
            size: [Width, Height] of the rich text block
            color: [R, G, B, A] default text color values

        Returns:
            Dict containing success status and widget properties
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": rich_text_name,
                "text": text,
                "position": position,
                "size": size,
                "color": color
            }

            logger.info(f"Adding Rich Text to widget with params: {params}")
            response = unreal.send_command("add_rich_text_to_widget", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add Rich Text response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding Rich Text to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_multi_line_text_to_widget(
        ctx: Context,
        widget_name: str,
        multi_line_text_name: str,
        text: str = "",
        hint_text: str = "",
        position: List[float] = [0.0, 0.0],
        size: List[float] = [300.0, 120.0],
        is_read_only: bool = False
    ) -> Dict[str, Any]:
        """
        Add a multi-line text widget to a UMG Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            multi_line_text_name: Name to give the new multi-line text widget
            text: Initial text content
            hint_text: Placeholder text shown when empty
            position: [X, Y] position in the root canvas
            size: [Width, Height] of the widget
            is_read_only: Whether the widget should be read only

        Returns:
            Dict containing success status and widget properties
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": multi_line_text_name,
                "text": text,
                "hint_text": hint_text,
                "position": position,
                "size": size,
                "is_read_only": is_read_only
            }

            logger.info(f"Adding Multi-Line Text to widget with params: {params}")
            response = unreal.send_command("add_multi_line_text_to_widget", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add Multi-Line Text response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding Multi-Line Text to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_named_slot_to_widget(
        ctx: Context,
        widget_name: str,
        named_slot_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [200.0, 100.0]
    ) -> Dict[str, Any]:
        """
        Add a Named Slot widget to a UMG Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            named_slot_name: Name to give the new Named Slot
            position: [X, Y] position in the root canvas
            size: [Width, Height] of the named slot

        Returns:
            Dict containing success status and widget properties
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": named_slot_name,
                "position": position,
                "size": size
            }

            logger.info(f"Adding Named Slot to widget with params: {params}")
            response = unreal.send_command("add_named_slot_to_widget", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add Named Slot response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding Named Slot to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_list_view_to_widget(
        ctx: Context,
        widget_name: str,
        list_view_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [300.0, 200.0]
    ) -> Dict[str, Any]:
        """
        Add a List View widget to a UMG Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            list_view_name: Name to give the new List View
            position: [X, Y] position in the root canvas
            size: [Width, Height] of the list view

        Returns:
            Dict containing success status and widget properties
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": list_view_name,
                "position": position,
                "size": size
            }

            logger.info(f"Adding List View to widget with params: {params}")
            response = unreal.send_command("add_list_view_to_widget", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add List View response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding List View to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_tile_view_to_widget(
        ctx: Context,
        widget_name: str,
        tile_view_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [300.0, 200.0]
    ) -> Dict[str, Any]:
        """
        Add a Tile View widget to a UMG Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            tile_view_name: Name to give the new Tile View
            position: [X, Y] position in the root canvas
            size: [Width, Height] of the tile view

        Returns:
            Dict containing success status and widget properties
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": tile_view_name,
                "position": position,
                "size": size
            }

            logger.info(f"Adding Tile View to widget with params: {params}")
            response = unreal.send_command("add_tile_view_to_widget", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add Tile View response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding Tile View to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_tree_view_to_widget(
        ctx: Context,
        widget_name: str,
        tree_view_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [300.0, 200.0]
    ) -> Dict[str, Any]:
        """
        Add a Tree View widget to a UMG Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            tree_view_name: Name to give the new Tree View
            position: [X, Y] position in the root canvas
            size: [Width, Height] of the tree view

        Returns:
            Dict containing success status and widget properties
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": tree_view_name,
                "position": position,
                "size": size
            }

            logger.info(f"Adding Tree View to widget with params: {params}")
            response = unreal.send_command("add_tree_view_to_widget", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add Tree View response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding Tree View to widget: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def remove_widget_from_blueprint(
        ctx: Context,
        widget_name: str,
        child_widget_name: str
    ) -> Dict[str, Any]:
        """
        Remove a source widget from a UMG Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            child_widget_name: Name of the child widget to remove

        Returns:
            Dict containing success status and removed widget info
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": child_widget_name
            }

            logger.info(f"Removing widget from blueprint with params: {params}")
            response = unreal.send_command("remove_widget_from_blueprint", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Remove widget from blueprint response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error removing widget from blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_widget_slot_layout(
        ctx: Context,
        widget_name: str,
        child_widget_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [100.0, 100.0],
        alignment: List[float] = [0.0, 0.0],
        anchors: List[float] = [0.0, 0.0, 0.0, 0.0],
        offsets: List[float] = [],
        auto_size: bool = False,
        z_order: int = 0
    ) -> Dict[str, Any]:
        """
        Update CanvasPanelSlot layout properties for a widget inside a Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            child_widget_name: Name of the child widget to update
            position: [X, Y] position in the canvas
            size: [Width, Height] size in the canvas
            alignment: [X, Y] alignment pivot
            anchors: [MinX, MinY, MaxX, MaxY] anchors
            offsets: [Left, Top, Right, Bottom] slot offsets
            auto_size: Whether to size to desired content
            z_order: Canvas slot Z-order

        Returns:
            Dict containing success status and updated layout info
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": child_widget_name,
                "position": position,
                "size": size,
                "alignment": alignment,
                "anchors": anchors,
                "auto_size": auto_size,
                "z_order": z_order
            }
            if offsets:
                params["offsets"] = offsets

            logger.info(f"Setting widget slot layout with params: {params}")
            response = unreal.send_command("set_widget_slot_layout", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Set widget slot layout response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error setting widget slot layout: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_widget_visibility(
        ctx: Context,
        widget_name: str,
        child_widget_name: str,
        visibility: str
    ) -> Dict[str, Any]:
        """
        Set the visibility of a widget inside a Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            child_widget_name: Name of the child widget to update
            visibility: Visibility enum name, such as Visible, Hidden, Collapsed, HitTestInvisible, SelfHitTestInvisible

        Returns:
            Dict containing success status and updated visibility
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": child_widget_name,
                "visibility": visibility
            }

            logger.info(f"Setting widget visibility with params: {params}")
            response = unreal.send_command("set_widget_visibility", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Set widget visibility response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error setting widget visibility: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_widget_style(
        ctx: Context,
        widget_name: str,
        child_widget_name: str,
        style: Dict[str, Any]
    ) -> Dict[str, Any]:
        """
        Set stable style fields on a widget inside a Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            child_widget_name: Name of the child widget to update
            style: Style object. Currently supports stable fields on Button, ProgressBar, CheckBox, Border, and Image.

        Returns:
            Dict containing success status and updated style fields
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": child_widget_name,
                "style": style,
            }

            logger.info(f"Setting widget style with params: {params}")
            response = unreal.send_command("set_widget_style", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Set widget style response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error setting widget style: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_widget_brush(
        ctx: Context,
        widget_name: str,
        child_widget_name: str,
        brush_asset_path: str = "",
        texture_asset_path: str = "",
        material_asset_path: str = "",
        tint_color: List[float] = [],
        image_size: List[float] = [],
        match_size: bool = True
    ) -> Dict[str, Any]:
        """
        Set the brush resource or tint of a widget inside a Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            child_widget_name: Name of the child widget to update
            brush_asset_path: Optional SlateBrushAsset path
            texture_asset_path: Optional texture asset path
            material_asset_path: Optional material asset path
            tint_color: Optional [R, G, B, A] tint color
            image_size: Optional [Width, Height], currently only for Image
            match_size: Whether texture brush should adopt source texture size when supported

        Returns:
            Dict containing success status and updated brush info
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "widget_name": child_widget_name,
                "match_size": match_size,
            }
            if brush_asset_path:
                params["brush_asset_path"] = brush_asset_path
            if texture_asset_path:
                params["texture_asset_path"] = texture_asset_path
            if material_asset_path:
                params["material_asset_path"] = material_asset_path
            if tint_color:
                params["tint_color"] = tint_color
            if image_size:
                params["image_size"] = image_size

            logger.info(f"Setting widget brush with params: {params}")
            response = unreal.send_command("set_widget_brush", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Set widget brush response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error setting widget brush: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def create_widget_animation(
        ctx: Context,
        widget_name: str,
        animation_name: str = "",
        start_time: float = 0.0,
        end_time: float = 1.0,
        display_rate: int = 20
    ) -> Dict[str, Any]:
        """
        Create a Widget animation inside a Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            animation_name: Optional animation name; auto-generates a unique name when empty
            start_time: Animation start time in seconds
            end_time: Animation end time in seconds
            display_rate: Display frame rate used by the animation timeline

        Returns:
            Dict containing success status and animation metadata
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "start_time": start_time,
                "end_time": end_time,
                "display_rate": display_rate,
            }
            if animation_name:
                params["animation_name"] = animation_name

            logger.info(f"Creating widget animation with params: {params}")
            response = unreal.send_command("create_widget_animation", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Create widget animation response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error creating widget animation: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_widget_animation_keyframe(
        ctx: Context,
        widget_name: str,
        animation_name: str,
        target_widget_name: str,
        property_name: str,
        time: float,
        value: Any,
        interpolation: str = "cubic"
    ) -> Dict[str, Any]:
        """
        Add a keyframe to a Widget animation track inside a Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            animation_name: Name of the target Widget animation
            target_widget_name: Name of the widget to animate
            property_name: Animated property name. Supports render_opacity and common render_transform channels
            time: Keyframe time in seconds
            value: Keyframe value. Number for scalar channels, [X, Y] for 2D channels
            interpolation: Key interpolation mode: cubic, linear, constant

        Returns:
            Dict containing success status and written channel information
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name,
                "animation_name": animation_name,
                "target_widget_name": target_widget_name,
                "property_name": property_name,
                "time": time,
                "value": value,
                "interpolation": interpolation,
            }

            logger.info(f"Adding widget animation keyframe with params: {params}")
            response = unreal.send_command("add_widget_animation_keyframe", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add widget animation keyframe response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding widget animation keyframe: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def open_widget_blueprint_editor(
        ctx: Context,
        widget_name: str
    ) -> Dict[str, Any]:
        """
        Open the Widget Blueprint editor for a Widget Blueprint asset.

        Args:
            widget_name: Name of the target Widget Blueprint

        Returns:
            Dict containing success status and resolved asset information
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": widget_name
            }

            logger.info(f"Opening Widget Blueprint editor with params: {params}")
            response = unreal.send_command("open_widget_blueprint_editor", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Open Widget Blueprint editor response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error opening Widget Blueprint editor: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    logger.info("UMG tools registered successfully") 
