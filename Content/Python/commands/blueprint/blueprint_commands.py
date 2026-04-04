"""Local Python implementations for Blueprint asset commands."""

from __future__ import annotations

import re
from dataclasses import dataclass
from typing import Any, Dict, List, Optional

import unreal


class BlueprintCommandError(RuntimeError):
    """Raised when a Blueprint asset command cannot be completed."""


@dataclass
class ResolvedBlueprintAsset:
    """Structured Blueprint asset lookup result."""

    asset: unreal.Blueprint
    asset_name: str
    asset_path: str


class BlueprintPropertyHelper:
    """Apply property and compile operations to Blueprint assets."""

    PAWN_PROPERTY_SPECS = [
        ("auto_possess_player", "AutoPossessPlayer"),
        ("use_controller_rotation_yaw", "bUseControllerRotationYaw"),
        ("use_controller_rotation_pitch", "bUseControllerRotationPitch"),
        ("use_controller_rotation_roll", "bUseControllerRotationRoll"),
        ("can_be_damaged", "bCanBeDamaged"),
    ]

    def compile_blueprint(self, resolved_blueprint: ResolvedBlueprintAsset) -> Dict[str, Any]:
        blueprint_asset = resolved_blueprint.asset
        compile_errors: List[str] = []

        for owner_name, compile_callable in self._iterate_compile_callables():
            try:
                compile_callable(blueprint_asset)
                return {
                    "success": True,
                    "name": resolved_blueprint.asset_name,
                    "asset_path": resolved_blueprint.asset_path,
                    "compiled": True,
                    "implementation": "local_python",
                    "compile_api": owner_name,
                }
            except Exception as exc:
                compile_errors.append(f"{owner_name}: {exc}")

        raise BlueprintCommandError(
            "Blueprint 编译失败，未找到可用的 Python 编译入口"
            if not compile_errors
            else "Blueprint 编译失败: " + " | ".join(compile_errors)
        )

    def set_blueprint_property(
        self,
        resolved_blueprint: ResolvedBlueprintAsset,
        property_name: str,
        property_value: Any,
    ) -> Dict[str, Any]:
        default_object = self._require_default_object(resolved_blueprint)
        resolved_property_name = self._resolve_editor_property_name(default_object, property_name)
        current_value = default_object.get_editor_property(resolved_property_name)
        converted_value = self._convert_property_value(current_value, property_value, resolved_property_name)

        self._modify_object(default_object)
        try:
            default_object.set_editor_property(resolved_property_name, converted_value)
        except Exception as exc:
            raise BlueprintCommandError(f"设置 Blueprint 属性失败: {property_name}") from exc

        self._mark_blueprint_modified(resolved_blueprint.asset)
        applied_value = default_object.get_editor_property(resolved_property_name)
        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "asset_path": resolved_blueprint.asset_path,
            "property": property_name,
            "resolved_property": resolved_property_name,
            "property_value": self._serialize_property_value(applied_value),
            "implementation": "local_python",
        }

    def set_pawn_properties(
        self,
        resolved_blueprint: ResolvedBlueprintAsset,
        params: Dict[str, Any],
    ) -> Dict[str, Any]:
        default_object = self._require_default_object(resolved_blueprint)
        results: Dict[str, Any] = {}
        any_property_specified = False
        any_property_set = False

        for param_name, property_name in self.PAWN_PROPERTY_SPECS:
            if param_name not in params:
                continue

            any_property_specified = True
            raw_value = params[param_name]
            try:
                resolved_property_name = self._resolve_editor_property_name(default_object, property_name)
                current_value = default_object.get_editor_property(resolved_property_name)
                converted_value = self._convert_property_value(current_value, raw_value, resolved_property_name)
                self._modify_object(default_object)
                default_object.set_editor_property(resolved_property_name, converted_value)
                applied_value = default_object.get_editor_property(resolved_property_name)
                results[property_name] = {
                    "success": True,
                    "resolved_property": resolved_property_name,
                    "property_value": self._serialize_property_value(applied_value),
                }
                any_property_set = True
            except Exception as exc:
                results[property_name] = {
                    "success": False,
                    "error": str(exc),
                }

        if not any_property_specified:
            raise BlueprintCommandError("No properties specified to set")

        if any_property_set:
            self._mark_blueprint_modified(resolved_blueprint.asset)

        return {
            "success": any_property_set,
            "blueprint": resolved_blueprint.asset_name,
            "asset_path": resolved_blueprint.asset_path,
            "results": results,
            "implementation": "local_python",
        }

    def set_game_mode_default_pawn(
        self,
        resolved_game_mode: ResolvedBlueprintAsset,
        resolved_pawn: ResolvedBlueprintAsset,
    ) -> Dict[str, Any]:
        default_object = self._require_default_object(resolved_game_mode)
        if not isinstance(default_object, unreal.GameModeBase):
            raise BlueprintCommandError("Target blueprint is not a GameModeBase")

        generated_class = self._require_generated_class(resolved_pawn)
        resolved_property_name = self._resolve_editor_property_name(default_object, "DefaultPawnClass")

        self._modify_object(default_object)
        try:
            default_object.set_editor_property(resolved_property_name, generated_class)
        except Exception as exc:
            raise BlueprintCommandError("设置 GameMode 默认 Pawn 失败") from exc

        self._mark_blueprint_modified(resolved_game_mode.asset)
        self.compile_blueprint(resolved_game_mode)

        return {
            "success": True,
            "game_mode": resolved_game_mode.asset_name,
            "game_mode_path": resolved_game_mode.asset_path,
            "default_pawn": resolved_pawn.asset_name,
            "default_pawn_path": resolved_pawn.asset_path,
            "compiled": True,
            "implementation": "local_python",
        }

    def set_blueprint_variable_default(
        self,
        resolved_blueprint: ResolvedBlueprintAsset,
        variable_name: str,
        default_value: Any,
    ) -> Dict[str, Any]:
        normalized_variable_name = str(variable_name or "").strip()
        if not normalized_variable_name:
            raise BlueprintCommandError("缺少 'variable_name' 参数")

        precompile_result = self.compile_blueprint(resolved_blueprint)
        property_result = self.set_blueprint_property(
            resolved_blueprint,
            normalized_variable_name,
            default_value,
        )
        compile_result = self.compile_blueprint(resolved_blueprint)

        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "asset_path": resolved_blueprint.asset_path,
            "variable_name": normalized_variable_name,
            "default_value": property_result["property_value"],
            "resolved_property": property_result["resolved_property"],
            "precompiled": bool(precompile_result.get("compiled", False)),
            "precompile_api": precompile_result.get("compile_api", ""),
            "compiled": bool(compile_result.get("compiled", False)),
            "compile_api": compile_result.get("compile_api", ""),
            "implementation": "local_python",
        }

    @staticmethod
    def _iterate_compile_callables() -> List[tuple[str, Any]]:
        compile_callables: List[tuple[str, Any]] = []
        for owner_name in ["BlueprintEditorLibrary", "KismetEditorUtilities"]:
            owner = getattr(unreal, owner_name, None)
            compile_callable = getattr(owner, "compile_blueprint", None) if owner is not None else None
            if callable(compile_callable):
                compile_callables.append((owner_name, compile_callable))
        return compile_callables

    @staticmethod
    def _require_default_object(resolved_blueprint: ResolvedBlueprintAsset) -> unreal.Object:
        generated_class = BlueprintPropertyHelper._require_generated_class(resolved_blueprint)
        default_object = None
        get_default_object = getattr(unreal, "get_default_object", None)
        if callable(get_default_object):
            try:
                default_object = get_default_object(generated_class)
            except Exception:
                default_object = None

        if default_object is None and hasattr(generated_class, "get_default_object"):
            default_object = generated_class.get_default_object()

        if default_object is None:
            raise BlueprintCommandError("Failed to get default object")
        return default_object

    @staticmethod
    def _require_generated_class(resolved_blueprint: ResolvedBlueprintAsset) -> Any:
        blueprint_asset = resolved_blueprint.asset
        generated_class = blueprint_asset.generated_class() if hasattr(blueprint_asset, "generated_class") else None
        if generated_class is None:
            raise BlueprintCommandError(f"Blueprint 还没有生成类: {resolved_blueprint.asset_name}")
        return generated_class

    @staticmethod
    def _modify_object(target_object: Any) -> None:
        modify = getattr(target_object, "modify", None)
        if callable(modify):
            modify()

    @staticmethod
    def _mark_blueprint_modified(blueprint_asset: unreal.Blueprint) -> None:
        mark_modified = getattr(getattr(unreal, "BlueprintEditorLibrary", None), "mark_blueprint_as_modified", None)
        if callable(mark_modified):
            try:
                mark_modified(blueprint_asset)
            except Exception:
                pass

        modify = getattr(blueprint_asset, "modify", None)
        if callable(modify):
            modify()

        post_edit_change = getattr(blueprint_asset, "post_edit_change", None)
        if callable(post_edit_change):
            post_edit_change()

        mark_package_dirty = getattr(blueprint_asset, "mark_package_dirty", None)
        if callable(mark_package_dirty):
            mark_package_dirty()

        outermost = blueprint_asset.get_outermost() if hasattr(blueprint_asset, "get_outermost") else None
        if outermost is not None:
            mark_outer_dirty = getattr(outermost, "mark_package_dirty", None)
            if callable(mark_outer_dirty):
                mark_outer_dirty()

    def _resolve_editor_property_name(self, target_object: Any, requested_name: str) -> str:
        normalized_name = str(requested_name or "").strip()
        if not normalized_name:
            raise BlueprintCommandError("缺少 'property_name' 参数")

        for candidate_name in self._build_property_name_candidates(normalized_name):
            try:
                target_object.get_editor_property(candidate_name)
                return candidate_name
            except Exception:
                continue

        raise BlueprintCommandError(f"Property not found: {normalized_name}")

    def _convert_property_value(self, current_value: Any, raw_value: Any, property_name: str) -> Any:
        if isinstance(current_value, bool):
            return bool(raw_value)
        if isinstance(current_value, int) and not isinstance(current_value, bool):
            return int(raw_value)
        if isinstance(current_value, float):
            return float(raw_value)
        if isinstance(current_value, str):
            return str(raw_value)
        if self._is_name_like(current_value):
            return unreal.Name(str(raw_value))
        if isinstance(current_value, unreal.Vector):
            return self._coerce_vector(property_name, raw_value)
        if isinstance(current_value, unreal.Rotator):
            return self._coerce_rotator(property_name, raw_value)
        if isinstance(current_value, unreal.LinearColor):
            return self._coerce_linear_color(property_name, raw_value)

        enum_value = self._try_resolve_enum_value(type(current_value), raw_value)
        if enum_value is not None:
            return enum_value

        resolved_reference = self._try_resolve_object_reference(raw_value)
        if resolved_reference is not None:
            return resolved_reference

        return raw_value

    def _serialize_property_value(self, property_value: Any) -> Any:
        if property_value is None:
            return None
        if isinstance(property_value, unreal.Vector):
            return self._vector_to_list(property_value)
        if isinstance(property_value, unreal.Rotator):
            return self._rotator_to_list(property_value)
        if isinstance(property_value, unreal.LinearColor):
            return self._linear_color_to_list(property_value)
        if isinstance(property_value, (bool, int, float, str)):
            return property_value
        if self._is_name_like(property_value):
            return str(property_value)
        if hasattr(property_value, "get_path_name"):
            return property_value.get_path_name()
        if hasattr(property_value, "name"):
            return str(property_value.name)
        if isinstance(property_value, list):
            return [self._serialize_property_value(item) for item in property_value]
        if isinstance(property_value, tuple):
            return [self._serialize_property_value(item) for item in property_value]
        if isinstance(property_value, dict):
            return {
                str(key): self._serialize_property_value(value)
                for key, value in property_value.items()
            }
        return str(property_value)

    @staticmethod
    def _build_property_name_candidates(property_name: str) -> List[str]:
        candidates: List[str] = [property_name]
        snake_case_name = BlueprintPropertyHelper._camel_to_snake(property_name)
        if snake_case_name:
            candidates.append(snake_case_name)

        if property_name.startswith("b") and len(property_name) > 1 and property_name[1].isupper():
            without_bool_prefix = property_name[1:]
            candidates.append(without_bool_prefix)
            stripped_snake_case = BlueprintPropertyHelper._camel_to_snake(without_bool_prefix)
            if stripped_snake_case:
                candidates.append(stripped_snake_case)

        unique_candidates: List[str] = []
        for candidate in candidates:
            if candidate and candidate not in unique_candidates:
                unique_candidates.append(candidate)
        return unique_candidates

    @staticmethod
    def _camel_to_snake(value: str) -> str:
        normalized_value = str(value or "").strip()
        if not normalized_value:
            return ""
        snake_case_value = re.sub(r"(?<!^)(?=[A-Z])", "_", normalized_value).lower()
        return snake_case_value

    @staticmethod
    def _coerce_vector(property_name: str, raw_value: Any) -> unreal.Vector:
        if not isinstance(raw_value, list) or len(raw_value) != 3:
            raise BlueprintCommandError(f"Blueprint 属性 {property_name} 需要 [X, Y, Z]")
        return unreal.Vector(float(raw_value[0]), float(raw_value[1]), float(raw_value[2]))

    @staticmethod
    def _coerce_rotator(property_name: str, raw_value: Any) -> unreal.Rotator:
        if not isinstance(raw_value, list) or len(raw_value) != 3:
            raise BlueprintCommandError(f"Blueprint 属性 {property_name} 需要 [Pitch, Yaw, Roll]")
        return unreal.Rotator(float(raw_value[0]), float(raw_value[1]), float(raw_value[2]))

    @staticmethod
    def _coerce_linear_color(property_name: str, raw_value: Any) -> unreal.LinearColor:
        if not isinstance(raw_value, list) or len(raw_value) not in (3, 4):
            raise BlueprintCommandError(f"Blueprint 属性 {property_name} 需要 [R, G, B] 或 [R, G, B, A]")
        alpha = float(raw_value[3]) if len(raw_value) == 4 else 1.0
        return unreal.LinearColor(float(raw_value[0]), float(raw_value[1]), float(raw_value[2]), alpha)

    @staticmethod
    def _try_resolve_object_reference(raw_value: Any) -> Any:
        if not isinstance(raw_value, str):
            return None

        normalized_value = raw_value.strip()
        if not normalized_value or not normalized_value.startswith("/"):
            return None

        try:
            resolved_class = unreal.load_class(None, normalized_value)
            if resolved_class is not None:
                return resolved_class
        except Exception:
            pass

        try:
            resolved_object = unreal.load_object(None, normalized_value)
            if resolved_object is not None:
                return resolved_object
        except Exception:
            pass

        try:
            resolved_asset = unreal.EditorAssetLibrary.load_asset(normalized_value)
            if resolved_asset is not None:
                return resolved_asset
        except Exception:
            pass
        return None

    @staticmethod
    def _vector_to_list(vector: unreal.Vector) -> List[float]:
        return [float(vector.x), float(vector.y), float(vector.z)]

    @staticmethod
    def _rotator_to_list(rotator: unreal.Rotator) -> List[float]:
        return [float(rotator.pitch), float(rotator.yaw), float(rotator.roll)]

    @staticmethod
    def _linear_color_to_list(color: unreal.LinearColor) -> List[float]:
        return [float(color.r), float(color.g), float(color.b), float(color.a)]

    @staticmethod
    def _is_name_like(value: Any) -> bool:
        return value.__class__.__name__ == "Name"

    @staticmethod
    def _try_resolve_enum_value(enum_type: Any, raw_value: Any) -> Any:
        if not isinstance(raw_value, (str, int, float)):
            return None

        if isinstance(raw_value, (int, float)):
            try:
                return enum_type(int(raw_value))
            except Exception:
                return None

        normalized_value = str(raw_value).strip()
        if not normalized_value:
            return None

        if normalized_value.isdigit():
            try:
                return enum_type(int(normalized_value))
            except Exception:
                return None

        if "::" in normalized_value:
            normalized_value = normalized_value.split("::", 1)[1]

        direct_member = getattr(enum_type, normalized_value, None)
        if direct_member is not None:
            return direct_member

        normalized_casefold = normalized_value.casefold()
        for attribute_name in dir(enum_type):
            if attribute_name.casefold() == normalized_casefold:
                return getattr(enum_type, attribute_name)
        return None


class BlueprintComponentHelper:
    """Edit Blueprint SCS components through SubobjectDataSubsystem."""

    def __init__(self, property_helper: BlueprintPropertyHelper) -> None:
        self._property_helper = property_helper

    def add_component_to_blueprint(
        self,
        resolved_blueprint: ResolvedBlueprintAsset,
        params: Dict[str, Any],
    ) -> Dict[str, Any]:
        component_type = str(params.get("component_type", "")).strip()
        if not component_type:
            raise BlueprintCommandError("缺少 'component_type' 参数")

        component_name = str(params.get("component_name", "")).strip()
        if not component_name:
            raise BlueprintCommandError("缺少 'component_name' 参数")

        subsystem = self._get_subobject_data_subsystem()
        blueprint_handle = self._get_blueprint_root_handle(subsystem, resolved_blueprint)
        parent_handle = self._resolve_parent_handle(
            subsystem,
            resolved_blueprint,
            str(params.get("parent_name", "")).strip(),
            blueprint_handle,
        )
        component_class = self._resolve_component_class(component_type)

        add_params = unreal.AddNewSubobjectParams()
        add_params.parent_handle = parent_handle
        add_params.new_class = component_class
        add_params.blueprint_context = resolved_blueprint.asset
        add_params.conform_transform_to_parent = True

        new_handle, fail_reason = subsystem.add_new_subobject(add_params)
        if not self._is_handle_valid(new_handle):
            fail_reason_text = str(fail_reason).strip()
            raise BlueprintCommandError(f"添加 Blueprint 组件失败: {fail_reason_text or component_type}")

        try:
            renamed = subsystem.rename_subobject(new_handle, unreal.Text(component_name))
        except Exception:
            renamed = False
        if not renamed:
            raise BlueprintCommandError(f"重命名新组件失败: {component_name}")

        component = self._require_component_from_handle(subsystem, resolved_blueprint, new_handle)
        try:
            self._modify_object(component)
            self._apply_scene_transform(component, params)
            self._apply_component_properties(component, params.get("component_properties"))
        except Exception as exc:
            subsystem.delete_subobject(blueprint_handle, new_handle, resolved_blueprint.asset)
            raise BlueprintCommandError(f"初始化 Blueprint 组件失败: {exc}") from exc

        self._mark_blueprint_structurally_modified(resolved_blueprint.asset)
        compile_result = self._property_helper.compile_blueprint(resolved_blueprint)
        return {
            "success": True,
            "component_name": component_name,
            "component_type": component.get_class().get_name(),
            "component_class_path": component.get_class().get_path_name(),
            "parent_name": str(params.get("parent_name", "")).strip(),
            "blueprint_name": resolved_blueprint.asset_name,
            "asset_path": resolved_blueprint.asset_path,
            "compiled": bool(compile_result.get("compiled", False)),
            "compile_api": compile_result.get("compile_api", ""),
            "implementation": "local_python",
        }

    def remove_component_from_blueprint(
        self,
        resolved_blueprint: ResolvedBlueprintAsset,
        params: Dict[str, Any],
    ) -> Dict[str, Any]:
        component_name = str(params.get("component_name", "")).strip()
        if not component_name:
            raise BlueprintCommandError("缺少 'component_name' 参数")

        subsystem = self._get_subobject_data_subsystem()
        blueprint_handle = self._get_blueprint_root_handle(subsystem, resolved_blueprint)
        component_handle, component = self._find_component_handle_and_object(
            subsystem,
            resolved_blueprint,
            component_name,
        )
        if component_handle is None or component is None:
            raise BlueprintCommandError(f"未找到目标组件: {component_name}")

        component_data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(component_handle)
        if unreal.SubobjectDataBlueprintFunctionLibrary.is_root_component(component_data):
            raise BlueprintCommandError("不允许删除 Blueprint 的根组件")
        if not unreal.SubobjectDataBlueprintFunctionLibrary.can_delete(component_data):
            raise BlueprintCommandError(f"当前组件不允许删除: {component_name}")

        removed_component = self._build_component_payload(
            subsystem,
            resolved_blueprint,
            component_handle,
            component,
        )
        deleted_count = int(subsystem.delete_subobject(blueprint_handle, component_handle, resolved_blueprint.asset))
        if deleted_count <= 0:
            raise BlueprintCommandError(f"删除 Blueprint 组件失败: {component_name}")

        self._mark_blueprint_structurally_modified(resolved_blueprint.asset)
        compile_result = self._property_helper.compile_blueprint(resolved_blueprint)
        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "asset_path": resolved_blueprint.asset_path,
            "component_name": removed_component["component_name"],
            "removed_component": removed_component,
            "deleted_count": deleted_count,
            "compiled": bool(compile_result.get("compiled", False)),
            "compile_api": compile_result.get("compile_api", ""),
            "implementation": "local_python",
        }

    def attach_component_in_blueprint(
        self,
        resolved_blueprint: ResolvedBlueprintAsset,
        params: Dict[str, Any],
    ) -> Dict[str, Any]:
        component_name = str(params.get("component_name", "")).strip()
        if not component_name:
            raise BlueprintCommandError("缺少 'component_name' 参数")

        parent_name = str(params.get("parent_name", "")).strip()
        if not parent_name:
            raise BlueprintCommandError("缺少 'parent_name' 参数")

        subsystem = self._get_subobject_data_subsystem()
        component_handle, component = self._find_component_handle_and_object(
            subsystem,
            resolved_blueprint,
            component_name,
        )
        if component_handle is None or component is None:
            raise BlueprintCommandError(f"未找到目标组件: {component_name}")

        component_data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(component_handle)
        if not unreal.SubobjectDataBlueprintFunctionLibrary.can_reparent(component_data):
            raise BlueprintCommandError(f"当前组件不允许重新挂接: {component_name}")

        parent_handle, parent_component = self._find_component_handle_and_object(
            subsystem,
            resolved_blueprint,
            parent_name,
        )
        if parent_handle is None or parent_component is None:
            raise BlueprintCommandError(f"未找到父组件: {parent_name}")
        if not isinstance(component, unreal.SceneComponent):
            raise BlueprintCommandError(f"目标组件不是 SceneComponent: {component_name}")
        if not isinstance(parent_component, unreal.SceneComponent):
            raise BlueprintCommandError(f"父组件不是 SceneComponent: {parent_name}")
        if component.get_name().casefold() == parent_component.get_name().casefold():
            raise BlueprintCommandError("组件不能附着到自己")
        if self._is_descendant_component(subsystem, resolved_blueprint, component_handle, parent_handle):
            raise BlueprintCommandError("不允许把组件附着到它自己的子组件")

        socket_name = str(params.get("socket_name", "")).strip()
        keep_world_transform = bool(params.get("keep_world_transform", False))
        attached = False
        try:
            attached = bool(subsystem.attach_subobject(parent_handle, component_handle))
        except Exception:
            attached = False
        if not attached:
            reparent_params = unreal.ReparentSubobjectParams()
            reparent_params.new_parent_handle = parent_handle
            reparent_params.blueprint_context = resolved_blueprint.asset
            preview_actor = self._get_blueprint_preview_actor(resolved_blueprint.asset)
            if preview_actor is not None:
                reparent_params.actor_preview_context = preview_actor
            try:
                attached = bool(subsystem.reparent_subobject(reparent_params, component_handle))
            except Exception:
                attached = False
        if not attached:
            raise BlueprintCommandError(
                f"重新挂接 Blueprint 组件失败: {component_name} -> {parent_component.get_name()}"
            )

        self._modify_object(component)
        attachment_rule = self._get_attachment_rule(keep_world_transform)
        attach_succeeded = component.attach_to_component(
            parent_component,
            unreal.Name(socket_name or "None"),
            attachment_rule,
            attachment_rule,
            attachment_rule,
            False,
        )
        if attach_succeeded is False:
            raise BlueprintCommandError(
                f"写入组件挂接关系失败: {component_name} -> {parent_component.get_name()}"
            )

        self._mark_blueprint_structurally_modified(resolved_blueprint.asset)
        compile_result = self._property_helper.compile_blueprint(resolved_blueprint)
        attached_component = self._build_component_payload(
            subsystem,
            resolved_blueprint,
            component_handle,
            component,
        )
        attached_component["parent_name"] = parent_component.get_name()
        if socket_name:
            attached_component["socket_name"] = socket_name
        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "asset_path": resolved_blueprint.asset_path,
            "component_name": attached_component["component_name"],
            "parent_name": parent_component.get_name(),
            "socket_name": socket_name,
            "keep_world_transform": keep_world_transform,
            "attached_component": attached_component,
            "compiled": bool(compile_result.get("compiled", False)),
            "compile_api": compile_result.get("compile_api", ""),
            "implementation": "local_python",
        }

    def set_component_property(
        self,
        resolved_blueprint: ResolvedBlueprintAsset,
        params: Dict[str, Any],
    ) -> Dict[str, Any]:
        if "property_value" not in params:
            raise BlueprintCommandError("Missing 'property_value' parameter")

        component_name = str(params.get("component_name", "")).strip()
        if not component_name:
            raise BlueprintCommandError("缺少 'component_name' 参数")

        property_name = str(params.get("property_name", "")).strip()
        if not property_name:
            raise BlueprintCommandError("缺少 'property_name' 参数")

        component = self._find_component_by_name(resolved_blueprint, component_name)
        if not component:
            raise BlueprintCommandError(f"Component not found: {component_name}")

        resolved_property_name = self._property_helper._resolve_editor_property_name(component, property_name)
        current_value = component.get_editor_property(resolved_property_name)
        converted_value = self._property_helper._convert_property_value(
            current_value,
            params["property_value"],
            resolved_property_name,
        )

        self._modify_object(component)
        try:
            component.set_editor_property(resolved_property_name, converted_value)
        except Exception as exc:
            raise BlueprintCommandError(f"设置组件属性失败: {property_name}") from exc

        self._property_helper._mark_blueprint_modified(resolved_blueprint.asset)
        applied_value = component.get_editor_property(resolved_property_name)
        return {
            "success": True,
            "component": component_name,
            "property": property_name,
            "resolved_property": resolved_property_name,
            "property_value": self._property_helper._serialize_property_value(applied_value),
            "implementation": "local_python",
        }

    def set_physics_properties(
        self,
        resolved_blueprint: ResolvedBlueprintAsset,
        params: Dict[str, Any],
    ) -> Dict[str, Any]:
        component_name = str(params.get("component_name", "")).strip()
        if not component_name:
            raise BlueprintCommandError("缺少 'component_name' 参数")

        component = self._find_component_by_name(resolved_blueprint, component_name)
        if not component:
            raise BlueprintCommandError(f"Component not found: {component_name}")
        if not isinstance(component, unreal.PrimitiveComponent):
            raise BlueprintCommandError("Component is not a primitive component")

        self._modify_object(component)
        if "simulate_physics" in params:
            component.set_simulate_physics(bool(params["simulate_physics"]))
        if "gravity_enabled" in params:
            component.set_enable_gravity(bool(params["gravity_enabled"]))
        if "mass" in params:
            component.set_mass_override_in_kg(unreal.Name("None"), float(params["mass"]))
        if "linear_damping" in params:
            component.set_linear_damping(float(params["linear_damping"]))
        if "angular_damping" in params:
            component.set_angular_damping(float(params["angular_damping"]))

        self._property_helper._mark_blueprint_modified(resolved_blueprint.asset)
        result = {
            "success": True,
            "component": component_name,
            "simulate_physics": bool(component.get_editor_property("simulate_physics")),
            "gravity_enabled": bool(component.get_editor_property("enable_gravity")),
            "linear_damping": float(component.get_editor_property("linear_damping")),
            "angular_damping": float(component.get_editor_property("angular_damping")),
            "implementation": "local_python",
        }
        if hasattr(component, "get_mass"):
            try:
                result["mass"] = float(component.get_mass())
            except Exception:
                pass
        return result

    def set_static_mesh_properties(
        self,
        resolved_blueprint: ResolvedBlueprintAsset,
        params: Dict[str, Any],
    ) -> Dict[str, Any]:
        component_name = str(params.get("component_name", "")).strip()
        if not component_name:
            raise BlueprintCommandError("缺少 'component_name' 参数")

        component = self._find_component_by_name(resolved_blueprint, component_name)
        if not component:
            raise BlueprintCommandError(f"Component not found: {component_name}")
        if not isinstance(component, unreal.StaticMeshComponent):
            raise BlueprintCommandError("Component is not a static mesh component")

        self._modify_object(component)
        static_mesh_path = str(params.get("static_mesh", "")).strip()
        if static_mesh_path:
            static_mesh = self._load_asset(static_mesh_path)
            if not isinstance(static_mesh, unreal.StaticMesh):
                raise BlueprintCommandError(f"静态网格资源无效: {static_mesh_path}")
            component.set_static_mesh(static_mesh)

        material_path = str(params.get("material", "")).strip()
        if material_path:
            material = self._load_asset(material_path)
            if not isinstance(material, unreal.MaterialInterface):
                raise BlueprintCommandError(f"材质资源无效: {material_path}")
            component.set_material(0, material)

        self._property_helper._mark_blueprint_modified(resolved_blueprint.asset)
        result = {
            "success": True,
            "component": component_name,
            "implementation": "local_python",
        }
        current_mesh = component.get_editor_property("static_mesh")
        if current_mesh:
            result["static_mesh"] = current_mesh.get_path_name()
        current_material = component.get_material(0)
        if current_material:
            result["material"] = current_material.get_path_name()
        return result

    def _find_component_by_name(
        self,
        resolved_blueprint: ResolvedBlueprintAsset,
        component_name: str,
    ) -> Optional[unreal.ActorComponent]:
        _, component = self._find_component_handle_and_object(
            self._get_subobject_data_subsystem(),
            resolved_blueprint,
            component_name,
        )
        return component

    def _find_component_handle_and_object(
        self,
        subsystem: unreal.SubobjectDataSubsystem,
        resolved_blueprint: ResolvedBlueprintAsset,
        component_name: str,
    ) -> tuple[Optional[Any], Optional[unreal.ActorComponent]]:
        normalized_name = component_name.strip().casefold()
        for handle in list(subsystem.k2_gather_subobject_data_for_blueprint(resolved_blueprint.asset) or []):
            if not self._is_handle_valid(handle):
                continue
            component = self._get_component_from_handle(subsystem, resolved_blueprint, handle)
            if not component:
                continue
            if component.get_name().casefold() == normalized_name:
                return handle, component
            data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
            variable_name = str(unreal.SubobjectDataBlueprintFunctionLibrary.get_variable_name(data))
            if variable_name.casefold() == normalized_name:
                return handle, component
        return None, None

    @staticmethod
    def _get_subobject_data_subsystem() -> unreal.SubobjectDataSubsystem:
        subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        if not subsystem:
            raise BlueprintCommandError("SubobjectDataSubsystem 不可用")
        return subsystem

    def _get_blueprint_root_handle(
        self,
        subsystem: unreal.SubobjectDataSubsystem,
        resolved_blueprint: ResolvedBlueprintAsset,
    ) -> Any:
        handles = list(subsystem.k2_gather_subobject_data_for_blueprint(resolved_blueprint.asset) or [])
        for handle in handles:
            if self._is_handle_valid(handle):
                return handle
        raise BlueprintCommandError(f"无法收集 Blueprint 组件句柄: {resolved_blueprint.asset_name}")

    def _resolve_parent_handle(
        self,
        subsystem: unreal.SubobjectDataSubsystem,
        resolved_blueprint: ResolvedBlueprintAsset,
        parent_name: str,
        blueprint_handle: Any,
    ) -> Any:
        if not parent_name:
            return blueprint_handle

        normalized_name = parent_name.casefold()
        for handle in list(subsystem.k2_gather_subobject_data_for_blueprint(resolved_blueprint.asset) or []):
            if not self._is_handle_valid(handle):
                continue
            data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
            variable_name = str(unreal.SubobjectDataBlueprintFunctionLibrary.get_variable_name(data))
            if variable_name.casefold() == normalized_name:
                return handle
            component = self._get_component_from_handle(subsystem, resolved_blueprint, handle)
            if component and component.get_name().casefold() == normalized_name:
                return handle
        raise BlueprintCommandError(f"Parent component not found: {parent_name}")

    def _require_component_from_handle(
        self,
        subsystem: unreal.SubobjectDataSubsystem,
        resolved_blueprint: ResolvedBlueprintAsset,
        handle: Any,
    ) -> unreal.ActorComponent:
        component = self._get_component_from_handle(subsystem, resolved_blueprint, handle)
        if not component:
            raise BlueprintCommandError("组件已创建，但无法解析组件对象")
        return component

    @staticmethod
    def _get_component_from_handle(
        subsystem: unreal.SubobjectDataSubsystem,
        resolved_blueprint: ResolvedBlueprintAsset,
        handle: Any,
    ) -> Optional[unreal.ActorComponent]:
        if not BlueprintComponentHelper._is_handle_valid(handle):
            return None
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        blueprint_object = unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(
            data,
            resolved_blueprint.asset,
        )
        if isinstance(blueprint_object, unreal.ActorComponent):
            return blueprint_object

        associated_object = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(data)
        if isinstance(associated_object, unreal.ActorComponent):
            return associated_object

        found_data = unreal.SubobjectData()
        if subsystem.k2_find_subobject_data_from_handle(handle, found_data):
            blueprint_object = unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(
                found_data,
                resolved_blueprint.asset,
            )
            if isinstance(blueprint_object, unreal.ActorComponent):
                return blueprint_object
        return None

    def _apply_scene_transform(
        self,
        component: unreal.ActorComponent,
        params: Dict[str, Any],
    ) -> None:
        if not isinstance(component, unreal.SceneComponent):
            return

        if "location" in params:
            component.set_editor_property(
                "relative_location",
                self._property_helper._coerce_vector("location", params["location"]),
            )
        if "rotation" in params:
            component.set_editor_property(
                "relative_rotation",
                self._property_helper._coerce_rotator("rotation", params["rotation"]),
            )
        if "scale" in params:
            component.set_editor_property(
                "relative_scale3d",
                self._property_helper._coerce_vector("scale", params["scale"]),
            )

    def _apply_component_properties(
        self,
        component: unreal.ActorComponent,
        raw_properties: Any,
    ) -> None:
        if raw_properties is None:
            return
        if not isinstance(raw_properties, dict):
            raise BlueprintCommandError("'component_properties' 必须是对象")

        for property_name, property_value in raw_properties.items():
            normalized_property_name = str(property_name).strip()
            if not normalized_property_name:
                continue
            resolved_property_name = self._property_helper._resolve_editor_property_name(
                component,
                normalized_property_name,
            )
            current_value = component.get_editor_property(resolved_property_name)
            converted_value = self._property_helper._convert_property_value(
                current_value,
                property_value,
                resolved_property_name,
            )
            component.set_editor_property(resolved_property_name, converted_value)

    def _build_component_payload(
        self,
        subsystem: unreal.SubobjectDataSubsystem,
        resolved_blueprint: ResolvedBlueprintAsset,
        handle: Any,
        component: unreal.ActorComponent,
    ) -> Dict[str, Any]:
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        payload: Dict[str, Any] = {
            "component_name": component.get_name(),
            "component_class": component.get_class().get_name(),
            "component_class_path": component.get_class().get_path_name(),
            "variable_name": str(unreal.SubobjectDataBlueprintFunctionLibrary.get_variable_name(data)),
            "is_root_component": bool(unreal.SubobjectDataBlueprintFunctionLibrary.is_root_component(data)),
        }

        parent_handle = self._get_parent_handle(data)
        if self._is_handle_valid(parent_handle):
            parent_component = self._get_component_from_handle(subsystem, resolved_blueprint, parent_handle)
            if parent_component:
                payload["parent_name"] = parent_component.get_name()

        if isinstance(component, unreal.SceneComponent):
            payload["relative_location"] = self._property_helper._vector_to_list(
                component.get_editor_property("relative_location")
            )
            payload["relative_rotation"] = self._property_helper._rotator_to_list(
                component.get_editor_property("relative_rotation")
            )
            payload["relative_scale3d"] = self._property_helper._vector_to_list(
                component.get_editor_property("relative_scale3d")
            )
            attach_socket_name = str(component.get_editor_property("attach_socket_name"))
            if attach_socket_name and attach_socket_name != "None":
                payload["socket_name"] = attach_socket_name

        return payload

    def _is_descendant_component(
        self,
        subsystem: unreal.SubobjectDataSubsystem,
        resolved_blueprint: ResolvedBlueprintAsset,
        possible_parent_handle: Any,
        candidate_child_handle: Any,
    ) -> bool:
        current_handle = candidate_child_handle
        while self._is_handle_valid(current_handle):
            if self._handles_equal(current_handle, possible_parent_handle):
                return True

            current_data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(current_handle)
            parent_handle = self._get_parent_handle(current_data)
            if not self._is_handle_valid(parent_handle):
                return False

            current_handle = parent_handle
        return False

    @staticmethod
    def _handles_equal(left_handle: Any, right_handle: Any) -> bool:
        if not BlueprintComponentHelper._is_handle_valid(left_handle):
            return False
        if not BlueprintComponentHelper._is_handle_valid(right_handle):
            return False

        try:
            left_data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(left_handle)
            right_data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(right_handle)
            left_object = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(left_data)
            right_object = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(right_data)
            if left_object is not None and right_object is not None:
                return left_object.get_path_name() == right_object.get_path_name()
        except Exception:
            pass

        return str(left_handle) == str(right_handle)

    @staticmethod
    def _get_parent_handle(data: Any) -> Any:
        parent_handle_getter = unreal.SubobjectDataBlueprintFunctionLibrary.get_parent_handle
        try:
            return parent_handle_getter(data)
        except TypeError:
            parent_handle = unreal.SubobjectDataHandle()
            parent_handle_getter(data, parent_handle)
            return parent_handle

    @staticmethod
    def _get_blueprint_preview_actor(blueprint_asset: unreal.Blueprint) -> Optional[unreal.Actor]:
        editor_subsystem = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
        if not editor_subsystem:
            return None

        preview_methods = [
            "get_preview_actor",
            "find_preview_actor_for_asset",
            "find_actor_for_asset",
        ]
        for method_name in preview_methods:
            preview_getter = getattr(editor_subsystem, method_name, None)
            if not callable(preview_getter):
                continue
            try:
                preview_actor = preview_getter(blueprint_asset)
            except Exception:
                preview_actor = None
            if isinstance(preview_actor, unreal.Actor):
                return preview_actor
        return None

    @staticmethod
    def _get_attachment_rule(keep_world_transform: bool) -> unreal.AttachmentRule:
        if keep_world_transform:
            return unreal.AttachmentRule.KEEP_WORLD
        return unreal.AttachmentRule.KEEP_RELATIVE

    @staticmethod
    def _modify_object(target_object: Any) -> None:
        modify = getattr(target_object, "modify", None)
        if callable(modify):
            modify()

    @staticmethod
    def _is_handle_valid(handle: Any) -> bool:
        return bool(unreal.SubobjectDataBlueprintFunctionLibrary.is_handle_valid(handle))

    @staticmethod
    def _resolve_component_class(component_type: str) -> Any:
        candidates: List[str] = [component_type]
        if not component_type.endswith("Component"):
            candidates.append(f"{component_type}Component")
        if not component_type.startswith("U"):
            candidates.append(f"U{component_type}")
            if not component_type.endswith("Component"):
                candidates.append(f"U{component_type}Component")

        for candidate in candidates:
            resolved_class = getattr(unreal, candidate, None)
            if resolved_class is None:
                lowered_candidate = candidate.casefold()
                for attribute_name in dir(unreal):
                    if attribute_name.casefold() == lowered_candidate:
                        resolved_class = getattr(unreal, attribute_name, None)
                        break
            if resolved_class is None:
                try:
                    resolved_class = unreal.load_class(None, candidate)
                except Exception:
                    resolved_class = None
            if resolved_class is None:
                continue
            try:
                if isinstance(resolved_class, type) and issubclass(resolved_class, unreal.ActorComponent):
                    return resolved_class
            except TypeError:
                continue

        raise BlueprintCommandError(f"Unknown component type: {component_type}")

    @staticmethod
    def _load_asset(asset_path: str) -> Optional[unreal.Object]:
        try:
            loaded_asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        except Exception:
            loaded_asset = None
        if loaded_asset:
            return loaded_asset
        try:
            return unreal.load_object(None, asset_path)
        except Exception:
            return None

    @staticmethod
    def _mark_blueprint_structurally_modified(blueprint_asset: unreal.Blueprint) -> None:
        mark_structural = getattr(
            getattr(unreal, "BlueprintEditorLibrary", None),
            "mark_blueprint_as_structurally_modified",
            None,
        )
        if callable(mark_structural):
            try:
                mark_structural(blueprint_asset)
                return
            except Exception:
                pass
        BlueprintPropertyHelper._mark_blueprint_modified(blueprint_asset)


class BlueprintFunctionHelper:
    """Create and remove Blueprint function graphs through BlueprintEditorLibrary."""

    def __init__(self, property_helper: BlueprintPropertyHelper) -> None:
        self._property_helper = property_helper

    def add_blueprint_function(
        self,
        resolved_blueprint: ResolvedBlueprintAsset,
        function_name: str,
    ) -> Dict[str, Any]:
        normalized_function_name = str(function_name or "").strip()
        if not normalized_function_name:
            raise BlueprintCommandError("缺少 'function_name' 参数")

        existing_graph = self._find_function_graph(resolved_blueprint.asset, normalized_function_name)
        if existing_graph is not None:
            return {
                "success": True,
                "blueprint_name": resolved_blueprint.asset_name,
                "asset_path": resolved_blueprint.asset_path,
                "function_name": existing_graph.get_name(),
                "graph_name": existing_graph.get_name(),
                "graph_path": existing_graph.get_path_name(),
                "created": False,
                "compiled": False,
                "implementation": "local_python",
                "message": "Blueprint 函数已存在，无需重复创建",
            }

        add_function_graph = getattr(getattr(unreal, "BlueprintEditorLibrary", None), "add_function_graph", None)
        if not callable(add_function_graph):
            raise BlueprintCommandError("BlueprintEditorLibrary.add_function_graph 不可用")

        try:
            graph = add_function_graph(resolved_blueprint.asset, normalized_function_name)
        except Exception as exc:
            raise BlueprintCommandError(f"创建 Blueprint 函数失败: {normalized_function_name}") from exc
        if graph is None:
            raise BlueprintCommandError(f"创建 Blueprint 函数失败: {normalized_function_name}")

        compile_result = self._property_helper.compile_blueprint(resolved_blueprint)
        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "asset_path": resolved_blueprint.asset_path,
            "function_name": graph.get_name(),
            "graph_name": graph.get_name(),
            "graph_path": graph.get_path_name(),
            "created": True,
            "compiled": bool(compile_result.get("compiled", False)),
            "compile_api": compile_result.get("compile_api", ""),
            "implementation": "local_python",
        }

    def delete_blueprint_function(
        self,
        resolved_blueprint: ResolvedBlueprintAsset,
        function_name: str,
    ) -> Dict[str, Any]:
        normalized_function_name = str(function_name or "").strip()
        if not normalized_function_name:
            raise BlueprintCommandError("缺少 'function_name' 参数")

        graph = self._find_function_graph(resolved_blueprint.asset, normalized_function_name)
        if graph is None:
            raise BlueprintCommandError(f"未找到 Blueprint 函数: {normalized_function_name}")

        remove_function_graph = getattr(getattr(unreal, "BlueprintEditorLibrary", None), "remove_function_graph", None)
        if not callable(remove_function_graph):
            raise BlueprintCommandError("BlueprintEditorLibrary.remove_function_graph 不可用")

        removed_graph_name = graph.get_name()
        removed_graph_path = graph.get_path_name()
        try:
            remove_function_graph(resolved_blueprint.asset, unreal.Name(removed_graph_name))
        except Exception as exc:
            raise BlueprintCommandError(f"删除 Blueprint 函数失败: {removed_graph_name}") from exc

        compile_result = self._property_helper.compile_blueprint(resolved_blueprint)
        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "asset_path": resolved_blueprint.asset_path,
            "function_name": removed_graph_name,
            "graph_name": removed_graph_name,
            "graph_path": removed_graph_path,
            "deleted": True,
            "compiled": bool(compile_result.get("compiled", False)),
            "compile_api": compile_result.get("compile_api", ""),
            "implementation": "local_python",
        }

    @staticmethod
    def _find_function_graph(blueprint_asset: unreal.Blueprint, function_name: str) -> Optional[unreal.EdGraph]:
        find_graph = getattr(getattr(unreal, "BlueprintEditorLibrary", None), "find_graph", None)
        if callable(find_graph):
            try:
                graph = find_graph(blueprint_asset, function_name)
                if graph is not None:
                    return graph
            except Exception:
                pass

        function_graphs = getattr(blueprint_asset, "function_graphs", None)
        if function_graphs is None:
            try:
                function_graphs = blueprint_asset.get_editor_property("function_graphs")
            except Exception:
                function_graphs = None
        if function_graphs:
            normalized_function_name = function_name.casefold()
            for graph in list(function_graphs):
                if graph is not None and graph.get_name().casefold() == normalized_function_name:
                    return graph
        return None


class BlueprintGraphHelper:
    """Create and delete Blueprint graphs through BlueprintEditorLibrary."""

    def __init__(self, property_helper: BlueprintPropertyHelper) -> None:
        self._property_helper = property_helper

    def create_blueprint_graph(
        self,
        resolved_blueprint: ResolvedBlueprintAsset,
        graph_name: str,
        graph_type: str,
    ) -> Dict[str, Any]:
        normalized_graph_name = str(graph_name or "").strip()
        normalized_graph_type = str(graph_type or "graph").strip().lower() or "graph"
        if not normalized_graph_name:
            raise BlueprintCommandError("缺少 'graph_name' 参数")

        existing_graph = self._find_graph(resolved_blueprint.asset, normalized_graph_name)
        if existing_graph is not None:
            raise BlueprintCommandError(f"Graph already exists: {normalized_graph_name}")

        if normalized_graph_type != "function":
            raise BlueprintCommandError(
                f"create_blueprint_graph 的本地Python路径当前只支持 function，实际收到: {normalized_graph_type}"
            )

        add_function_graph = getattr(getattr(unreal, "BlueprintEditorLibrary", None), "add_function_graph", None)
        if not callable(add_function_graph):
            raise BlueprintCommandError("BlueprintEditorLibrary.add_function_graph 不可用")

        try:
            graph = add_function_graph(resolved_blueprint.asset, normalized_graph_name)
        except Exception as exc:
            raise BlueprintCommandError(f"创建 Blueprint 图失败: {normalized_graph_name}") from exc
        if graph is None:
            raise BlueprintCommandError(f"创建 Blueprint 图失败: {normalized_graph_name}")

        BlueprintComponentHelper._mark_blueprint_structurally_modified(resolved_blueprint.asset)
        graph_payload = self._build_graph_payload(
            graph,
            resolved_blueprint.asset,
            graph_type_override=normalized_graph_type,
        )
        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "asset_path": resolved_blueprint.asset_path,
            "graph": graph_payload,
            "graph_name": graph_payload["graph_name"],
            "graph_path": graph_payload["graph_path"],
            "graph_type": graph_payload["graph_type"],
            "created": True,
            "implementation": "local_python",
        }

    def delete_blueprint_graph(
        self,
        resolved_blueprint: ResolvedBlueprintAsset,
        graph_name: str,
    ) -> Dict[str, Any]:
        normalized_graph_name = str(graph_name or "").strip()
        if not normalized_graph_name:
            raise BlueprintCommandError("缺少 'graph_name' 参数")

        graph = self._find_graph(resolved_blueprint.asset, normalized_graph_name)
        if graph is None:
            raise BlueprintCommandError(f"未找到 Blueprint 图: {normalized_graph_name}")

        graph_type = self._resolve_graph_type(resolved_blueprint.asset, graph)
        removed_graph_name = graph.get_name()
        removed_graph_path = graph.get_path_name()
        if graph_type == "graph":
            raise BlueprintCommandError("不能删除最后一个主图")
        self._remove_graph(resolved_blueprint.asset, graph, graph_type)

        BlueprintComponentHelper._mark_blueprint_structurally_modified(resolved_blueprint.asset)
        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "asset_path": resolved_blueprint.asset_path,
            "graph_name": removed_graph_name,
            "graph_path": removed_graph_path,
            "graph_type": graph_type,
            "deleted": True,
            "implementation": "local_python",
        }

    @staticmethod
    def _find_graph(blueprint_asset: unreal.Blueprint, graph_name: str) -> Optional[unreal.EdGraph]:
        find_graph = getattr(getattr(unreal, "BlueprintEditorLibrary", None), "find_graph", None)
        if callable(find_graph):
            try:
                graph = find_graph(blueprint_asset, graph_name)
                if graph is not None:
                    return graph
            except Exception:
                pass

        normalized_graph_name = graph_name.casefold()
        for property_name in ("ubergraph_pages", "function_graphs", "macro_graphs"):
            try:
                graphs = blueprint_asset.get_editor_property(property_name)
            except Exception:
                graphs = None
            if not graphs:
                continue
            for graph in list(graphs):
                if graph is not None and graph.get_name().casefold() == normalized_graph_name:
                    return graph
        return None

    @staticmethod
    def _resolve_graph_type(blueprint_asset: unreal.Blueprint, graph: unreal.EdGraph) -> str:
        graph_path = graph.get_path_name()
        for property_name, resolved_type in (("function_graphs", "function"), ("macro_graphs", "macro")):
            try:
                graphs = blueprint_asset.get_editor_property(property_name)
            except Exception:
                graphs = None
            if not graphs:
                continue
            for candidate_graph in list(graphs):
                if candidate_graph is not None and candidate_graph.get_path_name() == graph_path:
                    return resolved_type

        find_event_graph = getattr(getattr(unreal, "BlueprintEditorLibrary", None), "find_event_graph", None)
        if callable(find_event_graph):
            try:
                event_graph = find_event_graph(blueprint_asset)
                if event_graph is not None and event_graph.get_path_name() == graph_path:
                    return "graph"
            except Exception:
                pass

        normalized_name = graph.get_name().casefold()
        if normalized_name in {"userconstructionscript"}:
            return "function"
        return "unknown"

    @staticmethod
    def _remove_graph(blueprint_asset: unreal.Blueprint, graph: unreal.EdGraph, graph_type: str) -> None:
        if graph_type == "function":
            remove_function_graph = getattr(getattr(unreal, "BlueprintEditorLibrary", None), "remove_function_graph", None)
            if callable(remove_function_graph):
                try:
                    remove_function_graph(blueprint_asset, unreal.Name(graph.get_name()))
                    return
                except Exception:
                    pass

        remove_graph = getattr(getattr(unreal, "BlueprintEditorLibrary", None), "remove_graph", None)
        if not callable(remove_graph):
            raise BlueprintCommandError("BlueprintEditorLibrary.remove_graph 不可用")

        try:
            remove_graph(blueprint_asset, graph)
        except Exception as exc:
            raise BlueprintCommandError(f"删除 Blueprint 图失败: {graph.get_name()}") from exc

    @classmethod
    def _build_graph_payload(
        cls,
        graph: unreal.EdGraph,
        blueprint_asset: unreal.Blueprint,
        graph_type_override: Optional[str] = None,
    ) -> Dict[str, Any]:
        graph_nodes = getattr(graph, "nodes", None)
        node_count = len(list(graph_nodes)) if graph_nodes is not None else 0
        return {
            "graph_name": graph.get_name(),
            "graph_path": graph.get_path_name(),
            "graph_class": graph.get_class().get_name(),
            "graph_type": graph_type_override or cls._resolve_graph_type(blueprint_asset, graph),
            "node_count": node_count,
        }


class BlueprintMemberHelper:
    """Rename Blueprint graph-like members through BlueprintEditorLibrary."""

    SUPPORTED_MEMBER_TYPES = {"auto", "graph", "function", "macro"}
    VARIABLE_TYPE_ALIASES = {
        "bool": "bool",
        "boolean": "bool",
        "int": "int",
        "integer": "int",
        "float": "real",
        "real": "real",
        "string": "string",
        "name": "name",
        "text": "text",
    }

    def __init__(self, property_helper: BlueprintPropertyHelper) -> None:
        self._property_helper = property_helper

    def rename_blueprint_member(
        self,
        resolved_blueprint: ResolvedBlueprintAsset,
        old_name: str,
        new_name: str,
        member_type: str,
    ) -> Dict[str, Any]:
        normalized_old_name = str(old_name or "").strip()
        normalized_new_name = str(new_name or "").strip()
        normalized_member_type = str(member_type or "auto").strip().lower() or "auto"

        if not normalized_old_name:
            raise BlueprintCommandError("缺少 'old_name' 参数")
        if not normalized_new_name:
            raise BlueprintCommandError("缺少 'new_name' 参数")
        if normalized_member_type not in self.SUPPORTED_MEMBER_TYPES:
            raise BlueprintCommandError(f"不支持的 'member_type': {member_type}")

        graph = self._find_graph(resolved_blueprint.asset, normalized_old_name)
        if graph is None:
            raise BlueprintCommandError(f"未找到 Blueprint 成员图: {normalized_old_name}")

        actual_member_type = self._resolve_graph_member_type(resolved_blueprint.asset, graph)
        effective_member_type = actual_member_type
        if normalized_member_type in {"function", "graph"} and actual_member_type == "graph":
            effective_member_type = normalized_member_type

        if normalized_member_type != "auto" and normalized_member_type != effective_member_type:
            raise BlueprintCommandError(
                f"Blueprint 成员类型不匹配: 期望 {normalized_member_type}，实际 {actual_member_type}"
            )

        if normalized_old_name.casefold() == normalized_new_name.casefold():
            return {
                "success": True,
                "blueprint_name": resolved_blueprint.asset_name,
                "asset_path": resolved_blueprint.asset_path,
                "old_name": graph.get_name(),
                "new_name": graph.get_name(),
                "member_type": effective_member_type,
                "renamed": False,
                "compiled": False,
                "implementation": "local_python",
                "message": "Blueprint 成员名称未变化，无需重命名",
            }

        existing_graph = self._find_graph(resolved_blueprint.asset, normalized_new_name)
        if existing_graph is not None:
            raise BlueprintCommandError(f"目标 Blueprint 成员已存在: {normalized_new_name}")

        rename_graph = getattr(getattr(unreal, "BlueprintEditorLibrary", None), "rename_graph", None)
        if not callable(rename_graph):
            raise BlueprintCommandError("BlueprintEditorLibrary.rename_graph 不可用")

        old_graph_name = graph.get_name()
        old_graph_path = graph.get_path_name()
        try:
            rename_graph(graph, normalized_new_name)
        except Exception as exc:
            raise BlueprintCommandError(f"重命名 Blueprint 成员失败: {normalized_old_name}") from exc

        compile_result = self._property_helper.compile_blueprint(resolved_blueprint)
        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "asset_path": resolved_blueprint.asset_path,
            "old_name": old_graph_name,
            "new_name": graph.get_name(),
            "old_graph_path": old_graph_path,
            "new_graph_path": graph.get_path_name(),
            "member_type": effective_member_type,
            "renamed": True,
            "compiled": bool(compile_result.get("compiled", False)),
            "compile_api": compile_result.get("compile_api", ""),
            "implementation": "local_python",
        }

    def add_blueprint_variable(
        self,
        resolved_blueprint: ResolvedBlueprintAsset,
        variable_name: str,
        variable_type: str,
        is_exposed: bool,
    ) -> Dict[str, Any]:
        normalized_variable_name = str(variable_name or "").strip()
        normalized_variable_type = str(variable_type or "").strip()
        if not normalized_variable_name:
            raise BlueprintCommandError("缺少 'variable_name' 参数")
        if not normalized_variable_type:
            raise BlueprintCommandError("缺少 'variable_type' 参数")

        add_member_variable = getattr(getattr(unreal, "BlueprintEditorLibrary", None), "add_member_variable", None)
        if not callable(add_member_variable):
            raise BlueprintCommandError("BlueprintEditorLibrary.add_member_variable 不可用")

        pin_type = self._resolve_variable_pin_type(normalized_variable_type)
        try:
            variable_created = bool(add_member_variable(resolved_blueprint.asset, normalized_variable_name, pin_type))
        except Exception as exc:
            raise BlueprintCommandError(f"添加 Blueprint 变量失败: {normalized_variable_name}") from exc

        if not variable_created:
            raise BlueprintCommandError(f"添加 Blueprint 变量失败: {normalized_variable_name}")

        if is_exposed:
            set_instance_editable = getattr(
                getattr(unreal, "BlueprintEditorLibrary", None),
                "set_blueprint_variable_instance_editable",
                None,
            )
            if not callable(set_instance_editable):
                raise BlueprintCommandError("BlueprintEditorLibrary.set_blueprint_variable_instance_editable 不可用")
            try:
                set_instance_editable(resolved_blueprint.asset, normalized_variable_name, True)
            except Exception as exc:
                raise BlueprintCommandError(
                    f"设置 Blueprint 变量可编辑失败: {normalized_variable_name}"
                ) from exc

        self._property_helper._mark_blueprint_modified(resolved_blueprint.asset)
        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "asset_path": resolved_blueprint.asset_path,
            "variable_name": normalized_variable_name,
            "variable_type": normalized_variable_type,
            "is_exposed": is_exposed,
            "implementation": "local_python",
        }

    def remove_unused_blueprint_variables(
        self,
        resolved_blueprint: ResolvedBlueprintAsset,
    ) -> Dict[str, Any]:
        remove_unused_variables = getattr(
            getattr(unreal, "BlueprintEditorLibrary", None),
            "remove_unused_variables",
            None,
        )
        if not callable(remove_unused_variables):
            raise BlueprintCommandError("BlueprintEditorLibrary.remove_unused_variables 不可用")

        try:
            removed_count = int(remove_unused_variables(resolved_blueprint.asset))
        except Exception as exc:
            raise BlueprintCommandError(
                f"清理未使用 Blueprint 变量失败: {resolved_blueprint.asset_name}"
            ) from exc

        compiled = False
        compile_api = ""
        if removed_count > 0:
            compile_result = self._property_helper.compile_blueprint(resolved_blueprint)
            compiled = bool(compile_result.get("compiled", False))
            compile_api = str(compile_result.get("compile_api", ""))

        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "asset_path": resolved_blueprint.asset_path,
            "removed_count": removed_count,
            "compiled": compiled,
            "compile_api": compile_api,
            "implementation": "local_python",
        }

    @staticmethod
    def _find_graph(blueprint_asset: unreal.Blueprint, graph_name: str) -> Optional[unreal.EdGraph]:
        find_graph = getattr(getattr(unreal, "BlueprintEditorLibrary", None), "find_graph", None)
        if callable(find_graph):
            try:
                graph = find_graph(blueprint_asset, graph_name)
                if graph is not None:
                    return graph
            except Exception:
                pass
        return None

    def _resolve_graph_member_type(self, blueprint_asset: unreal.Blueprint, graph: unreal.EdGraph) -> str:
        if self._graph_exists_in_collection(blueprint_asset, "function_graphs", graph):
            return "function"
        if self._graph_exists_in_collection(blueprint_asset, "macro_graphs", graph):
            return "macro"
        return "graph"

    @staticmethod
    def _graph_exists_in_collection(
        blueprint_asset: unreal.Blueprint,
        property_name: str,
        target_graph: unreal.EdGraph,
    ) -> bool:
        try:
            graphs = blueprint_asset.get_editor_property(property_name)
        except Exception:
            graphs = None
        if not graphs:
            return False
        target_path = target_graph.get_path_name()
        for graph in list(graphs):
            if graph is not None and graph.get_path_name() == target_path:
                return True
        return False

    @classmethod
    def _resolve_variable_pin_type(cls, variable_type: str) -> unreal.EdGraphPinType:
        normalized_variable_type = str(variable_type or "").strip().lower()
        if not normalized_variable_type:
            raise BlueprintCommandError("缺少 'variable_type' 参数")

        if normalized_variable_type == "vector":
            return unreal.BlueprintEditorLibrary.get_struct_type(unreal.Vector.static_struct())

        primitive_type_name = cls.VARIABLE_TYPE_ALIASES.get(normalized_variable_type)
        if primitive_type_name is None:
            raise BlueprintCommandError(f"不支持的 Blueprint 变量类型: {variable_type}")
        return unreal.BlueprintEditorLibrary.get_basic_type_by_name(primitive_type_name)


class BlueprintAssetHelper:
    """Resolve Blueprint assets and parent classes for local commands."""

    SCRIPT_MODULES = [
        "Engine",
        "CoreUObject",
        "UMG",
        "Blutility",
    ]

    def resolve_blueprint_asset(self, blueprint_reference: str) -> ResolvedBlueprintAsset:
        normalized_reference = (blueprint_reference or "").strip()
        if not normalized_reference:
            raise BlueprintCommandError("缺少 'blueprint_name' 参数")

        for candidate_path in self._build_direct_asset_candidates(normalized_reference):
            asset = unreal.EditorAssetLibrary.load_asset(candidate_path)
            if asset and isinstance(asset, unreal.Blueprint):
                return ResolvedBlueprintAsset(
                    asset=asset,
                    asset_name=asset.get_name(),
                    asset_path=self._to_package_path(asset.get_path_name()),
                )

        matched_assets = self._find_blueprint_assets(normalized_reference)
        if len(matched_assets) == 1:
            asset = matched_assets[0].get_asset()
            if asset and isinstance(asset, unreal.Blueprint):
                return ResolvedBlueprintAsset(
                    asset=asset,
                    asset_name=asset.get_name(),
                    asset_path=self._to_package_path(asset.get_path_name()),
                )

        if len(matched_assets) > 1:
            matched_paths = ", ".join(
                self._to_package_path(asset_data.get_object_path_string()) for asset_data in matched_assets[:5]
            )
            raise BlueprintCommandError(
                f"找到多个同名 Blueprint，请改用完整路径: {normalized_reference}，候选={matched_paths}"
            )

        raise BlueprintCommandError(f"Blueprint not found: {normalized_reference}")

    def resolve_parent_class(self, parent_class_name: str) -> type:
        normalized_name = (parent_class_name or "").strip()
        if not normalized_name:
            return unreal.Actor

        direct_class = self._load_class_by_reference(normalized_name)
        if direct_class is not None:
            return direct_class

        short_candidates = self._build_parent_short_name_candidates(normalized_name)
        for candidate_name in short_candidates:
            candidate_class = getattr(unreal, candidate_name, None)
            if isinstance(candidate_class, type):
                return candidate_class

        class_base_name = self._normalize_parent_class_base_name(normalized_name)
        script_modules = list(self.SCRIPT_MODULES)
        project_name = unreal.SystemLibrary.get_project_name()
        if project_name:
            script_modules.append(project_name)

        for module_name in script_modules:
            candidate_path = f"/Script/{module_name}.{class_base_name}"
            candidate_class = self._load_class_by_reference(candidate_path)
            if candidate_class is not None:
                return candidate_class

        raise BlueprintCommandError(f"无法解析 Blueprint 父类 '{normalized_name}'")

    def validate_parent_class(self, parent_class: type) -> None:
        if issubclass(parent_class, unreal.EditorUtilityWidget):
            raise BlueprintCommandError(
                "EditorUtilityWidget 请使用 create_umg_widget_blueprint 创建，而不是 create_blueprint"
            )

    @staticmethod
    def get_parent_class(blueprint_asset: unreal.Blueprint) -> Optional[type]:
        generated_class = blueprint_asset.generated_class() if hasattr(blueprint_asset, "generated_class") else None
        if generated_class is not None:
            get_super_class = getattr(generated_class, "get_super_class", None)
            if callable(get_super_class):
                try:
                    return get_super_class()
                except Exception:
                    pass
            try:
                return generated_class.get_editor_property("super_struct")
            except Exception:
                pass
        try:
            return blueprint_asset.get_editor_property("parent_class")
        except Exception:
            return None

    @staticmethod
    def blueprint_type_to_text(parent_class: Any) -> str:
        if isinstance(parent_class, type) and issubclass(parent_class, unreal.BlueprintFunctionLibrary):
            return "function_library"
        return "normal"

    @staticmethod
    def _build_direct_asset_candidates(blueprint_reference: str) -> List[str]:
        candidates: List[str] = []
        if blueprint_reference.startswith("/"):
            candidates.append(blueprint_reference)
            if "." not in blueprint_reference:
                asset_name = blueprint_reference.rsplit("/", 1)[-1]
                candidates.append(f"{blueprint_reference}.{asset_name}")
            else:
                package_path = blueprint_reference.split(".", 1)[0]
                candidates.append(package_path)
        else:
            candidates.append(f"/Game/Blueprints/{blueprint_reference}")
            candidates.append(f"/Game/Blueprints/{blueprint_reference}.{blueprint_reference}")
        return BlueprintAssetHelper._unique_preserving_order(candidates)

    def _find_blueprint_assets(self, blueprint_reference: str) -> List[unreal.AssetData]:
        asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
        blueprint_class_path = unreal.TopLevelAssetPath("/Script/Engine", "Blueprint")
        blueprint_assets = list(asset_registry.get_assets_by_class(blueprint_class_path, True) or [])

        normalized_reference = blueprint_reference.casefold()
        matched_assets: List[unreal.AssetData] = []
        for asset_data in blueprint_assets:
            asset_name = str(asset_data.asset_name)
            package_name = str(asset_data.package_name)
            object_path = f"{package_name}.{asset_name}"
            if (
                asset_name.casefold() == normalized_reference
                or package_name.casefold() == normalized_reference
                or object_path.casefold() == normalized_reference
            ):
                matched_assets.append(asset_data)
        return matched_assets

    @staticmethod
    def _load_class_by_reference(class_reference: str) -> type | None:
        try:
            loaded_class = unreal.load_class(None, class_reference)
        except Exception:
            loaded_class = None
        if isinstance(loaded_class, type):
            return loaded_class
        return None

    @staticmethod
    def _build_parent_short_name_candidates(parent_class_name: str) -> List[str]:
        candidates: List[str] = [parent_class_name]
        if parent_class_name and parent_class_name[0] in {"A", "U"} and len(parent_class_name) > 1:
            candidates.append(parent_class_name[1:])
        else:
            candidates.append(f"A{parent_class_name}")
            candidates.append(f"U{parent_class_name}")
        return BlueprintAssetHelper._unique_preserving_order(candidates)

    @staticmethod
    def _normalize_parent_class_base_name(parent_class_name: str) -> str:
        class_name = parent_class_name.split(".")[-1]
        if class_name and class_name[0] in {"A", "U"} and len(class_name) > 1:
            return class_name[1:]
        return class_name

    @staticmethod
    def _to_package_path(object_path: str) -> str:
        return object_path.split(".", 1)[0] if "." in object_path else object_path

    @staticmethod
    def _unique_preserving_order(values: List[str]) -> List[str]:
        unique_values: List[str] = []
        for value in values:
            if value and value not in unique_values:
                unique_values.append(value)
        return unique_values


class BlueprintCommandDispatcher:
    """Dispatch supported local Blueprint asset commands."""

    def __init__(self) -> None:
        self._asset_helper = BlueprintAssetHelper()
        self._property_helper = BlueprintPropertyHelper()
        self._component_helper = BlueprintComponentHelper(self._property_helper)
        self._function_helper = BlueprintFunctionHelper(self._property_helper)
        self._graph_helper = BlueprintGraphHelper(self._property_helper)
        self._member_helper = BlueprintMemberHelper(self._property_helper)

    def handle(self, command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        if command_name == "create_blueprint":
            return self._handle_create_blueprint(params)
        if command_name == "create_child_blueprint":
            return self._handle_create_child_blueprint(params)
        if command_name == "add_component_to_blueprint":
            return self._handle_add_component_to_blueprint(params)
        if command_name == "remove_component_from_blueprint":
            return self._handle_remove_component_from_blueprint(params)
        if command_name == "attach_component_in_blueprint":
            return self._handle_attach_component_in_blueprint(params)
        if command_name == "set_component_property":
            return self._handle_set_component_property(params)
        if command_name == "set_physics_properties":
            return self._handle_set_physics_properties(params)
        if command_name == "compile_blueprint":
            return self._handle_compile_blueprint(params)
        if command_name == "compile_blueprints":
            return self._handle_compile_blueprints(params)
        if command_name == "set_static_mesh_properties":
            return self._handle_set_static_mesh_properties(params)
        if command_name == "set_blueprint_property":
            return self._handle_set_blueprint_property(params)
        if command_name == "set_pawn_properties":
            return self._handle_set_pawn_properties(params)
        if command_name == "set_game_mode_default_pawn":
            return self._handle_set_game_mode_default_pawn(params)
        if command_name == "set_blueprint_variable_default":
            return self._handle_set_blueprint_variable_default(params)
        if command_name == "add_blueprint_function":
            return self._handle_add_blueprint_function(params)
        if command_name == "delete_blueprint_function":
            return self._handle_delete_blueprint_function(params)
        if command_name == "create_blueprint_graph":
            return self._handle_create_blueprint_graph(params)
        if command_name == "delete_blueprint_graph":
            return self._handle_delete_blueprint_graph(params)
        if command_name == "rename_blueprint_member":
            return self._handle_rename_blueprint_member(params)
        if command_name == "add_blueprint_variable":
            return self._handle_add_blueprint_variable(params)
        if command_name == "remove_unused_blueprint_variables":
            return self._handle_remove_unused_blueprint_variables(params)
        if command_name == "save_blueprint":
            return self._handle_save_blueprint(params)
        if command_name == "open_blueprint_editor":
            return self._handle_open_blueprint_editor(params)
        if command_name == "reparent_blueprint":
            return self._handle_reparent_blueprint(params)
        raise BlueprintCommandError(f"不支持的本地 Blueprint 命令: {command_name}")

    def _handle_create_blueprint(self, params: Dict[str, Any]) -> Dict[str, Any]:
        blueprint_name = self._require_string(params, "name")

        parent_class = self._asset_helper.resolve_parent_class(str(params.get("parent_class", "")))
        self._asset_helper.validate_parent_class(parent_class)
        package_path = self._normalize_create_path(str(params.get("path", "/Game/Blueprints")).strip() or "/Game/Blueprints")
        return self._create_blueprint_asset(
            blueprint_name=blueprint_name,
            package_path=package_path,
            parent_class=parent_class,
        )

    def _handle_create_child_blueprint(self, params: Dict[str, Any]) -> Dict[str, Any]:
        blueprint_name = self._require_string(params, "name")
        resolved_parent_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "parent_blueprint_name")
        )
        parent_class = self._property_helper._require_generated_class(resolved_parent_blueprint)
        package_path = str(params.get("path", "")).strip()
        if not package_path:
            package_path = resolved_parent_blueprint.asset_path.rsplit("/", 1)[0]
        package_path = self._normalize_create_path(package_path)

        result = self._create_blueprint_asset(
            blueprint_name=blueprint_name,
            package_path=package_path,
            parent_class=parent_class,
        )
        result["parent_blueprint_name"] = resolved_parent_blueprint.asset_name
        result["parent_blueprint_path"] = resolved_parent_blueprint.asset_path
        return result

    def _handle_save_blueprint(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "blueprint_name")
        )
        if not unreal.EditorAssetLibrary.save_loaded_asset(resolved_blueprint.asset):
            raise BlueprintCommandError(f"Failed to save blueprint: {resolved_blueprint.asset_name}")

        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "asset_path": resolved_blueprint.asset_path,
        }

    def _handle_add_component_to_blueprint(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "blueprint_name")
        )
        return self._component_helper.add_component_to_blueprint(resolved_blueprint, params)

    def _handle_remove_component_from_blueprint(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "blueprint_name")
        )
        return self._component_helper.remove_component_from_blueprint(resolved_blueprint, params)

    def _handle_attach_component_in_blueprint(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "blueprint_name")
        )
        return self._component_helper.attach_component_in_blueprint(resolved_blueprint, params)

    def _handle_set_component_property(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "blueprint_name")
        )
        return self._component_helper.set_component_property(resolved_blueprint, params)

    def _handle_set_physics_properties(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "blueprint_name")
        )
        return self._component_helper.set_physics_properties(resolved_blueprint, params)

    def _handle_compile_blueprint(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "blueprint_name")
        )
        return self._property_helper.compile_blueprint(resolved_blueprint)

    def _handle_compile_blueprints(self, params: Dict[str, Any]) -> Dict[str, Any]:
        raw_blueprint_names = params.get("blueprint_names")
        if not isinstance(raw_blueprint_names, list) or not raw_blueprint_names:
            raise BlueprintCommandError("缺少 'blueprint_names' 参数，且必须为非空数组")

        stop_on_error = bool(params.get("stop_on_error", False))
        save_after_compile = bool(params.get("save", False))

        results: List[Dict[str, Any]] = []
        compiled_count = 0
        failed_count = 0

        for raw_blueprint_name in raw_blueprint_names:
            blueprint_reference = str(raw_blueprint_name or "").strip()
            if not blueprint_reference:
                failed_count += 1
                results.append({
                    "success": False,
                    "blueprint_name": "",
                    "asset_path": "",
                    "implementation": "local_python",
                    "message": "blueprint_names 中存在空白项",
                })
                if stop_on_error:
                    break
                continue

            try:
                resolved_blueprint = self._asset_helper.resolve_blueprint_asset(blueprint_reference)
                compile_result = self._property_helper.compile_blueprint(resolved_blueprint)
                compile_result["requested_blueprint_name"] = blueprint_reference

                if save_after_compile:
                    saved = unreal.EditorAssetLibrary.save_loaded_asset(resolved_blueprint.asset)
                    compile_result["saved"] = bool(saved)
                    if not saved:
                        raise BlueprintCommandError(f"保存 Blueprint 失败: {resolved_blueprint.asset_name}")

                results.append(compile_result)
                compiled_count += 1
            except Exception as exc:
                failed_count += 1
                resolved_asset_path = ""
                resolved_asset_name = blueprint_reference
                try:
                    resolved_blueprint = self._asset_helper.resolve_blueprint_asset(blueprint_reference)
                    resolved_asset_path = resolved_blueprint.asset_path
                    resolved_asset_name = resolved_blueprint.asset_name
                except Exception:
                    pass

                results.append({
                    "success": False,
                    "blueprint_name": resolved_asset_name,
                    "asset_path": resolved_asset_path,
                    "requested_blueprint_name": blueprint_reference,
                    "implementation": "local_python",
                    "message": str(exc),
                })
                if stop_on_error:
                    break

        return {
            "success": failed_count == 0,
            "implementation": "local_python",
            "total_count": len(results),
            "compiled_count": compiled_count,
            "failed_count": failed_count,
            "stop_on_error": stop_on_error,
            "save": save_after_compile,
            "results": results,
        }

    def _handle_set_static_mesh_properties(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "blueprint_name")
        )
        return self._component_helper.set_static_mesh_properties(resolved_blueprint, params)

    def _handle_set_blueprint_property(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if "property_value" not in params:
            raise BlueprintCommandError("Missing 'property_value' parameter")

        resolved_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "blueprint_name")
        )
        property_name = self._require_string(params, "property_name")
        return self._property_helper.set_blueprint_property(
            resolved_blueprint,
            property_name,
            params["property_value"],
        )

    def _handle_set_pawn_properties(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "blueprint_name")
        )
        return self._property_helper.set_pawn_properties(resolved_blueprint, params)

    def _handle_set_game_mode_default_pawn(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_game_mode = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "game_mode_name")
        )
        resolved_pawn = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "pawn_blueprint_name")
        )
        return self._property_helper.set_game_mode_default_pawn(
            resolved_game_mode,
            resolved_pawn,
        )

    def _handle_set_blueprint_variable_default(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if "default_value" not in params:
            raise BlueprintCommandError("Missing 'default_value' parameter")

        resolved_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "blueprint_name")
        )
        variable_name = self._require_string(params, "variable_name")
        return self._property_helper.set_blueprint_variable_default(
            resolved_blueprint,
            variable_name,
            params["default_value"],
        )

    def _handle_add_blueprint_function(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "blueprint_name")
        )
        function_name = self._require_string(params, "function_name")
        return self._function_helper.add_blueprint_function(resolved_blueprint, function_name)

    def _handle_delete_blueprint_function(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "blueprint_name")
        )
        function_name = self._require_string(params, "function_name")
        return self._function_helper.delete_blueprint_function(resolved_blueprint, function_name)

    def _handle_create_blueprint_graph(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "blueprint_name")
        )
        graph_name = self._require_string(params, "graph_name")
        graph_type = str(params.get("graph_type", "graph"))
        return self._graph_helper.create_blueprint_graph(resolved_blueprint, graph_name, graph_type)

    def _handle_delete_blueprint_graph(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "blueprint_name")
        )
        graph_name = self._require_string(params, "graph_name")
        return self._graph_helper.delete_blueprint_graph(resolved_blueprint, graph_name)

    def _handle_rename_blueprint_member(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "blueprint_name")
        )
        old_name = self._require_string(params, "old_name")
        new_name = self._require_string(params, "new_name")
        member_type = str(params.get("member_type", "auto"))
        return self._member_helper.rename_blueprint_member(
            resolved_blueprint,
            old_name,
            new_name,
            member_type,
        )

    def _handle_add_blueprint_variable(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "blueprint_name")
        )
        variable_name = self._require_string(params, "variable_name")
        variable_type = self._require_string(params, "variable_type")
        is_exposed = bool(params.get("is_exposed", False))
        return self._member_helper.add_blueprint_variable(
            resolved_blueprint,
            variable_name,
            variable_type,
            is_exposed,
        )

    def _handle_remove_unused_blueprint_variables(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "blueprint_name")
        )
        return self._member_helper.remove_unused_blueprint_variables(resolved_blueprint)

    def _handle_open_blueprint_editor(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "blueprint_name")
        )
        asset_editor_subsystem = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
        if not asset_editor_subsystem:
            raise BlueprintCommandError("AssetEditorSubsystem is unavailable")

        opened = asset_editor_subsystem.open_editor_for_assets([resolved_blueprint.asset])
        if not opened:
            raise BlueprintCommandError(f"Failed to open blueprint editor: {resolved_blueprint.asset_name}")

        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "asset_path": resolved_blueprint.asset_path,
        }

    def _handle_reparent_blueprint(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._asset_helper.resolve_blueprint_asset(
            self._require_string(params, "blueprint_name")
        )
        new_parent_class = self._asset_helper.resolve_parent_class(
            self._require_string(params, "new_parent_class")
        )

        previous_parent_class = self._asset_helper.get_parent_class(resolved_blueprint.asset)
        previous_parent_path = self._class_to_path(previous_parent_class) if previous_parent_class else ""
        new_parent_path = self._class_to_path(new_parent_class)
        if previous_parent_path == new_parent_path:
            return {
                "success": True,
                "blueprint_name": resolved_blueprint.asset_name,
                "asset_path": resolved_blueprint.asset_path,
                "previous_parent_class": previous_parent_path,
                "new_parent_class": new_parent_path,
                "compiled": False,
                "saved": False,
                "implementation": "local_python",
                "message": "Blueprint 已经使用目标父类，无需改父类",
            }

        reparent_callable = getattr(getattr(unreal, "BlueprintEditorLibrary", None), "reparent_blueprint", None)
        if not callable(reparent_callable):
            raise BlueprintCommandError("BlueprintEditorLibrary.reparent_blueprint 不可用")

        try:
            reparent_callable(resolved_blueprint.asset, new_parent_class)
        except Exception as exc:
            raise BlueprintCommandError(
                f"Blueprint 改父类失败: {resolved_blueprint.asset_name} -> {new_parent_path}"
            ) from exc

        save_asset = bool(params.get("save", True))
        saved = False
        if save_asset:
            saved = bool(unreal.EditorAssetLibrary.save_loaded_asset(resolved_blueprint.asset))
            if not saved:
                raise BlueprintCommandError(f"保存 Blueprint 失败: {resolved_blueprint.asset_name}")

        current_parent_class = self._asset_helper.get_parent_class(resolved_blueprint.asset)
        current_parent_path = self._class_to_path(current_parent_class) if current_parent_class else ""

        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "asset_path": resolved_blueprint.asset_path,
            "previous_parent_class": previous_parent_path,
            "new_parent_class": current_parent_path or new_parent_path,
            "compiled": True,
            "saved": saved,
            "implementation": "local_python",
        }

    @staticmethod
    def _require_string(params: Dict[str, Any], field_name: str) -> str:
        value = str(params.get(field_name, "")).strip()
        if not value:
            raise BlueprintCommandError(f"缺少 '{field_name}' 参数")
        return value

    def _create_blueprint_asset(
        self,
        blueprint_name: str,
        package_path: str,
        parent_class: type,
    ) -> Dict[str, Any]:
        unreal.EditorAssetLibrary.make_directory(package_path)

        asset_path = f"{package_path}/{blueprint_name}"
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            raise BlueprintCommandError(f"Blueprint already exists: {blueprint_name}")

        created_blueprint = unreal.BlueprintEditorLibrary.create_blueprint_asset_with_parent(
            asset_path,
            parent_class,
        )
        if not created_blueprint:
            raise BlueprintCommandError("Failed to create blueprint")

        if not unreal.EditorAssetLibrary.save_loaded_asset(created_blueprint):
            raise BlueprintCommandError(f"Failed to save blueprint after create: {blueprint_name}")

        generated_class = created_blueprint.generated_class() if hasattr(created_blueprint, "generated_class") else None
        generated_class_type_path = (
            generated_class.get_class().get_path_name()
            if generated_class and hasattr(generated_class, "get_class")
            else ""
        )

        return {
            "success": True,
            "name": blueprint_name,
            "path": asset_path,
            "parent_class": self._class_to_path(parent_class),
            "blueprint_class": created_blueprint.get_class().get_path_name(),
            "generated_class_type": generated_class_type_path,
            "blueprint_type": self._asset_helper.blueprint_type_to_text(parent_class),
            "implementation": "local_python",
        }

    @staticmethod
    def _normalize_create_path(package_path: str) -> str:
        normalized_path = str(package_path or "").strip() or "/Game/Blueprints"
        if not normalized_path.startswith("/Game"):
            raise BlueprintCommandError("'path' must start with /Game")
        if normalized_path.endswith("/"):
            normalized_path = normalized_path[:-1]
        return normalized_path

    @staticmethod
    def _class_to_path(class_type: Optional[type]) -> str:
        if class_type is None:
            return ""
        if not isinstance(class_type, type) and hasattr(class_type, "get_path_name"):
            try:
                return class_type.get_path_name()
            except Exception:
                pass
        default_object = class_type.get_default_object() if hasattr(class_type, "get_default_object") else None
        if default_object and hasattr(default_object, "get_class"):
            return default_object.get_class().get_path_name()
        return str(class_type)


def handle_blueprint_command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
    """Module entry point used by the C++ local Python bridge."""
    dispatcher = BlueprintCommandDispatcher()
    try:
        return dispatcher.handle(command_name, params)
    except BlueprintCommandError as exc:
        return {
            "success": False,
            "error": str(exc),
        }
