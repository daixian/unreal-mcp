"""
Asset Tools for Unreal MCP.

This module provides tools for searching assets and retrieving structured asset metadata.
"""

import logging
from typing import Dict, Any, List, Optional
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


def _build_prefixed_asset_lookup_params(
    prefix: str,
    asset_path: str,
    object_path: str,
    asset_name: str,
    name: str
) -> Dict[str, Any]:
    """Build prefixed asset lookup params for target/replacement style commands."""
    base_params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
    return {f"{prefix}{key}": value for key, value in base_params.items()}


def register_asset_tools(mcp: FastMCP):
    """Register asset tools with the MCP server."""

    @mcp.tool()
    def search_assets(
        ctx: Context,
        path: str = "/Game",
        query: str = "",
        query_mode: str = "contains",
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
            "query_mode": query_mode,
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
    def create_asset(
        ctx: Context,
        name: str,
        asset_class: str,
        path: str = "/Game",
        factory_class: str = "",
        parent_class: str = "",
        unique_name: bool = False,
        save_asset: bool = True
    ) -> Dict[str, Any]:
        """Create a generic asset through a UE factory, using a built-in default factory when available."""
        del ctx

        params: Dict[str, Any] = {
            "name": name,
            "asset_class": asset_class,
            "path": path,
            "factory_class": factory_class,
            "parent_class": parent_class,
            "unique_name": unique_name,
            "save_asset": save_asset,
        }
        return _send_asset_command("create_asset", params)

    @mcp.tool()
    def create_curve(
        ctx: Context,
        curve_name: str,
        path: str = "/Game",
        curve_type: str = "CurveFloat",
        unique_name: bool = False,
        save_asset: bool = True
    ) -> Dict[str, Any]:
        """Create a curve asset such as CurveFloat, CurveVector, or CurveLinearColor."""
        del ctx

        params: Dict[str, Any] = {
            "curve_name": curve_name,
            "name": curve_name,
            "path": path,
            "curve_type": curve_type,
            "unique_name": unique_name,
            "save_asset": save_asset,
        }
        return _send_asset_command("create_curve", params)

    @mcp.tool()
    def create_data_asset(
        ctx: Context,
        data_asset_name: str,
        data_asset_class: str,
        path: str = "/Game",
        unique_name: bool = False,
        save_asset: bool = True
    ) -> Dict[str, Any]:
        """Create a DataAsset instance from DataAsset or one of its subclasses."""
        del ctx

        params: Dict[str, Any] = {
            "data_asset_name": data_asset_name,
            "name": data_asset_name,
            "data_asset_class": data_asset_class,
            "path": path,
            "unique_name": unique_name,
            "save_asset": save_asset,
        }
        return _send_asset_command("create_data_asset", params)

    @mcp.tool()
    def create_primary_data_asset(
        ctx: Context,
        data_asset_name: str,
        data_asset_class: str,
        path: str = "/Game",
        unique_name: bool = False,
        save_asset: bool = True
    ) -> Dict[str, Any]:
        """Create a PrimaryDataAsset instance from PrimaryDataAsset or one of its subclasses."""
        del ctx

        params: Dict[str, Any] = {
            "primary_data_asset_name": data_asset_name,
            "name": data_asset_name,
            "data_asset_class": data_asset_class,
            "path": path,
            "unique_name": unique_name,
            "save_asset": save_asset,
        }
        return _send_asset_command("create_primary_data_asset", params)

    @mcp.tool()
    def create_data_table(
        ctx: Context,
        table_name: str,
        row_struct: str,
        path: str = "/Game",
        unique_name: bool = False,
        save_asset: bool = True
    ) -> Dict[str, Any]:
        """Create a DataTable asset with the given row struct."""
        del ctx

        params: Dict[str, Any] = {
            "table_name": table_name,
            "name": table_name,
            "row_struct": row_struct,
            "path": path,
            "unique_name": unique_name,
            "save_asset": save_asset,
        }
        return _send_asset_command("create_data_table", params)

    @mcp.tool()
    def get_data_table_rows(
        ctx: Context,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = "",
        row_name: str = ""
    ) -> Dict[str, Any]:
        """Read one or all rows from a DataTable asset."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        if row_name:
            params["row_name"] = row_name
        return _send_asset_command("get_data_table_rows", params)

    @mcp.tool()
    def import_data_table(
        ctx: Context,
        source_file: str,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = "",
        format: str = "auto",
        save_asset: bool = True
    ) -> Dict[str, Any]:
        """Import CSV or JSON data into an existing DataTable asset."""
        del ctx

        if not source_file.strip():
            return {"success": False, "message": "source_file is required"}

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        params["source_file"] = source_file
        params["format"] = format
        params["save_asset"] = save_asset
        return _send_asset_command("import_data_table", params)

    @mcp.tool()
    def set_data_table_row(
        ctx: Context,
        row_name: str,
        row_data: Dict[str, Any],
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = "",
        save_asset: bool = True
    ) -> Dict[str, Any]:
        """Create or replace a single DataTable row using exported JSON field names."""
        del ctx

        if not row_name.strip():
            return {"success": False, "message": "row_name is required"}
        if not isinstance(row_data, dict):
            return {"success": False, "message": "row_data must be an object"}

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        params["row_name"] = row_name
        params["row_data"] = row_data
        params["save_asset"] = save_asset
        return _send_asset_command("set_data_table_row", params)

    @mcp.tool()
    def export_data_table(
        ctx: Context,
        export_file: str,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = "",
        format: str = "auto"
    ) -> Dict[str, Any]:
        """Export an existing DataTable asset to a CSV or JSON file."""
        del ctx

        if not export_file.strip():
            return {"success": False, "message": "export_file is required"}

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        params["export_file"] = export_file
        params["format"] = format
        return _send_asset_command("export_data_table", params)

    @mcp.tool()
    def remove_data_table_row(
        ctx: Context,
        row_name: str,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = "",
        save_asset: bool = True
    ) -> Dict[str, Any]:
        """Remove a single row from a DataTable asset."""
        del ctx

        if not row_name.strip():
            return {"success": False, "message": "row_name is required"}

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        params["row_name"] = row_name
        params["save_asset"] = save_asset
        return _send_asset_command("remove_data_table_row", params)

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
    def checkout_asset(
        ctx: Context,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = ""
    ) -> Dict[str, Any]:
        """Checkout a loaded asset resolved by path, object path, or asset name."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        return _send_asset_command("checkout_asset", params)

    @mcp.tool()
    def submit_asset(
        ctx: Context,
        description: str,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = "",
        save_asset: bool = True,
        silent: bool = False,
        keep_checked_out: bool = False
    ) -> Dict[str, Any]:
        """Submit a loaded asset through the current source control provider."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        if not description.strip():
            return {"success": False, "message": "description is required"}

        params["description"] = description
        params["save_asset"] = save_asset
        params["silent"] = silent
        params["keep_checked_out"] = keep_checked_out
        return _send_asset_command("submit_asset", params)

    @mcp.tool()
    def revert_asset(
        ctx: Context,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = "",
        silent: bool = False
    ) -> Dict[str, Any]:
        """Revert a loaded asset through the current source control provider."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        params["silent"] = silent
        return _send_asset_command("revert_asset", params)

    @mcp.tool()
    def get_source_control_status(
        ctx: Context,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = ""
    ) -> Dict[str, Any]:
        """Query source control state for a loaded asset."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        return _send_asset_command("get_source_control_status", params)

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
    def batch_rename_assets(
        ctx: Context,
        operations: List[Dict[str, Any]],
        overwrite: bool = False,
        stop_on_error: bool = True
    ) -> Dict[str, Any]:
        """Rename multiple assets in one request."""
        del ctx

        if not operations:
            return {"success": False, "message": "operations is required"}
        return _send_asset_command("batch_rename_assets", {
            "operations": operations,
            "overwrite": overwrite,
            "stop_on_error": stop_on_error,
        })

    @mcp.tool()
    def batch_move_assets(
        ctx: Context,
        asset_paths: List[str],
        destination_path: str,
        overwrite: bool = False,
        stop_on_error: bool = True
    ) -> Dict[str, Any]:
        """Move multiple assets into the same destination path."""
        del ctx

        if not asset_paths:
            return {"success": False, "message": "asset_paths is required"}
        return _send_asset_command("batch_move_assets", {
            "asset_paths": asset_paths,
            "destination_path": destination_path,
            "overwrite": overwrite,
            "stop_on_error": stop_on_error,
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
    def set_asset_metadata(
        ctx: Context,
        metadata: Optional[Dict[str, Any]] = None,
        remove_metadata_keys: Optional[List[str]] = None,
        clear_existing: bool = False,
        save_asset: bool = True,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = ""
    ) -> Dict[str, Any]:
        """Set metadata tags on a loaded asset and optionally remove existing keys."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}

        metadata = metadata or {}
        remove_metadata_keys = remove_metadata_keys or []
        if not metadata and not remove_metadata_keys and not clear_existing:
            return {"success": False, "message": "metadata, remove_metadata_keys or clear_existing is required"}

        if metadata:
            params["metadata"] = metadata
        if remove_metadata_keys:
            params["remove_metadata_keys"] = remove_metadata_keys
        params["clear_existing"] = clear_existing
        params["save_asset"] = save_asset
        return _send_asset_command("set_asset_metadata", params)

    @mcp.tool()
    def consolidate_assets(
        ctx: Context,
        source_assets: List[str],
        target_asset_path: str = "",
        target_object_path: str = "",
        target_asset_name: str = "",
        target_name: str = ""
    ) -> Dict[str, Any]:
        """Consolidate duplicate assets into one target asset."""
        del ctx

        params = _build_prefixed_asset_lookup_params(
            "target_",
            target_asset_path,
            target_object_path,
            target_asset_name,
            target_name
        )
        if not params:
            return {"success": False, "message": "One of target_asset_path, target_object_path, target_asset_name or target_name is required"}
        if not source_assets:
            return {"success": False, "message": "source_assets is required"}

        params["source_assets"] = source_assets
        return _send_asset_command("consolidate_assets", params)

    @mcp.tool()
    def replace_asset_references(
        ctx: Context,
        assets_to_replace: List[str],
        replacement_asset_path: str = "",
        replacement_object_path: str = "",
        replacement_asset_name: str = "",
        replacement_name: str = ""
    ) -> Dict[str, Any]:
        """Replace references by using Unreal's delete-and-consolidate workflow."""
        del ctx

        params = _build_prefixed_asset_lookup_params(
            "replacement_",
            replacement_asset_path,
            replacement_object_path,
            replacement_asset_name,
            replacement_name
        )
        if not params:
            return {"success": False, "message": "One of replacement_asset_path, replacement_object_path, replacement_asset_name or replacement_name is required"}
        if not assets_to_replace:
            return {"success": False, "message": "assets_to_replace is required"}

        params["assets_to_replace"] = assets_to_replace
        return _send_asset_command("replace_asset_references", params)

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
    def create_material_function(
        ctx: Context,
        name: str,
        path: str = "/Game/Materials",
        function_class: str = "MaterialFunction"
    ) -> Dict[str, Any]:
        """Create a new Material Function asset."""
        del ctx

        return _send_asset_command("create_material_function", {
            "name": name,
            "path": path,
            "function_class": function_class,
        })

    @mcp.tool()
    def create_render_target(
        ctx: Context,
        name: str,
        path: str = "/Game/RenderTargets",
        width: int = 1024,
        height: int = 1024,
        format: str = "RTF_RGBA16F",
        clear_color: Optional[List[float]] = None,
        auto_generate_mips: bool = False
    ) -> Dict[str, Any]:
        """Create a new Texture Render Target 2D asset."""
        del ctx

        params: Dict[str, Any] = {
            "name": name,
            "path": path,
            "width": width,
            "height": height,
            "format": format,
            "auto_generate_mips": auto_generate_mips,
        }
        if clear_color is not None:
            if len(clear_color) != 4:
                return {"success": False, "message": "clear_color must contain 4 floats [R, G, B, A]"}
            params["clear_color"] = [float(channel) for channel in clear_color]
        return _send_asset_command("create_render_target", params)

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

    @mcp.tool()
    def assign_material_to_actor(
        ctx: Context,
        material_asset_path: str,
        name: str = "",
        actor_path: str = "",
        slot_index: int = 0,
        slot_name: str = "",
        world_type: str = "auto"
    ) -> Dict[str, Any]:
        """Assign a material to all compatible mesh components on an actor."""
        del ctx

        params: Dict[str, Any] = {
            "material_asset_path": material_asset_path,
            "world_type": world_type,
            "slot_index": slot_index,
        }
        if name:
            params["name"] = name
        if actor_path:
            params["actor_path"] = actor_path
        if slot_name:
            params["slot_name"] = slot_name
        if not name and not actor_path:
            return {"success": False, "message": "name or actor_path is required"}
        return _send_asset_command("assign_material_to_actor", params)

    @mcp.tool()
    def assign_material_to_component(
        ctx: Context,
        material_asset_path: str,
        component_name: str,
        name: str = "",
        actor_path: str = "",
        slot_index: int = 0,
        slot_name: str = "",
        world_type: str = "auto"
    ) -> Dict[str, Any]:
        """Assign a material to a specific mesh component on an actor."""
        del ctx

        params: Dict[str, Any] = {
            "material_asset_path": material_asset_path,
            "component_name": component_name,
            "world_type": world_type,
            "slot_index": slot_index,
        }
        if name:
            params["name"] = name
        if actor_path:
            params["actor_path"] = actor_path
        if slot_name:
            params["slot_name"] = slot_name
        if not name and not actor_path:
            return {"success": False, "message": "name or actor_path is required"}
        return _send_asset_command("assign_material_to_component", params)

    @mcp.tool()
    def replace_material_slot(
        ctx: Context,
        material_asset_path: str,
        name: str = "",
        actor_path: str = "",
        component_name: str = "",
        slot_index: int = -1,
        slot_name: str = "",
        world_type: str = "auto"
    ) -> Dict[str, Any]:
        """Replace a specific material slot on an actor or mesh component."""
        del ctx

        if not name and not actor_path:
            return {"success": False, "message": "name or actor_path is required"}
        if slot_index < 0 and not slot_name:
            return {"success": False, "message": "slot_index or slot_name is required"}

        params: Dict[str, Any] = {
            "material_asset_path": material_asset_path,
            "world_type": world_type,
        }
        if name:
            params["name"] = name
        if actor_path:
            params["actor_path"] = actor_path
        if component_name:
            params["component_name"] = component_name
        if slot_index >= 0:
            params["slot_index"] = slot_index
        if slot_name:
            params["slot_name"] = slot_name
        return _send_asset_command("replace_material_slot", params)

    @mcp.tool()
    def add_material_expression(
        ctx: Context,
        expression_class: str,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = "",
        node_position: Optional[List[int]] = None,
        selected_asset_path: str = "",
        property_values: Optional[Dict[str, Any]] = None
    ) -> Dict[str, Any]:
        """Add a material expression node to a Material graph."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}

        params["expression_class"] = expression_class
        if node_position is not None:
            if len(node_position) != 2:
                return {"success": False, "message": "node_position must contain 2 integers [X, Y]"}
            params["node_position"] = [int(node_position[0]), int(node_position[1])]
        if selected_asset_path:
            params["selected_asset_path"] = selected_asset_path
        if property_values:
            params["property_values"] = property_values
        return _send_asset_command("add_material_expression", params)

    @mcp.tool()
    def connect_material_expressions(
        ctx: Context,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = "",
        from_expression_path: str = "",
        from_expression_name: str = "",
        from_expression_index: int = -1,
        from_output_name: str = "",
        to_expression_path: str = "",
        to_expression_name: str = "",
        to_expression_index: int = -1,
        to_input_name: str = "",
        material_property: str = ""
    ) -> Dict[str, Any]:
        """Connect two material expressions, or connect an expression output to a material property."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}

        if not from_expression_path and not from_expression_name and from_expression_index < 0:
            return {"success": False, "message": "One of from_expression_path, from_expression_name or from_expression_index is required"}

        if material_property and (to_expression_path or to_expression_name or to_expression_index >= 0):
            return {"success": False, "message": "material_property cannot be used together with to_expression_*"}
        if not material_property and not to_expression_path and not to_expression_name and to_expression_index < 0:
            return {"success": False, "message": "Provide material_property or one of to_expression_path, to_expression_name or to_expression_index"}

        if from_expression_path:
            params["from_expression_path"] = from_expression_path
        if from_expression_name:
            params["from_expression_name"] = from_expression_name
        if from_expression_index >= 0:
            params["from_expression_index"] = from_expression_index
        if from_output_name:
            params["from_output_name"] = from_output_name

        if material_property:
            params["material_property"] = material_property
        else:
            if to_expression_path:
                params["to_expression_path"] = to_expression_path
            if to_expression_name:
                params["to_expression_name"] = to_expression_name
            if to_expression_index >= 0:
                params["to_expression_index"] = to_expression_index
            if to_input_name:
                params["to_input_name"] = to_input_name

        return _send_asset_command("connect_material_expressions", params)

    @mcp.tool()
    def layout_material_graph(
        ctx: Context,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = ""
    ) -> Dict[str, Any]:
        """Auto-layout expressions in a Material graph."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        return _send_asset_command("layout_material_graph", params)

    @mcp.tool()
    def compile_material(
        ctx: Context,
        asset_path: str = "",
        object_path: str = "",
        asset_name: str = "",
        name: str = ""
    ) -> Dict[str, Any]:
        """Recompile and save a Material asset."""
        del ctx

        params = _build_asset_lookup_params(asset_path, object_path, asset_name, name)
        if not params:
            return {"success": False, "message": "One of asset_path, object_path, asset_name or name is required"}
        return _send_asset_command("compile_material", params)

    logger.info("资产工具注册完成")
