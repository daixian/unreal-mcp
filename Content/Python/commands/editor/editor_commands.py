"""Local Python implementations for editor management commands."""

from __future__ import annotations

import json
from dataclasses import dataclass
from typing import Any, Dict, List, Optional

import unreal


class EditorCommandError(RuntimeError):
    """Raised when an editor command cannot be completed."""


@dataclass
class ResolvedAsset:
    """Structured asset lookup result for editor operations."""

    asset_data: unreal.AssetData
    asset_object: unreal.Object | None = None

    def to_identity(self) -> Dict[str, Any]:
        class_path = self.asset_data.asset_class_path
        package_name = str(self.asset_data.package_name)
        asset_name = str(self.asset_data.asset_name)
        return {
            "asset_name": asset_name,
            "asset_class": str(class_path.asset_name),
            "asset_class_path": f"{class_path.package_name}.{class_path.asset_name}",
            "asset_path": package_name,
            "package_name": package_name,
            "package_path": str(self.asset_data.package_path),
            "object_path": f"{package_name}.{asset_name}",
        }


@dataclass
class ResolvedWorld:
    """Structured world lookup result for editor operations."""

    world: unreal.World
    resolved_world_type: str


@dataclass
class ResolvedActor:
    """Structured actor lookup result for editor operations."""

    actor: unreal.Actor
    world: unreal.World
    resolved_world_type: str


class EditorSelectionSerializer:
    """Serialize selected actors, components and assets into MCP payloads."""

    def serialize_actor(
        self,
        actor: unreal.Actor,
        include_components: bool,
        detailed_components: bool,
    ) -> Dict[str, Any]:
        actor_payload: Dict[str, Any] = {
            "name": actor.get_name(),
            "class": actor.get_class().get_name(),
            "path": actor.get_path_name(),
            "location": self._vector_to_list(actor.get_actor_location()),
            "rotation": self._rotator_to_list(actor.get_actor_rotation()),
            "scale": self._vector_to_list(actor.get_actor_scale3d()),
            "tags": [str(tag) for tag in list(actor.tags or [])],
        }

        level = actor.get_level()
        if level:
            actor_payload["level_path"] = level.get_path_name()

        if hasattr(actor, "get_actor_label"):
            actor_payload["label"] = actor.get_actor_label()
        if hasattr(actor, "get_folder_path"):
            actor_payload["folder_path"] = str(actor.get_folder_path())

        if include_components:
            actor_components = list(actor.get_components_by_class(unreal.ActorComponent) or [])
            actor_payload["components"] = [
                self.serialize_component(component, detailed_components)
                for component in actor_components
                if component
            ]
            actor_payload["component_count"] = len(actor_payload["components"])

        return actor_payload

    def serialize_component(
        self,
        component: unreal.ActorComponent,
        detailed: bool,
    ) -> Dict[str, Any]:
        component_payload: Dict[str, Any] = {
            "name": component.get_name(),
            "class": component.get_class().get_name(),
            "path": component.get_path_name(),
        }

        owner = component.get_owner()
        if owner:
            component_payload["owner_name"] = owner.get_name()
            component_payload["owner_path"] = owner.get_path_name()

        if isinstance(component, unreal.SceneComponent):
            component_payload["relative_location"] = self._vector_to_list(
                component.get_editor_property("relative_location")
            )
            component_payload["relative_rotation"] = self._rotator_to_list(
                component.get_editor_property("relative_rotation")
            )
            component_payload["relative_scale"] = self._vector_to_list(
                component.get_editor_property("relative_scale3d")
            )
            component_payload["mobility"] = self._mobility_to_string(
                component.get_editor_property("mobility")
            )

            if detailed:
                component_payload["world_location"] = self._vector_to_list(component.get_world_location())
                component_payload["world_rotation"] = self._rotator_to_list(component.get_world_rotation())
                component_payload["world_scale"] = self._vector_to_list(component.get_world_scale())

        if isinstance(component, unreal.PrimitiveComponent):
            component_payload["hidden_in_game"] = bool(component.get_editor_property("hidden_in_game"))
            component_payload["cast_shadow"] = bool(component.get_editor_property("cast_shadow"))
            component_payload["generate_overlap_events"] = bool(
                component.get_editor_property("generate_overlap_events")
            )

        if isinstance(component, unreal.StaticMeshComponent):
            static_mesh = component.get_editor_property("static_mesh")
            component_payload["static_mesh_name"] = static_mesh.get_name() if static_mesh else ""
            component_payload["static_mesh_path"] = static_mesh.get_path_name() if static_mesh else ""
            material_payload: List[Dict[str, Any]] = []
            material_count = int(component.get_num_materials())
            for material_index in range(material_count):
                material = component.get_material(material_index)
                material_payload.append(
                    {
                        "index": material_index,
                        "name": material.get_name() if material else "",
                        "path": material.get_path_name() if material else "",
                    }
                )
            component_payload["materials"] = material_payload

        if isinstance(component, unreal.LightComponent):
            component_payload["intensity"] = float(component.get_editor_property("intensity"))
            component_payload["light_color"] = self._light_color_to_list(component)
            component_payload["cast_shadows"] = bool(component.get_editor_property("cast_shadows"))
            component_payload["affects_world"] = bool(component.get_editor_property("affects_world"))
            component_payload["indirect_lighting_intensity"] = float(
                component.get_editor_property("indirect_lighting_intensity")
            )
            component_payload["use_temperature"] = bool(component.get_editor_property("use_temperature"))
            component_payload["temperature"] = float(component.get_editor_property("temperature"))
            component_payload["cast_volumetric_shadow"] = bool(
                component.get_editor_property("cast_volumetric_shadow")
            )
            component_payload["volumetric_scattering_intensity"] = float(
                component.get_editor_property("volumetric_scattering_intensity")
            )
            component_payload["mobility"] = self._mobility_to_string(
                component.get_editor_property("mobility")
            )

        if isinstance(component, unreal.LocalLightComponent):
            component_payload["attenuation_radius"] = float(
                component.get_editor_property("attenuation_radius")
            )
            component_payload["source_radius"] = float(component.get_editor_property("source_radius"))
            component_payload["source_length"] = float(component.get_editor_property("source_length"))

        if isinstance(component, unreal.SpotLightComponent):
            component_payload["inner_cone_angle"] = float(component.get_editor_property("inner_cone_angle"))
            component_payload["outer_cone_angle"] = float(component.get_editor_property("outer_cone_angle"))

        return component_payload

    def serialize_asset(self, asset_data: unreal.AssetData, include_tags: bool) -> Dict[str, Any]:
        asset_payload = ResolvedAsset(asset_data=asset_data).to_identity()
        if include_tags:
            asset_payload.update(self._collect_tags_payload(asset_data, 128))
        return asset_payload

    @staticmethod
    def _collect_tags_payload(asset_data: unreal.AssetData, max_tag_count: int) -> Dict[str, Any]:
        tags_and_values = getattr(asset_data, "tags_and_values", None)
        if not tags_and_values:
            return {"tags": {}, "tag_count": 0}
        tag_names = list(tags_and_values.keys())[:max_tag_count]
        tags = {str(tag_name): str(tags_and_values.get(tag_name, "")) for tag_name in tag_names}
        return {
            "tags": tags,
            "tag_count": len(tags),
        }

    @staticmethod
    def _vector_to_list(vector: Any) -> List[float]:
        return [float(vector.x), float(vector.y), float(vector.z)]

    @staticmethod
    def _rotator_to_list(rotator: Any) -> List[float]:
        return [float(rotator.pitch), float(rotator.yaw), float(rotator.roll)]

    @staticmethod
    def _linear_color_to_list(color: Any) -> List[float]:
        return [float(color.r), float(color.g), float(color.b), float(color.a)]

    @classmethod
    def _light_color_to_list(cls, component: unreal.LightComponent) -> List[float]:
        if hasattr(component, "get_light_color"):
            return cls._linear_color_to_list(component.get_light_color())
        return cls._linear_color_to_list(component.get_editor_property("light_color"))

    @staticmethod
    def _mobility_to_string(mobility: Any) -> str:
        if hasattr(mobility, "name"):
            return str(mobility.name)
        mobility_text = str(mobility)
        if ":" in mobility_text:
            mobility_text = mobility_text.split(":", 1)[0]
        mobility_text = mobility_text.strip("<>")
        if "." in mobility_text:
            mobility_text = mobility_text.split(".")[-1]
        return mobility_text


class EditorCommandDispatcher:
    """Dispatch supported local editor commands."""

    def __init__(self) -> None:
        self._selection_serializer = EditorSelectionSerializer()
        self._actor_query_helper = ActorQueryHelper(self._selection_serializer)
        self._actor_edit_helper = ActorEditHelper(self._selection_serializer, self._actor_query_helper)
        self._world_settings_helper = WorldSettingsCommandHelper(self._actor_query_helper)
        self._viewport_helper = ViewportCommandHelper(self._actor_query_helper)
        self._utility_helper = EditorUtilityCommandHelper()
        self._light_helper = LightCommandHelper(self._selection_serializer)
        self._render_helper = RenderCommandHelper(self._selection_serializer)

    def handle(self, command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        if command_name == "load_level":
            return self._handle_load_level(params)
        if command_name == "save_current_level":
            return self._handle_save_current_level(params)
        if command_name == "open_asset_editor":
            return self._handle_open_asset_editor(params)
        if command_name == "close_asset_editor":
            return self._handle_close_asset_editor(params)
        if command_name == "get_actors_in_level":
            return self._handle_get_actors_in_level(params)
        if command_name == "find_actors_by_name":
            return self._handle_find_actors_by_name(params)
        if command_name == "find_actors":
            return self._handle_find_actors(params)
        if command_name == "spawn_actor":
            return self._handle_spawn_actor(params)
        if command_name == "spawn_actor_from_class":
            return self._handle_spawn_actor_from_class(params)
        if command_name == "delete_actor":
            return self._handle_delete_actor(params)
        if command_name == "duplicate_actor":
            return self._handle_duplicate_actor(params)
        if command_name == "resolve_actor_for_selection":
            return self._handle_resolve_actor_for_selection(params)
        if command_name == "set_actor_transform":
            return self._handle_set_actor_transform(params)
        if command_name == "set_actors_transform":
            return self._handle_set_actors_transform(params)
        if command_name == "set_actor_property":
            return self._handle_set_actor_property(params)
        if command_name == "set_actor_tags":
            return self._handle_set_actor_tags(params)
        if command_name == "set_actor_folder_path":
            return self._handle_set_actor_folder_path(params)
        if command_name == "set_actor_visibility":
            return self._handle_set_actor_visibility(params)
        if command_name == "set_actor_mobility":
            return self._handle_set_actor_mobility(params)
        if command_name == "spawn_blueprint_actor":
            return self._handle_spawn_blueprint_actor(params)
        if command_name == "get_actor_properties":
            return self._handle_get_actor_properties(params)
        if command_name == "get_actor_components":
            return self._handle_get_actor_components(params)
        if command_name == "get_scene_components":
            return self._handle_get_scene_components(params)
        if command_name == "get_world_settings":
            return self._handle_get_world_settings(params)
        if command_name == "set_world_settings":
            return self._handle_set_world_settings(params)
        if command_name == "focus_viewport":
            return self._handle_focus_viewport(params)
        if command_name == "run_editor_utility_widget":
            return self._handle_run_editor_utility_widget(params)
        if command_name == "run_editor_utility_blueprint":
            return self._handle_run_editor_utility_blueprint(params)
        if command_name == "get_selected_actors":
            return self._handle_get_selected_actors(params)
        if command_name == "get_editor_selection":
            return self._handle_get_editor_selection(params)
        if command_name == "create_light":
            return self._handle_create_light(params)
        if command_name == "set_light_properties":
            return self._handle_set_light_properties(params)
        if command_name == "capture_scene_to_render_target":
            return self._handle_capture_scene_to_render_target(params)
        if command_name == "set_post_process_settings":
            return self._handle_set_post_process_settings(params)
        if command_name == "attach_actor":
            return self._handle_attach_actor(params)
        if command_name == "detach_actor":
            return self._handle_detach_actor(params)
        raise EditorCommandError(f"不支持的本地编辑器命令: {command_name}")

    def _handle_load_level(self, params: Dict[str, Any]) -> Dict[str, Any]:
        level_path = self._require_string(params, "level_path")
        level_subsystem = self._get_level_editor_subsystem()
        if not level_subsystem.load_level(level_path):
            raise EditorCommandError(f"加载关卡失败: {level_path}")

        return {
            "success": True,
            "level_path": level_path,
        }

    def _handle_save_current_level(self, params: Dict[str, Any]) -> Dict[str, Any]:
        del params
        level_subsystem = self._get_level_editor_subsystem()
        if not level_subsystem.save_current_level():
            raise EditorCommandError("保存当前关卡失败")

        editor_world = self._get_editor_world()
        current_level_path = editor_world.get_outermost().get_name() if editor_world else ""
        return {
            "success": True,
            "level_path": current_level_path,
        }

    def _handle_open_asset_editor(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_asset = self._resolve_asset(self._require_string(params, "asset_path"))
        asset_editor_subsystem = self._get_asset_editor_subsystem()
        opened = asset_editor_subsystem.open_editor_for_assets([resolved_asset.asset_object])
        if not opened:
            raise EditorCommandError(f"打开资源编辑器失败: {resolved_asset.to_identity()['asset_path']}")

        result = resolved_asset.to_identity()
        result["success"] = True
        return result

    def _handle_close_asset_editor(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_asset = self._resolve_asset(self._require_string(params, "asset_path"))
        asset_editor_subsystem = self._get_asset_editor_subsystem()
        closed_editor_count = int(asset_editor_subsystem.close_all_editors_for_asset(resolved_asset.asset_object))

        result = resolved_asset.to_identity()
        result.update(
            {
                "success": True,
                "closed_editor_count": closed_editor_count,
                "had_open_editor": closed_editor_count > 0,
            }
        )
        return result

    def _handle_get_actors_in_level(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_query_helper.get_actors_in_level(params)

    def _handle_find_actors_by_name(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_query_helper.find_actors_by_name(params)

    def _handle_find_actors(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_query_helper.find_actors(params)

    def _handle_spawn_actor(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_edit_helper.spawn_actor(params)

    def _handle_spawn_actor_from_class(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_edit_helper.spawn_actor_from_class(params)

    def _handle_delete_actor(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_edit_helper.delete_actor(params)

    def _handle_duplicate_actor(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_edit_helper.duplicate_actor(params)

    def _handle_resolve_actor_for_selection(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_edit_helper.resolve_actor_for_selection(params)

    def _handle_set_actor_transform(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_edit_helper.set_actor_transform(params)

    def _handle_set_actors_transform(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_edit_helper.set_actors_transform(params)

    def _handle_set_actor_property(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_edit_helper.set_actor_property(params)

    def _handle_set_actor_tags(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_edit_helper.set_actor_tags(params)

    def _handle_set_actor_folder_path(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_edit_helper.set_actor_folder_path(params)

    def _handle_set_actor_visibility(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_edit_helper.set_actor_visibility(params)

    def _handle_set_actor_mobility(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_edit_helper.set_actor_mobility(params)

    def _handle_spawn_blueprint_actor(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_edit_helper.spawn_blueprint_actor(params)

    def _handle_get_actor_properties(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_query_helper.get_actor_properties(params)

    def _handle_get_actor_components(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_query_helper.get_actor_components(params)

    def _handle_get_scene_components(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_query_helper.get_scene_components(params)

    def _handle_get_world_settings(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._world_settings_helper.get_world_settings(params)

    def _handle_set_world_settings(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._world_settings_helper.set_world_settings(params)

    def _handle_focus_viewport(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._viewport_helper.focus_viewport(params)

    def _handle_run_editor_utility_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._utility_helper.run_editor_utility_widget(params)

    def _handle_run_editor_utility_blueprint(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._utility_helper.run_editor_utility_blueprint(params)

    def _handle_get_selected_actors(self, params: Dict[str, Any]) -> Dict[str, Any]:
        include_components = bool(params.get("include_components", False))
        detailed_components = bool(params.get("detailed_components", True))
        actor_subsystem = self._get_editor_actor_subsystem()
        selected_actors = list(actor_subsystem.get_selected_level_actors() or [])
        actor_payload = [
            self._selection_serializer.serialize_actor(actor, include_components, detailed_components)
            for actor in selected_actors
            if actor
        ]
        return {
            "success": True,
            "actors": actor_payload,
            "actor_count": len(actor_payload),
        }

    def _handle_get_editor_selection(self, params: Dict[str, Any]) -> Dict[str, Any]:
        include_components = bool(params.get("include_components", False))
        detailed_components = bool(params.get("detailed_components", True))
        include_tags = bool(params.get("include_tags", False))

        actor_subsystem = self._get_editor_actor_subsystem()
        selected_actors = list(actor_subsystem.get_selected_level_actors() or [])
        actor_payload = [
            self._selection_serializer.serialize_actor(actor, include_components, detailed_components)
            for actor in selected_actors
            if actor
        ]

        selected_assets = list(unreal.EditorUtilityLibrary.get_selected_asset_data() or [])
        asset_payload = [
            self._selection_serializer.serialize_asset(asset_data, include_tags)
            for asset_data in selected_assets
            if asset_data and asset_data.is_valid()
        ]

        result = {
            "success": True,
            "actors": actor_payload,
            "actor_count": len(actor_payload),
            "assets": asset_payload,
            "asset_count": len(asset_payload),
            "selection_count": len(actor_payload) + len(asset_payload),
        }
        editor_world = self._get_editor_world()
        if editor_world:
            self._append_editor_world_info(result, editor_world)
        return result

    def _handle_create_light(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._light_helper.create_light(params)

    def _handle_set_light_properties(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._light_helper.set_light_properties(params)

    def _handle_capture_scene_to_render_target(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._render_helper.capture_scene_to_render_target(params)

    def _handle_set_post_process_settings(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._render_helper.set_post_process_settings(params)

    def _handle_attach_actor(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_edit_helper.attach_actor(params)

    def _handle_detach_actor(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._actor_edit_helper.detach_actor(params)

    @staticmethod
    def _require_string(params: Dict[str, Any], field_name: str) -> str:
        value = str(params.get(field_name, "")).strip()
        if not value:
            raise EditorCommandError(f"缺少 '{field_name}' 参数")
        return value

    @staticmethod
    def _get_level_editor_subsystem() -> unreal.LevelEditorSubsystem:
        level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if not level_subsystem:
            raise EditorCommandError("LevelEditorSubsystem 不可用")
        return level_subsystem

    @staticmethod
    def _get_asset_editor_subsystem() -> unreal.AssetEditorSubsystem:
        asset_editor_subsystem = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
        if not asset_editor_subsystem:
            raise EditorCommandError("AssetEditorSubsystem 不可用")
        return asset_editor_subsystem

    @staticmethod
    def _get_editor_actor_subsystem() -> unreal.EditorActorSubsystem:
        actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if not actor_subsystem:
            raise EditorCommandError("EditorActorSubsystem 不可用")
        return actor_subsystem

    @staticmethod
    def _get_editor_world() -> unreal.World | None:
        editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        if not editor_subsystem:
            return None
        if not hasattr(editor_subsystem, "get_editor_world"):
            return None
        return editor_subsystem.get_editor_world()

    @staticmethod
    def _resolve_asset(asset_reference: str) -> ResolvedAsset:
        asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_reference)
        asset_object = unreal.EditorAssetLibrary.load_asset(asset_reference)
        if not asset_data.is_valid() or not asset_object:
            raise EditorCommandError(f"加载资源失败: {asset_reference}")
        return ResolvedAsset(asset_data=asset_data, asset_object=asset_object)

    @staticmethod
    def _append_editor_world_info(result: Dict[str, Any], editor_world: unreal.World) -> None:
        result["resolved_world_type"] = "editor"
        result["world_type"] = "Editor"
        result["world_name"] = editor_world.get_name()
        result["world_path"] = editor_world.get_path_name()


class ActorQueryHelper:
    """Query actors from the target editor or PIE world through local Python APIs."""

    def __init__(self, serializer: EditorSelectionSerializer) -> None:
        self._serializer = serializer

    def get_actors_in_level(self, params: Dict[str, Any]) -> Dict[str, Any]:
        include_components = bool(params.get("include_components", False))
        detailed_components = bool(params.get("detailed_components", True))
        resolved_world = self._resolve_world(str(params.get("world_type", "auto")))
        actors = self._get_all_actors(resolved_world.world)

        actor_payload = [
            self._serializer.serialize_actor(actor, include_components, detailed_components)
            for actor in actors
            if actor
        ]

        result = {
            "success": True,
            "actors": actor_payload,
            "actor_count": len(actor_payload),
        }
        self._append_world_info(result, resolved_world)
        return result

    def find_actors_by_name(self, params: Dict[str, Any]) -> Dict[str, Any]:
        pattern = self._require_string(params, "pattern")
        include_components = bool(params.get("include_components", False))
        detailed_components = bool(params.get("detailed_components", True))
        resolved_world = self._resolve_world(str(params.get("world_type", "auto")))
        matching_actors = [
            actor
            for actor in self._get_all_actors(resolved_world.world)
            if actor and pattern in actor.get_name()
        ]

        actor_payload = [
            self._serializer.serialize_actor(actor, include_components, detailed_components)
            for actor in matching_actors
        ]

        result = {
            "success": True,
            "pattern": pattern,
            "actors": actor_payload,
            "actor_count": len(actor_payload),
        }
        self._append_world_info(result, resolved_world)
        return result

    def find_actors(self, params: Dict[str, Any]) -> Dict[str, Any]:
        include_components = bool(params.get("include_components", False))
        detailed_components = bool(params.get("detailed_components", True))
        include_data_layers = bool(params.get("include_data_layers", False))
        resolved_world = self._resolve_world(str(params.get("world_type", "auto")))
        filters = self._build_actor_filters(params)

        if filters["data_layer"] and not self._supports_data_layer_queries():
            raise EditorCommandError("当前 UE Python API 未暴露稳定的 Actor DataLayer 查询能力")

        matching_actors = [
            actor
            for actor in self._get_all_actors(resolved_world.world)
            if actor and self._actor_matches_filters(actor, filters)
        ]
        sorted_actors = self._sort_actors(matching_actors, filters["sort_by"], filters["sort_desc"])

        actor_payload: List[Dict[str, Any]] = []
        for actor in sorted_actors:
            serialized_actor = self._serializer.serialize_actor(
                actor,
                include_components=include_components,
                detailed_components=detailed_components,
            )
            if include_data_layers or filters["data_layer"]:
                serialized_actor["data_layers"] = self._get_actor_data_layers(actor)
                serialized_actor["data_layer_count"] = len(serialized_actor["data_layers"])
            actor_payload.append(serialized_actor)

        result = {
            "success": True,
            "actors": actor_payload,
            "actor_count": len(actor_payload),
            "filters": self._serialize_filters(filters, include_data_layers),
        }
        self._append_world_info(result, resolved_world)
        return result

    def get_actor_properties(self, params: Dict[str, Any]) -> Dict[str, Any]:
        include_components = bool(params.get("include_components", True))
        detailed_components = bool(params.get("detailed_components", True))
        resolved_actor = self._resolve_actor(params)

        result = self._serializer.serialize_actor(
            resolved_actor.actor,
            include_components,
            detailed_components,
        )
        result["success"] = True
        self._append_world_info(
            result,
            ResolvedWorld(resolved_actor.world, resolved_actor.resolved_world_type),
        )
        return result

    def get_actor_components(self, params: Dict[str, Any]) -> Dict[str, Any]:
        detailed_components = bool(params.get("detailed_components", True))
        resolved_actor = self._resolve_actor(params)
        actor_payload = self._serializer.serialize_actor(
            resolved_actor.actor,
            include_components=True,
            detailed_components=detailed_components,
        )
        components = actor_payload.get("components", [])
        result = {
            "success": True,
            "actor": actor_payload,
            "components": components,
            "component_count": len(components),
        }
        self._append_world_info(
            result,
            ResolvedWorld(resolved_actor.world, resolved_actor.resolved_world_type),
        )
        return result

    def get_scene_components(self, params: Dict[str, Any]) -> Dict[str, Any]:
        detailed_components = bool(params.get("detailed_components", True))
        pattern = str(params.get("pattern", "")).strip()
        resolved_world = self._resolve_world(str(params.get("world_type", "auto")))

        actor_payload: List[Dict[str, Any]] = []
        total_components = 0
        for actor in self._get_all_actors(resolved_world.world):
            if not actor:
                continue
            if pattern and pattern not in actor.get_name():
                continue

            serialized_actor = self._serializer.serialize_actor(
                actor,
                include_components=True,
                detailed_components=detailed_components,
            )
            actor_payload.append(serialized_actor)
            total_components += len(serialized_actor.get("components", []))

        result = {
            "success": True,
            "actors": actor_payload,
            "actor_count": len(actor_payload),
            "component_count": total_components,
        }
        self._append_world_info(result, resolved_world)
        return result

    @staticmethod
    def _get_all_actors(world: unreal.World) -> List[unreal.Actor]:
        return list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor) or [])

    @staticmethod
    def _resolve_world(requested_world_type: str) -> ResolvedWorld:
        normalized_world_type = (requested_world_type or "auto").strip().casefold() or "auto"
        editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        if not editor_subsystem:
            raise EditorCommandError("UnrealEditorSubsystem 不可用")

        editor_world = editor_subsystem.get_editor_world() if hasattr(editor_subsystem, "get_editor_world") else None
        game_world = editor_subsystem.get_game_world() if hasattr(editor_subsystem, "get_game_world") else None

        if normalized_world_type == "editor":
            if not editor_world:
                raise EditorCommandError("当前没有可用的 editor 世界")
            return ResolvedWorld(world=editor_world, resolved_world_type="editor")
        if normalized_world_type == "pie":
            if not game_world:
                raise EditorCommandError("当前没有运行中的 PIE/Game 世界")
            return ResolvedWorld(world=game_world, resolved_world_type="pie")
        if normalized_world_type == "auto":
            if game_world:
                return ResolvedWorld(world=game_world, resolved_world_type="pie")
            if editor_world:
                return ResolvedWorld(world=editor_world, resolved_world_type="editor")
            raise EditorCommandError("当前没有可用的 world")
        raise EditorCommandError("world_type 仅支持 auto、editor、pie")

    @staticmethod
    def _append_world_info(result: Dict[str, Any], resolved_world: ResolvedWorld) -> None:
        result["resolved_world_type"] = resolved_world.resolved_world_type
        result["world_type"] = ActorQueryHelper._serialize_world_type(resolved_world.resolved_world_type)
        result["world_name"] = resolved_world.world.get_name()
        result["world_path"] = resolved_world.world.get_path_name()

    def _resolve_actor(self, params: Dict[str, Any]) -> ResolvedActor:
        actor_name = str(params.get("name", "")).strip()
        actor_path = str(params.get("actor_path", "")).strip()
        if not actor_name and not actor_path:
            raise EditorCommandError("缺少 'name' 或 'actor_path' 参数")

        resolved_world = self._resolve_world(str(params.get("world_type", "auto")))
        target_actor: unreal.Actor | None = None
        for actor in self._get_all_actors(resolved_world.world):
            if not actor:
                continue
            if actor_path and actor.get_path_name().casefold() == actor_path.casefold():
                target_actor = actor
                break
            if actor_name and actor.get_name().casefold() == actor_name.casefold():
                target_actor = actor
                break
            if actor_name and hasattr(actor, "get_actor_label"):
                if actor.get_actor_label().casefold() == actor_name.casefold():
                    target_actor = actor
                    break

        if not target_actor:
            target_reference = actor_path or actor_name
            raise EditorCommandError(f"未找到目标 Actor: {target_reference}")

        return ResolvedActor(
            actor=target_actor,
            world=resolved_world.world,
            resolved_world_type=resolved_world.resolved_world_type,
        )

    @staticmethod
    def _serialize_world_type(resolved_world_type: str) -> str:
        if resolved_world_type == "pie":
            return "PIE"
        return "Editor"

    def _build_actor_filters(self, params: Dict[str, Any]) -> Dict[str, Any]:
        tags_param = params.get("tags", [])
        if tags_param is None:
            tags_param = []
        if not isinstance(tags_param, list):
            raise EditorCommandError("'tags' 必须是字符串数组")

        normalized_tags = []
        for raw_tag in tags_param:
            tag_text = str(raw_tag).strip()
            if tag_text:
                normalized_tags.append(tag_text)

        single_tag = str(params.get("tag", "")).strip()
        if single_tag:
            normalized_tags.append(single_tag)

        return {
            "name_pattern": str(params.get("name_pattern", "")).strip(),
            "class_name": str(params.get("class_name", "")).strip(),
            "folder_path": str(params.get("folder_path", "")).strip(),
            "path_contains": str(params.get("path_contains", "")).strip(),
            "data_layer": str(params.get("data_layer", "")).strip(),
            "tags": normalized_tags,
            "sort_by": self._normalize_sort_by(str(params.get("sort_by", "name")).strip()),
            "sort_desc": bool(params.get("sort_desc", False)),
        }

    @staticmethod
    def _normalize_sort_by(sort_by: str) -> str:
        normalized_sort_by = sort_by.casefold() or "name"
        allowed_sort_fields = {
            "name",
            "label",
            "path",
            "class",
            "folder_path",
        }
        if normalized_sort_by not in allowed_sort_fields:
            raise EditorCommandError(
                f"sort_by 仅支持 {', '.join(sorted(allowed_sort_fields))}"
            )
        return normalized_sort_by

    def _actor_matches_filters(self, actor: unreal.Actor, filters: Dict[str, Any]) -> bool:
        name_pattern = filters["name_pattern"].casefold()
        if name_pattern:
            actor_name = actor.get_name().casefold()
            actor_label = actor.get_actor_label().casefold() if hasattr(actor, "get_actor_label") else ""
            if name_pattern not in actor_name and name_pattern not in actor_label:
                return False

        class_name = filters["class_name"].casefold()
        if class_name and actor.get_class().get_name().casefold() != class_name:
            return False

        folder_path = filters["folder_path"].casefold()
        if folder_path:
            actor_folder_path = str(actor.get_folder_path()).strip().casefold() if hasattr(actor, "get_folder_path") else ""
            if actor_folder_path != folder_path:
                return False

        path_contains = filters["path_contains"].casefold()
        if path_contains and path_contains not in actor.get_path_name().casefold():
            return False

        requested_tags = filters["tags"]
        if requested_tags:
            actor_tags = {str(tag).casefold() for tag in list(actor.tags or [])}
            if any(tag.casefold() not in actor_tags for tag in requested_tags):
                return False

        requested_data_layer = filters["data_layer"].casefold()
        if requested_data_layer:
            actor_data_layers = self._get_actor_data_layers(actor)
            if not any(
                requested_data_layer in entry["name"].casefold()
                or requested_data_layer in entry["path"].casefold()
                for entry in actor_data_layers
            ):
                return False

        return True

    @staticmethod
    def _supports_data_layer_queries() -> bool:
        return hasattr(unreal.Actor, "get_data_layer_assets")

    @staticmethod
    def _get_actor_data_layers(actor: unreal.Actor) -> List[Dict[str, str]]:
        if not hasattr(actor, "get_data_layer_assets"):
            return []

        try:
            data_layer_assets = actor.get_data_layer_assets(False)
        except TypeError:
            data_layer_assets = actor.get_data_layer_assets()
        except Exception as exc:
            raise EditorCommandError(f"读取 Actor DataLayer 失败: {actor.get_name()}") from exc

        data_layer_payload: List[Dict[str, str]] = []
        for data_layer_asset in list(data_layer_assets or []):
            if not data_layer_asset:
                continue
            data_layer_payload.append(
                {
                    "name": data_layer_asset.get_name(),
                    "path": data_layer_asset.get_path_name(),
                }
            )
        return data_layer_payload

    def _sort_actors(
        self,
        actors: List[unreal.Actor],
        sort_by: str,
        sort_desc: bool,
    ) -> List[unreal.Actor]:
        return sorted(
            actors,
            key=lambda actor: self._get_actor_sort_key(actor, sort_by),
            reverse=sort_desc,
        )

    @staticmethod
    def _get_actor_sort_key(actor: unreal.Actor, sort_by: str) -> str:
        if sort_by == "label":
            return actor.get_actor_label().casefold() if hasattr(actor, "get_actor_label") else actor.get_name().casefold()
        if sort_by == "path":
            return actor.get_path_name().casefold()
        if sort_by == "class":
            return actor.get_class().get_name().casefold()
        if sort_by == "folder_path":
            return str(actor.get_folder_path()).casefold() if hasattr(actor, "get_folder_path") else ""
        return actor.get_name().casefold()

    @staticmethod
    def _serialize_filters(filters: Dict[str, Any], include_data_layers: bool) -> Dict[str, Any]:
        return {
            "name_pattern": filters["name_pattern"],
            "class_name": filters["class_name"],
            "folder_path": filters["folder_path"],
            "path_contains": filters["path_contains"],
            "data_layer": filters["data_layer"],
            "tags": list(filters["tags"]),
            "sort_by": filters["sort_by"],
            "sort_desc": filters["sort_desc"],
            "include_data_layers": include_data_layers,
        }

    @staticmethod
    def _require_string(params: Dict[str, Any], field_name: str) -> str:
        value = str(params.get(field_name, "")).strip()
        if not value:
            raise EditorCommandError(f"缺少 '{field_name}' 参数")
        return value


class ActorEditHelper:
    """Edit level actors through local editor Python APIs."""

    ACTOR_CLASS_MAP: Dict[str, Any] = {
        "staticmeshactor": unreal.StaticMeshActor,
        "pointlight": unreal.PointLight,
        "spotlight": unreal.SpotLight,
        "directionallight": unreal.DirectionalLight,
        "cameraactor": unreal.CameraActor,
    }
    MOBILITY_MAP: Dict[str, Any] = {
        "static": unreal.ComponentMobility.STATIC,
        "stationary": unreal.ComponentMobility.STATIONARY,
        "movable": unreal.ComponentMobility.MOVABLE,
    }

    def __init__(self, serializer: EditorSelectionSerializer, actor_query_helper: ActorQueryHelper) -> None:
        self._serializer = serializer
        self._actor_query_helper = actor_query_helper

    def spawn_actor(self, params: Dict[str, Any]) -> Dict[str, Any]:
        actor_name = self._require_string(params, "name")
        actor_type_text = self._require_string(params, "type")
        actor_class = self._resolve_actor_class(actor_type_text)
        resolved_world = self._actor_query_helper._resolve_world("editor")

        existing_actor = self._find_actor_by_name(resolved_world.world, actor_name)
        if existing_actor:
            raise EditorCommandError(f"已存在同名 Actor: {actor_name}")

        actor_subsystem = self._get_editor_actor_subsystem()
        location = self._get_vector_param(params, "location", [0.0, 0.0, 0.0])
        rotation = self._get_rotator_param(params, "rotation", [0.0, 0.0, 0.0])
        scale = self._get_vector_param(params, "scale", [1.0, 1.0, 1.0])

        actor = actor_subsystem.spawn_actor_from_class(actor_class, location=location, rotation=rotation)
        if not actor:
            raise EditorCommandError(f"创建 Actor 失败: {actor_name}")

        try:
            actor.rename(actor_name)
            if hasattr(actor, "set_actor_label"):
                actor.set_actor_label(actor_name)
            actor.set_actor_scale3d(scale)

            if issubclass(actor_class, unreal.StaticMeshActor) and "static_mesh" in params:
                self._apply_static_mesh(actor, self._require_string(params, "static_mesh"))
        except Exception as exc:
            self._destroy_actor(actor)
            raise EditorCommandError(f"初始化 Actor 失败: {exc}") from exc

        result = self._serializer.serialize_actor(actor, include_components=True, detailed_components=True)
        result["success"] = True
        result["created"] = True
        result["requested_name"] = actor_name
        result["requested_type"] = actor_type_text.strip()
        self._actor_query_helper._append_world_info(result, resolved_world)
        return result

    def spawn_actor_from_class(self, params: Dict[str, Any]) -> Dict[str, Any]:
        actor_name = self._require_string(params, "actor_name")
        class_path = self._require_string(params, "class_path")
        requested_world_type = str(params.get("world_type", "editor"))
        resolved_world = self._actor_query_helper._resolve_world(requested_world_type)
        if resolved_world.resolved_world_type != "editor":
            raise EditorCommandError("spawn_actor_from_class 当前只支持 editor 世界")

        existing_actor = self._find_actor_by_name(resolved_world.world, actor_name)
        if existing_actor:
            raise EditorCommandError(f"已存在同名 Actor: {actor_name}")

        actor_class = self._resolve_actor_class_reference(class_path)
        location = self._get_vector_param(params, "location", [0.0, 0.0, 0.0])
        rotation = self._get_rotator_param(params, "rotation", [0.0, 0.0, 0.0])
        scale = self._get_vector_param(params, "scale", [1.0, 1.0, 1.0])
        actor_subsystem = self._get_editor_actor_subsystem()
        actor = actor_subsystem.spawn_actor_from_class(actor_class, location=location, rotation=rotation)
        if not actor:
            raise EditorCommandError(f"按类路径生成 Actor 失败: {actor_name}")

        try:
            actor.rename(actor_name)
            if hasattr(actor, "set_actor_label"):
                actor.set_actor_label(actor_name)
            actor.set_actor_scale3d(scale)
        except Exception as exc:
            self._destroy_actor(actor)
            raise EditorCommandError(f"初始化类路径 Actor 失败: {exc}") from exc

        result = self._serializer.serialize_actor(actor, include_components=True, detailed_components=True)
        result["success"] = True
        result["created"] = True
        result["requested_name"] = actor_name
        result["class_path"] = class_path
        result["resolved_class_path"] = actor_class.get_path_name() if hasattr(actor_class, "get_path_name") else str(actor_class)
        result["resolved_class_name"] = actor_class.get_name() if hasattr(actor_class, "get_name") else str(actor_class)
        result["implementation"] = "local_python"
        self._actor_query_helper._append_world_info(result, resolved_world)
        return result

    def delete_actor(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_actor = self._resolve_editor_actor(params)
        deleted_actor = self._serializer.serialize_actor(
            resolved_actor.actor,
            include_components=True,
            detailed_components=True,
        )
        self._destroy_actor(resolved_actor.actor)

        result = {
            "success": True,
            "deleted_actor": deleted_actor,
        }
        self._actor_query_helper._append_world_info(
            result,
            ResolvedWorld(resolved_actor.world, resolved_actor.resolved_world_type),
        )
        return result

    def duplicate_actor(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_actor = self._actor_query_helper._resolve_actor(params)
        offset = self._get_vector_param(params, "offset", [0.0, 0.0, 0.0]) if "offset" in params else unreal.Vector(0.0, 0.0, 0.0)
        actor_subsystem = self._get_editor_actor_subsystem()
        duplicated_actor = actor_subsystem.duplicate_actor(
            resolved_actor.actor,
            to_world=resolved_actor.world,
            offset=offset,
        )
        if not duplicated_actor:
            raise EditorCommandError(f"复制 Actor 失败: {resolved_actor.actor.get_name()}")

        result = self._serializer.serialize_actor(
            duplicated_actor,
            include_components=True,
            detailed_components=True,
        )
        result["success"] = True
        result["source_actor"] = resolved_actor.actor.get_name()
        self._actor_query_helper._append_world_info(
            result,
            ResolvedWorld(resolved_actor.world, resolved_actor.resolved_world_type),
        )
        return result

    def resolve_actor_for_selection(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_actor = self._actor_query_helper._resolve_actor(params)
        result = {
            "success": True,
            "actor": resolved_actor.actor.get_name(),
            "actor_path": resolved_actor.actor.get_path_name(),
            "label": resolved_actor.actor.get_actor_label() if hasattr(resolved_actor.actor, "get_actor_label") else "",
        }
        self._actor_query_helper._append_world_info(
            result,
            ResolvedWorld(resolved_actor.world, resolved_actor.resolved_world_type),
        )
        return result

    def set_actor_transform(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_actor = self._resolve_editor_actor(params)
        actor = resolved_actor.actor
        updated_fields: List[str] = []

        if "location" in params:
            actor.set_actor_location(self._get_vector_param(params, "location", [0.0, 0.0, 0.0]), False, False)
            updated_fields.append("location")
        if "rotation" in params:
            actor.set_actor_rotation(self._get_rotator_param(params, "rotation", [0.0, 0.0, 0.0]), False)
            updated_fields.append("rotation")
        if "scale" in params:
            actor.set_actor_scale3d(self._get_vector_param(params, "scale", [1.0, 1.0, 1.0]))
            updated_fields.append("scale")

        if not updated_fields:
            raise EditorCommandError("至少需要提供 location、rotation、scale 之一")

        result = self._serializer.serialize_actor(actor, include_components=True, detailed_components=True)
        result["success"] = True
        result["updated_fields"] = updated_fields
        self._actor_query_helper._append_world_info(
            result,
            ResolvedWorld(resolved_actor.world, resolved_actor.resolved_world_type),
        )
        return result

    def set_actors_transform(self, params: Dict[str, Any]) -> Dict[str, Any]:
        actor_names = params.get("actor_names")
        if not isinstance(actor_names, list) or len(actor_names) == 0:
            raise EditorCommandError("缺少 'actor_names' 参数")

        has_location = "location" in params
        has_rotation = "rotation" in params
        has_scale = "scale" in params
        if not has_location and not has_rotation and not has_scale:
            raise EditorCommandError("至少需要提供 location、rotation、scale 之一")

        location = self._get_vector_param(params, "location", [0.0, 0.0, 0.0]) if has_location else None
        rotation = self._get_rotator_param(params, "rotation", [0.0, 0.0, 0.0]) if has_rotation else None
        scale = self._get_vector_param(params, "scale", [1.0, 1.0, 1.0]) if has_scale else None

        actor_payload: List[Dict[str, Any]] = []
        first_resolved_world: Optional[ResolvedWorld] = None
        requested_world_type = str(params.get("world_type", "auto"))

        for raw_actor_name in actor_names:
            actor_name = str(raw_actor_name).strip()
            if not actor_name:
                continue

            resolved_actor = self._actor_query_helper._resolve_actor(
                {
                    "name": actor_name,
                    "world_type": requested_world_type,
                }
            )

            if first_resolved_world is None:
                first_resolved_world = ResolvedWorld(
                    world=resolved_actor.world,
                    resolved_world_type=resolved_actor.resolved_world_type,
                )

            actor = resolved_actor.actor
            if location is not None:
                actor.set_actor_location(location, False, False)
            if rotation is not None:
                actor.set_actor_rotation(rotation, False)
            if scale is not None:
                actor.set_actor_scale3d(scale)

            actor_payload.append(
                self._serializer.serialize_actor(
                    actor,
                    include_components=False,
                    detailed_components=True,
                )
            )

        result = {
            "success": True,
            "actors": actor_payload,
            "actor_count": len(actor_payload),
        }
        if first_resolved_world is not None:
            self._actor_query_helper._append_world_info(result, first_resolved_world)
        return result

    def set_actor_property(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_actor = self._resolve_editor_actor(params)
        property_name = self._require_string(params, "property_name")
        if "property_value" not in params:
            raise EditorCommandError("缺少 'property_value' 参数")

        actor = resolved_actor.actor
        property_value = params["property_value"]
        converted_value = self._convert_property_value(actor, property_name, property_value)

        try:
            actor.modify()
            actor.set_editor_property(property_name, converted_value)
            if hasattr(actor, "post_edit_change"):
                actor.post_edit_change()
        except Exception as exc:
            raise EditorCommandError(f"设置属性失败 {property_name}: {exc}") from exc

        result = {
            "success": True,
            "actor": actor.get_name(),
            "property": property_name,
            "property_value": self._serialize_property_value(actor, property_name),
            "actor_details": self._serializer.serialize_actor(
                actor,
                include_components=True,
                detailed_components=True,
            ),
        }
        self._actor_query_helper._append_world_info(
            result,
            ResolvedWorld(resolved_actor.world, resolved_actor.resolved_world_type),
        )
        return result

    def set_actor_tags(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_actor = self._resolve_editor_actor(params)
        raw_tags = params.get("tags")
        if not isinstance(raw_tags, list):
            raise EditorCommandError("'tags' 必须是字符串数组")

        tag_names: List[unreal.Name] = []
        serialized_tags: List[str] = []
        for raw_tag in raw_tags:
            tag_text = str(raw_tag).strip()
            if not tag_text:
                continue
            tag_name = unreal.Name(tag_text)
            tag_names.append(tag_name)
            serialized_tags.append(str(tag_name))

        actor = resolved_actor.actor
        actor.modify()
        actor.tags = tag_names
        if hasattr(actor, "post_edit_change"):
            actor.post_edit_change()

        result = {
            "success": True,
            "actor": actor.get_name(),
            "actor_path": actor.get_path_name(),
            "tags": serialized_tags,
            "tag_count": len(serialized_tags),
            "actor_details": self._serializer.serialize_actor(
                actor,
                include_components=True,
                detailed_components=True,
            ),
            "implementation": "local_python",
        }
        self._actor_query_helper._append_world_info(
            result,
            ResolvedWorld(resolved_actor.world, resolved_actor.resolved_world_type),
        )
        return result

    def set_actor_folder_path(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_actor = self._resolve_editor_actor(params)
        folder_path = str(params.get("folder_path", "")).strip()
        actor = resolved_actor.actor

        actor.modify()
        actor.set_folder_path(unreal.Name(folder_path))
        if hasattr(actor, "post_edit_change"):
            actor.post_edit_change()

        result = {
            "success": True,
            "actor": actor.get_name(),
            "actor_path": actor.get_path_name(),
            "folder_path": str(actor.get_folder_path()) if hasattr(actor, "get_folder_path") else folder_path,
            "actor_details": self._serializer.serialize_actor(
                actor,
                include_components=True,
                detailed_components=True,
            ),
            "implementation": "local_python",
        }
        self._actor_query_helper._append_world_info(
            result,
            ResolvedWorld(resolved_actor.world, resolved_actor.resolved_world_type),
        )
        return result

    def set_actor_visibility(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_actor = self._resolve_editor_actor(params)
        if "visible" not in params:
            raise EditorCommandError("缺少 'visible' 参数")

        visible = bool(params["visible"])
        actor = resolved_actor.actor
        root_component = self._get_root_scene_component(actor)

        actor.modify()
        if hasattr(actor, "set_actor_hidden_in_game"):
            actor.set_actor_hidden_in_game(not visible)
        if hasattr(actor, "set_is_temporarily_hidden_in_editor"):
            actor.set_is_temporarily_hidden_in_editor(not visible)
        if root_component and hasattr(root_component, "set_hidden_in_game"):
            root_component.set_hidden_in_game(not visible)
        elif root_component and hasattr(root_component, "set_editor_property"):
            root_component.modify()
            try:
                root_component.set_editor_property("hidden_in_game", not visible)
            except Exception:
                pass
        if hasattr(actor, "post_edit_change"):
            actor.post_edit_change()

        result = {
            "success": True,
            "actor": actor.get_name(),
            "actor_path": actor.get_path_name(),
            "visible": visible,
            "hidden_in_game": not visible,
            "editor_temporarily_hidden": not visible,
            "actor_details": self._serializer.serialize_actor(
                actor,
                include_components=True,
                detailed_components=True,
            ),
            "implementation": "local_python",
        }
        self._actor_query_helper._append_world_info(
            result,
            ResolvedWorld(resolved_actor.world, resolved_actor.resolved_world_type),
        )
        return result

    def set_actor_mobility(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_actor = self._resolve_editor_actor(params)
        mobility_text = self._require_string(params, "mobility").casefold()
        if mobility_text not in self.MOBILITY_MAP:
            raise EditorCommandError("mobility 仅支持 Static、Stationary、Movable")

        actor = resolved_actor.actor
        root_component = self._get_root_scene_component(actor)
        if not root_component or not hasattr(root_component, "get_editor_property"):
            raise EditorCommandError("目标 Actor 缺少可设置 mobility 的根 SceneComponent")

        actor.modify()
        root_component.modify()
        if hasattr(root_component, "set_mobility"):
            root_component.set_mobility(self.MOBILITY_MAP[mobility_text])
        else:
            root_component.set_editor_property("mobility", self.MOBILITY_MAP[mobility_text])
        if hasattr(actor, "post_edit_change"):
            actor.post_edit_change()

        applied_mobility = self._serializer._mobility_to_string(root_component.get_editor_property("mobility"))
        result = {
            "success": True,
            "actor": actor.get_name(),
            "actor_path": actor.get_path_name(),
            "mobility": applied_mobility,
            "root_component": root_component.get_name(),
            "root_component_path": root_component.get_path_name(),
            "actor_details": self._serializer.serialize_actor(
                actor,
                include_components=True,
                detailed_components=True,
            ),
            "implementation": "local_python",
        }
        self._actor_query_helper._append_world_info(
            result,
            ResolvedWorld(resolved_actor.world, resolved_actor.resolved_world_type),
        )
        return result

    def spawn_blueprint_actor(self, params: Dict[str, Any]) -> Dict[str, Any]:
        blueprint_name = self._require_string(params, "blueprint_name")
        actor_name = self._require_string(params, "actor_name")
        resolved_world = self._actor_query_helper._resolve_world("editor")

        existing_actor = self._find_actor_by_name(resolved_world.world, actor_name)
        if existing_actor:
            raise EditorCommandError(f"已存在同名 Actor: {actor_name}")

        blueprint_asset = self._resolve_blueprint_asset(blueprint_name)
        generated_class = self._resolve_blueprint_generated_class(blueprint_asset)
        location = self._get_vector_param(params, "location", [0.0, 0.0, 0.0])
        rotation = self._get_rotator_param(params, "rotation", [0.0, 0.0, 0.0])
        scale = self._get_vector_param(params, "scale", [1.0, 1.0, 1.0])

        actor_subsystem = self._get_editor_actor_subsystem()
        actor = actor_subsystem.spawn_actor_from_class(generated_class, location=location, rotation=rotation)
        if not actor:
            raise EditorCommandError(f"生成 Blueprint Actor 失败: {actor_name}")

        try:
            actor.rename(actor_name)
            if hasattr(actor, "set_actor_label"):
                actor.set_actor_label(actor_name)
            actor.set_actor_scale3d(scale)
        except Exception as exc:
            self._destroy_actor(actor)
            raise EditorCommandError(f"初始化 Blueprint Actor 失败: {exc}") from exc

        result = self._serializer.serialize_actor(actor, include_components=True, detailed_components=True)
        result["success"] = True
        result["created"] = True
        result["requested_name"] = actor_name
        result["blueprint_name"] = blueprint_name
        result["blueprint_asset_path"] = blueprint_asset.get_path_name()
        result["blueprint_generated_class"] = generated_class.get_path_name()
        self._actor_query_helper._append_world_info(result, resolved_world)
        return result

    def attach_actor(self, params: Dict[str, Any]) -> Dict[str, Any]:
        child_actor = self._actor_query_helper._resolve_actor(params)
        parent_params = self._build_parent_actor_params(params, child_actor.resolved_world_type)
        parent_actor = self._actor_query_helper._resolve_actor(parent_params)
        socket_name = str(params.get("socket_name", "")).strip()
        keep_world_transform = bool(params.get("keep_world_transform", True))
        attachment_rule = self._get_attachment_rule(keep_world_transform)

        attached = child_actor.actor.attach_to_actor(
            parent_actor.actor,
            socket_name,
            attachment_rule,
            attachment_rule,
            attachment_rule,
        )
        if not attached:
            raise EditorCommandError("挂接 Actor 失败")

        result = {
            "success": True,
            "child_actor": child_actor.actor.get_name(),
            "child_actor_path": child_actor.actor.get_path_name(),
            "parent_actor": parent_actor.actor.get_name(),
            "parent_actor_path": parent_actor.actor.get_path_name(),
            "socket_name": socket_name,
            "keep_world_transform": keep_world_transform,
            "implementation": "local_python",
        }
        self._actor_query_helper._append_world_info(
            result,
            ResolvedWorld(child_actor.world, child_actor.resolved_world_type),
        )
        return result

    def detach_actor(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_actor = self._actor_query_helper._resolve_actor(params)
        keep_world_transform = bool(params.get("keep_world_transform", True))
        detachment_rule = self._get_detachment_rule(keep_world_transform)
        resolved_actor.actor.detach_from_actor(
            detachment_rule,
            detachment_rule,
            detachment_rule,
        )

        result = {
            "success": True,
            "actor": resolved_actor.actor.get_name(),
            "actor_path": resolved_actor.actor.get_path_name(),
            "keep_world_transform": keep_world_transform,
            "implementation": "local_python",
        }
        self._actor_query_helper._append_world_info(
            result,
            ResolvedWorld(resolved_actor.world, resolved_actor.resolved_world_type),
        )
        return result

    def _resolve_editor_actor(self, params: Dict[str, Any]) -> ResolvedActor:
        scoped_params = dict(params)
        scoped_params["world_type"] = "editor"
        return self._actor_query_helper._resolve_actor(scoped_params)

    def _build_parent_actor_params(
        self,
        params: Dict[str, Any],
        fallback_world_type: str,
    ) -> Dict[str, Any]:
        parent_params: Dict[str, Any] = {
            "world_type": str(params.get("world_type", fallback_world_type) or fallback_world_type),
        }
        parent_name = str(params.get("parent_name", "")).strip()
        parent_actor_path = str(params.get("parent_actor_path", "")).strip()
        if parent_name:
            parent_params["name"] = parent_name
        if parent_actor_path:
            parent_params["actor_path"] = parent_actor_path
        if "name" not in parent_params and "actor_path" not in parent_params:
            raise EditorCommandError("缺少 'parent_name' 或 'parent_actor_path' 参数")
        return parent_params

    def _find_actor_by_name(self, world: unreal.World, actor_name: str) -> Optional[unreal.Actor]:
        for actor in self._actor_query_helper._get_all_actors(world):
            if actor and actor.get_name() == actor_name:
                return actor
        return None

    @staticmethod
    def _get_root_scene_component(actor: unreal.Actor) -> Any:
        root_component = actor.get_root_component() if hasattr(actor, "get_root_component") else None
        if root_component:
            return root_component
        if hasattr(actor, "get_editor_property"):
            try:
                return actor.get_editor_property("root_component")
            except Exception:
                return None
        return None

    def _resolve_blueprint_asset(self, blueprint_name: str) -> unreal.Object:
        normalized_name = blueprint_name.strip()
        candidate_paths = [normalized_name]
        if not normalized_name.startswith("/"):
            candidate_paths.append(f"/Game/Blueprints/{normalized_name}")
        else:
            candidate_paths.append(normalized_name.rsplit(".", 1)[0])

        for candidate_path in candidate_paths:
            asset = unreal.EditorAssetLibrary.load_asset(candidate_path)
            if asset:
                return asset

        if not normalized_name.startswith("/"):
            raise EditorCommandError(
                f"Blueprint '{blueprint_name}' 未找到，当前仍要求资源位于 /Game/Blueprints 下或传完整资产路径"
            )
        raise EditorCommandError(f"Blueprint 资产未找到: {blueprint_name}")

    def _resolve_actor_class_reference(self, class_path: str) -> Any:
        normalized_reference = class_path.strip()
        if not normalized_reference:
            raise EditorCommandError("缺少 'class_path' 参数")

        resolved_class = None
        if normalized_reference.startswith("/"):
            try:
                resolved_class = unreal.load_class(None, normalized_reference)
            except Exception:
                resolved_class = None

            if not resolved_class:
                blueprint_asset = unreal.EditorAssetLibrary.load_asset(normalized_reference)
                if blueprint_asset:
                    try:
                        resolved_class = self._resolve_blueprint_generated_class(blueprint_asset)
                    except EditorCommandError:
                        resolved_class = None
        else:
            resolved_class = getattr(unreal, normalized_reference, None)
            if resolved_class is None:
                lowered_reference = normalized_reference.casefold()
                for attribute_name in dir(unreal):
                    if attribute_name.casefold() == lowered_reference:
                        resolved_class = getattr(unreal, attribute_name, None)
                        if resolved_class is not None:
                            break

        if not resolved_class:
            raise EditorCommandError(f"无法解析 class_path: {class_path}")

        try:
            if isinstance(resolved_class, type) and not issubclass(resolved_class, unreal.Actor):
                raise EditorCommandError(f"class_path 不是 Actor 类: {class_path}")
        except TypeError:
            pass

        return resolved_class

    @staticmethod
    def _resolve_blueprint_generated_class(blueprint_asset: unreal.Object) -> Any:
        generated_class = None
        if hasattr(blueprint_asset, "generated_class"):
            generated_class = blueprint_asset.generated_class()
        if not generated_class and hasattr(blueprint_asset, "get_editor_property"):
            try:
                generated_class = blueprint_asset.get_editor_property("generated_class")
            except Exception:
                generated_class = None
        if not generated_class:
            raise EditorCommandError(f"Blueprint 缺少可生成的类: {blueprint_asset.get_path_name()}")
        return generated_class

    def _convert_property_value(
        self,
        actor: unreal.Actor,
        property_name: str,
        property_value: Any,
    ) -> Any:
        try:
            current_value = actor.get_editor_property(property_name)
        except Exception as exc:
            raise EditorCommandError(f"未找到属性: {property_name}") from exc

        if isinstance(current_value, bool):
            return bool(property_value)
        if isinstance(current_value, int) and not isinstance(current_value, bool):
            return int(property_value)
        if isinstance(current_value, float):
            return float(property_value)
        if isinstance(current_value, str):
            return str(property_value)
        if self._is_name_like(current_value):
            return unreal.Name(str(property_value))

        enum_type = type(current_value)
        resolved_enum = self._try_resolve_enum_value(enum_type, property_value)
        if resolved_enum is not None:
            return resolved_enum

        return property_value

    def _serialize_property_value(self, actor: unreal.Actor, property_name: str) -> Any:
        property_value = actor.get_editor_property(property_name)
        if self._is_name_like(property_value):
            return str(property_value)
        if hasattr(property_value, "name"):
            return str(property_value.name)
        if isinstance(property_value, unreal.Vector):
            return self._vector_to_list(property_value)
        if isinstance(property_value, unreal.Rotator):
            return self._rotator_to_list(property_value)
        if isinstance(property_value, unreal.LinearColor):
            return self._serializer._linear_color_to_list(property_value)
        if isinstance(property_value, (bool, int, float, str)):
            return property_value
        return str(property_value)

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

    @staticmethod
    def _resolve_actor_class(actor_type_text: str) -> Any:
        normalized_type = actor_type_text.strip().casefold()
        if normalized_type not in ActorEditHelper.ACTOR_CLASS_MAP:
            raise EditorCommandError(f"不支持的 Actor 类型: {actor_type_text}")
        return ActorEditHelper.ACTOR_CLASS_MAP[normalized_type]

    @staticmethod
    def _apply_static_mesh(actor: unreal.Actor, mesh_path: str) -> None:
        mesh_asset = unreal.EditorAssetLibrary.load_asset(mesh_path)
        if not mesh_asset:
            raise EditorCommandError(f"加载静态网格失败: {mesh_path}")
        if not isinstance(mesh_asset, unreal.StaticMesh):
            raise EditorCommandError(f"资源不是 StaticMesh: {mesh_path}")

        if not isinstance(actor, unreal.StaticMeshActor):
            raise EditorCommandError("只有 StaticMeshActor 支持 static_mesh 参数")

        static_mesh_component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if not static_mesh_component:
            raise EditorCommandError("StaticMeshActor 缺少 StaticMeshComponent")
        static_mesh_component.set_static_mesh(mesh_asset)

    @staticmethod
    def _destroy_actor(actor: unreal.Actor) -> None:
        actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if actor_subsystem and hasattr(actor_subsystem, "destroy_actor"):
            destroyed = actor_subsystem.destroy_actor(actor)
            if destroyed:
                return

        if hasattr(actor, "destroy_actor") and actor.destroy_actor():
            return

        raise EditorCommandError(f"删除 Actor 失败: {actor.get_name()}")

    @staticmethod
    def _get_attachment_rule(keep_world_transform: bool) -> unreal.AttachmentRule:
        if keep_world_transform:
            return unreal.AttachmentRule.KEEP_WORLD
        return unreal.AttachmentRule.KEEP_RELATIVE

    @staticmethod
    def _get_detachment_rule(keep_world_transform: bool) -> unreal.DetachmentRule:
        if keep_world_transform:
            return unreal.DetachmentRule.KEEP_WORLD
        return unreal.DetachmentRule.KEEP_RELATIVE

    @staticmethod
    def _get_editor_actor_subsystem() -> unreal.EditorActorSubsystem:
        actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if not actor_subsystem:
            raise EditorCommandError("EditorActorSubsystem 不可用")
        return actor_subsystem

    @staticmethod
    def _get_vector_param(params: Dict[str, Any], field_name: str, default_value: List[float]) -> unreal.Vector:
        values = params.get(field_name, default_value)
        if not isinstance(values, list) or len(values) != 3:
            raise EditorCommandError(f"'{field_name}' 必须是 3 个数字 [X, Y, Z]")
        return unreal.Vector(float(values[0]), float(values[1]), float(values[2]))

    @staticmethod
    def _get_rotator_param(params: Dict[str, Any], field_name: str, default_value: List[float]) -> unreal.Rotator:
        values = params.get(field_name, default_value)
        if not isinstance(values, list) or len(values) != 3:
            raise EditorCommandError(f"'{field_name}' 必须是 3 个数字 [Pitch, Yaw, Roll]")
        return unreal.Rotator(pitch=float(values[0]), yaw=float(values[1]), roll=float(values[2]))

    @staticmethod
    def _vector_to_list(vector: unreal.Vector) -> List[float]:
        return [float(vector.x), float(vector.y), float(vector.z)]

    @staticmethod
    def _rotator_to_list(rotator: unreal.Rotator) -> List[float]:
        return [float(rotator.pitch), float(rotator.yaw), float(rotator.roll)]

    @staticmethod
    def _require_string(params: Dict[str, Any], field_name: str) -> str:
        value = str(params.get(field_name, "")).strip()
        if not value:
            raise EditorCommandError(f"缺少 '{field_name}' 参数")
        return value


class WorldSettingsCommandHelper:
    """Query and edit editor world settings through local Python APIs."""

    DEFAULT_PROPERTY_NAMES = [
        "kill_z",
        "global_gravity_set",
        "global_gravity_z",
        "enable_world_bounds_checks",
        "enable_navigation_system",
        "enable_ai_system",
        "default_game_mode",
        "world_to_meters",
        "enable_world_origin_rebasing",
        "enable_large_worlds",
        "custom_time_dilation",
        "min_undilated_frame_time",
        "max_undilated_frame_time",
        "navigation_system_config",
        "runtime_grid",
    ]

    def __init__(self, actor_query_helper: ActorQueryHelper) -> None:
        self._actor_query_helper = actor_query_helper

    def get_world_settings(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_world = self._resolve_target_world(params)
        world_settings = self._get_world_settings_actor(resolved_world.world)
        property_names = self._get_requested_property_names(params)

        properties: Dict[str, Any] = {}
        missing_properties: List[str] = []
        for property_name in property_names:
            if not property_name:
                continue
            try:
                properties[property_name] = self._serialize_property_value(
                    world_settings.get_editor_property(property_name)
                )
            except Exception:
                missing_properties.append(property_name)

        result = self._build_base_result(world_settings, resolved_world)
        result.update(
            {
                "success": True,
                "properties": properties,
                "property_count": len(properties),
                "requested_properties": property_names,
                "missing_properties": missing_properties,
            }
        )
        return result

    def set_world_settings(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_world = self._resolve_target_world(params)
        world_settings = self._get_world_settings_actor(resolved_world.world)
        requested_settings = self._get_requested_settings(params)
        if len(requested_settings) == 0:
            raise EditorCommandError("缺少 'settings' 参数，且至少要提供一个属性")

        applied_properties: Dict[str, Any] = {}
        world_settings.modify()

        for property_name, raw_value in requested_settings.items():
            converted_value = self._convert_property_value(world_settings, property_name, raw_value)
            try:
                world_settings.set_editor_property(property_name, converted_value)
            except Exception as exc:
                raise EditorCommandError(f"设置 WorldSettings 属性失败 {property_name}: {exc}") from exc
            applied_properties[property_name] = self._serialize_property_value(
                world_settings.get_editor_property(property_name)
            )

        if hasattr(world_settings, "post_edit_change"):
            world_settings.post_edit_change()
        outermost = world_settings.get_outermost() if hasattr(world_settings, "get_outermost") else None
        if outermost and hasattr(outermost, "set_dirty_flag"):
            outermost.set_dirty_flag(True)

        result = self._build_base_result(world_settings, resolved_world)
        result.update(
            {
                "success": True,
                "updated_properties": applied_properties,
                "updated_property_count": len(applied_properties),
            }
        )
        return result

    def _resolve_target_world(self, params: Dict[str, Any]) -> ResolvedWorld:
        requested_world_type = str(params.get("world_type", "editor") or "editor").strip().casefold()
        if requested_world_type == "auto":
            requested_world_type = "editor"
        if requested_world_type != "editor":
            raise EditorCommandError("get_world_settings/set_world_settings 当前只支持 editor 世界")
        return self._actor_query_helper._resolve_world("editor")

    @staticmethod
    def _get_world_settings_actor(world: unreal.World) -> unreal.WorldSettings:
        if not world:
            raise EditorCommandError("当前没有可用的编辑器 World")
        world_settings = world.get_world_settings() if hasattr(world, "get_world_settings") else None
        if not world_settings:
            raise EditorCommandError("当前 World 缺少 WorldSettings")
        return world_settings

    def _build_base_result(
        self,
        world_settings: unreal.WorldSettings,
        resolved_world: ResolvedWorld,
    ) -> Dict[str, Any]:
        result = {
            "world_settings_name": world_settings.get_name(),
            "world_settings_path": world_settings.get_path_name(),
            "world_settings_class": world_settings.get_class().get_path_name(),
            "implementation": "local_python",
        }
        self._actor_query_helper._append_world_info(result, resolved_world)
        return result

    def _get_requested_property_names(self, params: Dict[str, Any]) -> List[str]:
        raw_property_names = params.get("property_names")
        if raw_property_names is None:
            return list(self.DEFAULT_PROPERTY_NAMES)
        if not isinstance(raw_property_names, list):
            raise EditorCommandError("'property_names' 必须是字符串数组")
        property_names: List[str] = []
        for raw_name in raw_property_names:
            property_name = str(raw_name).strip()
            if property_name:
                property_names.append(property_name)
        if len(property_names) == 0:
            raise EditorCommandError("'property_names' 不能为空数组")
        return property_names

    def _get_requested_settings(self, params: Dict[str, Any]) -> Dict[str, Any]:
        raw_settings = params.get("settings")
        if isinstance(raw_settings, dict):
            return {
                str(key).strip(): value
                for key, value in raw_settings.items()
                if str(key).strip()
            }
        if isinstance(raw_settings, str):
            settings_text = raw_settings.strip()
            if not settings_text:
                return {}
            try:
                parsed_settings = json.loads(settings_text)
            except json.JSONDecodeError as exc:
                raise EditorCommandError(f"'settings' 不是合法 JSON 对象: {exc}") from exc
            if not isinstance(parsed_settings, dict):
                raise EditorCommandError("'settings' JSON 必须是对象")
            return {
                str(key).strip(): value
                for key, value in parsed_settings.items()
                if str(key).strip()
            }

        inline_settings: Dict[str, Any] = {}
        for property_name in self.DEFAULT_PROPERTY_NAMES:
            if property_name in params:
                inline_settings[property_name] = params[property_name]
        return inline_settings

    def _convert_property_value(
        self,
        world_settings: unreal.WorldSettings,
        property_name: str,
        property_value: Any,
    ) -> Any:
        try:
            current_value = world_settings.get_editor_property(property_name)
        except Exception as exc:
            raise EditorCommandError(f"未找到 WorldSettings 属性: {property_name}") from exc

        if isinstance(current_value, bool):
            return bool(property_value)
        if isinstance(current_value, int) and not isinstance(current_value, bool):
            return int(property_value)
        if isinstance(current_value, float):
            return float(property_value)
        if isinstance(current_value, str):
            return str(property_value)
        if self._is_name_like(current_value):
            return unreal.Name(str(property_value))
        if isinstance(current_value, unreal.Vector):
            return self._coerce_vector(property_name, property_value)
        if isinstance(current_value, unreal.Rotator):
            return self._coerce_rotator(property_name, property_value)
        if isinstance(current_value, unreal.LinearColor):
            return self._coerce_linear_color(property_name, property_value)

        enum_value = self._try_resolve_enum_value(type(current_value), property_value)
        if enum_value is not None:
            return enum_value

        resolved_reference = self._try_resolve_object_reference(property_value)
        if resolved_reference is not None:
            return resolved_reference

        return property_value

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
        return str(property_value)

    @staticmethod
    def _coerce_vector(property_name: str, raw_value: Any) -> unreal.Vector:
        if not isinstance(raw_value, list) or len(raw_value) != 3:
            raise EditorCommandError(f"WorldSettings 属性 {property_name} 需要 [X, Y, Z]")
        return unreal.Vector(float(raw_value[0]), float(raw_value[1]), float(raw_value[2]))

    @staticmethod
    def _coerce_rotator(property_name: str, raw_value: Any) -> unreal.Rotator:
        if not isinstance(raw_value, list) or len(raw_value) != 3:
            raise EditorCommandError(f"WorldSettings 属性 {property_name} 需要 [Pitch, Yaw, Roll]")
        return unreal.Rotator(float(raw_value[0]), float(raw_value[1]), float(raw_value[2]))

    @staticmethod
    def _coerce_linear_color(property_name: str, raw_value: Any) -> unreal.LinearColor:
        if not isinstance(raw_value, list) or len(raw_value) not in (3, 4):
            raise EditorCommandError(f"WorldSettings 属性 {property_name} 需要 [R, G, B] 或 [R, G, B, A]")
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


class ViewportCommandHelper:
    """Control the active editor viewport through local Python APIs."""

    def __init__(self, actor_query_helper: ActorQueryHelper) -> None:
        self._actor_query_helper = actor_query_helper

    def focus_viewport(self, params: Dict[str, Any]) -> Dict[str, Any]:
        target_reference = str(params.get("target", "")).strip()
        if not target_reference and "location" not in params:
            raise EditorCommandError("至少需要提供 'target' 或 'location'")

        distance = float(params.get("distance", 1000.0))
        if distance < 0.0:
            raise EditorCommandError("'distance' 不能为负数")

        target_actor: Optional[unreal.Actor] = None
        if target_reference:
            resolved_actor = self._actor_query_helper._resolve_actor(
                {
                    "name": target_reference,
                    "actor_path": target_reference,
                    "world_type": "editor",
                }
            )
            target_actor = resolved_actor.actor
            focus_location = target_actor.get_actor_location()
        else:
            focus_location = self._get_vector_param(params, "location")

        camera_rotation = self._resolve_camera_rotation(params)
        camera_location = self._build_camera_location(focus_location, camera_rotation, distance)

        editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        if not editor_subsystem or not hasattr(editor_subsystem, "set_level_viewport_camera_info"):
            raise EditorCommandError("UnrealEditorSubsystem 不支持设置视口相机")
        editor_subsystem.set_level_viewport_camera_info(camera_location, camera_rotation)

        level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if level_subsystem and hasattr(level_subsystem, "editor_invalidate_viewports"):
            level_subsystem.editor_invalidate_viewports()

        resolved_world = self._actor_query_helper._resolve_world("editor")
        result = {
            "success": True,
            "distance": distance,
            "focus_location": self._vector_to_list(focus_location),
            "camera_location": self._vector_to_list(camera_location),
            "camera_rotation": self._rotator_to_list(camera_rotation),
        }
        if target_actor:
            result["target"] = target_reference
            result["target_actor_name"] = target_actor.get_name()
            result["target_actor_path"] = target_actor.get_path_name()
        self._actor_query_helper._append_world_info(result, resolved_world)
        return result

    @staticmethod
    def _resolve_camera_rotation(params: Dict[str, Any]) -> unreal.Rotator:
        if "orientation" in params:
            return ViewportCommandHelper._get_rotator_param(params, "orientation")

        editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        if editor_subsystem and hasattr(editor_subsystem, "get_level_viewport_camera_info"):
            camera_info = editor_subsystem.get_level_viewport_camera_info()
            if isinstance(camera_info, tuple) and len(camera_info) == 2:
                return camera_info[1]
        return unreal.Rotator(0.0, 0.0, 0.0)

    @staticmethod
    def _build_camera_location(
        focus_location: unreal.Vector,
        camera_rotation: unreal.Rotator,
        distance: float,
    ) -> unreal.Vector:
        forward_vector = unreal.MathLibrary.get_forward_vector(camera_rotation)
        return unreal.Vector(
            float(focus_location.x) - float(forward_vector.x) * distance,
            float(focus_location.y) - float(forward_vector.y) * distance,
            float(focus_location.z) - float(forward_vector.z) * distance,
        )

    @staticmethod
    def _get_vector_param(params: Dict[str, Any], field_name: str) -> unreal.Vector:
        values = params.get(field_name)
        if not isinstance(values, list) or len(values) != 3:
            raise EditorCommandError(f"'{field_name}' 必须是 3 个数字 [X, Y, Z]")
        return unreal.Vector(float(values[0]), float(values[1]), float(values[2]))

    @staticmethod
    def _get_rotator_param(params: Dict[str, Any], field_name: str) -> unreal.Rotator:
        values = params.get(field_name)
        if not isinstance(values, list) or len(values) != 3:
            raise EditorCommandError(f"'{field_name}' 必须是 3 个数字 [Pitch, Yaw, Roll]")
        return unreal.Rotator(pitch=float(values[0]), yaw=float(values[1]), roll=float(values[2]))

    @staticmethod
    def _vector_to_list(vector: unreal.Vector) -> List[float]:
        return [float(vector.x), float(vector.y), float(vector.z)]

    @staticmethod
    def _rotator_to_list(rotator: unreal.Rotator) -> List[float]:
        return [float(rotator.pitch), float(rotator.yaw), float(rotator.roll)]


class EditorUtilityCommandHelper:
    """Run editor utility assets through local editor Python APIs."""

    def run_editor_utility_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        asset_path = self._require_string(params, "asset_path")
        editor_utility_subsystem = self._get_editor_utility_subsystem()
        resolved_asset = self._resolve_asset(asset_path)
        utility_widget_blueprint = resolved_asset.asset_object
        if not isinstance(utility_widget_blueprint, unreal.EditorUtilityWidgetBlueprint):
            raise EditorCommandError(f"资产不是 Editor Utility Widget Blueprint: {asset_path}")

        requested_tab_id = str(params.get("tab_id", "")).strip()
        if requested_tab_id:
            resolved_tab_id = unreal.Name(requested_tab_id)
            spawned_widget = editor_utility_subsystem.spawn_and_register_tab_with_id(
                utility_widget_blueprint,
                resolved_tab_id,
            )
        else:
            spawn_result = editor_utility_subsystem.spawn_and_register_tab_and_get_id(
                utility_widget_blueprint
            )
            if not isinstance(spawn_result, tuple) or len(spawn_result) != 2:
                raise EditorCommandError("运行 Editor Utility Widget 时未返回有效标签页信息")
            spawned_widget = spawn_result[0]
            resolved_tab_id = spawn_result[1]

        if not spawned_widget:
            raise EditorCommandError(f"运行 Editor Utility Widget 失败: {asset_path}")

        result = resolved_asset.to_identity()
        result.update(
            {
                "success": True,
                "asset_path": asset_path,
                "object_path": utility_widget_blueprint.get_path_name(),
                "tab_id": str(resolved_tab_id),
                "widget_name": spawned_widget.get_name(),
                "widget_class": spawned_widget.get_class().get_path_name(),
                "implementation": "local_python",
            }
        )
        return result

    def run_editor_utility_blueprint(self, params: Dict[str, Any]) -> Dict[str, Any]:
        asset_path = self._require_string(params, "asset_path")
        editor_utility_subsystem = self._get_editor_utility_subsystem()
        resolved_asset = self._resolve_asset(asset_path)
        utility_asset = resolved_asset.asset_object

        if isinstance(utility_asset, unreal.EditorUtilityWidgetBlueprint):
            raise EditorCommandError("Editor Utility Widget 请改用 run_editor_utility_widget")
        if not isinstance(utility_asset, unreal.EditorUtilityBlueprint):
            raise EditorCommandError(f"资产不是 Editor Utility Blueprint: {asset_path}")
        if not editor_utility_subsystem.can_run(utility_asset):
            raise EditorCommandError(f"Editor Utility Blueprint 不可运行或缺少有效 Run 入口: {asset_path}")
        if not editor_utility_subsystem.try_run(utility_asset):
            raise EditorCommandError(f"运行 Editor Utility Blueprint 失败: {asset_path}")

        generated_class = None
        if hasattr(utility_asset, "get_editor_property"):
            try:
                generated_class = utility_asset.get_editor_property("generated_class")
            except Exception:
                generated_class = None

        result = resolved_asset.to_identity()
        result.update(
            {
                "success": True,
                "asset_path": asset_path,
                "object_path": utility_asset.get_path_name(),
                "generated_class": generated_class.get_path_name() if generated_class else "",
                "implementation": "local_python",
            }
        )
        return result

    @staticmethod
    def _get_editor_utility_subsystem() -> unreal.EditorUtilitySubsystem:
        editor_utility_subsystem = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
        if not editor_utility_subsystem:
            raise EditorCommandError("EditorUtilitySubsystem 不可用")
        return editor_utility_subsystem

    @staticmethod
    def _resolve_asset(asset_reference: str) -> ResolvedAsset:
        asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_reference)
        asset_object = unreal.EditorAssetLibrary.load_asset(asset_reference)
        if not asset_data.is_valid() or not asset_object:
            raise EditorCommandError(f"加载资源失败: {asset_reference}")
        return ResolvedAsset(asset_data=asset_data, asset_object=asset_object)

    @staticmethod
    def _require_string(params: Dict[str, Any], field_name: str) -> str:
        value = str(params.get(field_name, "")).strip()
        if not value:
            raise EditorCommandError(f"缺少 '{field_name}' 参数")
        return value


class LightCommandHelper:
    """Create and update light actors through local editor Python APIs."""

    LIGHT_CLASS_MAP: Dict[str, Any] = {
        "point": unreal.PointLight,
        "pointlight": unreal.PointLight,
        "spot": unreal.SpotLight,
        "spotlight": unreal.SpotLight,
        "directional": unreal.DirectionalLight,
        "directionallight": unreal.DirectionalLight,
    }

    MOBILITY_MAP: Dict[str, Any] = {
        "static": unreal.ComponentMobility.STATIC,
        "stationary": unreal.ComponentMobility.STATIONARY,
        "movable": unreal.ComponentMobility.MOVABLE,
    }

    def __init__(self, serializer: EditorSelectionSerializer) -> None:
        self._serializer = serializer

    def create_light(self, params: Dict[str, Any]) -> Dict[str, Any]:
        actor_name = self._require_string(params, "actor_name")
        light_type = self._normalize_light_type(self._require_string(params, "light_type"))
        resolved_world = self._resolve_world(str(params.get("world_type", "editor")))
        if resolved_world.resolved_world_type != "editor":
            raise EditorCommandError("create_light 当前只支持 editor 世界")

        editor_actor_subsystem = self._get_editor_actor_subsystem()
        location = self._get_vector_param(params, "location", [0.0, 0.0, 0.0])
        rotation = self._get_rotator_param(params, "rotation", [0.0, 0.0, 0.0])
        actor = editor_actor_subsystem.spawn_actor_from_class(
            self.LIGHT_CLASS_MAP[light_type],
            location=location,
            rotation=rotation,
        )
        if not actor:
            raise EditorCommandError(f"创建灯光 Actor 失败: {actor_name}")

        actor.set_actor_label(actor_name)

        light_component = self._require_light_component(actor)
        applied_properties = self._apply_light_properties(light_component, params)

        result = self._serialize_light_actor(actor, light_component, resolved_world)
        result["success"] = True
        result["created"] = True
        result["requested_actor_name"] = actor_name
        result["light_type"] = light_type
        result["applied_properties"] = applied_properties
        return result

    def set_light_properties(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_actor = self._resolve_actor(params)
        light_component = self._require_light_component(resolved_actor.actor)
        applied_properties = self._apply_light_properties(light_component, params)
        if not applied_properties:
            raise EditorCommandError("至少需要提供一个可写灯光属性")

        result = self._serialize_light_actor(
            resolved_actor.actor,
            light_component,
            ResolvedWorld(resolved_actor.world, resolved_actor.resolved_world_type),
        )
        result["success"] = True
        result["created"] = False
        result["applied_properties"] = applied_properties
        return result

    def _resolve_world(self, requested_world_type: str) -> ResolvedWorld:
        normalized_world_type = (requested_world_type or "auto").strip().casefold() or "auto"
        editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        if not editor_subsystem:
            raise EditorCommandError("UnrealEditorSubsystem 不可用")

        editor_world = editor_subsystem.get_editor_world() if hasattr(editor_subsystem, "get_editor_world") else None
        game_world = editor_subsystem.get_game_world() if hasattr(editor_subsystem, "get_game_world") else None

        if normalized_world_type == "editor":
            if not editor_world:
                raise EditorCommandError("当前没有可用的 editor 世界")
            return ResolvedWorld(world=editor_world, resolved_world_type="editor")
        if normalized_world_type == "pie":
            if not game_world:
                raise EditorCommandError("当前没有运行中的 PIE/Game 世界")
            return ResolvedWorld(world=game_world, resolved_world_type="pie")
        if normalized_world_type == "auto":
            if game_world:
                return ResolvedWorld(world=game_world, resolved_world_type="pie")
            if editor_world:
                return ResolvedWorld(world=editor_world, resolved_world_type="editor")
            raise EditorCommandError("当前没有可用的 world")
        raise EditorCommandError("world_type 仅支持 auto、editor、pie")

    def _resolve_actor(self, params: Dict[str, Any]) -> ResolvedActor:
        actor_name = str(params.get("name", "")).strip()
        actor_path = str(params.get("actor_path", "")).strip()
        if not actor_name and not actor_path:
            raise EditorCommandError("缺少 'name' 或 'actor_path' 参数")

        resolved_world = self._resolve_world(str(params.get("world_type", "auto")))
        actors = list(unreal.GameplayStatics.get_all_actors_of_class(resolved_world.world, unreal.Actor) or [])

        target_actor: Optional[unreal.Actor] = None
        for actor in actors:
            if not actor:
                continue
            if actor_path and actor.get_path_name().casefold() == actor_path.casefold():
                target_actor = actor
                break
            if actor_name and actor.get_name().casefold() == actor_name.casefold():
                target_actor = actor
                break
            if actor_name and hasattr(actor, "get_actor_label"):
                if actor.get_actor_label().casefold() == actor_name.casefold():
                    target_actor = actor
                    break

        if not target_actor:
            target_reference = actor_path or actor_name
            raise EditorCommandError(f"未找到目标 Actor: {target_reference}")

        return ResolvedActor(
            actor=target_actor,
            world=resolved_world.world,
            resolved_world_type=resolved_world.resolved_world_type,
        )

    def _serialize_light_actor(
        self,
        actor: unreal.Actor,
        light_component: unreal.LightComponent,
        resolved_world: ResolvedWorld | unreal.World,
    ) -> Dict[str, Any]:
        actor_payload = self._serializer.serialize_actor(actor, include_components=True, detailed_components=True)
        actor_payload["light_component_name"] = light_component.get_name()
        actor_payload["light_component_path"] = light_component.get_path_name()
        actor_payload["light_component_class"] = light_component.get_class().get_name()
        actor_payload["light"] = self._serializer.serialize_component(light_component, detailed=True)
        world = resolved_world.world if isinstance(resolved_world, ResolvedWorld) else resolved_world
        resolved_world_type = (
            resolved_world.resolved_world_type if isinstance(resolved_world, ResolvedWorld) else "editor"
        )
        actor_payload["resolved_world_type"] = resolved_world_type
        actor_payload["world_name"] = world.get_name()
        actor_payload["world_path"] = world.get_path_name()
        return actor_payload

    def _apply_light_properties(self, light_component: unreal.LightComponent, params: Dict[str, Any]) -> Dict[str, Any]:
        applied: Dict[str, Any] = {}

        if "intensity" in params:
            value = float(params["intensity"])
            light_component.set_intensity(value)
            applied["intensity"] = value

        if "light_color" in params:
            color = self._get_linear_color_param(params, "light_color")
            light_component.set_light_color(color, True)
            applied["light_color"] = self._serializer._linear_color_to_list(color)

        for field_name in ("indirect_lighting_intensity", "temperature", "volumetric_scattering_intensity"):
            if field_name in params:
                value = float(params[field_name])
                light_component.set_editor_property(field_name, value)
                applied[field_name] = value

        for field_name in ("cast_shadows", "affects_world", "use_temperature", "cast_volumetric_shadow"):
            if field_name in params:
                value = bool(params[field_name])
                light_component.set_editor_property(field_name, value)
                applied[field_name] = value

        if "mobility" in params:
            mobility_text = str(params["mobility"]).strip().casefold()
            if mobility_text not in self.MOBILITY_MAP:
                raise EditorCommandError("mobility 仅支持 Static、Stationary、Movable")
            light_component.set_editor_property("mobility", self.MOBILITY_MAP[mobility_text])
            applied["mobility"] = mobility_text

        if isinstance(light_component, unreal.LocalLightComponent):
            for field_name in ("attenuation_radius", "source_radius", "source_length"):
                if field_name in params:
                    value = float(params[field_name])
                    light_component.set_editor_property(field_name, value)
                    applied[field_name] = value

        if isinstance(light_component, unreal.SpotLightComponent):
            for field_name in ("inner_cone_angle", "outer_cone_angle"):
                if field_name in params:
                    value = float(params[field_name])
                    light_component.set_editor_property(field_name, value)
                    applied[field_name] = value

        light_component.modify()
        owner = light_component.get_owner()
        if owner:
            owner.modify()
            if hasattr(owner, "post_edit_change"):
                owner.post_edit_change()

        return applied

    @staticmethod
    def _normalize_light_type(light_type: str) -> str:
        normalized = (light_type or "").strip().casefold()
        if normalized not in LightCommandHelper.LIGHT_CLASS_MAP:
            raise EditorCommandError("light_type 仅支持 PointLight、SpotLight、DirectionalLight")
        return normalized

    @staticmethod
    def _require_light_component(actor: unreal.Actor) -> unreal.LightComponent:
        light_component = actor.get_component_by_class(unreal.LightComponent)
        if not light_component:
            raise EditorCommandError(f"Actor 不是灯光 Actor: {actor.get_name()}")
        return light_component

    @staticmethod
    def _get_editor_actor_subsystem() -> unreal.EditorActorSubsystem:
        actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if not actor_subsystem:
            raise EditorCommandError("EditorActorSubsystem 不可用")
        return actor_subsystem

    @staticmethod
    def _get_vector_param(params: Dict[str, Any], field_name: str, default_value: List[float]) -> unreal.Vector:
        values = params.get(field_name, default_value)
        if not isinstance(values, list) or len(values) != 3:
            raise EditorCommandError(f"'{field_name}' 必须是 3 个数字 [X, Y, Z]")
        return unreal.Vector(float(values[0]), float(values[1]), float(values[2]))

    @staticmethod
    def _get_rotator_param(params: Dict[str, Any], field_name: str, default_value: List[float]) -> unreal.Rotator:
        values = params.get(field_name, default_value)
        if not isinstance(values, list) or len(values) != 3:
            raise EditorCommandError(f"'{field_name}' 必须是 3 个数字 [Pitch, Yaw, Roll]")
        return unreal.Rotator(pitch=float(values[0]), yaw=float(values[1]), roll=float(values[2]))

    @staticmethod
    def _get_linear_color_param(params: Dict[str, Any], field_name: str) -> unreal.LinearColor:
        values = params.get(field_name)
        if not isinstance(values, list) or len(values) not in {3, 4}:
            raise EditorCommandError(f"'{field_name}' 必须是 3 或 4 个数字 [R, G, B, A]")
        alpha = float(values[3]) if len(values) == 4 else 1.0
        return unreal.LinearColor(float(values[0]), float(values[1]), float(values[2]), alpha)

    @staticmethod
    def _require_string(params: Dict[str, Any], field_name: str) -> str:
        value = str(params.get(field_name, "")).strip()
        if not value:
            raise EditorCommandError(f"缺少 '{field_name}' 参数")
        return value


class RenderCommandHelper:
    """Handle scene capture and post-process volume commands through local Python APIs."""

    CAPTURE_SOURCE_MAP: Dict[str, Any] = {
        "scs_base_color": unreal.SceneCaptureSource.SCS_BASE_COLOR,
        "scs_device_depth": unreal.SceneCaptureSource.SCS_DEVICE_DEPTH,
        "scs_final_color_hdr": unreal.SceneCaptureSource.SCS_FINAL_COLOR_HDR,
        "scs_final_color_ldr": unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,
        "scs_final_tone_curve_hdr": unreal.SceneCaptureSource.SCS_FINAL_TONE_CURVE_HDR,
        "scs_normal": unreal.SceneCaptureSource.SCS_NORMAL,
        "scs_scene_color_hdr": unreal.SceneCaptureSource.SCS_SCENE_COLOR_HDR,
        "scs_scene_color_hdr_no_alpha": unreal.SceneCaptureSource.SCS_SCENE_COLOR_HDR_NO_ALPHA,
        "scs_scene_color_scene_depth": unreal.SceneCaptureSource.SCS_SCENE_COLOR_SCENE_DEPTH,
        "scs_scene_depth": unreal.SceneCaptureSource.SCS_SCENE_DEPTH,
    }

    def __init__(self, serializer: EditorSelectionSerializer) -> None:
        self._serializer = serializer

    def capture_scene_to_render_target(self, params: Dict[str, Any]) -> Dict[str, Any]:
        render_target_reference = self._require_string(params, "render_target_asset_path")
        render_target = unreal.EditorAssetLibrary.load_asset(render_target_reference)
        if not render_target:
            raise EditorCommandError(f"加载 RenderTarget 失败: {render_target_reference}")
        if not isinstance(render_target, unreal.TextureRenderTarget2D):
            raise EditorCommandError(f"目标资源不是 TextureRenderTarget2D: {render_target_reference}")

        resolved_world = self._resolve_world(str(params.get("world_type", "editor")))
        requested_world_type = str(params.get("world_type", "editor")).strip().casefold() or "editor"
        if requested_world_type == "pie":
            raise EditorCommandError("capture_scene_to_render_target 当前只支持 editor 世界")

        actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if not actor_subsystem:
            raise EditorCommandError("EditorActorSubsystem 不可用")

        capture_actor, used_temporary_actor = self._resolve_or_spawn_capture_actor(
            params=params,
            resolved_world=resolved_world,
            actor_subsystem=actor_subsystem,
        )

        try:
            capture_component = self._require_capture_component(capture_actor)
            capture_component.modify()
            capture_component.set_editor_property("texture_target", render_target)

            applied_capture_source = ""
            capture_source = str(params.get("capture_source", "")).strip()
            if capture_source:
                capture_source_enum = self._resolve_capture_source(capture_source)
                capture_component.set_editor_property("capture_source", capture_source_enum)
                applied_capture_source = self._capture_source_to_string(capture_source_enum)

            if "fov_angle" in params:
                fov_angle = float(params["fov_angle"])
                capture_component.set_editor_property("fov_angle", fov_angle)

            if "post_process_blend_weight" in params:
                blend_weight = float(params["post_process_blend_weight"])
                capture_component.set_editor_property("post_process_blend_weight", blend_weight)

            if "capture_every_frame" in params:
                capture_component.set_editor_property("capture_every_frame", bool(params["capture_every_frame"]))

            if "capture_on_movement" in params:
                capture_component.set_editor_property("capture_on_movement", bool(params["capture_on_movement"]))

            if "primitive_render_mode" in params:
                primitive_render_mode = self._resolve_enum_by_name(
                    enum_type=unreal.SceneCapturePrimitiveRenderMode,
                    raw_value=params["primitive_render_mode"],
                    field_name="primitive_render_mode",
                )
                capture_component.set_editor_property("primitive_render_mode", primitive_render_mode)

            capture_component.capture_scene()
            render_target.modify()
            unreal.EditorAssetLibrary.save_loaded_asset(render_target)

            result = {
                "success": True,
                "render_target_asset_path": render_target.get_path_name(),
                "render_target_name": render_target.get_name(),
                "render_target_size": [
                    int(render_target.get_editor_property("size_x")),
                    int(render_target.get_editor_property("size_y")),
                ],
                "capture_actor_name": capture_actor.get_name(),
                "capture_actor_path": capture_actor.get_path_name(),
                "capture_component_name": capture_component.get_name(),
                "capture_component_path": capture_component.get_path_name(),
                "used_temporary_actor": used_temporary_actor,
                "resolved_world_type": resolved_world.resolved_world_type,
                "world_name": resolved_world.world.get_name(),
                "world_path": resolved_world.world.get_path_name(),
                "capture_source": applied_capture_source
                or self._capture_source_to_string(capture_component.get_editor_property("capture_source")),
                "fov_angle": float(capture_component.get_editor_property("fov_angle")),
                "post_process_blend_weight": float(
                    capture_component.get_editor_property("post_process_blend_weight")
                ),
            }
            return result
        finally:
            if used_temporary_actor and capture_actor:
                actor_subsystem.destroy_actor(capture_actor)

    def set_post_process_settings(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_actor = self._resolve_actor(
            params=params,
            actor_class=unreal.PostProcessVolume,
            actor_class_name="PostProcessVolume",
        )
        volume = resolved_actor.actor

        applied_properties: Dict[str, Any] = {}
        for field_name in ("enabled", "unbound"):
            if field_name in params:
                value = bool(params[field_name])
                volume.set_editor_property(field_name, value)
                applied_properties[field_name] = value

        for field_name in ("blend_radius", "blend_weight", "priority"):
            if field_name in params:
                value = float(params[field_name])
                volume.set_editor_property(field_name, value)
                applied_properties[field_name] = value

        settings_param = params.get("settings", {})
        if settings_param is None:
            settings_param = {}
        if not isinstance(settings_param, dict):
            raise EditorCommandError("'settings' 必须是对象")

        settings = volume.get_editor_property("settings")
        applied_settings: Dict[str, Any] = {}
        for property_name, raw_value in settings_param.items():
            normalized_property_name = str(property_name).strip()
            if not normalized_property_name:
                continue
            converted_value = self._convert_post_process_value(settings, normalized_property_name, raw_value)
            settings.set_editor_property(normalized_property_name, converted_value)
            override_property_name = self._resolve_post_process_override_property(settings, normalized_property_name)
            if override_property_name:
                settings.set_editor_property(override_property_name, True)
            applied_settings[normalized_property_name] = self._serialize_value(
                settings.get_editor_property(normalized_property_name)
            )

        if not applied_properties and not applied_settings:
            raise EditorCommandError("至少需要提供一个 Volume 参数或 settings 字段")

        volume.set_editor_property("settings", settings)
        volume.modify()
        if hasattr(volume, "post_edit_change"):
            volume.post_edit_change()

        result = self._serializer.serialize_actor(volume, include_components=True, detailed_components=True)
        result.update(
            {
                "success": True,
                "resolved_world_type": resolved_actor.resolved_world_type,
                "world_name": resolved_actor.world.get_name(),
                "world_path": resolved_actor.world.get_path_name(),
                "applied_properties": applied_properties,
                "applied_settings": applied_settings,
                "settings_count": len(applied_settings),
                "blend_radius": float(volume.get_editor_property("blend_radius")),
                "blend_weight": float(volume.get_editor_property("blend_weight")),
                "priority": float(volume.get_editor_property("priority")),
                "enabled": bool(volume.get_editor_property("enabled")),
                "unbound": bool(volume.get_editor_property("unbound")),
            }
        )
        return result

    def _resolve_or_spawn_capture_actor(
        self,
        params: Dict[str, Any],
        resolved_world: ResolvedWorld,
        actor_subsystem: unreal.EditorActorSubsystem,
    ) -> tuple[unreal.Actor, bool]:
        actor_name = str(params.get("name", "")).strip()
        actor_path = str(params.get("actor_path", "")).strip()
        if actor_name or actor_path:
            resolved_actor = self._resolve_actor(
                params=params,
                actor_class=unreal.SceneCapture2D,
                actor_class_name="SceneCapture2D",
            )
            return resolved_actor.actor, False

        location, rotation = self._resolve_capture_transform(params)
        actor = actor_subsystem.spawn_actor_from_class(
            unreal.SceneCapture2D,
            location=location,
            rotation=rotation,
        )
        if not actor:
            raise EditorCommandError("创建临时 SceneCapture2D 失败")
        actor.set_actor_label("MCP_TempSceneCapture")
        return actor, True

    def _resolve_capture_transform(self, params: Dict[str, Any]) -> tuple[unreal.Vector, unreal.Rotator]:
        if "location" in params or "rotation" in params:
            location = self._get_vector_param(params, "location", [0.0, 0.0, 0.0])
            rotation = self._get_rotator_param(params, "rotation", [0.0, 0.0, 0.0])
            return location, rotation

        editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        if not editor_subsystem or not hasattr(editor_subsystem, "get_level_viewport_camera_info"):
            raise EditorCommandError("无法读取当前视口相机位置，请显式提供 location/rotation")
        camera_info = editor_subsystem.get_level_viewport_camera_info()
        if not isinstance(camera_info, tuple) or len(camera_info) != 2:
            raise EditorCommandError("读取视口相机信息失败，请显式提供 location/rotation")
        return camera_info[0], camera_info[1]

    def _resolve_actor(
        self,
        params: Dict[str, Any],
        actor_class: Any,
        actor_class_name: str,
    ) -> ResolvedActor:
        actor_name = str(params.get("name", "")).strip()
        actor_path = str(params.get("actor_path", "")).strip()
        if not actor_name and not actor_path:
            raise EditorCommandError("缺少 'name' 或 'actor_path' 参数")

        resolved_world = self._resolve_world(str(params.get("world_type", "auto")))
        actors = list(unreal.GameplayStatics.get_all_actors_of_class(resolved_world.world, actor_class) or [])

        target_actor: Optional[unreal.Actor] = None
        for actor in actors:
            if not actor:
                continue
            if actor_path and actor.get_path_name().casefold() == actor_path.casefold():
                target_actor = actor
                break
            if actor_name and actor.get_name().casefold() == actor_name.casefold():
                target_actor = actor
                break
            if actor_name and hasattr(actor, "get_actor_label"):
                if actor.get_actor_label().casefold() == actor_name.casefold():
                    target_actor = actor
                    break

        if not target_actor:
            raise EditorCommandError(f"未找到目标 {actor_class_name}: {actor_path or actor_name}")

        return ResolvedActor(
            actor=target_actor,
            world=resolved_world.world,
            resolved_world_type=resolved_world.resolved_world_type,
        )

    def _resolve_world(self, requested_world_type: str) -> ResolvedWorld:
        normalized_world_type = (requested_world_type or "auto").strip().casefold() or "auto"
        editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        if not editor_subsystem:
            raise EditorCommandError("UnrealEditorSubsystem 不可用")

        editor_world = editor_subsystem.get_editor_world() if hasattr(editor_subsystem, "get_editor_world") else None
        game_world = editor_subsystem.get_game_world() if hasattr(editor_subsystem, "get_game_world") else None

        if normalized_world_type == "editor":
            if not editor_world:
                raise EditorCommandError("当前没有可用的 editor 世界")
            return ResolvedWorld(world=editor_world, resolved_world_type="editor")
        if normalized_world_type == "pie":
            if not game_world:
                raise EditorCommandError("当前没有运行中的 PIE/Game 世界")
            return ResolvedWorld(world=game_world, resolved_world_type="pie")
        if normalized_world_type == "auto":
            if game_world:
                return ResolvedWorld(world=game_world, resolved_world_type="pie")
            if editor_world:
                return ResolvedWorld(world=editor_world, resolved_world_type="editor")
            raise EditorCommandError("当前没有可用的 world")
        raise EditorCommandError("world_type 仅支持 auto、editor、pie")

    def _resolve_capture_source(self, capture_source: str) -> Any:
        normalized_capture_source = capture_source.strip().casefold()
        if not normalized_capture_source:
            raise EditorCommandError("capture_source 不能为空")
        if normalized_capture_source in self.CAPTURE_SOURCE_MAP:
            return self.CAPTURE_SOURCE_MAP[normalized_capture_source]
        raise EditorCommandError(f"不支持的 capture_source: {capture_source}")

    @staticmethod
    def _capture_source_to_string(capture_source: Any) -> str:
        if hasattr(capture_source, "name"):
            return str(capture_source.name)
        capture_source_text = str(capture_source)
        if "." in capture_source_text:
            capture_source_text = capture_source_text.split(".")[-1]
        return capture_source_text

    @staticmethod
    def _require_capture_component(actor: unreal.Actor) -> unreal.SceneCaptureComponent2D:
        capture_component = actor.get_component_by_class(unreal.SceneCaptureComponent2D)
        if not capture_component:
            raise EditorCommandError(f"Actor 不是 SceneCapture2D: {actor.get_name()}")
        return capture_component

    @staticmethod
    def _resolve_post_process_override_property(settings: unreal.PostProcessSettings, property_name: str) -> str:
        override_property_name = f"override_{property_name}"
        try:
            settings.get_editor_property(override_property_name)
            return override_property_name
        except Exception:
            return ""

    def _convert_post_process_value(
        self,
        settings: unreal.PostProcessSettings,
        property_name: str,
        raw_value: Any,
    ) -> Any:
        try:
            current_value = settings.get_editor_property(property_name)
        except Exception as exc:
            raise EditorCommandError(f"PostProcessSettings 不支持属性 '{property_name}'") from exc

        if isinstance(current_value, bool):
            return bool(raw_value)
        if isinstance(current_value, int) and not isinstance(current_value, bool):
            return int(raw_value)
        if isinstance(current_value, float):
            return float(raw_value)
        if isinstance(current_value, str):
            return str(raw_value)
        if isinstance(current_value, unreal.LinearColor):
            if not isinstance(raw_value, list) or len(raw_value) not in {3, 4}:
                raise EditorCommandError(
                    f"PostProcessSettings.{property_name} 需要 [R,G,B] 或 [R,G,B,A]"
                )
            alpha = float(raw_value[3]) if len(raw_value) == 4 else 1.0
            return unreal.LinearColor(
                float(raw_value[0]),
                float(raw_value[1]),
                float(raw_value[2]),
                alpha,
            )
        if isinstance(current_value, unreal.Vector):
            if not isinstance(raw_value, list) or len(raw_value) != 3:
                raise EditorCommandError(f"PostProcessSettings.{property_name} 需要 [X,Y,Z]")
            return unreal.Vector(float(raw_value[0]), float(raw_value[1]), float(raw_value[2]))
        if isinstance(raw_value, str):
            return self._resolve_enum_by_name(type(current_value), raw_value, property_name)
        return raw_value

    @staticmethod
    def _resolve_enum_by_name(enum_type: Any, raw_value: Any, field_name: str) -> Any:
        normalized_value = str(raw_value).strip()
        if not normalized_value:
            raise EditorCommandError(f"'{field_name}' 不能为空")

        direct_member = getattr(enum_type, normalized_value, None)
        if direct_member is not None:
            return direct_member

        normalized_casefold = normalized_value.casefold()
        for attribute_name in dir(enum_type):
            if attribute_name.casefold() == normalized_casefold:
                return getattr(enum_type, attribute_name)

        raise EditorCommandError(f"'{field_name}' 不支持值: {raw_value}")

    def _serialize_value(self, value: Any) -> Any:
        if isinstance(value, unreal.LinearColor):
            return self._serializer._linear_color_to_list(value)
        if isinstance(value, unreal.Vector):
            return self._serializer._vector_to_list(value)
        if isinstance(value, unreal.Rotator):
            return self._serializer._rotator_to_list(value)
        if hasattr(value, "name"):
            return str(value.name)
        if isinstance(value, (bool, int, float, str)):
            return value
        return str(value)

    @staticmethod
    def _get_vector_param(params: Dict[str, Any], field_name: str, default_value: List[float]) -> unreal.Vector:
        values = params.get(field_name, default_value)
        if not isinstance(values, list) or len(values) != 3:
            raise EditorCommandError(f"'{field_name}' 必须是 3 个数字 [X, Y, Z]")
        return unreal.Vector(float(values[0]), float(values[1]), float(values[2]))

    @staticmethod
    def _get_rotator_param(params: Dict[str, Any], field_name: str, default_value: List[float]) -> unreal.Rotator:
        values = params.get(field_name, default_value)
        if not isinstance(values, list) or len(values) != 3:
            raise EditorCommandError(f"'{field_name}' 必须是 3 个数字 [Pitch, Yaw, Roll]")
        return unreal.Rotator(pitch=float(values[0]), yaw=float(values[1]), roll=float(values[2]))

    @staticmethod
    def _require_string(params: Dict[str, Any], field_name: str) -> str:
        value = str(params.get(field_name, "")).strip()
        if not value:
            raise EditorCommandError(f"缺少 '{field_name}' 参数")
        return value


def handle_editor_command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
    """Module entry point used by the C++ local Python bridge."""
    dispatcher = EditorCommandDispatcher()
    try:
        return dispatcher.handle(command_name, params)
    except EditorCommandError as exc:
        return {
            "success": False,
            "error": str(exc),
        }
