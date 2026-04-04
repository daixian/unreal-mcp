"""Local Python implementations for project-level UnrealMCP commands."""

from __future__ import annotations

from dataclasses import dataclass
import enum
from typing import Any, Dict, List, Tuple

import unreal


class ProjectCommandError(RuntimeError):
    """Raised when a project command cannot be completed."""


@dataclass
class ResolvedProjectWorld:
    """Runtime world selected for a project command."""

    world: unreal.World
    resolved_world_type: str


@dataclass
class ResolvedRuntimePlayer:
    """Runtime local player context used by Enhanced Input assignment."""

    world: unreal.World
    resolved_world_type: str
    player_index: int
    local_player: unreal.LocalPlayer
    input_subsystem: unreal.EnhancedInputLocalPlayerSubsystem
    controller: unreal.PlayerController | None
    target_actor: unreal.Actor | None


class InputMappingCommandHelper:
    """Handle legacy input mapping commands with local Python APIs."""

    def create_input_mapping(self, command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        mapping_name = self._require_mapping_name(params)
        key_name = self._require_string(params, "key")
        mapping_key = self._parse_input_key(key_name)
        input_settings = self._get_input_settings()

        input_type = str(params.get("input_type", "Action")).strip() or "Action"
        is_axis_mapping = command_name == "create_input_axis_mapping" or input_type.casefold() == "axis"
        normalized_input_type = "Axis" if is_axis_mapping else "Action"

        if is_axis_mapping:
            scale = float(params.get("scale", 1.0))
            axis_mapping = unreal.InputAxisKeyMapping()
            axis_mapping.axis_name = unreal.Name(mapping_name)
            axis_mapping.key = mapping_key
            axis_mapping.scale = scale
            input_settings.add_axis_mapping(axis_mapping, False)
            self._save_input_settings(input_settings)
            return {
                "success": True,
                "mapping_name": mapping_name,
                "key": key_name,
                "input_type": normalized_input_type,
                "scale": scale,
                "implementation": "local_python",
            }

        action_mapping = unreal.InputActionKeyMapping()
        action_mapping.action_name = unreal.Name(mapping_name)
        action_mapping.key = mapping_key
        action_mapping.shift = bool(params.get("shift", False))
        action_mapping.ctrl = bool(params.get("ctrl", False))
        action_mapping.alt = bool(params.get("alt", False))
        action_mapping.cmd = bool(params.get("cmd", False))
        input_settings.add_action_mapping(action_mapping, False)
        self._save_input_settings(input_settings)
        return {
            "success": True,
            "mapping_name": mapping_name,
            "key": key_name,
            "input_type": normalized_input_type,
            "shift": bool(action_mapping.shift),
            "ctrl": bool(action_mapping.ctrl),
            "alt": bool(action_mapping.alt),
            "cmd": bool(action_mapping.cmd),
            "implementation": "local_python",
        }

    def list_input_mappings(self, params: Dict[str, Any]) -> Dict[str, Any]:
        input_settings = self._get_input_settings()
        input_type = str(params.get("input_type", "All")).strip() or "All"
        include_action = input_type.casefold() != "axis"
        include_axis = input_type.casefold() != "action"

        mapping_name_filter = str(params.get("mapping_name") or params.get("action_name") or "").strip()
        has_mapping_name_filter = bool(mapping_name_filter)

        key_filter_name = str(params.get("key", "")).strip()
        has_key_filter = bool(key_filter_name)
        key_filter = self._parse_input_key(key_filter_name) if has_key_filter else None

        action_mappings: List[Dict[str, Any]] = []
        axis_mappings: List[Dict[str, Any]] = []

        if include_action:
            for action_mapping in list(input_settings.get_editor_property("action_mappings") or []):
                if has_mapping_name_filter and str(action_mapping.action_name).casefold() != mapping_name_filter.casefold():
                    continue
                if has_key_filter and action_mapping.key.export_text() != key_filter.export_text():
                    continue
                action_mappings.append(self._serialize_action_mapping(action_mapping))

        if include_axis:
            for axis_mapping in list(input_settings.get_editor_property("axis_mappings") or []):
                if has_mapping_name_filter and str(axis_mapping.axis_name).casefold() != mapping_name_filter.casefold():
                    continue
                if has_key_filter and axis_mapping.key.export_text() != key_filter.export_text():
                    continue
                axis_mappings.append(self._serialize_axis_mapping(axis_mapping))

        return {
            "success": True,
            "input_type": input_type,
            "action_mappings": action_mappings,
            "axis_mappings": axis_mappings,
            "total_count": len(action_mappings) + len(axis_mappings),
            "implementation": "local_python",
        }

    def remove_input_mapping(self, params: Dict[str, Any]) -> Dict[str, Any]:
        mapping_name = self._require_mapping_name(params)
        input_settings = self._get_input_settings()
        input_type = str(params.get("input_type", "Action")).strip() or "Action"
        is_axis_mapping = input_type.casefold() == "axis"

        key_filter_name = str(params.get("key", "")).strip()
        has_key_filter = bool(key_filter_name)
        key_filter = self._parse_input_key(key_filter_name) if has_key_filter else None

        removed_count = 0
        if is_axis_mapping:
            has_scale_filter = "scale" in params
            scale_filter = float(params.get("scale", 1.0))
            for axis_mapping in list(input_settings.get_editor_property("axis_mappings") or []):
                if str(axis_mapping.axis_name).casefold() != mapping_name.casefold():
                    continue
                if has_key_filter and axis_mapping.key.export_text() != key_filter.export_text():
                    continue
                if has_scale_filter and not self._is_same_scale(float(axis_mapping.scale), scale_filter):
                    continue
                input_settings.remove_axis_mapping(axis_mapping, False)
                removed_count += 1
        else:
            shift_filter = self._optional_bool(params, "shift")
            ctrl_filter = self._optional_bool(params, "ctrl")
            alt_filter = self._optional_bool(params, "alt")
            cmd_filter = self._optional_bool(params, "cmd")
            for action_mapping in list(input_settings.get_editor_property("action_mappings") or []):
                if str(action_mapping.action_name).casefold() != mapping_name.casefold():
                    continue
                if has_key_filter and action_mapping.key.export_text() != key_filter.export_text():
                    continue
                if shift_filter is not None and bool(action_mapping.shift) != shift_filter:
                    continue
                if ctrl_filter is not None and bool(action_mapping.ctrl) != ctrl_filter:
                    continue
                if alt_filter is not None and bool(action_mapping.alt) != alt_filter:
                    continue
                if cmd_filter is not None and bool(action_mapping.cmd) != cmd_filter:
                    continue
                input_settings.remove_action_mapping(action_mapping, False)
                removed_count += 1

        if removed_count <= 0:
            raise ProjectCommandError(f"没有找到可删除的输入映射: {mapping_name}")

        self._save_input_settings(input_settings)

        result = {
            "success": True,
            "mapping_name": mapping_name,
            "input_type": "Axis" if is_axis_mapping else "Action",
            "removed_count": removed_count,
            "implementation": "local_python",
        }
        if has_key_filter:
            result["key"] = key_filter_name
        return result

    @staticmethod
    def _get_input_settings() -> unreal.InputSettings:
        input_settings = unreal.InputSettings.get_input_settings()
        if not input_settings:
            raise ProjectCommandError("获取输入设置失败")
        return input_settings

    @staticmethod
    def _save_input_settings(input_settings: unreal.InputSettings) -> None:
        input_settings.force_rebuild_keymaps()
        input_settings.save_key_mappings()

    @staticmethod
    def _parse_input_key(key_name: str) -> unreal.Key:
        key = unreal.Key()
        key.import_text(key_name)
        if not unreal.InputLibrary.key_is_valid(key):
            raise ProjectCommandError(f"无效按键名: {key_name}")
        return key

    @staticmethod
    def _serialize_action_mapping(action_mapping: unreal.InputActionKeyMapping) -> Dict[str, Any]:
        return {
            "mapping_name": str(action_mapping.action_name),
            "key": action_mapping.key.export_text(),
            "input_type": "Action",
            "shift": bool(action_mapping.shift),
            "ctrl": bool(action_mapping.ctrl),
            "alt": bool(action_mapping.alt),
            "cmd": bool(action_mapping.cmd),
        }

    @staticmethod
    def _serialize_axis_mapping(axis_mapping: unreal.InputAxisKeyMapping) -> Dict[str, Any]:
        return {
            "mapping_name": str(axis_mapping.axis_name),
            "key": axis_mapping.key.export_text(),
            "input_type": "Axis",
            "scale": float(axis_mapping.scale),
        }

    @staticmethod
    def _require_mapping_name(params: Dict[str, Any]) -> str:
        mapping_name = str(params.get("mapping_name") or params.get("action_name") or "").strip()
        if not mapping_name:
            raise ProjectCommandError("缺少 'mapping_name' 或 'action_name' 参数")
        return mapping_name

    @staticmethod
    def _require_string(params: Dict[str, Any], field_name: str) -> str:
        value = str(params.get(field_name, "")).strip()
        if not value:
            raise ProjectCommandError(f"缺少 '{field_name}' 参数")
        return value

    @staticmethod
    def _optional_bool(params: Dict[str, Any], field_name: str) -> bool | None:
        if field_name not in params:
            return None
        return bool(params[field_name])

    @staticmethod
    def _is_same_scale(left: float, right: float) -> bool:
        return abs(left - right) <= 1e-6


class ProjectSettingsCommandHelper:
    """Handle project settings queries with local Python APIs."""

    _SUPPORTED_SETTINGS = {
        "gamemapssettings": ("GameMapsSettings", unreal.GameMapsSettings.get_game_maps_settings, "DefaultEngine.ini"),
        "ugamemapssettings": ("GameMapsSettings", unreal.GameMapsSettings.get_game_maps_settings, "DefaultEngine.ini"),
        "maps": ("GameMapsSettings", unreal.GameMapsSettings.get_game_maps_settings, "DefaultEngine.ini"),
        "mapsandmodes": ("GameMapsSettings", unreal.GameMapsSettings.get_game_maps_settings, "DefaultEngine.ini"),
        "/script/enginesettings.gamemapssettings": (
            "GameMapsSettings",
            unreal.GameMapsSettings.get_game_maps_settings,
            "DefaultEngine.ini",
        ),
        "inputsettings": ("InputSettings", unreal.InputSettings.get_input_settings, "DefaultInput.ini"),
        "uinputsettings": ("InputSettings", unreal.InputSettings.get_input_settings, "DefaultInput.ini"),
        "input": ("InputSettings", unreal.InputSettings.get_input_settings, "DefaultInput.ini"),
        "/script/engine.inputsettings": ("InputSettings", unreal.InputSettings.get_input_settings, "DefaultInput.ini"),
    }

    _PROPERTY_TYPE_OVERRIDES = {
        "bool": "bool",
        "int": "int32",
        "float": "float",
        "str": "FString",
        "name": "FName",
        "text": "FText",
        "softobjectpath": "FSoftObjectPath",
        "softclasspath": "FSoftClassPath",
    }

    def get_project_setting(self, params: Dict[str, Any]) -> Dict[str, Any]:
        settings_class_text = self._require_string(params, "settings_class")
        property_name = self._require_property_name(params)
        settings_object, resolved_settings_class, config_file = self._resolve_settings_object(settings_class_text)

        property_value = self._read_property_value(settings_object, resolved_settings_class, property_name)
        serialized_value = self._serialize_property_value(property_value)

        return {
            "success": True,
            "settings_class": resolved_settings_class,
            "property_name": property_name,
            "property_cpp_type": serialized_value["property_cpp_type"],
            "exported_value": serialized_value["exported_value"],
            "value": serialized_value["value"],
            "config_file": config_file,
            "implementation": "local_python",
        }

    def set_project_setting(self, params: Dict[str, Any]) -> Dict[str, Any]:
        settings_class_text = self._require_string(params, "settings_class")
        property_name = self._require_property_name(params)
        if "value" not in params:
            raise ProjectCommandError("缺少 'value' 参数")

        settings_object, resolved_settings_class, config_file = self._resolve_settings_object(settings_class_text)
        if resolved_settings_class != "InputSettings":
            raise ProjectCommandError(
                f"set_project_setting 当前仅支持通过本地 Python 写入 InputSettings，当前为: {resolved_settings_class}"
            )

        current_value = self._read_property_value(settings_object, resolved_settings_class, property_name)
        coerced_value = self._coerce_project_property_value(
            params["value"],
            current_value,
            resolved_settings_class,
            property_name,
        )

        try:
            settings_object.set_editor_property(property_name, coerced_value)
        except Exception as exc:
            raise ProjectCommandError(
                f"写入设置 {resolved_settings_class}.{property_name} 失败"
            ) from exc

        self._persist_settings_object(settings_object, resolved_settings_class)

        updated_value = self._read_property_value(settings_object, resolved_settings_class, property_name)
        serialized_value = self._serialize_property_value(updated_value)
        return {
            "success": True,
            "settings_class": resolved_settings_class,
            "property_name": property_name,
            "property_cpp_type": serialized_value["property_cpp_type"],
            "exported_value": serialized_value["exported_value"],
            "value": serialized_value["value"],
            "config_file": config_file,
            "implementation": "local_python",
        }

    def _resolve_settings_object(
        self,
        settings_class_text: str,
    ) -> Tuple[Any, str, str]:
        normalized_settings_class = settings_class_text.strip()
        supported_entry = self._SUPPORTED_SETTINGS.get(normalized_settings_class.casefold())
        if supported_entry is None:
            raise ProjectCommandError(
                f"暂不支持 settings_class={normalized_settings_class}，目前支持: GameMapsSettings/InputSettings"
            )

        resolved_settings_class, getter, config_name = supported_entry
        settings_object = getter()
        if not settings_object:
            raise ProjectCommandError(f"获取 {resolved_settings_class} 失败")

        config_file = unreal.Paths.combine([unreal.Paths.project_config_dir(), config_name])
        return settings_object, resolved_settings_class, config_file

    def _read_property_value(
        self,
        settings_object: Any,
        resolved_settings_class: str,
        property_name: str,
    ) -> Any:
        try:
            return settings_object.get_editor_property(property_name)
        except Exception as exc:
            raise ProjectCommandError(
                f"设置 {resolved_settings_class} 上不存在属性: {property_name}"
            ) from exc

    def _serialize_property_value(self, property_value: Any) -> Dict[str, Any]:
        exported_value = self._export_value(property_value)
        return {
            "value": self._json_safe_value(property_value, exported_value),
            "exported_value": exported_value,
            "property_cpp_type": self._infer_cpp_type(property_value),
        }

    def _coerce_project_property_value(
        self,
        raw_value: Any,
        current_value: Any,
        resolved_settings_class: str,
        property_name: str,
    ) -> Any:
        if current_value is None:
            return raw_value
        if isinstance(current_value, bool):
            return self._coerce_bool_value(raw_value, resolved_settings_class, property_name)
        if isinstance(current_value, int) and not isinstance(current_value, bool):
            return self._coerce_int_value(raw_value, resolved_settings_class, property_name)
        if isinstance(current_value, float):
            return self._coerce_float_value(raw_value, resolved_settings_class, property_name)
        if isinstance(current_value, str):
            return str(raw_value)
        if isinstance(current_value, enum.Enum):
            return self._coerce_enum_value(type(current_value), raw_value, resolved_settings_class, property_name)
        if isinstance(current_value, unreal.Object):
            return self._coerce_object_value(current_value, raw_value, resolved_settings_class, property_name)
        if hasattr(current_value, "import_text"):
            return self._coerce_import_text_value(type(current_value), raw_value, resolved_settings_class, property_name)

        raise ProjectCommandError(
            f"本地 Python 暂不支持写入 {resolved_settings_class}.{property_name} 的属性类型: {type(current_value).__name__}"
        )

    def _persist_settings_object(self, settings_object: Any, resolved_settings_class: str) -> None:
        if resolved_settings_class != "InputSettings" or not isinstance(settings_object, unreal.InputSettings):
            raise ProjectCommandError(f"本地 Python 暂不支持持久化 {resolved_settings_class}")

        settings_object.force_rebuild_keymaps()
        settings_object.save_key_mappings()

    @staticmethod
    def _coerce_bool_value(raw_value: Any, resolved_settings_class: str, property_name: str) -> bool:
        if isinstance(raw_value, bool):
            return raw_value
        if isinstance(raw_value, (int, float)):
            return bool(raw_value)

        normalized_value = str(raw_value).strip().casefold()
        if normalized_value in {"true", "1", "yes", "on"}:
            return True
        if normalized_value in {"false", "0", "no", "off"}:
            return False

        raise ProjectCommandError(
            f"无法把值 {raw_value!r} 转成布尔类型: {resolved_settings_class}.{property_name}"
        )

    @staticmethod
    def _coerce_int_value(raw_value: Any, resolved_settings_class: str, property_name: str) -> int:
        try:
            return int(raw_value)
        except (TypeError, ValueError) as exc:
            raise ProjectCommandError(
                f"无法把值 {raw_value!r} 转成整数类型: {resolved_settings_class}.{property_name}"
            ) from exc

    @staticmethod
    def _coerce_float_value(raw_value: Any, resolved_settings_class: str, property_name: str) -> float:
        try:
            return float(raw_value)
        except (TypeError, ValueError) as exc:
            raise ProjectCommandError(
                f"无法把值 {raw_value!r} 转成浮点类型: {resolved_settings_class}.{property_name}"
            ) from exc

    def _coerce_enum_value(
        self,
        enum_type: type[enum.Enum],
        raw_value: Any,
        resolved_settings_class: str,
        property_name: str,
    ) -> enum.Enum:
        if isinstance(raw_value, enum_type):
            return raw_value

        if isinstance(raw_value, str):
            normalized_value = raw_value.strip()
            tail_value = normalized_value.split(".")[-1].casefold()
            for member in enum_type:
                if member.name.casefold() == tail_value:
                    return member
            raise ProjectCommandError(
                f"无法把值 {raw_value!r} 解析为枚举 {enum_type.__name__}: {resolved_settings_class}.{property_name}"
            )

        try:
            return enum_type(int(raw_value))
        except (TypeError, ValueError) as exc:
            raise ProjectCommandError(
                f"无法把值 {raw_value!r} 转成枚举 {enum_type.__name__}: {resolved_settings_class}.{property_name}"
            ) from exc

    @staticmethod
    def _coerce_object_value(
        template_value: unreal.Object,
        raw_value: Any,
        resolved_settings_class: str,
        property_name: str,
    ) -> unreal.Object:
        object_path = str(raw_value).strip()
        if not object_path:
            raise ProjectCommandError(
                f"对象路径不能为空: {resolved_settings_class}.{property_name}"
            )

        loaded_object = unreal.load_object(None, object_path)
        if not loaded_object:
            raise ProjectCommandError(
                f"无法加载对象路径 {object_path}: {resolved_settings_class}.{property_name}"
            )

        expected_class = template_value.get_class()
        if expected_class and not loaded_object.get_class().is_child_of(expected_class):
            raise ProjectCommandError(
                f"对象 {object_path} 的类型 {loaded_object.get_class().get_name()} 与目标属性不兼容: "
                f"{resolved_settings_class}.{property_name}"
            )

        return loaded_object

    @staticmethod
    def _coerce_import_text_value(
        value_type: type,
        raw_value: Any,
        resolved_settings_class: str,
        property_name: str,
    ) -> Any:
        try:
            coerced_value = value_type()
        except Exception as exc:
            raise ProjectCommandError(
                f"本地 Python 无法构造属性类型 {value_type.__name__}: {resolved_settings_class}.{property_name}"
            ) from exc

        try:
            coerced_value.import_text(str(raw_value))
            return coerced_value
        except Exception as exc:
            raise ProjectCommandError(
                f"无法把值 {raw_value!r} 导入属性类型 {value_type.__name__}: {resolved_settings_class}.{property_name}"
            ) from exc

    def _export_value(self, property_value: Any) -> str:
        if property_value is None:
            return "None"
        if isinstance(property_value, bool):
            return "True" if property_value else "False"
        if isinstance(property_value, (int, float)):
            return str(property_value)
        if isinstance(property_value, str):
            return property_value
        if hasattr(property_value, "export_text"):
            exported_value = property_value.export_text()
            if exported_value is not None:
                return str(exported_value)
        if isinstance(property_value, unreal.Object):
            return property_value.get_path_name()
        return str(property_value)

    def _json_safe_value(self, property_value: Any, exported_value: str) -> Any:
        if property_value is None:
            return None
        if isinstance(property_value, (bool, int, float, str)):
            return property_value
        if isinstance(property_value, unreal.Object):
            return property_value.get_path_name()
        if hasattr(property_value, "export_text"):
            return exported_value
        return str(property_value)

    def _infer_cpp_type(self, property_value: Any) -> str:
        if property_value is None:
            return "None"

        python_type_name = type(property_value).__name__
        override_type = self._PROPERTY_TYPE_OVERRIDES.get(python_type_name.casefold())
        if override_type:
            return override_type

        if isinstance(property_value, unreal.Object):
            return f"{property_value.get_class().get_name()}*"

        return python_type_name

    @staticmethod
    def _require_string(params: Dict[str, Any], field_name: str) -> str:
        value = str(params.get(field_name, "")).strip()
        if not value:
            raise ProjectCommandError(f"缺少 '{field_name}' 参数")
        return value

    @staticmethod
    def _require_property_name(params: Dict[str, Any]) -> str:
        property_name = str(params.get("property_name") or params.get("setting_name") or "").strip()
        if not property_name:
            raise ProjectCommandError("缺少 'property_name' 参数")
        return property_name


class EnhancedInputAssetResolver:
    """Resolve Enhanced Input assets and package paths."""

    def normalize_package_path(self, raw_path: str, default_path: str) -> str:
        package_path = (raw_path or "").strip() or default_path
        if not package_path.startswith("/"):
            package_path = f"/{package_path}"
        package_path = package_path.rstrip("/")
        if not package_path.startswith("/Game"):
            raise ProjectCommandError(f"资源目录必须以 /Game 开头: {package_path}")
        if "." in package_path:
            raise ProjectCommandError(f"资源目录不能包含对象路径后缀: {package_path}")
        return package_path

    def build_asset_identity(self, raw_name: str, package_path: str) -> Tuple[str, str]:
        asset_name = (raw_name or "").strip()
        if not asset_name:
            raise ProjectCommandError("资源名不能为空")
        if any(separator in asset_name for separator in ("/", "\\", ".")):
            raise ProjectCommandError(f"资源名不能包含路径分隔符或对象后缀: {asset_name}")
        return asset_name, f"{package_path}/{asset_name}"

    def ensure_directory(self, package_path: str) -> None:
        if unreal.EditorAssetLibrary.does_directory_exist(package_path):
            return
        if not unreal.EditorAssetLibrary.make_directory(package_path):
            raise ProjectCommandError(f"创建目录失败: {package_path}")

    def resolve_asset_reference(self, reference: str, expected_class: Any, asset_label: str) -> Any:
        normalized_reference = (reference or "").strip()
        if not normalized_reference:
            raise ProjectCommandError(f"{asset_label} 引用不能为空")

        direct_candidates = [normalized_reference]
        if normalized_reference.startswith("/") and "." not in normalized_reference:
            asset_name = normalized_reference.rsplit("/", 1)[-1]
            if asset_name:
                direct_candidates.append(f"{normalized_reference}.{asset_name}")

        for candidate in direct_candidates:
            asset_data = unreal.EditorAssetLibrary.find_asset_data(candidate)
            if asset_data and asset_data.is_valid():
                asset_object = asset_data.get_asset()
                if isinstance(asset_object, expected_class):
                    return asset_object

        matches: List[Any] = []
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        for asset_data in registry.get_all_assets():
            package_name = str(asset_data.package_name)
            asset_name = str(asset_data.asset_name)
            object_path = f"{package_name}.{asset_name}"
            if (
                asset_name.casefold() != normalized_reference.casefold()
                and package_name.casefold() != normalized_reference.casefold()
                and object_path.casefold() != normalized_reference.casefold()
            ):
                continue

            asset_object = asset_data.get_asset()
            if isinstance(asset_object, expected_class):
                matches.append(asset_object)

        if len(matches) == 1:
            return matches[0]
        if len(matches) > 1:
            sample_paths = ", ".join(asset.get_path_name() for asset in matches[:5])
            raise ProjectCommandError(f"{asset_label} 引用匹配到多个结果: {sample_paths}")
        raise ProjectCommandError(f"未找到 {asset_label}: {normalized_reference}")

    @staticmethod
    def save_asset(asset_path: str) -> None:
        if not unreal.EditorAssetLibrary.save_asset(asset_path, False):
            raise ProjectCommandError(f"保存资源失败: {asset_path}")

    @staticmethod
    def asset_path_from_object_path(object_path: str) -> str:
        object_text = (object_path or "").strip()
        if not object_text:
            raise ProjectCommandError("对象路径不能为空")
        if "." in object_text:
            return object_text.split(".", 1)[0]
        return object_text


class EnhancedInputCommandHelper:
    """Handle Enhanced Input asset creation and mapping edits with local Python APIs."""

    VALUE_TYPE_MAP: Dict[str, unreal.InputActionValueType] = {
        "boolean": unreal.InputActionValueType.BOOLEAN,
        "bool": unreal.InputActionValueType.BOOLEAN,
        "digital": unreal.InputActionValueType.BOOLEAN,
        "axis1d": unreal.InputActionValueType.AXIS1D,
        "1d": unreal.InputActionValueType.AXIS1D,
        "float": unreal.InputActionValueType.AXIS1D,
        "axis2d": unreal.InputActionValueType.AXIS2D,
        "2d": unreal.InputActionValueType.AXIS2D,
        "vector2d": unreal.InputActionValueType.AXIS2D,
        "axis3d": unreal.InputActionValueType.AXIS3D,
        "3d": unreal.InputActionValueType.AXIS3D,
        "vector": unreal.InputActionValueType.AXIS3D,
        "vector3d": unreal.InputActionValueType.AXIS3D,
    }

    ACCUMULATION_MAP: Dict[str, unreal.InputActionAccumulationBehavior] = {
        "takehighestabsolutevalue": unreal.InputActionAccumulationBehavior.TAKE_HIGHEST_ABSOLUTE_VALUE,
        "highest": unreal.InputActionAccumulationBehavior.TAKE_HIGHEST_ABSOLUTE_VALUE,
        "default": unreal.InputActionAccumulationBehavior.TAKE_HIGHEST_ABSOLUTE_VALUE,
        "cumulative": unreal.InputActionAccumulationBehavior.CUMULATIVE,
        "sum": unreal.InputActionAccumulationBehavior.CUMULATIVE,
    }

    REGISTRATION_MAP: Dict[str, unreal.MappingContextRegistrationTrackingMode] = {
        "untracked": unreal.MappingContextRegistrationTrackingMode.UNTRACKED,
        "default": unreal.MappingContextRegistrationTrackingMode.UNTRACKED,
        "countregistrations": unreal.MappingContextRegistrationTrackingMode.COUNT_REGISTRATIONS,
        "count_registrations": unreal.MappingContextRegistrationTrackingMode.COUNT_REGISTRATIONS,
        "count": unreal.MappingContextRegistrationTrackingMode.COUNT_REGISTRATIONS,
    }

    def __init__(self) -> None:
        self._resolver = EnhancedInputAssetResolver()
        self._runtime_resolver = RuntimePlayerResolver()

    def create_input_action_asset(self, params: Dict[str, Any]) -> Dict[str, Any]:
        action_name = self._require_string(params, "action_name")
        package_path = self._resolver.normalize_package_path(str(params.get("path", "")), "/Game/Input")
        asset_name, asset_path = self._resolver.build_asset_identity(action_name, package_path)
        self._resolver.ensure_directory(package_path)
        self._ensure_asset_absent(asset_path, "Input Action")

        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        input_action = asset_tools.create_asset(asset_name, package_path, unreal.InputAction, None)
        if not input_action:
            raise ProjectCommandError(f"创建 Input Action 失败: {asset_path}")

        input_action.set_editor_property(
            "value_type",
            self._parse_value_type(str(params.get("value_type", "Boolean"))),
        )
        input_action.set_editor_property(
            "accumulation_behavior",
            self._parse_accumulation_behavior(str(params.get("accumulation_behavior", "TakeHighestAbsoluteValue"))),
        )
        input_action.set_editor_property("consume_input", bool(params.get("consume_input", True)))
        input_action.set_editor_property("trigger_when_paused", bool(params.get("trigger_when_paused", False)))
        input_action.set_editor_property(
            "consumes_action_and_axis_mappings",
            bool(params.get("consume_legacy_keys", False)),
        )

        self._resolver.save_asset(asset_path)
        return {
            "success": True,
            "action_name": asset_name,
            "asset_path": asset_path,
            "value_type": self._value_type_to_string(input_action.get_editor_property("value_type")),
            "accumulation_behavior": self._accumulation_behavior_to_string(
                input_action.get_editor_property("accumulation_behavior")
            ),
            "consume_input": bool(input_action.get_editor_property("consume_input")),
            "trigger_when_paused": bool(input_action.get_editor_property("trigger_when_paused")),
            "consume_legacy_keys": bool(input_action.get_editor_property("consumes_action_and_axis_mappings")),
            "implementation": "local_python",
        }

    def create_input_mapping_context(self, params: Dict[str, Any]) -> Dict[str, Any]:
        context_name = self._require_string(params, "context_name")
        package_path = self._resolver.normalize_package_path(str(params.get("path", "")), "/Game/Input")
        asset_name, asset_path = self._resolver.build_asset_identity(context_name, package_path)
        self._resolver.ensure_directory(package_path)
        self._ensure_asset_absent(asset_path, "Input Mapping Context")

        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        mapping_context = asset_tools.create_asset(asset_name, package_path, unreal.InputMappingContext, None)
        if not mapping_context:
            raise ProjectCommandError(f"创建 Input Mapping Context 失败: {asset_path}")

        description = str(params.get("description", ""))
        if description:
            mapping_context.set_editor_property("context_description", description)
        mapping_context.set_editor_property(
            "registration_tracking_mode",
            self._parse_registration_tracking_mode(str(params.get("registration_tracking_mode", "Untracked"))),
        )

        self._resolver.save_asset(asset_path)
        return {
            "success": True,
            "context_name": asset_name,
            "asset_path": asset_path,
            "description": str(mapping_context.get_editor_property("context_description")),
            "registration_tracking_mode": self._registration_tracking_mode_to_string(
                mapping_context.get_editor_property("registration_tracking_mode")
            ),
            "mapping_count": self._get_mapping_count(mapping_context),
            "implementation": "local_python",
        }

    def add_mapping_to_context(self, params: Dict[str, Any]) -> Dict[str, Any]:
        mapping_context_reference = self._require_string(params, "mapping_context")
        input_action_reference = self._require_string(params, "input_action")
        key_name = self._require_string(params, "key")

        mapping_context = self._resolver.resolve_asset_reference(
            mapping_context_reference,
            unreal.InputMappingContext,
            "Input Mapping Context",
        )
        input_action = self._resolver.resolve_asset_reference(
            input_action_reference,
            unreal.InputAction,
            "Input Action",
        )
        mapping_key = InputMappingCommandHelper._parse_input_key(key_name)

        previous_mapping_count = self._get_mapping_count(mapping_context)
        mapping_context.map_key(input_action, mapping_key)

        mapping_context_path = self._resolver.asset_path_from_object_path(mapping_context.get_path_name())
        self._resolver.save_asset(mapping_context_path)

        return {
            "success": True,
            "mapping_context": mapping_context.get_name(),
            "mapping_context_path": mapping_context_path,
            "input_action": input_action.get_name(),
            "input_action_path": self._resolver.asset_path_from_object_path(input_action.get_path_name()),
            "key": mapping_key.export_text(),
            "value_type": self._value_type_to_string(input_action.get_editor_property("value_type")),
            "mapping_index": previous_mapping_count,
            "mapping_count": self._get_mapping_count(mapping_context),
            "mapping_name": input_action.get_name(),
            "implementation": "local_python",
        }

    def assign_mapping_context(self, params: Dict[str, Any]) -> Dict[str, Any]:
        mapping_context_reference = self._require_string(params, "mapping_context")
        mapping_context = self._resolver.resolve_asset_reference(
            mapping_context_reference,
            unreal.InputMappingContext,
            "Input Mapping Context",
        )

        runtime_player = self._runtime_resolver.resolve_runtime_player(params)
        priority = self._runtime_resolver.parse_player_index(params.get("priority", 0), "priority")
        clear_existing = bool(params.get("clear_existing", False))

        modify_context_options = unreal.ModifyContextOptions()
        modify_context_options.ignore_all_pressed_keys_until_release = bool(
            params.get(
                "ignore_all_pressed_keys_until_release",
                modify_context_options.ignore_all_pressed_keys_until_release,
            )
        )
        modify_context_options.force_immediately = bool(
            params.get("force_immediately", modify_context_options.force_immediately)
        )
        modify_context_options.notify_user_settings = bool(
            params.get("notify_user_settings", modify_context_options.notify_user_settings)
        )

        if clear_existing:
            runtime_player.input_subsystem.clear_all_mappings()

        runtime_player.input_subsystem.add_mapping_context(mapping_context, priority, modify_context_options)

        result = {
            "success": True,
            "mapping_context": mapping_context.get_name(),
            "mapping_context_path": self._resolver.asset_path_from_object_path(mapping_context.get_path_name()),
            "priority": priority,
            "player_index": runtime_player.player_index,
            "clear_existing": clear_existing,
            "ignore_all_pressed_keys_until_release": bool(
                modify_context_options.ignore_all_pressed_keys_until_release
            ),
            "force_immediately": bool(modify_context_options.force_immediately),
            "notify_user_settings": bool(modify_context_options.notify_user_settings),
            "controller_name": runtime_player.controller.get_name() if runtime_player.controller else "",
            "controller_path": runtime_player.controller.get_path_name() if runtime_player.controller else "",
            "local_player_name": runtime_player.local_player.get_name(),
            "local_player_path": runtime_player.local_player.get_path_name(),
            "input_subsystem_path": runtime_player.input_subsystem.get_path_name(),
            "implementation": "local_python",
        }
        if runtime_player.target_actor is not None:
            result["target_actor_name"] = runtime_player.target_actor.get_name()
            result["target_actor_path"] = runtime_player.target_actor.get_path_name()
            result["target_actor_class"] = runtime_player.target_actor.get_class().get_name()

        self._runtime_resolver.append_world_info(result, runtime_player.world, runtime_player.resolved_world_type)
        return result

    def _ensure_asset_absent(self, asset_path: str, asset_label: str) -> None:
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            raise ProjectCommandError(f"{asset_label} 已存在: {asset_path}")

    @staticmethod
    def _get_mapping_count(mapping_context: unreal.InputMappingContext) -> int:
        mapping_data = mapping_context.get_editor_property("default_key_mappings")
        mappings = mapping_data.get_editor_property("mappings")
        return len(list(mappings))

    def _parse_value_type(self, raw_value_type: str) -> unreal.InputActionValueType:
        normalized_value = (raw_value_type or "").strip().casefold() or "boolean"
        if normalized_value not in self.VALUE_TYPE_MAP:
            raise ProjectCommandError("无效 value_type，可选值: Boolean/Axis1D/Axis2D/Axis3D")
        return self.VALUE_TYPE_MAP[normalized_value]

    def _parse_accumulation_behavior(self, raw_behavior: str) -> unreal.InputActionAccumulationBehavior:
        normalized_behavior = (raw_behavior or "").strip().casefold() or "takehighestabsolutevalue"
        if normalized_behavior not in self.ACCUMULATION_MAP:
            raise ProjectCommandError(
                "无效 accumulation_behavior，可选值: TakeHighestAbsoluteValue/Cumulative"
            )
        return self.ACCUMULATION_MAP[normalized_behavior]

    def _parse_registration_tracking_mode(
        self,
        raw_mode: str,
    ) -> unreal.MappingContextRegistrationTrackingMode:
        normalized_mode = (raw_mode or "").strip().casefold() or "untracked"
        if normalized_mode not in self.REGISTRATION_MAP:
            raise ProjectCommandError(
                "无效 registration_tracking_mode，可选值: Untracked/CountRegistrations"
            )
        return self.REGISTRATION_MAP[normalized_mode]

    @staticmethod
    def _value_type_to_string(value_type: unreal.InputActionValueType) -> str:
        value_map = {
            unreal.InputActionValueType.BOOLEAN: "Boolean",
            unreal.InputActionValueType.AXIS1D: "Axis1D",
            unreal.InputActionValueType.AXIS2D: "Axis2D",
            unreal.InputActionValueType.AXIS3D: "Axis3D",
        }
        return value_map.get(value_type, "Unknown")

    @staticmethod
    def _accumulation_behavior_to_string(behavior: unreal.InputActionAccumulationBehavior) -> str:
        behavior_map = {
            unreal.InputActionAccumulationBehavior.TAKE_HIGHEST_ABSOLUTE_VALUE: "TakeHighestAbsoluteValue",
            unreal.InputActionAccumulationBehavior.CUMULATIVE: "Cumulative",
        }
        return behavior_map.get(behavior, "Unknown")

    @staticmethod
    def _registration_tracking_mode_to_string(
        tracking_mode: unreal.MappingContextRegistrationTrackingMode,
    ) -> str:
        tracking_map = {
            unreal.MappingContextRegistrationTrackingMode.UNTRACKED: "Untracked",
            unreal.MappingContextRegistrationTrackingMode.COUNT_REGISTRATIONS: "CountRegistrations",
        }
        return tracking_map.get(tracking_mode, "Unknown")

    @staticmethod
    def _require_string(params: Dict[str, Any], field_name: str) -> str:
        value = str(params.get(field_name, "")).strip()
        if not value:
            raise ProjectCommandError(f"缺少 '{field_name}' 参数")
        return value


class RuntimePlayerResolver:
    """Resolve runtime world, actor and local player context for project commands."""

    def resolve_runtime_player(self, params: Dict[str, Any]) -> ResolvedRuntimePlayer:
        resolved_world = self.resolve_world(str(params.get("world_type", "auto")))
        if resolved_world.resolved_world_type != "pie":
            raise ProjectCommandError("assign_mapping_context 需要运行中的 PIE 或 VR Preview 世界")

        player_index = self.parse_player_index(params.get("player_index", 0), "player_index")
        if self._has_actor_reference(params):
            return self._resolve_runtime_player_from_actor(params, resolved_world, player_index)
        return self._resolve_runtime_player_from_index(resolved_world, player_index)

    def resolve_world(self, requested_world_type: str) -> ResolvedProjectWorld:
        normalized_world_type = (requested_world_type or "auto").strip().casefold() or "auto"
        editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        if editor_subsystem is None:
            raise ProjectCommandError("UnrealEditorSubsystem 不可用")

        editor_world = editor_subsystem.get_editor_world() if hasattr(editor_subsystem, "get_editor_world") else None
        game_world = editor_subsystem.get_game_world() if hasattr(editor_subsystem, "get_game_world") else None

        if normalized_world_type == "editor":
            if editor_world is None:
                raise ProjectCommandError("当前没有可用的 editor 世界")
            return ResolvedProjectWorld(world=editor_world, resolved_world_type="editor")
        if normalized_world_type == "pie":
            if game_world is None:
                raise ProjectCommandError("当前没有运行中的 PIE/Game 世界")
            return ResolvedProjectWorld(world=game_world, resolved_world_type="pie")
        if normalized_world_type == "auto":
            if game_world is not None:
                return ResolvedProjectWorld(world=game_world, resolved_world_type="pie")
            if editor_world is not None:
                return ResolvedProjectWorld(world=editor_world, resolved_world_type="editor")
            raise ProjectCommandError("当前没有可用的 world")
        raise ProjectCommandError("world_type 仅支持 auto、editor、pie")

    def append_world_info(self, result: Dict[str, Any], world: unreal.World, resolved_world_type: str) -> None:
        result["resolved_world_type"] = resolved_world_type
        result["world_type"] = "PIE" if resolved_world_type == "pie" else "Editor"
        result["world_name"] = world.get_name()
        result["world_path"] = world.get_path_name()

    @staticmethod
    def parse_player_index(raw_value: Any, field_name: str) -> int:
        try:
            player_index = int(raw_value)
        except (TypeError, ValueError) as exc:
            raise ProjectCommandError(f"'{field_name}' 必须是整数") from exc
        if player_index < 0:
            raise ProjectCommandError(f"'{field_name}' 不能小于 0")
        return player_index

    @staticmethod
    def _has_actor_reference(params: Dict[str, Any]) -> bool:
        return bool(str(params.get("name", "")).strip() or str(params.get("actor_path", "")).strip())

    def _resolve_runtime_player_from_actor(
        self,
        params: Dict[str, Any],
        resolved_world: ResolvedProjectWorld,
        player_index: int,
    ) -> ResolvedRuntimePlayer:
        target_actor = self._resolve_actor(params, resolved_world)
        target_controller = None
        if isinstance(target_actor, unreal.PlayerController):
            target_controller = target_actor
        elif isinstance(target_actor, unreal.Pawn):
            target_controller = target_actor.get_controller()
            if target_controller is None or not isinstance(target_controller, unreal.PlayerController):
                raise ProjectCommandError(f"Pawn 当前没有本地 PlayerController: {target_actor.get_name()}")
        else:
            raise ProjectCommandError("assign_mapping_context 只能绑定到 Pawn 或 PlayerController")

        local_player = self._resolve_local_player_from_controller(target_controller)
        input_subsystem = self._resolve_input_subsystem_from_local_player(local_player, resolved_world.world)
        return ResolvedRuntimePlayer(
            world=resolved_world.world,
            resolved_world_type=resolved_world.resolved_world_type,
            player_index=player_index,
            local_player=local_player,
            input_subsystem=input_subsystem,
            controller=target_controller,
            target_actor=target_actor,
        )

    def _resolve_runtime_player_from_index(
        self,
        resolved_world: ResolvedProjectWorld,
        player_index: int,
    ) -> ResolvedRuntimePlayer:
        local_players = self._get_runtime_local_players(resolved_world.world)
        if player_index >= len(local_players):
            raise ProjectCommandError(f"找不到本地 LocalPlayer，player_index={player_index}")

        local_player = local_players[player_index]
        input_subsystem = self._resolve_input_subsystem_from_local_player(local_player, resolved_world.world)
        controller = unreal.GameplayStatics.get_player_controller(resolved_world.world, player_index)
        return ResolvedRuntimePlayer(
            world=resolved_world.world,
            resolved_world_type=resolved_world.resolved_world_type,
            player_index=player_index,
            local_player=local_player,
            input_subsystem=input_subsystem,
            controller=controller if isinstance(controller, unreal.PlayerController) else None,
            target_actor=None,
        )

    def _resolve_actor(
        self,
        params: Dict[str, Any],
        resolved_world: ResolvedProjectWorld,
    ) -> unreal.Actor:
        actor_name = str(params.get("name", "")).strip()
        actor_path = str(params.get("actor_path", "")).strip()
        if not actor_name and not actor_path:
            raise ProjectCommandError("缺少 'name' 或 'actor_path' 参数")

        for actor in unreal.GameplayStatics.get_all_actors_of_class(resolved_world.world, unreal.Actor) or []:
            if actor is None:
                continue
            if actor_path and actor.get_path_name().casefold() == actor_path.casefold():
                return actor
            if actor_name and actor.get_name().casefold() == actor_name.casefold():
                return actor
            if actor_name and hasattr(actor, "get_actor_label"):
                actor_label = actor.get_actor_label()
                if actor_label and actor_label.casefold() == actor_name.casefold():
                    return actor

        raise ProjectCommandError(f"未找到目标 Actor: {actor_path or actor_name}")

    def _resolve_local_player_from_controller(self, controller: unreal.PlayerController) -> unreal.LocalPlayer:
        try:
            local_player = controller.get_editor_property("player")
        except Exception as exc:
            raise ProjectCommandError(
                f"读取 PlayerController.player 失败: {controller.get_name()}"
            ) from exc
        if local_player is None or not isinstance(local_player, unreal.LocalPlayer):
            raise ProjectCommandError(f"PlayerController 不是本地玩家控制器: {controller.get_name()}")
        return local_player

    def _resolve_input_subsystem_from_local_player(
        self,
        local_player: unreal.LocalPlayer,
        world: unreal.World,
    ) -> unreal.EnhancedInputLocalPlayerSubsystem:
        expected_local_player_path = local_player.get_path_name()
        for subsystem in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem):
            if subsystem is None or subsystem.get_name().startswith("Default__"):
                continue
            subsystem_world = subsystem.get_world() if hasattr(subsystem, "get_world") else None
            if subsystem_world != world:
                continue
            subsystem_local_player = subsystem.get_typed_outer(unreal.LocalPlayer)
            if subsystem_local_player is None:
                continue
            if subsystem_local_player.get_path_name() == expected_local_player_path:
                return subsystem
        raise ProjectCommandError("获取 UEnhancedInputLocalPlayerSubsystem 失败")

    def _get_runtime_local_players(self, world: unreal.World) -> List[unreal.LocalPlayer]:
        local_players: List[unreal.LocalPlayer] = []
        seen_paths = set()
        for local_player in unreal.ObjectIterator(unreal.LocalPlayer):
            if local_player is None or local_player.get_name().startswith("Default__"):
                continue
            local_player_world = local_player.get_world() if hasattr(local_player, "get_world") else None
            if local_player_world != world:
                continue
            local_player_path = local_player.get_path_name()
            if local_player_path in seen_paths:
                continue
            seen_paths.add(local_player_path)
            local_players.append(local_player)

        local_players.sort(key=lambda item: item.get_path_name())
        return local_players


class ProjectCommandDispatcher:
    """Dispatch supported local project commands."""

    def __init__(self) -> None:
        self._input_mapping_helper = InputMappingCommandHelper()
        self._project_settings_helper = ProjectSettingsCommandHelper()
        self._enhanced_input_helper = EnhancedInputCommandHelper()

    def handle(self, command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        if command_name in {"create_input_mapping", "create_input_axis_mapping"}:
            return self._input_mapping_helper.create_input_mapping(command_name, params)
        if command_name == "list_input_mappings":
            return self._input_mapping_helper.list_input_mappings(params)
        if command_name == "remove_input_mapping":
            return self._input_mapping_helper.remove_input_mapping(params)
        if command_name == "create_input_action_asset":
            return self._enhanced_input_helper.create_input_action_asset(params)
        if command_name == "create_input_mapping_context":
            return self._enhanced_input_helper.create_input_mapping_context(params)
        if command_name == "add_mapping_to_context":
            return self._enhanced_input_helper.add_mapping_to_context(params)
        if command_name == "assign_mapping_context":
            return self._enhanced_input_helper.assign_mapping_context(params)
        if command_name == "get_project_setting":
            return self._project_settings_helper.get_project_setting(params)
        if command_name == "set_project_setting":
            return self._project_settings_helper.set_project_setting(params)
        raise ProjectCommandError(f"不支持的本地项目命令: {command_name}")


def handle_project_command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
    """Module entry point used by the C++ local Python bridge."""
    dispatcher = ProjectCommandDispatcher()
    try:
        return dispatcher.handle(command_name, params)
    except ProjectCommandError as exc:
        return {
            "success": False,
            "error": str(exc),
        }
