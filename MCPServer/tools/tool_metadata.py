"""
UnrealMCP 工具元数据提取工具。

通过静态解析 `Python/tools` 下的工具注册文件，导出当前 MCP 工具列表与输入参数 schema。
"""

from __future__ import annotations

import ast
import re
from functools import lru_cache
from pathlib import Path
from typing import Any, Dict, List, Optional


TOOLS_DIRECTORY = Path(__file__).resolve().parent
PLUGIN_ROOT = TOOLS_DIRECTORY.parents[1]


def _is_tool_decorator(decorator: ast.expr) -> bool:
    """判断装饰器是否为 `@mcp.tool()`。"""
    if not isinstance(decorator, ast.Call):
        return False

    if not isinstance(decorator.func, ast.Attribute):
        return False

    return decorator.func.attr == "tool"


def _annotation_to_text(annotation: Optional[ast.expr]) -> str:
    """把类型注解节点转换为字符串。"""
    if annotation is None:
        return ""
    return ast.unparse(annotation)


def _literal_or_text(node: ast.AST) -> Any:
    """尽量把默认值还原成字面量，失败则退回源码文本。"""
    try:
        return ast.literal_eval(node)
    except Exception:
        return ast.unparse(node)


def _extract_subscript_parts(annotation: ast.Subscript) -> tuple[str, List[ast.expr]]:
    """提取泛型名称和泛型参数列表。"""
    container_name = _annotation_to_text(annotation.value)
    slice_node = annotation.slice

    if isinstance(slice_node, ast.Tuple):
        return container_name, list(slice_node.elts)

    return container_name, [slice_node]


def _annotation_to_schema(annotation: Optional[ast.expr]) -> Dict[str, Any]:
    """把 Python 注解转换为简单 JSON schema。"""
    if annotation is None:
        return {}

    if isinstance(annotation, ast.Name):
        if annotation.id == "str":
            return {"type": "string"}
        if annotation.id == "int":
            return {"type": "integer"}
        if annotation.id == "float":
            return {"type": "number"}
        if annotation.id == "bool":
            return {"type": "boolean"}
        if annotation.id in {"dict", "Dict"}:
            return {"type": "object"}
        if annotation.id in {"list", "List"}:
            return {"type": "array"}
        return {}

    if isinstance(annotation, ast.Subscript):
        container_name, generic_args = _extract_subscript_parts(annotation)

        if container_name in {"list", "List"}:
            item_schema = _annotation_to_schema(generic_args[0]) if generic_args else {}
            schema: Dict[str, Any] = {"type": "array"}
            if item_schema:
                schema["items"] = item_schema
            return schema

        if container_name in {"dict", "Dict"}:
            value_schema = _annotation_to_schema(generic_args[1]) if len(generic_args) > 1 else {}
            schema = {"type": "object"}
            if value_schema:
                schema["additionalProperties"] = value_schema
            return schema

        if container_name == "Optional" and generic_args:
            schema = _annotation_to_schema(generic_args[0])
            if schema:
                schema = dict(schema)
                schema["nullable"] = True
            else:
                schema = {"nullable": True}
            return schema

        if container_name == "Union":
            non_none_types: List[ast.expr] = []
            nullable = False
            for generic_arg in generic_args:
                if isinstance(generic_arg, ast.Constant) and generic_arg.value is None:
                    nullable = True
                    continue
                if isinstance(generic_arg, ast.Name) and generic_arg.id == "None":
                    nullable = True
                    continue
                non_none_types.append(generic_arg)

            if len(non_none_types) == 1:
                schema = _annotation_to_schema(non_none_types[0])
                if nullable:
                    schema = dict(schema)
                    schema["nullable"] = True
                return schema

            any_of = [_annotation_to_schema(item) for item in non_none_types]
            any_of = [item for item in any_of if item]
            if any_of:
                schema = {"anyOf": any_of}
                if nullable:
                    schema["nullable"] = True
                return schema
            return {"nullable": nullable} if nullable else {}

    return {}


def _parse_parameter_docs(docstring: str) -> Dict[str, str]:
    """从 Google 风格 docstring 中抽取参数说明。"""
    parameter_docs: Dict[str, str] = {}
    lines = docstring.splitlines()
    in_args_section = False
    current_name = ""

    for line in lines:
        stripped = line.strip()

        if stripped == "Args:":
            in_args_section = True
            current_name = ""
            continue

        if not in_args_section:
            continue

        if not stripped:
            current_name = ""
            continue

        if stripped.endswith(":") and stripped not in {"Args:"}:
            break

        match = re.match(r"^\s{4}([A-Za-z_][A-Za-z0-9_]*):\s*(.*)$", line)
        if match:
            current_name = match.group(1)
            parameter_docs[current_name] = match.group(2).strip()
            continue

        if current_name and line.startswith(" " * 8):
            extra_text = stripped
            if extra_text:
                parameter_docs[current_name] = (
                    f"{parameter_docs[current_name]} {extra_text}".strip()
                )
            continue

        if not line.startswith(" " * 4):
            break

    return parameter_docs


def _build_parameter_metadata(
    function_node: ast.FunctionDef | ast.AsyncFunctionDef,
    parameter_docs: Dict[str, str]
) -> List[Dict[str, Any]]:
    """构建函数参数元数据列表。"""
    parameters: List[Dict[str, Any]] = []
    args = list(function_node.args.args)
    defaults = list(function_node.args.defaults)
    default_offset = len(args) - len(defaults)

    for index, arg in enumerate(args):
        if arg.arg == "ctx":
            continue

        default_value = None
        has_default = index >= default_offset
        if has_default:
            default_value = _literal_or_text(defaults[index - default_offset])

        parameter_schema = _annotation_to_schema(arg.annotation)
        parameter_metadata: Dict[str, Any] = {
            "name": arg.arg,
            "annotation": _annotation_to_text(arg.annotation),
            "required": not has_default,
            "description": parameter_docs.get(arg.arg, ""),
        }

        if parameter_schema:
            parameter_metadata["schema"] = parameter_schema

        if has_default:
            parameter_metadata["default"] = default_value

        parameters.append(parameter_metadata)

    return parameters


def _build_tool_metadata(
    module_path: Path,
    function_node: ast.FunctionDef | ast.AsyncFunctionDef
) -> Dict[str, Any]:
    """根据 AST 节点构建单个工具的元数据。"""
    docstring = ast.get_docstring(function_node) or ""
    parameter_docs = _parse_parameter_docs(docstring)
    parameters = _build_parameter_metadata(function_node, parameter_docs)

    return {
        "name": function_node.name,
        "group": module_path.stem.replace("_tools", ""),
        "module": module_path.stem,
        "source_file": str(module_path.relative_to(PLUGIN_ROOT)).replace("\\", "/"),
        "description": docstring.splitlines()[0].strip() if docstring else "",
        "docstring": docstring,
        "parameters": parameters,
        "parameter_count": len(parameters),
    }


def _collect_module_tools(module_path: Path) -> List[Dict[str, Any]]:
    """提取单个注册文件中的所有 MCP 工具定义。"""
    syntax_tree = ast.parse(module_path.read_text(encoding="utf-8"))
    tools: List[Dict[str, Any]] = []

    for node in syntax_tree.body:
        if not isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            continue

        if not node.name.startswith("register_"):
            continue

        for child in node.body:
            if not isinstance(child, (ast.FunctionDef, ast.AsyncFunctionDef)):
                continue

            if not any(_is_tool_decorator(decorator) for decorator in child.decorator_list):
                continue

            tools.append(_build_tool_metadata(module_path, child))

    return tools


@lru_cache(maxsize=1)
def collect_tool_metadata() -> List[Dict[str, Any]]:
    """静态收集当前 `Python/tools` 下全部公开工具元数据。"""
    all_tools: List[Dict[str, Any]] = []
    for module_path in sorted(TOOLS_DIRECTORY.glob("*_tools.py")):
        all_tools.extend(_collect_module_tools(module_path))

    all_tools.sort(key=lambda item: item["name"])
    return all_tools


def list_tool_metadata(
    group: str = "",
    name_contains: str = "",
    include_parameters: bool = False
) -> Dict[str, Any]:
    """列出工具清单，并按需附带参数摘要。"""
    tools = collect_tool_metadata()
    normalized_group = group.strip().lower()
    normalized_name_contains = name_contains.strip().lower()

    filtered_tools: List[Dict[str, Any]] = []
    for tool in tools:
        if normalized_group and tool["group"].lower() != normalized_group:
            continue
        if normalized_name_contains and normalized_name_contains not in tool["name"].lower():
            continue

        tool_summary = {
            "name": tool["name"],
            "group": tool["group"],
            "module": tool["module"],
            "source_file": tool["source_file"],
            "description": tool["description"],
            "parameter_count": tool["parameter_count"],
            "parameter_names": [parameter["name"] for parameter in tool["parameters"]],
        }
        if include_parameters:
            tool_summary["parameters"] = tool["parameters"]

        filtered_tools.append(tool_summary)

    return {
        "success": True,
        "group": group,
        "name_contains": name_contains,
        "include_parameters": include_parameters,
        "tool_count": len(filtered_tools),
        "total_tool_count": len(tools),
        "tools": filtered_tools,
    }


def export_tool_schema(tool_name: str = "", group: str = "") -> Dict[str, Any]:
    """导出工具输入 schema。"""
    tools = collect_tool_metadata()
    normalized_tool_name = tool_name.strip().lower()
    normalized_group = group.strip().lower()

    exported_tools: List[Dict[str, Any]] = []
    for tool in tools:
        if normalized_tool_name and tool["name"].lower() != normalized_tool_name:
            continue
        if normalized_group and tool["group"].lower() != normalized_group:
            continue

        properties: Dict[str, Any] = {}
        required: List[str] = []

        for parameter in tool["parameters"]:
            parameter_schema = dict(parameter.get("schema", {}))
            if parameter["description"]:
                parameter_schema["description"] = parameter["description"]
            if "default" in parameter:
                parameter_schema["default"] = parameter["default"]

            properties[parameter["name"]] = parameter_schema
            if parameter["required"]:
                required.append(parameter["name"])

        exported_tools.append({
            "name": tool["name"],
            "group": tool["group"],
            "module": tool["module"],
            "source_file": tool["source_file"],
            "description": tool["description"],
            "input_schema": {
                "type": "object",
                "properties": properties,
                "required": required,
                "additionalProperties": False,
            },
        })

    return {
        "success": True,
        "tool_name": tool_name,
        "group": group,
        "tool_count": len(exported_tools),
        "total_tool_count": len(tools),
        "schema_version": "1.0",
        "tools": exported_tools,
    }
