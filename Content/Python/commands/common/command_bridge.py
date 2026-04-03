"""Bridge utilities for invoking local UnrealMCP Python command handlers."""

from __future__ import annotations

import importlib
import json
import sys
from typing import Any, Callable, Dict


class LocalCommandBridge:
    """Load a local Python module and execute a named handler."""

    @classmethod
    def run(
        cls,
        module_name: str,
        function_name: str,
        command_name: str,
        params_json: str,
    ) -> Dict[str, Any]:
        """Execute a local command handler and normalize its return payload."""
        module = cls._load_module(module_name)
        handler = cls._resolve_handler(module, function_name)
        params = cls._deserialize_params(params_json)
        result = handler(command_name, params)
        if not isinstance(result, dict):
            raise TypeError(f"Local handler '{module_name}.{function_name}' must return dict")
        return result

    @staticmethod
    def _load_module(module_name: str) -> Any:
        """Import or reload a module so iterative MCP debugging sees latest code."""
        if module_name in sys.modules:
            return importlib.reload(sys.modules[module_name])
        return importlib.import_module(module_name)

    @staticmethod
    def _resolve_handler(module: Any, function_name: str) -> Callable[[str, Dict[str, Any]], Dict[str, Any]]:
        handler = getattr(module, function_name, None)
        if handler is None or not callable(handler):
            raise AttributeError(f"Local handler not found: {module.__name__}.{function_name}")
        return handler

    @staticmethod
    def _deserialize_params(params_json: str) -> Dict[str, Any]:
        if not params_json:
            return {}
        params = json.loads(params_json)
        if not isinstance(params, dict):
            raise TypeError("Local command params must deserialize to dict")
        return params
