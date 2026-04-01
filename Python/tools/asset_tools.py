"""
Asset Tools for Unreal MCP.

This module provides tools for searching assets and retrieving structured asset metadata.
"""

import logging
from typing import Dict, Any
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

    logger.info("资产工具注册完成")
