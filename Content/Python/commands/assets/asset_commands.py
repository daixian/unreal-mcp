"""Local Python implementations for asset and Content Browser commands."""

from __future__ import annotations

from dataclasses import dataclass
import fnmatch
import json
import os
import re
from typing import Any, Dict, List, Optional, Tuple

import unreal


class AssetCommandError(RuntimeError):
    """Raised when an asset command cannot be completed."""


@dataclass
class ResolvedAsset:
    """Structured asset resolution result."""

    asset_data: unreal.AssetData

    def to_identity(self) -> Dict[str, Any]:
        """Build the standard asset identity payload."""
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
class ResolvedFactory:
    """Structured asset factory resolution result."""

    asset_class: Any
    asset_class_reference: str
    factory: unreal.Factory
    factory_class_reference: str
    used_default_factory: bool


@dataclass
class BatchAssetOperation:
    """Structured batch asset rename or move request."""

    source_asset_path: str
    destination_asset_path: str
    source_identity: Dict[str, Any]


@dataclass
class ReimportRequest:
    """Structured asset reimport request."""

    resolved_assets: List[ResolvedAsset]
    ask_for_new_file_if_missing: bool
    show_notification: bool
    force_new_file: bool
    automated: bool
    force_show_dialog: bool
    source_file_index: int
    preferred_reimport_file: str


class AssetResolver:
    """Resolve asset references from MCP params."""

    LOOKUP_FIELDS: List[str] = ["asset_path", "object_path", "path", "asset_name", "name"]

    def resolve_from_params(self, params: Dict[str, Any]) -> ResolvedAsset:
        """Resolve the first available asset reference from params."""
        for field_name in self.LOOKUP_FIELDS:
            reference = str(params.get(field_name, "")).strip()
            if reference:
                return self.resolve_reference(reference)
        raise AssetCommandError("缺少资源定位参数")

    def resolve_reference(self, reference: str) -> ResolvedAsset:
        """Resolve an asset reference by path, object path, or exact name."""
        normalized_reference = reference.strip()
        if not normalized_reference:
            raise AssetCommandError("资源引用不能为空")

        direct_candidates = [normalized_reference]
        if normalized_reference.startswith("/") and "." not in normalized_reference:
            asset_name = normalized_reference.rsplit("/", 1)[-1]
            if asset_name:
                direct_candidates.append(f"{normalized_reference}.{asset_name}")

        for candidate in direct_candidates:
            asset_data = unreal.EditorAssetLibrary.find_asset_data(candidate)
            if asset_data.is_valid():
                return ResolvedAsset(asset_data=asset_data)

        matches: List[ResolvedAsset] = []
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        for asset_data in registry.get_all_assets():
            package_name = str(asset_data.package_name)
            asset_name = str(asset_data.asset_name)
            object_path = f"{package_name}.{asset_name}"
            if (
                asset_name.lower() == normalized_reference.lower()
                or package_name.lower() == normalized_reference.lower()
                or object_path.lower() == normalized_reference.lower()
            ):
                matches.append(ResolvedAsset(asset_data=asset_data))

        if len(matches) == 1:
            return matches[0]
        if len(matches) > 1:
            sample_paths = ", ".join(match.to_identity()["object_path"] for match in matches[:5])
            raise AssetCommandError(f"资源引用匹配到多个结果: {sample_paths}")
        raise AssetCommandError(f"未找到资源: {normalized_reference}")


class AssetCreationResolver:
    """Resolve asset classes and factories for generic asset creation."""

    DEFAULT_FACTORIES: Dict[str, str] = {
        "blackboarddata": "BlackboardDataFactory",
        "blueprint": "BlueprintFactory",
        "curvefloat": "CurveFloatFactory",
        "curvelinearcolor": "CurveLinearColorFactory",
        "curvetable": "CurveTableFactory",
        "curvevector": "CurveVectorFactory",
        "datatable": "DataTableFactory",
        "levelsequence": "LevelSequenceFactoryNew",
        "material": "MaterialFactoryNew",
        "materialfunction": "MaterialFunctionFactoryNew",
        "materialfunctionmateriallayer": "MaterialFunctionMaterialLayerFactory",
        "materialfunctionmateriallayerblend": "MaterialFunctionMaterialLayerBlendFactory",
        "materialinstanceconstant": "MaterialInstanceConstantFactoryNew",
        "materialparametercollection": "MaterialParameterCollectionFactoryNew",
        "physicalmaterial": "PhysicalMaterialFactoryNew",
        "soundcue": "SoundCueFactoryNew",
        "texture2d": "Texture2DFactoryNew",
        "texturerendertarget2d": "TextureRenderTargetFactoryNew",
        "texturetarget2d": "TextureRenderTargetFactoryNew",
        "widgetblueprint": "WidgetBlueprintFactory",
        "world": "WorldFactory",
    }

    def resolve_creation_request(self, params: Dict[str, Any]) -> ResolvedFactory:
        """Resolve the asset class plus a compatible factory instance."""
        asset_class_reference = str(params.get("asset_class", "") or params.get("class_name", "")).strip()
        if not asset_class_reference:
            raise AssetCommandError("缺少 'asset_class' 参数")

        asset_class = self._resolve_class_reference(asset_class_reference, "asset_class")
        factory_class_reference = str(params.get("factory_class", "")).strip()
        used_default_factory = not factory_class_reference
        if not factory_class_reference:
            factory_class_reference = self._default_factory_for_asset_class(asset_class_reference)
            if not factory_class_reference:
                raise AssetCommandError(
                    "缺少 'factory_class' 参数，且当前 asset_class 没有内置默认工厂"
                )

        factory = self._instantiate_factory(factory_class_reference)
        return ResolvedFactory(
            asset_class=asset_class,
            asset_class_reference=self._normalize_class_reference(asset_class_reference, asset_class),
            factory=factory,
            factory_class_reference=self._normalize_factory_reference(factory_class_reference, factory),
            used_default_factory=used_default_factory,
        )

    def apply_factory_options(self, resolved_factory: ResolvedFactory, params: Dict[str, Any]) -> Dict[str, Any]:
        """Apply supported creation options onto the factory instance."""
        applied_options: Dict[str, Any] = {}

        parent_class_reference = str(params.get("parent_class", "")).strip()
        if parent_class_reference:
            parent_class = self._resolve_class_reference(parent_class_reference, "parent_class")
            self._set_factory_property_if_present(resolved_factory.factory, "parent_class", parent_class)
            applied_options["parent_class"] = self._normalize_class_reference(parent_class_reference, parent_class)

        if self._factory_has_property(resolved_factory.factory, "data_asset_class"):
            self._set_factory_property_if_present(resolved_factory.factory, "data_asset_class", resolved_factory.asset_class)
            applied_options["data_asset_class"] = resolved_factory.asset_class_reference

        return applied_options

    def _resolve_class_reference(self, class_reference: str, field_name: str) -> Any:
        """Resolve either a short Unreal Python class name or a full class path."""
        normalized_reference = class_reference.strip()
        if not normalized_reference:
            raise AssetCommandError(f"缺少 '{field_name}' 参数")

        if normalized_reference.startswith("/"):
            resolved_class = unreal.load_class(None, normalized_reference)
            if not resolved_class:
                raise AssetCommandError(f"无法解析 {field_name}: {normalized_reference}")
            return resolved_class

        resolved_class = getattr(unreal, normalized_reference, None)
        if resolved_class is not None:
            return resolved_class

        lowered_reference = normalized_reference.casefold()
        for attribute_name in dir(unreal):
            if attribute_name.casefold() == lowered_reference:
                resolved_class = getattr(unreal, attribute_name, None)
                if resolved_class is not None:
                    return resolved_class

        raise AssetCommandError(f"无法解析 {field_name}: {normalized_reference}")

    def _instantiate_factory(self, factory_reference: str) -> unreal.Factory:
        """Instantiate a factory from a short class name or a full class path."""
        factory_class = self._resolve_class_reference(factory_reference, "factory_class")
        try:
            factory = factory_class()
        except TypeError:
            factory = unreal.new_object(factory_class)

        if not isinstance(factory, unreal.Factory):
            raise AssetCommandError(f"factory_class 不是有效的 UE Factory: {factory_reference}")
        return factory

    def _default_factory_for_asset_class(self, asset_class_reference: str) -> str:
        """Return the built-in default factory reference for a known asset class."""
        normalized_key = asset_class_reference.rsplit(".", 1)[-1].rsplit("/", 1)[-1].casefold()
        return self.DEFAULT_FACTORIES.get(normalized_key, "")

    @staticmethod
    def _factory_has_property(factory: unreal.Factory, property_name: str) -> bool:
        try:
            factory.get_editor_property(property_name)
            return True
        except Exception:
            return False

    def _set_factory_property_if_present(self, factory: unreal.Factory, property_name: str, value: Any) -> None:
        if not self._factory_has_property(factory, property_name):
            raise AssetCommandError(f"工厂不支持属性 '{property_name}'")
        factory.set_editor_property(property_name, value)

    @staticmethod
    def _normalize_class_reference(class_reference: str, class_object: Any) -> str:
        if class_reference.startswith("/"):
            return class_reference
        return getattr(class_object, "__name__", class_reference)

    @staticmethod
    def _normalize_factory_reference(factory_reference: str, factory: unreal.Factory) -> str:
        if factory_reference.startswith("/"):
            return factory_reference
        return factory.get_class().get_name()


class ReimportCommandExecutor:
    """Execute reimport requests through local Python when Interchange is sufficient."""

    def __init__(self, resolver: AssetResolver) -> None:
        self._resolver = resolver

    def build_request(self, params: Dict[str, Any]) -> ReimportRequest:
        """Normalize MCP params into a structured reimport request."""
        resolved_assets = self._resolve_assets(params)
        ask_for_new_file_if_missing = bool(params.get("ask_for_new_file_if_missing", False))
        show_notification = bool(params.get("show_notification", True))
        force_new_file = bool(params.get("force_new_file", False))
        automated = bool(params.get("automated", True))
        force_show_dialog = bool(params.get("force_show_dialog", False))
        source_file_index = int(params.get("source_file_index", -1))
        preferred_reimport_file = self._normalize_preferred_reimport_file(
            str(params.get("preferred_reimport_file", "")).strip()
        )
        return ReimportRequest(
            resolved_assets=resolved_assets,
            ask_for_new_file_if_missing=ask_for_new_file_if_missing,
            show_notification=show_notification,
            force_new_file=force_new_file,
            automated=automated,
            force_show_dialog=force_show_dialog,
            source_file_index=source_file_index,
            preferred_reimport_file=preferred_reimport_file,
        )

    def execute(self, request: ReimportRequest) -> Dict[str, Any]:
        """Run an async reimport or request C++ fallback when Python coverage is insufficient."""
        fallback_reason = self._get_fallback_reason(request)
        if fallback_reason:
            return self._create_cpp_fallback_result(fallback_reason)

        interchange_manager = unreal.InterchangeManager.get_interchange_manager_scripted()
        if not interchange_manager:
            return self._create_cpp_fallback_result("InterchangeManager 不可用")

        reimported_assets: List[Dict[str, Any]] = []
        any_success = False

        for resolved_asset in request.resolved_assets:
            asset_object = resolved_asset.asset_data.get_asset()
            if not asset_object:
                raise AssetCommandError(f"加载待重导入资源失败: {resolved_asset.to_identity()['object_path']}")

            reimport_source_files = interchange_manager.can_reimport(asset_object)
            if reimport_source_files is None:
                return self._create_cpp_fallback_result(
                    f"当前资源未暴露 Interchange 重导入能力: {resolved_asset.to_identity()['object_path']}"
                )

            source_files = [str(source_file) for source_file in list(reimport_source_files) if str(source_file)]
            import_data = None
            original_source_files: Optional[List[str]] = None
            if request.preferred_reimport_file:
                import_data = interchange_manager.get_asset_import_data(asset_object)
                if not import_data or not hasattr(import_data, "scripted_add_filename"):
                    return self._create_cpp_fallback_result(
                        f"当前资源的 AssetImportData 不支持覆写 source file: {resolved_asset.to_identity()['object_path']}"
                    )
                original_source_files = list(source_files)
                override_index = request.source_file_index if request.source_file_index >= 0 else 0
                try:
                    import_data.scripted_add_filename(request.preferred_reimport_file, override_index, "")
                except Exception:
                    return self._create_cpp_fallback_result(
                        f"覆写 preferred_reimport_file 失败: {resolved_asset.to_identity()['object_path']}"
                    )

            import_parameters = unreal.ImportAssetParameters()
            import_parameters.is_automated = request.automated
            import_parameters.force_show_dialog = request.force_show_dialog
            import_parameters.reimport_asset = asset_object
            import_parameters.reimport_source_index = request.source_file_index
            import_parameters.replace_existing = True

            try:
                reimport_started = bool(
                    interchange_manager.scripted_reimport_asset_async(asset_object, import_parameters)
                )
            except Exception as exc:
                self._restore_source_files(import_data, original_source_files)
                raise AssetCommandError(
                    f"启动重导入失败: {resolved_asset.to_identity()['object_path']}，原因: {exc}"
                ) from exc

            if not reimport_started:
                self._restore_source_files(import_data, original_source_files)
            else:
                any_success = True

            item = resolved_asset.to_identity()
            item.update(
                {
                    "can_reimport": True,
                    "reimport_started": reimport_started,
                    "reimport_completed": False,
                    "reimport_result": self._reimport_result_to_string(True, reimport_started, False),
                    "source_files": source_files,
                }
            )
            reimported_assets.append(item)

        if not any_success:
            raise AssetCommandError("重导入未成功处理任何资源")

        return {
            "success": True,
            "assets": reimported_assets,
            "asset_count": len(reimported_assets),
            "ask_for_new_file_if_missing": request.ask_for_new_file_if_missing,
            "show_notification": request.show_notification,
            "force_new_file": request.force_new_file,
            "automated": request.automated,
            "force_show_dialog": request.force_show_dialog,
            "source_file_index": request.source_file_index,
            "preferred_reimport_file": request.preferred_reimport_file,
        }

    def _resolve_assets(self, params: Dict[str, Any]) -> List[ResolvedAsset]:
        """Resolve one or many reimport targets while preserving the old lookup precedence."""
        resolved_assets: List[ResolvedAsset] = []
        seen_asset_paths: set[str] = set()

        single_asset_reference = str(params.get("asset_path", "")).strip()
        if single_asset_reference:
            single_asset = self._resolver.resolve_reference(single_asset_reference)
            single_asset_path = single_asset.to_identity()["asset_path"]
            resolved_assets.append(single_asset)
            seen_asset_paths.add(single_asset_path)

        asset_paths_param = params.get("asset_paths") or []
        if asset_paths_param and not isinstance(asset_paths_param, list):
            raise AssetCommandError("'asset_paths' 必须是数组")

        for asset_reference in asset_paths_param:
            normalized_reference = str(asset_reference).strip()
            if not normalized_reference:
                continue

            resolved_asset = self._resolver.resolve_reference(normalized_reference)
            asset_path = resolved_asset.to_identity()["asset_path"]
            if asset_path in seen_asset_paths:
                continue

            resolved_assets.append(resolved_asset)
            seen_asset_paths.add(asset_path)

        if resolved_assets:
            return resolved_assets

        fallback_asset = self._resolver.resolve_from_params(params)
        return [fallback_asset]

    @staticmethod
    def _get_fallback_reason(request: ReimportRequest) -> str:
        """Return the reason when the current request still needs the legacy C++ path."""
        if (
            not hasattr(unreal, "InterchangeManager")
            or not hasattr(unreal.InterchangeManager, "get_interchange_manager_scripted")
            or not hasattr(unreal, "ImportAssetParameters")
        ):
            return "当前编辑器未暴露 Interchange Python API"
        if request.ask_for_new_file_if_missing:
            return "ask_for_new_file_if_missing=true 仍需走 C++"
        if not request.show_notification:
            return "show_notification=false 仍需走 C++"
        if request.force_new_file:
            return "force_new_file=true 仍需走 C++"
        if request.preferred_reimport_file and not os.path.isfile(request.preferred_reimport_file):
            return "preferred_reimport_file 当前需要 C++ 处理缺失文件语义"
        return ""

    @staticmethod
    def _normalize_preferred_reimport_file(preferred_reimport_file: str) -> str:
        """Normalize an optional preferred reimport source path."""
        if not preferred_reimport_file:
            return ""
        return os.path.abspath(os.path.expanduser(preferred_reimport_file))

    @staticmethod
    def _restore_source_files(
        asset_import_data: Any,
        source_files: Optional[List[str]],
    ) -> None:
        """Restore import data only when Python changed it but the async reimport never started."""
        if not asset_import_data or not source_files:
            return
        for index, source_file in enumerate(source_files):
            asset_import_data.scripted_add_filename(source_file, index, "")

    @staticmethod
    def _reimport_result_to_string(can_reimport: bool, started: bool, completed: bool) -> str:
        if not can_reimport:
            return "not_supported"
        if not started:
            return "failed"
        return "completed" if completed else "pending"

    @staticmethod
    def _create_cpp_fallback_result(reason: str) -> Dict[str, Any]:
        return {
            "success": False,
            "fallback_to_cpp": True,
            "error": reason,
        }


class MaterialCommandExecutor:
    """Execute local Python implementations for material asset commands."""

    DEFAULT_PACKAGE_PATH = "/Game/Materials"
    DEFAULT_RENDER_TARGET_PATH = "/Game/RenderTargets"
    DEFAULT_RENDER_TARGET_WIDTH = 1024
    DEFAULT_RENDER_TARGET_HEIGHT = 1024
    DEFAULT_RENDER_TARGET_FORMAT = "RTF_RGBA16F"

    def __init__(self, resolver: AssetResolver) -> None:
        self._resolver = resolver

    def create_material(self, params: Dict[str, Any]) -> Dict[str, Any]:
        material_name = str(params.get("name", "") or params.get("material_name", "")).strip()
        if not material_name:
            raise AssetCommandError("缺少 'name' 参数")

        package_path = self._normalize_package_path(str(params.get("path", "")).strip(), self.DEFAULT_PACKAGE_PATH)
        created_material = self._create_material_asset(
            asset_name=material_name,
            package_path=package_path,
            asset_class=unreal.Material,
            factory=unreal.MaterialFactoryNew(),
            duplicate_error_prefix="材质已存在",
            create_error_message="创建材质失败",
        )

        result = self._identity_or_fallback(created_material.get_path_name())
        result.update(
            {
                "success": True,
                "asset_class": "Material",
            }
        )
        return result

    def create_material_function(self, params: Dict[str, Any]) -> Dict[str, Any]:
        function_name = str(params.get("name", "") or params.get("function_name", "")).strip()
        if not function_name:
            raise AssetCommandError("缺少 'name' 参数")

        function_class_name = str(params.get("function_class", "MaterialFunction")).strip() or "MaterialFunction"
        function_class = self._resolve_material_function_class(function_class_name)
        package_path = self._normalize_package_path(str(params.get("path", "")).strip(), self.DEFAULT_PACKAGE_PATH)
        factory = unreal.MaterialFunctionFactoryNew()
        factory.set_editor_property("supported_class", function_class)
        created_function = self._create_material_asset(
            asset_name=function_name,
            package_path=package_path,
            asset_class=function_class,
            factory=factory,
            duplicate_error_prefix="材质函数已存在",
            create_error_message="创建材质函数失败",
        )

        result = self._identity_or_fallback(created_function.get_path_name())
        result.update(
            {
                "success": True,
                "asset_class": self._class_name(function_class),
                "function_class": self._class_name(function_class),
            }
        )
        return result

    def create_render_target(self, params: Dict[str, Any]) -> Dict[str, Any]:
        render_target_name = str(params.get("name", "") or params.get("render_target_name", "")).strip()
        if not render_target_name:
            raise AssetCommandError("缺少 'name' 参数")

        package_path = self._normalize_package_path(
            str(params.get("path", "")).strip(),
            self.DEFAULT_RENDER_TARGET_PATH,
        )
        width = self._parse_positive_int(params, "width", self.DEFAULT_RENDER_TARGET_WIDTH)
        height = self._parse_positive_int(params, "height", self.DEFAULT_RENDER_TARGET_HEIGHT)
        render_target_format_name = str(
            params.get("format", "") or params.get("render_target_format", self.DEFAULT_RENDER_TARGET_FORMAT)
        ).strip() or self.DEFAULT_RENDER_TARGET_FORMAT
        render_target_format = self._resolve_render_target_format(render_target_format_name)
        clear_color = self._parse_optional_linear_color(params, "clear_color", unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
        auto_generate_mips = bool(params.get("auto_generate_mips", False))

        created_render_target = self._create_material_asset(
            asset_name=render_target_name,
            package_path=package_path,
            asset_class=unreal.TextureRenderTarget2D,
            factory=unreal.TextureRenderTargetFactoryNew(),
            duplicate_error_prefix="渲染目标已存在",
            create_error_message="创建渲染目标失败",
        )
        created_render_target.set_editor_property("size_x", width)
        created_render_target.set_editor_property("size_y", height)
        created_render_target.set_editor_property("render_target_format", render_target_format)
        created_render_target.set_editor_property("clear_color", clear_color)
        created_render_target.set_editor_property("auto_generate_mips", auto_generate_mips)
        self._save_loaded_asset(
            created_render_target,
            created_render_target.get_path_name(),
            "保存渲染目标失败",
        )

        result = self._identity_or_fallback(created_render_target.get_path_name())
        result.update(
            {
                "success": True,
                "asset_class": "TextureRenderTarget2D",
                "width": width,
                "height": height,
                "format": self._enum_name(render_target_format),
                "clear_color": self._linear_color_to_list(clear_color),
                "auto_generate_mips": auto_generate_mips,
            }
        )
        return result

    def create_material_instance(self, params: Dict[str, Any]) -> Dict[str, Any]:
        instance_name = str(params.get("name", "") or params.get("material_instance_name", "")).strip()
        if not instance_name:
            raise AssetCommandError("缺少 'name' 参数")

        parent_reference = str(params.get("parent_material", "") or params.get("parent", "")).strip()
        if not parent_reference:
            raise AssetCommandError("缺少 'parent_material' 参数")

        parent_material = self._resolve_material_interface_by_reference(parent_reference)
        package_path = self._normalize_package_path(str(params.get("path", "")).strip(), self.DEFAULT_PACKAGE_PATH)
        created_instance = self._create_material_asset(
            asset_name=instance_name,
            package_path=package_path,
            asset_class=unreal.MaterialInstanceConstant,
            factory=unreal.MaterialInstanceConstantFactoryNew(),
            duplicate_error_prefix="材质实例已存在",
            create_error_message="创建材质实例失败",
        )

        unreal.MaterialEditingLibrary.set_material_instance_parent(created_instance, parent_material)
        unreal.MaterialEditingLibrary.update_material_instance(created_instance)
        self._save_loaded_asset(created_instance, created_instance.get_path_name(), "保存材质实例失败")

        result = self._identity_or_fallback(created_instance.get_path_name())
        result.update(
            {
                "success": True,
                "asset_class": "MaterialInstanceConstant",
                "parent_material": parent_material.get_path_name(),
            }
        )
        return result

    def get_material_parameters(self, params: Dict[str, Any]) -> Dict[str, Any]:
        material, resolved_asset = self._resolve_material_interface_from_params(params)
        result = resolved_asset.to_identity()
        is_material_instance = isinstance(material, unreal.MaterialInstance)
        result.update(
            {
                "success": True,
                "summary_kind": "MaterialInstance" if is_material_instance else "Material",
                "is_material_instance": is_material_instance,
            }
        )

        if is_material_instance:
            parent_material = getattr(material, "parent", None)
            result["parent_material"] = parent_material.get_path_name() if parent_material else ""

        child_instances = list(unreal.MaterialEditingLibrary.get_child_instances(material) or [])
        child_payload = sorted(
            (ResolvedAsset(asset_data=child_asset).to_identity() for child_asset in child_instances),
            key=lambda item: item["object_path"],
        )
        result["child_instances"] = child_payload
        result["child_instance_count"] = len(child_payload)

        scalar_parameters = self._collect_material_parameters(material, "scalar")
        vector_parameters = self._collect_material_parameters(material, "vector")
        texture_parameters = self._collect_material_parameters(material, "texture")
        static_switch_parameters = self._collect_material_parameters(material, "static_switch")

        result["scalar_parameters"] = scalar_parameters
        result["vector_parameters"] = vector_parameters
        result["texture_parameters"] = texture_parameters
        result["static_switch_parameters"] = static_switch_parameters
        result["scalar_parameter_count"] = len(scalar_parameters)
        result["vector_parameter_count"] = len(vector_parameters)
        result["texture_parameter_count"] = len(texture_parameters)
        result["static_switch_parameter_count"] = len(static_switch_parameters)
        return result

    def set_material_instance_scalar_parameter(self, params: Dict[str, Any]) -> Dict[str, Any]:
        material_instance, resolved_asset = self._resolve_material_instance_from_params(params)
        parameter_name = self._require_parameter_name(params)
        if "value" not in params:
            raise AssetCommandError("缺少 'value' 参数")

        value = float(params["value"])
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            material_instance,
            parameter_name,
            value,
        )
        self._save_material_instance(material_instance)

        result = resolved_asset.to_identity()
        result.update(
            {
                "success": True,
                "parameter_name": parameter_name,
                "value": unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(
                    material_instance,
                    parameter_name,
                ),
            }
        )
        return result

    def set_material_instance_vector_parameter(self, params: Dict[str, Any]) -> Dict[str, Any]:
        material_instance, resolved_asset = self._resolve_material_instance_from_params(params)
        parameter_name = self._require_parameter_name(params)
        value = self._parse_linear_color(params)

        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
            material_instance,
            parameter_name,
            value,
        )
        self._save_material_instance(material_instance)

        current_value = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(
            material_instance,
            parameter_name,
        )
        result = resolved_asset.to_identity()
        result.update(
            {
                "success": True,
                "parameter_name": parameter_name,
                "value": self._linear_color_to_list(current_value),
            }
        )
        return result

    def set_material_instance_texture_parameter(self, params: Dict[str, Any]) -> Dict[str, Any]:
        material_instance, resolved_asset = self._resolve_material_instance_from_params(params)
        parameter_name = self._require_parameter_name(params)
        texture_reference = str(params.get("texture_asset_path", "") or params.get("texture", "")).strip()
        if not texture_reference:
            raise AssetCommandError("缺少 'texture_asset_path' 参数")

        texture_asset = self._resolver.resolve_reference(texture_reference).asset_data.get_asset()
        if not texture_asset or not isinstance(texture_asset, unreal.Texture):
            raise AssetCommandError(f"资源不是 Texture: {texture_reference}")

        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
            material_instance,
            parameter_name,
            texture_asset,
        )
        self._save_material_instance(material_instance)

        current_texture = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(
            material_instance,
            parameter_name,
        )
        result = resolved_asset.to_identity()
        result.update(
            {
                "success": True,
                "parameter_name": parameter_name,
                "texture_name": current_texture.get_name() if current_texture else "",
                "texture_path": current_texture.get_path_name() if current_texture else "",
            }
        )
        return result

    def _resolve_material_interface_by_reference(self, reference: str) -> unreal.MaterialInterface:
        material_asset = self._resolver.resolve_reference(reference).asset_data.get_asset()
        if not material_asset or not isinstance(material_asset, unreal.MaterialInterface):
            raise AssetCommandError(f"资源不是材质或材质实例: {reference}")
        return material_asset

    def _resolve_material_interface_from_params(
        self,
        params: Dict[str, Any],
    ) -> Tuple[unreal.MaterialInterface, ResolvedAsset]:
        resolved_asset = self._resolver.resolve_from_params(params)
        material_asset = resolved_asset.asset_data.get_asset()
        if not material_asset or not isinstance(material_asset, unreal.MaterialInterface):
            raise AssetCommandError(
                f"资源不是材质或材质实例: {resolved_asset.to_identity()['object_path']}"
            )
        return material_asset, resolved_asset

    def _resolve_material_instance_from_params(
        self,
        params: Dict[str, Any],
    ) -> Tuple[unreal.MaterialInstanceConstant, ResolvedAsset]:
        material_asset, resolved_asset = self._resolve_material_interface_from_params(params)
        if not isinstance(material_asset, unreal.MaterialInstanceConstant):
            raise AssetCommandError(
                f"资源不是 MaterialInstanceConstant: {resolved_asset.to_identity()['object_path']}"
            )
        return material_asset, resolved_asset

    @staticmethod
    def _resolve_material_function_class(function_class_name: str) -> Any:
        normalized_name = function_class_name.strip()
        supported_classes = {
            "materialfunction": unreal.MaterialFunction,
            "materialfunctionmateriallayer": unreal.MaterialFunctionMaterialLayer,
            "materialfunctionmateriallayerblend": unreal.MaterialFunctionMaterialLayerBlend,
        }
        resolved_class = supported_classes.get(normalized_name.casefold())
        if resolved_class is None:
            raise AssetCommandError(
                "不支持的 function_class，当前只支持 MaterialFunction、MaterialFunctionMaterialLayer、MaterialFunctionMaterialLayerBlend"
            )
        return resolved_class

    @staticmethod
    def _resolve_render_target_format(format_name: str) -> Any:
        normalized_name = format_name.strip()
        if not normalized_name:
            normalized_name = MaterialCommandExecutor.DEFAULT_RENDER_TARGET_FORMAT
        if not hasattr(unreal, "TextureRenderTargetFormat"):
            raise AssetCommandError("当前编辑器未暴露 TextureRenderTargetFormat")
        enum_value = getattr(unreal.TextureRenderTargetFormat, normalized_name, None)
        if enum_value is None:
            raise AssetCommandError(f"不支持的 render_target_format: {format_name}")
        return enum_value

    @staticmethod
    def _normalize_package_path(package_path: str, default_path: str) -> str:
        normalized_path = package_path or default_path
        if not normalized_path.startswith("/Game"):
            raise AssetCommandError("'path' 必须以 /Game 开头")
        return normalized_path

    def _create_material_asset(
        self,
        asset_name: str,
        package_path: str,
        asset_class: Any,
        factory: unreal.Factory,
        duplicate_error_prefix: str,
        create_error_message: str,
    ) -> Any:
        asset_path = f"{package_path}/{asset_name}"
        if not unreal.EditorAssetLibrary.does_directory_exist(package_path):
            if not unreal.EditorAssetLibrary.make_directory(package_path):
                raise AssetCommandError(f"创建目录失败: {package_path}")

        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            raise AssetCommandError(f"{duplicate_error_prefix}: {asset_path}")

        created_asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name,
            package_path,
            asset_class,
            factory,
        )
        if not created_asset:
            raise AssetCommandError(create_error_message)

        self._save_loaded_asset(created_asset, asset_path, f"{create_error_message}后保存失败")
        return created_asset

    def _collect_material_parameters(self, material: unreal.MaterialInterface, parameter_kind: str) -> List[Dict[str, Any]]:
        material_lib = unreal.MaterialEditingLibrary
        getter_name = f"get_{parameter_kind}_parameter_names"
        parameter_names = list(getattr(material_lib, getter_name)(material) or [])
        sorted_names = sorted({str(parameter_name) for parameter_name in parameter_names}, key=str.casefold)
        collected: List[Dict[str, Any]] = []
        for parameter_name_text in sorted_names:
            parameter_name = unreal.Name(parameter_name_text)
            parameter_info = self._get_material_parameter_info(material, parameter_name)
            item = self._create_material_parameter_payload(material, parameter_name_text, parameter_info)
            self._fill_material_parameter_value(material, parameter_kind, parameter_name, item)
            collected.append(item)
        return collected

    def _get_material_parameter_info(
        self,
        material: unreal.MaterialInterface,
        parameter_name: unreal.Name,
    ) -> Optional[unreal.MaterialParameterInfo]:
        try:
            return material.get_parameter_info(
                unreal.MaterialParameterAssociation.GLOBAL_PARAMETER,
                parameter_name,
                None,
            )
        except Exception:
            return None

    def _create_material_parameter_payload(
        self,
        material: unreal.MaterialInterface,
        parameter_name_text: str,
        parameter_info: Optional[unreal.MaterialParameterInfo],
    ) -> Dict[str, Any]:
        if parameter_info:
            association = self._material_parameter_association_to_string(parameter_info.association)
            association_index = int(parameter_info.index)
        else:
            association = "global"
            association_index = -1

        source_asset_path = self._get_parameter_source_path(material, parameter_name_text)
        return {
            "name": parameter_name_text,
            "association": association,
            "association_index": association_index,
            "group": "",
            "description": "",
            "source_asset_path": source_asset_path,
            "override": False,
            "sort_priority": 0,
        }

    def _fill_material_parameter_value(
        self,
        material: unreal.MaterialInterface,
        parameter_kind: str,
        parameter_name: unreal.Name,
        item: Dict[str, Any],
    ) -> None:
        material_lib = unreal.MaterialEditingLibrary
        if parameter_kind == "scalar":
            value = self._get_scalar_parameter_value(material, parameter_name)
            item["value"] = value
            item["override"] = self._is_material_instance_parameter_overridden(
                material,
                parameter_kind,
                parameter_name,
                value,
            )
            return
        if parameter_kind == "vector":
            value = self._get_vector_parameter_value(material, parameter_name)
            item["value"] = self._linear_color_to_list(value)
            item["override"] = self._is_material_instance_parameter_overridden(
                material,
                parameter_kind,
                parameter_name,
                value,
            )
            return
        if parameter_kind == "texture":
            texture = self._get_texture_parameter_value(material, parameter_name)
            item["texture_name"] = texture.get_name() if texture else ""
            item["texture_path"] = texture.get_path_name() if texture else ""
            item["override"] = self._is_material_instance_parameter_overridden(
                material,
                parameter_kind,
                parameter_name,
                texture,
            )
            return
        if parameter_kind == "static_switch":
            value = self._get_static_switch_parameter_value(material, parameter_name)
            item["value"] = value
            item["dynamic"] = False
            item["override"] = self._is_material_instance_parameter_overridden(
                material,
                parameter_kind,
                parameter_name,
                value,
            )
            return
        raise AssetCommandError(f"不支持的材质参数类型: {parameter_kind}")

    def _get_parameter_source_path(self, material: unreal.MaterialInterface, parameter_name_text: str) -> str:
        material_lib = unreal.MaterialEditingLibrary
        parameter_name = unreal.Name(parameter_name_text)
        for getter_name in (
            "get_scalar_parameter_source",
            "get_vector_parameter_source",
            "get_texture_parameter_source",
            "get_static_switch_parameter_source",
        ):
            try:
                source = getattr(material_lib, getter_name)(material, parameter_name)
            except Exception:
                continue
            source_path = self._soft_object_path_to_string(source)
            if source_path:
                return source_path
        return ""

    @staticmethod
    def _soft_object_path_to_string(soft_object_path: Any) -> str:
        if not soft_object_path:
            return ""
        try:
            return str(soft_object_path.export_text())
        except Exception:
            return ""

    @staticmethod
    def _material_parameter_association_to_string(association: Any) -> str:
        if association == unreal.MaterialParameterAssociation.LAYER_PARAMETER:
            return "layer"
        if association == unreal.MaterialParameterAssociation.BLEND_PARAMETER:
            return "blend"
        return "global"

    @staticmethod
    def _linear_color_to_list(color: unreal.LinearColor) -> List[float]:
        return [float(color.r), float(color.g), float(color.b), float(color.a)]

    def _get_scalar_parameter_value(self, material: unreal.MaterialInterface, parameter_name: unreal.Name) -> float:
        material_lib = unreal.MaterialEditingLibrary
        if isinstance(material, unreal.MaterialInstance):
            return float(material_lib.get_material_instance_scalar_parameter_value(material, parameter_name))
        return float(material_lib.get_material_default_scalar_parameter_value(material, parameter_name))

    def _get_vector_parameter_value(
        self,
        material: unreal.MaterialInterface,
        parameter_name: unreal.Name,
    ) -> unreal.LinearColor:
        material_lib = unreal.MaterialEditingLibrary
        if isinstance(material, unreal.MaterialInstance):
            return material_lib.get_material_instance_vector_parameter_value(material, parameter_name)
        return material_lib.get_material_default_vector_parameter_value(material, parameter_name)

    def _get_texture_parameter_value(
        self,
        material: unreal.MaterialInterface,
        parameter_name: unreal.Name,
    ) -> Optional[unreal.Texture]:
        material_lib = unreal.MaterialEditingLibrary
        if isinstance(material, unreal.MaterialInstance):
            return material_lib.get_material_instance_texture_parameter_value(material, parameter_name)
        return material_lib.get_material_default_texture_parameter_value(material, parameter_name)

    def _get_static_switch_parameter_value(
        self,
        material: unreal.MaterialInterface,
        parameter_name: unreal.Name,
    ) -> bool:
        material_lib = unreal.MaterialEditingLibrary
        if isinstance(material, unreal.MaterialInstance):
            return bool(material_lib.get_material_instance_static_switch_parameter_value(material, parameter_name))
        return bool(material_lib.get_material_default_static_switch_parameter_value(material, parameter_name))

    def _is_material_instance_parameter_overridden(
        self,
        material: unreal.MaterialInterface,
        parameter_kind: str,
        parameter_name: unreal.Name,
        current_value: Any,
    ) -> bool:
        if not isinstance(material, unreal.MaterialInstance):
            return False

        parent_material = getattr(material, "parent", None)
        if not parent_material:
            return False

        if parameter_kind == "scalar":
            parent_value = self._get_scalar_parameter_value(parent_material, parameter_name)
            return abs(float(current_value) - float(parent_value)) > 1e-6
        if parameter_kind == "vector":
            parent_value = self._get_vector_parameter_value(parent_material, parameter_name)
            return any(
                abs(current_channel - parent_channel) > 1e-6
                for current_channel, parent_channel in zip(
                    self._linear_color_to_list(current_value),
                    self._linear_color_to_list(parent_value),
                )
            )
        if parameter_kind == "texture":
            parent_value = self._get_texture_parameter_value(parent_material, parameter_name)
            current_path = current_value.get_path_name() if current_value else ""
            parent_path = parent_value.get_path_name() if parent_value else ""
            return current_path != parent_path
        if parameter_kind == "static_switch":
            parent_value = self._get_static_switch_parameter_value(parent_material, parameter_name)
            return bool(current_value) != bool(parent_value)
        return False

    @staticmethod
    def _require_parameter_name(params: Dict[str, Any]) -> str:
        parameter_name = str(params.get("parameter_name", "")).strip()
        if not parameter_name:
            raise AssetCommandError("缺少 'parameter_name' 参数")
        return parameter_name

    @staticmethod
    def _parse_linear_color(params: Dict[str, Any]) -> unreal.LinearColor:
        value = params.get("value")
        if not isinstance(value, list):
            raise AssetCommandError("缺少 'value' 参数")
        if len(value) != 4:
            raise AssetCommandError("'value' 必须是 4 个数字 [R, G, B, A]")
        return unreal.LinearColor(float(value[0]), float(value[1]), float(value[2]), float(value[3]))

    @staticmethod
    def _parse_optional_linear_color(
        params: Dict[str, Any],
        field_name: str,
        default_value: unreal.LinearColor,
    ) -> unreal.LinearColor:
        value = params.get(field_name)
        if value is None:
            return default_value
        if not isinstance(value, list):
            raise AssetCommandError(f"'{field_name}' 必须是 4 个数字 [R, G, B, A]")
        if len(value) != 4:
            raise AssetCommandError(f"'{field_name}' 必须是 4 个数字 [R, G, B, A]")
        return unreal.LinearColor(float(value[0]), float(value[1]), float(value[2]), float(value[3]))

    @staticmethod
    def _parse_positive_int(params: Dict[str, Any], field_name: str, default_value: int) -> int:
        raw_value = params.get(field_name, default_value)
        value = int(raw_value)
        if value <= 0:
            raise AssetCommandError(f"'{field_name}' 必须大于 0")
        return value

    @staticmethod
    def _class_name(class_object: Any) -> str:
        return getattr(class_object, "__name__", str(class_object))

    @staticmethod
    def _enum_name(enum_value: Any) -> str:
        try:
            return str(enum_value.name)
        except Exception:
            return str(enum_value)

    @staticmethod
    def _save_loaded_asset(asset_object: Any, asset_path: str, error_prefix: str) -> None:
        if hasattr(asset_object, "mark_package_dirty"):
            asset_object.mark_package_dirty()
        if hasattr(asset_object, "post_edit_change"):
            asset_object.post_edit_change()
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset_object, only_if_is_dirty=False):
            raise AssetCommandError(f"{error_prefix}: {asset_path}")

    def _save_material_instance(self, material_instance: unreal.MaterialInstanceConstant) -> None:
        unreal.MaterialEditingLibrary.update_material_instance(material_instance)
        self._save_loaded_asset(material_instance, material_instance.get_path_name(), "保存材质实例失败")

    def _identity_or_fallback(self, asset_reference: str) -> Dict[str, Any]:
        try:
            return self._resolver.resolve_reference(asset_reference).to_identity()
        except AssetCommandError:
            return {"asset_path": asset_reference}


class AssetCommandDispatcher:
    """Dispatch local asset commands."""

    def __init__(self) -> None:
        self._resolver = AssetResolver()
        self._creation_resolver = AssetCreationResolver()
        self._reimport_executor = ReimportCommandExecutor(self._resolver)
        self._material_executor = MaterialCommandExecutor(self._resolver)

    def handle(self, command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """Dispatch a command to its local implementation."""
        command_handlers = {
            "make_directory": self._handle_make_directory,
            "duplicate_asset": self._handle_duplicate_asset,
            "search_assets": self._handle_search_assets,
            "get_asset_metadata": self._handle_get_asset_metadata,
            "create_asset": self._handle_create_asset,
            "save_asset": self._handle_save_asset,
            "import_asset": self._handle_import_asset,
            "export_asset": self._handle_export_asset,
            "reimport_asset": self._handle_reimport_asset,
            "rename_asset": self._handle_rename_asset,
            "move_asset": self._handle_move_asset,
            "batch_rename_assets": self._handle_batch_rename_assets,
            "batch_move_assets": self._handle_batch_move_assets,
            "delete_asset": self._handle_delete_asset,
            "set_asset_metadata": self._handle_set_asset_metadata,
            "consolidate_assets": self._handle_consolidate_assets,
            "replace_asset_references": self._handle_replace_asset_references,
            "get_selected_assets": self._handle_get_selected_assets,
            "sync_content_browser_to_assets": self._handle_sync_content_browser_to_assets,
            "save_all_dirty_assets": self._handle_save_all_dirty_assets,
            "create_material": self._material_executor.create_material,
            "create_material_function": self._material_executor.create_material_function,
            "create_render_target": self._material_executor.create_render_target,
            "create_material_instance": self._material_executor.create_material_instance,
            "get_material_parameters": self._material_executor.get_material_parameters,
            "set_material_instance_scalar_parameter": self._material_executor.set_material_instance_scalar_parameter,
            "set_material_instance_vector_parameter": self._material_executor.set_material_instance_vector_parameter,
            "set_material_instance_texture_parameter": self._material_executor.set_material_instance_texture_parameter,
        }
        handler = command_handlers.get(command_name)
        if handler is None:
            raise AssetCommandError(f"未支持的本地资源命令: {command_name}")
        return handler(params)

    def _handle_make_directory(self, params: Dict[str, Any]) -> Dict[str, Any]:
        directory_path = str(params.get("directory_path", "")).strip()
        if not directory_path:
            raise AssetCommandError("缺少 'directory_path' 参数")
        if not directory_path.startswith("/Game"):
            raise AssetCommandError("'directory_path' 必须以 /Game 开头")

        if unreal.EditorAssetLibrary.does_directory_exist(directory_path):
            return {
                "success": True,
                "already_exists": True,
                "directory_path": directory_path,
            }

        if not unreal.EditorAssetLibrary.make_directory(directory_path):
            raise AssetCommandError(f"创建目录失败: {directory_path}")

        return {
            "success": True,
            "already_exists": False,
            "directory_path": directory_path,
        }

    def _handle_search_assets(self, params: Dict[str, Any]) -> Dict[str, Any]:
        path = str(params.get("path", "/Game")).strip()
        query = str(params.get("query", "") or params.get("name_contains", "")).strip()
        query_mode = self._normalize_search_query_mode(str(params.get("query_mode", "contains")).strip())
        class_name = str(params.get("class_name", "")).strip()
        recursive_paths = bool(params.get("recursive_paths", True))
        include_tags = bool(params.get("include_tags", False))
        limit = max(1, min(int(params.get("limit", 50)), 200))

        filtered_assets: List[unreal.AssetData] = []
        for asset_data in self._enumerate_assets(path, recursive_paths):
            if not self._matches_class_filter(asset_data, class_name):
                continue

            if query:
                object_path = self._get_object_path(asset_data)
                asset_name = str(asset_data.asset_name)
                if not self._matches_search_query(query, query_mode, asset_name, object_path):
                    continue

            filtered_assets.append(asset_data)

        filtered_assets.sort(key=self._get_object_path)

        assets_payload: List[Dict[str, Any]] = []
        for asset_data in filtered_assets[:limit]:
            item = ResolvedAsset(asset_data=asset_data).to_identity()
            if include_tags:
                item.update(self._collect_tags_payload(asset_data, 32))
            assets_payload.append(item)

        return {
            "success": True,
            "path": path,
            "query": query,
            "query_mode": query_mode,
            "class_name": class_name,
            "total_matches": len(filtered_assets),
            "returned_count": len(assets_payload),
            "assets": assets_payload,
        }

    def _handle_duplicate_asset(self, params: Dict[str, Any]) -> Dict[str, Any]:
        source_asset_path = self._require_string(params, "source_asset_path")
        destination_asset_path = self._require_string(params, "destination_asset_path")
        overwrite = bool(params.get("overwrite", False))

        if not unreal.EditorAssetLibrary.does_asset_exist(source_asset_path):
            raise AssetCommandError(f"源资源不存在: {source_asset_path}")

        if unreal.EditorAssetLibrary.does_asset_exist(destination_asset_path):
            if not overwrite:
                raise AssetCommandError(f"目标资源已存在: {destination_asset_path}")
            if not unreal.EditorAssetLibrary.delete_asset(destination_asset_path):
                raise AssetCommandError(f"删除已有目标资源失败: {destination_asset_path}")

        duplicated_asset = unreal.EditorAssetLibrary.duplicate_asset(source_asset_path, destination_asset_path)
        if not duplicated_asset:
            raise AssetCommandError(f"复制资源失败: {source_asset_path} -> {destination_asset_path}")

        unreal.EditorAssetLibrary.save_asset(destination_asset_path, only_if_is_dirty=False)

        result = self._identity_or_fallback(destination_asset_path)
        result.update(
            {
                "success": True,
                "source_asset_path": source_asset_path,
                "destination_asset_path": destination_asset_path,
                "overwrite": overwrite,
            }
        )
        return result

    def _handle_get_asset_metadata(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_asset = self._resolver.resolve_from_params(params)
        result = resolved_asset.to_identity()
        result.update(self._collect_tags_payload(resolved_asset.asset_data, 128))
        result["dependencies"] = self._collect_dependency_paths(resolved_asset.asset_data, referencers=False)
        result["referencers"] = self._collect_dependency_paths(resolved_asset.asset_data, referencers=True)
        result["success"] = True
        return result

    def _handle_create_asset(self, params: Dict[str, Any]) -> Dict[str, Any]:
        asset_name = str(params.get("name", "") or params.get("asset_name", "")).strip()
        if not asset_name:
            raise AssetCommandError("缺少 'name' 参数")

        path = str(params.get("path", "/Game")).strip()
        if not path.startswith("/Game"):
            raise AssetCommandError("'path' 必须以 /Game 开头")

        unique_name = bool(params.get("unique_name", False))
        save_asset = bool(params.get("save_asset", True))
        resolved_factory = self._creation_resolver.resolve_creation_request(params)
        applied_factory_options = self._creation_resolver.apply_factory_options(resolved_factory, params)

        requested_asset_path = f"{path}/{asset_name}"
        final_asset_path, final_asset_name = self._resolve_create_asset_target_path(
            requested_asset_path=requested_asset_path,
            asset_name=asset_name,
            unique_name=unique_name,
        )

        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        created_asset = asset_tools.create_asset(
            final_asset_name,
            path,
            resolved_factory.asset_class,
            resolved_factory.factory,
        )
        if not created_asset:
            raise AssetCommandError(f"创建资产失败: {requested_asset_path}")

        if save_asset and not unreal.EditorAssetLibrary.save_loaded_asset(created_asset, only_if_is_dirty=False):
            raise AssetCommandError(f"保存新资产失败: {created_asset.get_path_name()}")

        result = self._identity_or_fallback(created_asset.get_path_name())
        result.update(
            {
                "success": True,
                "requested_asset_path": requested_asset_path,
                "requested_asset_name": asset_name,
                "path": path,
                "asset_class_request": resolved_factory.asset_class_reference,
                "factory_class": resolved_factory.factory_class_reference,
                "used_default_factory": resolved_factory.used_default_factory,
                "unique_name": unique_name,
                "save_asset": save_asset,
                "saved": save_asset,
                "factory_options": applied_factory_options,
            }
        )
        return result

    def _handle_save_asset(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_asset = self._resolver.resolve_from_params(params)
        asset_object = resolved_asset.asset_data.get_asset()
        if not asset_object:
            raise AssetCommandError(f"加载资源失败: {resolved_asset.to_identity()['object_path']}")

        only_if_dirty = bool(params.get("only_if_dirty", False))
        outermost = asset_object.get_outermost()
        dirty_packages = unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
        dirty_package_paths = {package.get_path_name() for package in dirty_packages if package}
        was_dirty = bool(outermost and outermost.get_path_name() in dirty_package_paths)
        save_attempted = (not only_if_dirty) or was_dirty
        saved = True
        if save_attempted:
            saved = bool(unreal.EditorAssetLibrary.save_loaded_asset(asset_object, only_if_is_dirty=False))
        if not saved:
            raise AssetCommandError(f"保存资源失败: {resolved_asset.to_identity()['object_path']}")

        result = resolved_asset.to_identity()
        result.update(
            {
                "success": True,
                "saved": True,
                "was_dirty": was_dirty,
                "only_if_dirty": only_if_dirty,
                "save_attempted": save_attempted,
            }
        )
        return result

    def _handle_import_asset(self, params: Dict[str, Any]) -> Dict[str, Any]:
        raw_source_files: List[str] = []
        filename = str(params.get("filename", "")).strip()
        if filename:
            raw_source_files.append(filename)

        source_files_param = params.get("source_files") or []
        if source_files_param and not isinstance(source_files_param, list):
            raise AssetCommandError("'source_files' 必须是数组")
        for source_file in source_files_param:
            normalized_source_file = str(source_file).strip()
            if normalized_source_file:
                raw_source_files.append(normalized_source_file)

        if not raw_source_files:
            raise AssetCommandError("缺少 'filename' 或 'source_files' 参数")

        destination_path = str(params.get("destination_path", "")).strip()
        if not destination_path:
            raise AssetCommandError("缺少 'destination_path' 参数")
        if not destination_path.startswith("/Game"):
            raise AssetCommandError("'destination_path' 必须以 /Game 开头")

        destination_name = str(params.get("destination_name", "")).strip()
        if destination_name and len(raw_source_files) > 1:
            raise AssetCommandError("'destination_name' 只能在导入单个文件时使用")

        replace_existing = bool(params.get("replace_existing", True))
        replace_existing_settings = bool(params.get("replace_existing_settings", False))
        automated = bool(params.get("automated", True))
        save_asset = bool(params.get("save", True))
        requested_async_import = bool(params.get("async_import", True))

        resolved_source_files = [
            self._normalize_existing_file_path(source_file)
            for source_file in raw_source_files
        ]

        tasks: List[unreal.AssetImportTask] = []
        expected_object_paths: List[str] = []
        for source_file in resolved_source_files:
            task = unreal.AssetImportTask()
            task.set_editor_properties(
                {
                    "filename": source_file,
                    "destination_path": destination_path,
                    "destination_name": destination_name,
                    "replace_existing": replace_existing,
                    "replace_existing_settings": replace_existing_settings,
                    "automated": automated,
                    "save": save_asset,
                    # UE5.7 下当前仍统一走异步导入，避免同步导入触发递归断言。
                    "async_": True,
                }
            )
            tasks.append(task)

            if destination_name:
                expected_object_paths.append(
                    f"{destination_path}/{destination_name}.{destination_name}"
                )

        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

        imported_object_paths: List[str] = []
        imported_assets: List[Dict[str, Any]] = []
        import_completed = True
        has_async_results = bool(tasks)

        for task in tasks:
            try:
                if not bool(task.is_async_import_complete()):
                    import_completed = False
            except Exception:
                import_completed = False

            for imported_object_path in list(task.imported_object_paths or []):
                normalized_object_path = str(imported_object_path).strip()
                if not normalized_object_path:
                    continue
                imported_object_paths.append(normalized_object_path)
                imported_assets.append(self._identity_or_fallback(normalized_object_path))

        if not imported_object_paths and not has_async_results:
            raise AssetCommandError("导入失败，未返回任何 ImportedObjectPaths")

        return {
            "success": True,
            "destination_path": destination_path,
            "destination_name": destination_name,
            "requested_async_import": requested_async_import,
            "replace_existing": replace_existing,
            "replace_existing_settings": replace_existing_settings,
            "automated": automated,
            "save": save_asset,
            "async_import": True,
            "import_completed": import_completed,
            "has_async_results": has_async_results,
            "source_files": resolved_source_files,
            "imported_object_paths": imported_object_paths,
            "imported_assets": imported_assets,
            "expected_object_paths": expected_object_paths,
            "imported_count": len(imported_object_paths),
            "task_count": len(tasks),
        }

    def _handle_export_asset(self, params: Dict[str, Any]) -> Dict[str, Any]:
        export_path_text = str(params.get("export_path", "")).strip()
        if not export_path_text:
            raise AssetCommandError("缺少 'export_path' 参数")
        export_path = os.path.abspath(export_path_text)

        resolved_assets = self._resolve_asset_list_from_params(params)
        if not resolved_assets:
            raise AssetCommandError("缺少 'asset_path' 或 'asset_paths' 参数")

        os.makedirs(export_path, exist_ok=True)
        clean_filenames = bool(params.get("clean_filenames", True))

        before_files = self._collect_directory_files(export_path)
        unreal.AssetToolsHelpers.get_asset_tools().export_assets(
            [asset.to_identity()["asset_path"] for asset in resolved_assets],
            export_path,
        )
        after_files = self._collect_directory_files(export_path)
        new_files = sorted(after_files - before_files)

        return {
            "success": True,
            "export_path": export_path,
            "clean_filenames": clean_filenames,
            "assets": [asset.to_identity() for asset in resolved_assets],
            "asset_count": len(resolved_assets),
            "new_files": new_files,
            "new_file_count": len(new_files),
        }

    def _handle_reimport_asset(self, params: Dict[str, Any]) -> Dict[str, Any]:
        request = self._reimport_executor.build_request(params)
        return self._reimport_executor.execute(request)

    def _handle_rename_asset(self, params: Dict[str, Any]) -> Dict[str, Any]:
        source_asset_path = self._require_string(params, "source_asset_path")
        destination_asset_path = self._require_string(params, "destination_asset_path")

        if not unreal.EditorAssetLibrary.does_asset_exist(source_asset_path):
            raise AssetCommandError(f"源资源不存在: {source_asset_path}")
        if unreal.EditorAssetLibrary.does_asset_exist(destination_asset_path):
            raise AssetCommandError(f"目标资源已存在: {destination_asset_path}")
        if not unreal.EditorAssetLibrary.rename_asset(source_asset_path, destination_asset_path):
            raise AssetCommandError(f"重命名资源失败: {source_asset_path} -> {destination_asset_path}")

        result = self._identity_or_fallback(destination_asset_path)
        result.update(
            {
                "success": True,
                "source_asset_path": source_asset_path,
                "destination_asset_path": destination_asset_path,
            }
        )
        return result

    def _handle_move_asset(self, params: Dict[str, Any]) -> Dict[str, Any]:
        result = self._handle_rename_asset(params)
        result["implementation_command"] = "move_asset"
        return result

    def _handle_batch_rename_assets(self, params: Dict[str, Any]) -> Dict[str, Any]:
        operations_param = params.get("operations")
        if not isinstance(operations_param, list) or not operations_param:
            raise AssetCommandError("缺少 'operations' 参数")

        overwrite = bool(params.get("overwrite", False))
        stop_on_error = bool(params.get("stop_on_error", True))
        results: List[Dict[str, Any]] = []
        successful_count = 0

        for index, operation_param in enumerate(operations_param):
            try:
                operation = self._resolve_batch_rename_operation(operation_param, index)
                operation_result = self._execute_batch_asset_rename(operation, overwrite)
                operation_result.update(
                    {
                        "index": index,
                        "operation_kind": "batch_rename_assets",
                    }
                )
                results.append(operation_result)
                successful_count += 1
            except AssetCommandError as exc:
                results.append(
                    {
                        "success": False,
                        "index": index,
                        "operation_kind": "batch_rename_assets",
                        "error": str(exc),
                    }
                )
                if stop_on_error:
                    break

        processed_count = len(results)
        failed_count = processed_count - successful_count
        return {
            "success": failed_count == 0 and processed_count == len(operations_param),
            "operation_count": len(operations_param),
            "processed_count": processed_count,
            "successful_count": successful_count,
            "failed_count": failed_count,
            "overwrite": overwrite,
            "stop_on_error": stop_on_error,
            "results": results,
        }

    def _handle_batch_move_assets(self, params: Dict[str, Any]) -> Dict[str, Any]:
        asset_paths = params.get("asset_paths")
        if not isinstance(asset_paths, list) or not asset_paths:
            raise AssetCommandError("缺少 'asset_paths' 参数")

        destination_path = str(params.get("destination_path", "")).strip()
        if not destination_path:
            raise AssetCommandError("缺少 'destination_path' 参数")
        if not destination_path.startswith("/Game"):
            raise AssetCommandError("'destination_path' 必须以 /Game 开头")

        overwrite = bool(params.get("overwrite", False))
        stop_on_error = bool(params.get("stop_on_error", True))
        results: List[Dict[str, Any]] = []
        successful_count = 0

        for index, asset_reference in enumerate(asset_paths):
            try:
                operation = self._build_batch_move_operation(asset_reference, destination_path)
                operation_result = self._execute_batch_asset_rename(operation, overwrite)
                operation_result.update(
                    {
                        "index": index,
                        "operation_kind": "batch_move_assets",
                    }
                )
                results.append(operation_result)
                successful_count += 1
            except AssetCommandError as exc:
                results.append(
                    {
                        "success": False,
                        "index": index,
                        "operation_kind": "batch_move_assets",
                        "asset_reference": str(asset_reference).strip(),
                        "error": str(exc),
                    }
                )
                if stop_on_error:
                    break

        processed_count = len(results)
        failed_count = processed_count - successful_count
        return {
            "success": failed_count == 0 and processed_count == len(asset_paths),
            "destination_path": destination_path,
            "asset_count": len(asset_paths),
            "processed_count": processed_count,
            "successful_count": successful_count,
            "failed_count": failed_count,
            "overwrite": overwrite,
            "stop_on_error": stop_on_error,
            "results": results,
        }

    def _handle_delete_asset(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_asset = self._resolver.resolve_from_params(params)
        asset_path = resolved_asset.to_identity()["asset_path"]
        if not unreal.EditorAssetLibrary.delete_asset(asset_path):
            raise AssetCommandError(f"删除资源失败: {asset_path}")

        result = resolved_asset.to_identity()
        result.update(
            {
                "success": True,
                "deleted": True,
            }
        )
        return result

    def _handle_set_asset_metadata(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_asset = self._resolver.resolve_from_params(params)
        asset_object = resolved_asset.asset_data.get_asset()
        if not asset_object:
            raise AssetCommandError(f"加载资源失败: {resolved_asset.to_identity()['object_path']}")

        asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
        if not asset_subsystem:
            raise AssetCommandError("EditorAssetSubsystem 不可用")

        metadata = params.get("metadata") or {}
        if not isinstance(metadata, dict):
            raise AssetCommandError("'metadata' 必须是对象")

        remove_metadata_keys = params.get("remove_metadata_keys") or params.get("remove_keys") or []
        if not isinstance(remove_metadata_keys, list):
            raise AssetCommandError("'remove_metadata_keys' 必须是数组")

        clear_existing = bool(params.get("clear_existing", False))
        save_asset = bool(params.get("save_asset", True))
        if not metadata and not remove_metadata_keys and not clear_existing:
            raise AssetCommandError("至少需要提供 'metadata'、'remove_metadata_keys' 或 'clear_existing'")

        existing_metadata = self._read_metadata(asset_subsystem, asset_object)
        removed_keys = set()

        modify = getattr(asset_object, "modify", None)
        if callable(modify):
            modify()

        if clear_existing:
            for metadata_key in existing_metadata:
                asset_subsystem.remove_metadata_tag(asset_object, metadata_key)
                removed_keys.add(str(metadata_key))

        for remove_key in remove_metadata_keys:
            normalized_key = str(remove_key).strip()
            if not normalized_key:
                continue
            asset_subsystem.remove_metadata_tag(asset_object, normalized_key)
            removed_keys.add(normalized_key)

        updated_keys: List[str] = []
        for metadata_key in sorted(metadata.keys()):
            normalized_key = str(metadata_key).strip()
            if not normalized_key:
                continue
            asset_subsystem.set_metadata_tag(
                asset_object,
                normalized_key,
                self._convert_metadata_value(metadata[metadata_key]),
            )
            updated_keys.append(normalized_key)

        outermost = asset_object.get_outermost() if hasattr(asset_object, "get_outermost") else None
        if outermost is not None:
            mark_dirty = getattr(outermost, "mark_package_dirty", None)
            if callable(mark_dirty):
                mark_dirty()

        saved = True
        if save_asset:
            saved = bool(unreal.EditorAssetLibrary.save_loaded_asset(asset_object, only_if_is_dirty=False))
            if not saved:
                raise AssetCommandError(f"保存资源失败: {resolved_asset.to_identity()['object_path']}")

        final_metadata = self._read_metadata(asset_subsystem, asset_object)

        result = resolved_asset.to_identity()
        result.update(
            {
                "success": True,
                "clear_existing": clear_existing,
                "save_asset": save_asset,
                "saved": saved,
                "updated_count": len(updated_keys),
                "removed_count": len(removed_keys),
                "metadata_count": len(final_metadata),
                "updated_keys": updated_keys,
                "removed_keys": sorted(removed_keys),
                "metadata": {key: final_metadata[key] for key in sorted(final_metadata.keys())},
            }
        )
        return result

    def _handle_get_selected_assets(self, params: Dict[str, Any]) -> Dict[str, Any]:
        include_tags = bool(params.get("include_tags", False))
        selected_assets = list(unreal.EditorUtilityLibrary.get_selected_asset_data() or [])

        assets_payload: List[Dict[str, Any]] = []
        for asset_data in selected_assets:
            item = ResolvedAsset(asset_data=asset_data).to_identity()
            if include_tags:
                item.update(self._collect_tags_payload(asset_data, 128))
            assets_payload.append(item)

        return {
            "success": True,
            "assets": assets_payload,
            "asset_count": len(assets_payload),
        }

    def _handle_consolidate_assets(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._handle_asset_consolidation_operation(
            params,
            target_prefix="target_",
            source_field_name="source_assets",
            operation_kind="consolidate_assets",
        )

    def _handle_replace_asset_references(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._handle_asset_consolidation_operation(
            params,
            target_prefix="replacement_",
            source_field_name="assets_to_replace",
            operation_kind="replace_asset_references",
        )

    def _handle_sync_content_browser_to_assets(self, params: Dict[str, Any]) -> Dict[str, Any]:
        asset_paths = params.get("asset_paths")
        if not isinstance(asset_paths, list):
            raise AssetCommandError("缺少 'asset_paths' 参数")

        synced_asset_paths: List[str] = []
        for asset_reference in asset_paths:
            normalized_reference = str(asset_reference).strip()
            if not normalized_reference:
                continue

            resolved_asset = self._resolver.resolve_reference(normalized_reference)
            asset_path = resolved_asset.to_identity()["asset_path"]
            if not unreal.EditorAssetLibrary.load_asset(asset_path):
                raise AssetCommandError(f"加载同步资源失败: {asset_path}")
            synced_asset_paths.append(asset_path)

        if not synced_asset_paths:
            raise AssetCommandError("没有提供可同步的有效资源")

        unreal.EditorAssetLibrary.sync_browser_to_objects(synced_asset_paths)
        return {
            "success": True,
            "asset_paths": synced_asset_paths,
            "asset_count": len(synced_asset_paths),
        }

    def _handle_save_all_dirty_assets(self, params: Dict[str, Any]) -> Dict[str, Any]:
        del params
        dirty_content = list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() or [])
        dirty_maps = list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() or [])
        packages_needed_saving = bool(dirty_content or dirty_maps)
        saved = bool(unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True))
        if not saved:
            raise AssetCommandError("保存脏包失败")

        return {
            "success": True,
            "packages_needed_saving": packages_needed_saving,
        }

    def _identity_or_fallback(self, asset_reference: str) -> Dict[str, Any]:
        """Return a resolved identity when possible, otherwise a minimal path payload."""
        try:
            return self._resolver.resolve_reference(asset_reference).to_identity()
        except AssetCommandError:
            return {"asset_path": asset_reference}

    @staticmethod
    def _resolve_create_asset_target_path(
        requested_asset_path: str,
        asset_name: str,
        unique_name: bool,
    ) -> Tuple[str, str]:
        if not unreal.EditorAssetLibrary.does_asset_exist(requested_asset_path):
            return requested_asset_path, asset_name
        if not unique_name:
            raise AssetCommandError(f"目标资产已存在: {requested_asset_path}")

        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        unique_asset_path, unique_asset_name = asset_tools.create_unique_asset_name(requested_asset_path, "")
        return str(unique_asset_path), str(unique_asset_name)

    def _handle_asset_consolidation_operation(
        self,
        params: Dict[str, Any],
        target_prefix: str,
        source_field_name: str,
        operation_kind: str,
    ) -> Dict[str, Any]:
        asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
        if not asset_subsystem:
            raise AssetCommandError("EditorAssetSubsystem 不可用")

        target_asset = self._resolve_prefixed_asset(params, target_prefix)
        source_asset_references = params.get(source_field_name)
        if not isinstance(source_asset_references, list):
            raise AssetCommandError(f"缺少 '{source_field_name}' 参数")

        target_identity = target_asset.to_identity()
        target_asset_object = target_asset.asset_data.get_asset()
        if not target_asset_object:
            raise AssetCommandError(f"加载目标资源失败: {target_identity['object_path']}")

        unique_source_assets: List[ResolvedAsset] = []
        source_asset_objects: List[unreal.Object] = []
        seen_object_paths: set[str] = set()
        for asset_reference in source_asset_references:
            normalized_reference = str(asset_reference).strip()
            if not normalized_reference:
                continue

            source_asset = self._resolver.resolve_reference(normalized_reference)
            source_identity = source_asset.to_identity()
            source_object_path = source_identity["object_path"]
            if source_identity["asset_path"] == target_identity["asset_path"]:
                raise AssetCommandError("目标资源不能同时出现在来源资源列表中")
            if source_object_path in seen_object_paths:
                continue

            source_asset_object = source_asset.asset_data.get_asset()
            if not source_asset_object:
                raise AssetCommandError(f"加载来源资源失败: {source_object_path}")
            if source_asset_object.get_class() != target_asset_object.get_class():
                raise AssetCommandError(
                    f"资源类型不匹配: {target_identity['object_path']} 与 {source_object_path} 必须是同类资源"
                )

            seen_object_paths.add(source_object_path)
            unique_source_assets.append(source_asset)
            source_asset_objects.append(source_asset_object)

        if not source_asset_objects:
            raise AssetCommandError("没有提供可处理的有效来源资源")

        if not asset_subsystem.consolidate_assets(target_asset_object, source_asset_objects):
            raise AssetCommandError(f"{operation_kind} 执行失败")

        result = dict(target_identity)
        result.update(
            {
                "success": True,
                "operation_kind": operation_kind,
                "deleted_source_assets": True,
                "source_asset_count": len(unique_source_assets),
                "deleted_asset_count": len(unique_source_assets),
                "source_assets": [asset.to_identity() for asset in unique_source_assets],
            }
        )
        return result

    def _resolve_prefixed_asset(self, params: Dict[str, Any], prefix: str) -> ResolvedAsset:
        lookup_fields = [
            f"{prefix}asset_path",
            f"{prefix}object_path",
            f"{prefix}asset_name",
            f"{prefix}name",
        ]
        for field_name in lookup_fields:
            reference = str(params.get(field_name, "")).strip()
            if reference:
                return self._resolver.resolve_reference(reference)
        raise AssetCommandError(
            f"缺少 '{prefix}asset_path'、'{prefix}object_path'、'{prefix}asset_name' 或 '{prefix}name' 参数"
        )

    def _resolve_asset_list_from_params(self, params: Dict[str, Any]) -> List[ResolvedAsset]:
        resolved_assets: List[ResolvedAsset] = []
        seen_asset_paths: set[str] = set()

        single_asset_reference = str(params.get("asset_path", "")).strip()
        if single_asset_reference:
            single_asset = self._resolver.resolve_reference(single_asset_reference)
            single_asset_path = single_asset.to_identity()["asset_path"]
            resolved_assets.append(single_asset)
            seen_asset_paths.add(single_asset_path)

        asset_paths_param = params.get("asset_paths") or []
        if asset_paths_param and not isinstance(asset_paths_param, list):
            raise AssetCommandError("'asset_paths' 必须是数组")

        for asset_reference in asset_paths_param:
            normalized_reference = str(asset_reference).strip()
            if not normalized_reference:
                continue

            resolved_asset = self._resolver.resolve_reference(normalized_reference)
            asset_path = resolved_asset.to_identity()["asset_path"]
            if asset_path in seen_asset_paths:
                continue

            resolved_assets.append(resolved_asset)
            seen_asset_paths.add(asset_path)

        return resolved_assets

    def _resolve_batch_rename_operation(self, operation_param: Any, index: int) -> BatchAssetOperation:
        if not isinstance(operation_param, dict):
            raise AssetCommandError(f"'operations[{index}]' 必须是对象")

        source_asset_path = str(operation_param.get("source_asset_path", "")).strip()
        if not source_asset_path:
            raise AssetCommandError(f"'operations[{index}].source_asset_path' 不能为空")

        source_asset = self._resolver.resolve_reference(source_asset_path)
        source_identity = source_asset.to_identity()
        destination_asset_path = str(operation_param.get("destination_asset_path", "")).strip()
        if not destination_asset_path:
            destination_path = str(operation_param.get("destination_path", "")).strip() or source_identity["package_path"]
            new_name = str(operation_param.get("new_name", "")).strip()
            if not new_name:
                raise AssetCommandError(
                    f"'operations[{index}]' 缺少 'destination_asset_path' 或 'new_name'"
                )
            self._validate_asset_name(new_name)
            destination_asset_path = f"{destination_path}/{new_name}"

        self._validate_destination_asset_path(destination_asset_path)
        return BatchAssetOperation(
            source_asset_path=source_identity["asset_path"],
            destination_asset_path=destination_asset_path,
            source_identity=source_identity,
        )

    def _build_batch_move_operation(self, asset_reference: Any, destination_path: str) -> BatchAssetOperation:
        normalized_reference = str(asset_reference).strip()
        if not normalized_reference:
            raise AssetCommandError("asset_paths 中包含空资源引用")

        source_asset = self._resolver.resolve_reference(normalized_reference)
        source_identity = source_asset.to_identity()
        destination_asset_path = f"{destination_path}/{source_identity['asset_name']}"
        self._validate_destination_asset_path(destination_asset_path)
        return BatchAssetOperation(
            source_asset_path=source_identity["asset_path"],
            destination_asset_path=destination_asset_path,
            source_identity=source_identity,
        )

    def _execute_batch_asset_rename(
        self,
        operation: BatchAssetOperation,
        overwrite: bool,
    ) -> Dict[str, Any]:
        source_asset_path = operation.source_asset_path
        destination_asset_path = operation.destination_asset_path
        if source_asset_path == destination_asset_path:
            result = dict(operation.source_identity)
            result.update(
                {
                    "success": True,
                    "source_asset_path": source_asset_path,
                    "destination_asset_path": destination_asset_path,
                    "overwrite": overwrite,
                    "no_op": True,
                }
            )
            return result

        if not unreal.EditorAssetLibrary.does_asset_exist(source_asset_path):
            raise AssetCommandError(f"源资源不存在: {source_asset_path}")

        if unreal.EditorAssetLibrary.does_asset_exist(destination_asset_path):
            if not overwrite:
                raise AssetCommandError(f"目标资源已存在: {destination_asset_path}")
            if not unreal.EditorAssetLibrary.delete_asset(destination_asset_path):
                raise AssetCommandError(f"删除已有目标资源失败: {destination_asset_path}")

        if not unreal.EditorAssetLibrary.rename_asset(source_asset_path, destination_asset_path):
            raise AssetCommandError(f"批量改名失败: {source_asset_path} -> {destination_asset_path}")

        result = self._identity_or_fallback(destination_asset_path)
        result.update(
            {
                "success": True,
                "source_asset_path": source_asset_path,
                "destination_asset_path": destination_asset_path,
                "overwrite": overwrite,
                "no_op": False,
            }
        )
        return result

    @staticmethod
    def _validate_destination_asset_path(destination_asset_path: str) -> None:
        normalized_path = destination_asset_path.strip()
        if not normalized_path:
            raise AssetCommandError("目标资源路径不能为空")
        if not normalized_path.startswith("/Game/"):
            raise AssetCommandError("目标资源路径必须以 /Game/ 开头")

        package_path, _, asset_name = normalized_path.rpartition("/")
        if not package_path or not asset_name:
            raise AssetCommandError(f"无效目标资源路径: {normalized_path}")
        AssetCommandDispatcher._validate_asset_name(asset_name)

    @staticmethod
    def _validate_asset_name(asset_name: str) -> None:
        normalized_name = asset_name.strip()
        if not normalized_name:
            raise AssetCommandError("资源名称不能为空")
        if "/" in normalized_name or "." in normalized_name:
            raise AssetCommandError(f"资源名称不合法: {normalized_name}")

    @staticmethod
    def _enumerate_assets(path: str, recursive_paths: bool) -> List[unreal.AssetData]:
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        if path:
            return list(registry.get_assets_by_path(path, recursive=recursive_paths) or [])
        return list(registry.get_all_assets() or [])

    @staticmethod
    def _get_object_path(asset_data: unreal.AssetData) -> str:
        object_path = getattr(asset_data, "object_path", None)
        if object_path:
            return str(object_path)
        package_name = str(asset_data.package_name)
        asset_name = str(asset_data.asset_name)
        return f"{package_name}.{asset_name}"

    @staticmethod
    def _matches_class_filter(asset_data: unreal.AssetData, class_name: str) -> bool:
        if not class_name:
            return True

        normalized_class_name = class_name.strip().casefold()
        class_path = asset_data.asset_class_path
        class_asset_name = str(class_path.asset_name)
        full_class_path = f"{class_path.package_name}.{class_path.asset_name}"
        return normalized_class_name in {
            class_asset_name.casefold(),
            full_class_path.casefold(),
            str(class_path).casefold(),
        }

    @staticmethod
    def _normalize_search_query_mode(query_mode: str) -> str:
        normalized_mode = (query_mode or "contains").strip().casefold()
        if not normalized_mode:
            return "contains"
        if normalized_mode not in {"contains", "wildcard", "regex"}:
            raise AssetCommandError(f"不支持的 query_mode: {query_mode}")
        return normalized_mode

    @staticmethod
    def _matches_search_query(query: str, query_mode: str, asset_name: str, object_path: str) -> bool:
        if query_mode == "contains":
            normalized_query = query.casefold()
            return normalized_query in asset_name.casefold() or normalized_query in object_path.casefold()

        if query_mode == "wildcard":
            normalized_query = query.casefold()
            return fnmatch.fnmatchcase(asset_name.casefold(), normalized_query) or fnmatch.fnmatchcase(
                object_path.casefold(),
                normalized_query,
            )

        try:
            pattern = re.compile(query, re.IGNORECASE)
        except re.error as exc:
            raise AssetCommandError(f"query_mode=regex 的表达式无效: {exc}") from exc
        return bool(pattern.search(asset_name) or pattern.search(object_path))

    @staticmethod
    def _collect_tags_payload(asset_data: unreal.AssetData, max_tag_count: int) -> Dict[str, Any]:
        tags_and_values = getattr(asset_data, "tags_and_values", None)
        if not tags_and_values:
            return {"tags": {}, "tag_count": 0}

        tag_keys = sorted(str(key) for key in tags_and_values.keys())
        limited_keys = tag_keys[:max_tag_count]
        tags = {key: str(tags_and_values[key]) for key in limited_keys}
        return {
            "tags": tags,
            "tag_count": len(limited_keys),
        }

    def _collect_dependency_paths(self, asset_data: unreal.AssetData, referencers: bool) -> List[str]:
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        package_name = asset_data.package_name
        dependency_options = self._build_dependency_options()
        names = (
            registry.get_referencers(package_name, dependency_options)
            if referencers
            else registry.get_dependencies(package_name, dependency_options)
        ) or []
        return sorted({str(name) for name in names if str(name)})

    @staticmethod
    def _build_dependency_options() -> unreal.AssetRegistryDependencyOptions:
        options = unreal.AssetRegistryDependencyOptions()
        options.include_hard_package_references = True
        options.include_soft_package_references = True
        options.include_searchable_names = True
        options.include_hard_management_references = True
        options.include_soft_management_references = True
        options.include_game_package_references = True
        options.include_editor_only_package_references = True
        return options

    @staticmethod
    def _read_metadata(
        asset_subsystem: unreal.EditorAssetSubsystem,
        asset_object: unreal.Object,
    ) -> Dict[str, str]:
        metadata_map = asset_subsystem.get_metadata_tag_values(asset_object)
        return {str(key): str(metadata_map[key]) for key in metadata_map.keys()}

    @staticmethod
    def _convert_metadata_value(value: Any) -> str:
        if value is None:
            return ""
        if isinstance(value, bool):
            return "true" if value else "false"
        if isinstance(value, (int, float)):
            return json.dumps(value, ensure_ascii=False)
        if isinstance(value, (list, dict)):
            return json.dumps(value, ensure_ascii=False, separators=(",", ":"))
        return str(value)

    @staticmethod
    def _require_string(params: Dict[str, Any], field_name: str) -> str:
        value = str(params.get(field_name, "")).strip()
        if not value:
            raise AssetCommandError(f"缺少 '{field_name}' 参数")
        return value

    @staticmethod
    def _normalize_existing_file_path(file_path: str) -> str:
        normalized_path = os.path.abspath(os.path.expanduser(file_path.strip()))
        if not os.path.isfile(normalized_path):
            raise AssetCommandError(f"导入源文件不存在: {normalized_path}")
        return normalized_path

    @staticmethod
    def _collect_directory_files(directory_path: str) -> set[str]:
        collected_files: set[str] = set()
        for current_root, _, filenames in os.walk(directory_path):
            for filename in filenames:
                collected_files.add(os.path.join(current_root, filename))
        return collected_files


def handle_asset_command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
    """Module entry point used by the C++ local Python bridge."""
    dispatcher = AssetCommandDispatcher()
    try:
        return dispatcher.handle(command_name, params)
    except AssetCommandError as exc:
        return {
            "success": False,
            "error": str(exc),
        }
