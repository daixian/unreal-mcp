"""
Metadata Tools for Unreal MCP.

This module provides tool self-description capabilities for UnrealMCP.
"""

import logging
from typing import Dict, Any
from mcp.server.fastmcp import FastMCP, Context

from tools.tool_metadata import list_tool_metadata, export_tool_schema as build_tool_schema_export


logger = logging.getLogger("UnrealMCP")


def register_meta_tools(mcp: FastMCP):
    """Register metadata tools with the MCP server."""

    @mcp.tool()
    def list_mcp_tools(
        ctx: Context,
        group: str = "",
        name_contains: str = "",
        include_parameters: bool = False
    ) -> Dict[str, Any]:
        """List currently registered UnrealMCP tools."""
        del ctx

        try:
            result = list_tool_metadata(
                group=group,
                name_contains=name_contains,
                include_parameters=include_parameters,
            )
            logger.info("列出 MCP 工具完成，命中数量=%s", result.get("tool_count", 0))
            return result
        except Exception as exc:
            logger.error("列出 MCP 工具失败: %s", exc)
            return {"success": False, "message": str(exc)}

    @mcp.tool()
    def export_tool_schema(
        ctx: Context,
        tool_name: str = "",
        group: str = ""
    ) -> Dict[str, Any]:
        """Export input schemas for UnrealMCP tools."""
        del ctx

        try:
            result = build_tool_schema_export(tool_name=tool_name, group=group)
            logger.info("导出工具 Schema 完成，命中数量=%s", result.get("tool_count", 0))
            return result
        except Exception as exc:
            logger.error("导出工具 Schema 失败: %s", exc)
            return {"success": False, "message": str(exc)}

    logger.info("元数据工具注册完成")
