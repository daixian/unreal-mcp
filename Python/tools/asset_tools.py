"""
Asset Tools for Unreal MCP.

This module provides tools for searching assets and retrieving structured asset metadata.
"""

import logging
from typing import Dict, Any, List
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")


def _send_asset_command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
    """Send an asset-related command to Unreal Engine."""
    from unreal_mcp_server import get_unreal_connection

    try:
        unreal = get_unreal_connection()
        if not unreal:
            logger.error("连接 Unreal Engine 失败，命令: %s", command_name)
            return {"success": False, "message": "Failed to connect to Unreal Engine"}

        response = unreal.send_command(command_name, params)
        if not response:
            logger.error("Unreal Engine 没有返回结果，命令: %s", command_name)
            return {"success": False, "message": "No response from Unreal Engine"}

        logger.info("资产命令执行完成: %s", command_name)
        return response

    except Exception as exc:
        error_message = f"Error executing asset command '{command_name}': {exc}"
        logger.error("执行资产命令时出错: %s", error_message)
        return {"success": False, "message": error_message}


def _build_asset_lookup_params(
    asset_path: str,
    object_path: str,
    asset_name: str,
    name: str
) -> Dict[str, Any]:
    """Build asset lookup params using the first non-empty identifier(s)."""
    params: Dict[str, Any] = {}
    if asset_path:
        params["asset_path"] = asset_path
    if object_path:
        params["object_path"] = object_path
    if asset_name:
        params["asset_name"] = asset_name
    if name:
        params["name"] = name
    return params


def register_asset_tools(mcp: FastMCP):
    """Register asset tools with the MCP server."""

    @mcp.tool()
    def search_assets(
        ctx: Context,
        path: str = "/Game",
        query: str = "",
        class_name: str = "",
        recursive_paths: bool = True,
        include_tags: bool = False,
        limit: int = 50
    ) -> Dict[str, Any]:
        """Search for assets with optional path, class and query filters."""
        del ctx

        params = {
            "path": path,
            "query": query,
            "class_name": class_name,
            "recursive_paths": recursive_paths,
            "include_tags": include_tags,
            "limit": limit
        }
        return _send_asset_command("search_assets", params)

    @mcp.tool()
    def get_asset_metadata(
        ctx: Context,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = ""
    ) -> Dict[str, Any]:
        """Get metadata, tags, dependencies and referencers for an asset."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        return _send_asset_command("get_asset_metadata", params)

    @mcp.tool()
    def get_asset_dependencies(
        ctx: Context,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = ""
    ) -> Dict[str, Any]:
        """Get detailed dependencies for an asset."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        return _send_asset_command("get_asset_dependencies", params)

    @mcp.tool()
    def get_asset_referencers(
        ctx: Context,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = ""
    ) -> Dict[str, Any]:
        """Get detailed referencers for an asset."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        return _send_asset_command("get_asset_referencers", params)

    @mcp.tool()
    def get_asset_summary(
        ctx: Context,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = ""
    ) -> Dict[str, Any]:
        """Load an asset and return a structured summary based on its type, including WidgetBlueprint layout details when available."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        return _send_asset_command("get_asset_summary", params)

    @mcp.tool()
    def save_asset(
        ctx: Context,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = "",
        only_if_dirty: bool = False
    ) -> Dict[str, Any]:
        """Save a loaded asset resolved by path, object path, or asset name."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        params["only_if_dirty"] = only_if_dirty
        return _send_asset_command("save_asset", params)

    @mcp.tool()
    def import_asset(
        ctx: Context,
        destination_path: str,
        filename: str = "",
        source_files: List[str] = [],
        destination_name: str = "",
        replace_existing: bool = True,
        replace_existing_settings: bool = False,
        automated: bool = True,
        save: bool = True,
        async_import: bool = True
    ) -> Dict[str, Any]:
        """Import one or more external files into the Content Browser."""
        del ctx

        if not filename and not source_files:
            return {"success": False, "message": "filename or source_files is required"}

        params: Dict[str, Any] = {
            "destination_path": destination_path,
            "destination_name": destination_name,
            "replace_existing": replace_existing,
            "replace_existing_settings": replace_existing_settings,
            "automated": automated,
            "save": save,
            "async_import": async_import,
        }
        if filename:
            params["filename"] = filename
        if source_files:
            params["source_files"] = source_files
        return _send_asset_command("import_asset", params)

    @mcp.tool()
    def export_asset(
        ctx: Context,
        export_path: str,
        asset_path: str = "",
        asset_paths: List[str] = [],
        clean_filenames: bool = True
    ) -> Dict[str, Any]:
        """Export one or more assets to an external directory."""
        del ctx

        if not asset_path and not asset_paths:
            return {"success": False, "message": "asset_path or asset_paths is required"}

        params: Dict[str, Any] = {
            "export_path": export_path,
            "clean_filenames": clean_filenames,
        }
        if asset_path:
            params["asset_path"] = asset_path
        if asset_paths:
            params["asset_paths"] = asset_paths
        return _send_asset_command("export_asset", params)

    @mcp.tool()
    def reimport_asset(
        ctx: Context,
        asset_path: str = "",
        asset_paths: List[str] = [],
        object_path: str = "",
        asset_name: str = "",
        name: str = "",
        source_file_index: int = -1,
        ask_for_new_file_if_missing: bool = False,
        show_notification: bool = True,
        force_new_file: bool = False,
        automated: bool = True,
        force_show_dialog: bool = False,
        preferred_reimport_file: str = ""
    ) -> Dict[str, Any]:
        """Reimport one or more assets from their recorded source files."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if asset_paths:
            params["asset_paths"] = asset_paths
        if not params:
            return {"success": False, "message": "One of asset_path, asset_paths, object_path, asset_name or name is required"}

        params["source_file_index"] = source_file_index
        params["ask_for_new_file_if_missing"] = ask_for_new_file_if_missing
        params["show_notification"] = show_notification
        params["force_new_file"] = force_new_file
        params["automated"] = automated
        params["force_show_dialog"] = force_show_dialog
        if preferred_reimport_file:
            params["preferred_reimport_file"] = preferred_reimport_file
        return _send_asset_command("reimport_asset", params)

    @mcp.tool()
    def fixup_redirectors(
        ctx: Context,
        directory_path: str = "",
        directory_paths: List[str] = [],
        asset_path: str = "",
        asset_paths: List[str] = [],
        recursive_paths: bool = True,
        checkout_dialog_prompt: bool = False,
        fixup_mode: str = "delete_fixed_up_redirectors"
    ) -> Dict[str, Any]:
        """Fix redirector references under given paths or from explicit redirector assets."""
        del ctx

        if not directory_path and not directory_paths and not asset_path and not asset_paths:
            return {"success": False, "message": "One of directory_path, directory_paths, asset_path or asset_paths is required"}

        params: Dict[str, Any] = {
            "recursive_paths": recursive_paths,
            "checkout_dialog_prompt": checkout_dialog_prompt,
            "fixup_mode": fixup_mode,
        }
        if directory_path:
            params["directory_path"] = directory_path
        if directory_paths:
            params["directory_paths"] = directory_paths
        if asset_path:
            params["asset_path"] = asset_path
        if asset_paths:
            params["asset_paths"] = asset_paths
        return _send_asset_command("fixup_redirectors", params)

    @mcp.tool()
    def rename_asset(
        ctx: Context,
        source_asset_path: str,
        destination_asset_path: str
    ) -> Dict[str, Any]:
        """Rename an asset to another content path."""
        del ctx

        return _send_asset_command("rename_asset", {
            "source_asset_path": source_asset_path,
            "destination_asset_path": destination_asset_path
        })

    @mcp.tool()
    def move_asset(
        ctx: Context,
        source_asset_path: str,
        destination_asset_path: str
    ) -> Dict[str, Any]:
        """Move an asset to another content path."""
        del ctx

        return _send_asset_command("move_asset", {
            "source_asset_path": source_asset_path,
            "destination_asset_path": destination_asset_path
        })

    @mcp.tool()
    def delete_asset(
        ctx: Context,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = ""
    ) -> Dict[str, Any]:
        """Delete a single asset resolved by path or name."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        return _send_asset_command("delete_asset", params)

    @mcp.tool()
    def get_selected_assets(
        ctx: Context,
        include_tags: bool = False
    ) -> Dict[str, Any]:
        """Get assets currently selected in the Content Browser."""
        del ctx

        return _send_asset_command("get_selected_assets", {
            "include_tags": include_tags
        })

    @mcp.tool()
    def sync_content_browser_to_assets(
        ctx: Context,
        asset_paths: list[str]
    ) -> Dict[str, Any]:
        """Sync the Content Browser selection to the provided asset paths."""
        del ctx

        if not asset_paths:
            return {"success": False, "message": "asset_paths is required"}
        return _send_asset_command("sync_content_browser_to_assets", {
            "asset_paths": asset_paths
        })

    @mcp.tool()
    def save_all_dirty_assets(ctx: Context) -> Dict[str, Any]:
        """Save all dirty assets and maps."""
        del ctx

        return _send_asset_command("save_all_dirty_assets", {})

    @mcp.tool()
    def get_blueprint_summary(
        ctx: Context,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = ""
    ) -> Dict[str, Any]:
        """Load a Blueprint asset and return Blueprint-specific structured data, including detailed WidgetBlueprint widget_tree, slot layout, bindings and animations."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        return _send_asset_command("get_blueprint_summary", params)

    @mcp.tool()
    def create_material(
        ctx: Context,
        name: str,
        path: str = "/Game/Materials"
    ) -> Dict[str, Any]:
        """Create a new Material asset."""
        del ctx

        return _send_asset_command("create_material", {
            "name": name,
            "path": path
        })

    @mcp.tool()
    def create_material_instance(
        ctx: Context,
        name: str,
        parent_material: str,
        path: str = "/Game/Materials"
    ) -> Dict[str, Any]:
        """Create a new Material Instance Constant asset."""
        del ctx

        return _send_asset_command("create_material_instance", {
            "name": name,
            "parent_material": parent_material,
            "path": path
        })

    @mcp.tool()
    def get_material_parameters(
        ctx: Context,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = ""
    ) -> Dict[str, Any]:
        """Get scalar, vector, texture and static switch parameters from a Material or Material Instance."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        return _send_asset_command("get_material_parameters", params)

    @mcp.tool()
    def set_material_instance_scalar_parameter(
        ctx: Context,
        parameter_name: str,
        value: float,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = ""
    ) -> Dict[str, Any]:
        """Set a scalar parameter on a Material Instance Constant."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        params["parameter_name"] = parameter_name
        params["value"] = value
        return _send_asset_command("set_material_instance_scalar_parameter", params)

    @mcp.tool()
    def set_material_instance_vector_parameter(
        ctx: Context,
        parameter_name: str,
        value: List[float],
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = ""
    ) -> Dict[str, Any]:
        """Set a vector parameter on a Material Instance Constant."""
        del ctx

        if len(value) != 4:
            return {"success": False, "message": "value must contain 4 floats [R, G, B, A]"}

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        params["parameter_name"] = parameter_name
        params["value"] = [float(channel) for channel in value]
        return _send_asset_command("set_material_instance_vector_parameter", params)

    @mcp.tool()
    def set_material_instance_texture_parameter(
        ctx: Context,
        parameter_name: str,
        texture_asset_path: str,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = ""
    ) -> Dict[str, Any]:
        """Set a texture parameter on a Material Instance Constant."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        params["parameter_name"] = parameter_name
        params["texture_asset_path"] = texture_asset_path
        return _send_asset_command("set_material_instance_texture_parameter", params)

    logger.info("资产工具注册完成")
