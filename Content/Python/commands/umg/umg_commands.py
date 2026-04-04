"""Local Python handlers for Widget Blueprint commands."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict, Optional

import unreal


class UMGCommandError(Exception):
    """Raised when a local UMG command cannot be completed."""


@dataclass
class ResolvedWidgetBlueprint:
    """Resolved Widget Blueprint asset metadata."""

    asset: unreal.WidgetBlueprint
    asset_name: str
    asset_path: str


class UMGAssetHelper:
    """Resolve Widget Blueprint assets and classes for local UMG commands."""

    DEFAULT_CREATE_PATH = "/Game/Widgets"
    DEFAULT_ROOT_CANVAS_NAME = "RootCanvas"

    def resolve_widget_blueprint(self, blueprint_reference: str) -> ResolvedWidgetBlueprint:
        normalized_reference = str(blueprint_reference or "").strip()
        if not normalized_reference:
            raise UMGCommandError("缺少 'blueprint_name' 参数")

        asset = self._load_widget_blueprint(normalized_reference)
        if asset is None:
            raise UMGCommandError(f"未找到 Widget Blueprint: {normalized_reference}")

        return ResolvedWidgetBlueprint(
            asset=asset,
            asset_name=asset.get_name(),
            asset_path=self._to_package_path(asset.get_path_name()),
        )

    def resolve_parent_class(self, parent_class_reference: str) -> type:
        normalized_reference = str(parent_class_reference or "").strip() or "UserWidget"
        resolved_class = None
        if normalized_reference.startswith("/"):
            resolved_class = unreal.load_class(None, normalized_reference)
        else:
            candidates = [normalized_reference]
            if not normalized_reference.startswith("U"):
                candidates.append(f"U{normalized_reference}")
            for candidate in candidates:
                resolved_class = getattr(unreal, candidate, None)
                if resolved_class is not None:
                    break
                for attribute_name in dir(unreal):
                    if attribute_name.casefold() == candidate.casefold():
                        resolved_class = getattr(unreal, attribute_name, None)
                        break
                if resolved_class is not None:
                    break

        if resolved_class is None:
            raise UMGCommandError(f"无效的 'parent_class': {normalized_reference}")
        if not issubclass(resolved_class, unreal.UserWidget):
            raise UMGCommandError(f"'parent_class' 不是 UserWidget 子类: {normalized_reference}")
        return resolved_class

    def normalize_package_path(self, package_path: str) -> str:
        normalized_path = str(package_path or "").strip() or self.DEFAULT_CREATE_PATH
        if not normalized_path.startswith("/"):
            normalized_path = f"/{normalized_path}"
        while normalized_path.endswith("/"):
            normalized_path = normalized_path[:-1]
        if not normalized_path.startswith("/Game"):
            raise UMGCommandError("'path' 必须以 /Game 开头")
        return normalized_path

    def ensure_root_canvas(self, widget_blueprint: unreal.WidgetBlueprint) -> unreal.CanvasPanel:
        existing_root = unreal.EditorUtilityLibrary.find_source_widget_by_name(
            widget_blueprint,
            self.DEFAULT_ROOT_CANVAS_NAME,
        )
        if existing_root is not None:
            if not isinstance(existing_root, unreal.CanvasPanel):
                raise UMGCommandError("RootCanvas 已存在，但不是 CanvasPanel")
            return existing_root

        try:
            created_root = unreal.EditorUtilityLibrary.add_source_widget(
                widget_blueprint,
                unreal.CanvasPanel,
                self.DEFAULT_ROOT_CANVAS_NAME,
                "",
            )
        except Exception as exc:
            raise UMGCommandError("Root Canvas Panel 不存在，且无法自动创建") from exc
        if created_root is None or not isinstance(created_root, unreal.CanvasPanel):
            raise UMGCommandError("创建 Root Canvas Panel 失败")
        return created_root

    def resolve_source_widget(self, widget_blueprint: unreal.WidgetBlueprint, widget_name: str) -> unreal.Widget:
        normalized_widget_name = str(widget_name or "").strip()
        if not normalized_widget_name:
            raise UMGCommandError("缺少控件名称参数")

        source_widget = unreal.EditorUtilityLibrary.find_source_widget_by_name(
            widget_blueprint,
            normalized_widget_name,
        )
        if source_widget is None:
            raise UMGCommandError(f"未找到控件: {normalized_widget_name}")
        return source_widget

    @staticmethod
    def resolve_generated_class(widget_blueprint: unreal.WidgetBlueprint) -> Any:
        generated_class = widget_blueprint.generated_class() if hasattr(widget_blueprint, "generated_class") else None
        if generated_class is None and hasattr(widget_blueprint, "get_editor_property"):
            try:
                generated_class = widget_blueprint.get_editor_property("generated_class")
            except Exception:
                generated_class = None
        if generated_class is None:
            raise UMGCommandError(f"无法解析 Widget Blueprint 的 GeneratedClass: {widget_blueprint.get_name()}")
        return generated_class

    @staticmethod
    def compile_and_save(widget_blueprint: unreal.WidgetBlueprint) -> None:
        try:
            unreal.BlueprintEditorLibrary.compile_blueprint(widget_blueprint)
        except Exception as exc:
            raise UMGCommandError(f"编译 Widget Blueprint 失败: {widget_blueprint.get_name()}") from exc

        if not unreal.EditorAssetLibrary.save_loaded_asset(widget_blueprint, only_if_is_dirty=False):
            raise UMGCommandError(f"保存 Widget Blueprint 失败: {widget_blueprint.get_name()}")

    @staticmethod
    def class_name(class_type: type) -> str:
        default_object = class_type.get_default_object() if hasattr(class_type, "get_default_object") else None
        resolved_class = default_object.get_class() if default_object and hasattr(default_object, "get_class") else None
        return resolved_class.get_name() if resolved_class is not None else str(class_type)

    def create_widget_blueprint(
        self,
        blueprint_name: str,
        parent_class: type,
        package_path: str,
    ) -> Dict[str, Any]:
        asset_path = f"{package_path}/{blueprint_name}"
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            raise UMGCommandError(f"Widget Blueprint '{blueprint_name}' 已存在")

        unreal.EditorAssetLibrary.make_directory(package_path)

        is_editor_utility = issubclass(parent_class, unreal.EditorUtilityWidget)
        factory_class = unreal.EditorUtilityWidgetBlueprintFactory if is_editor_utility else unreal.WidgetBlueprintFactory
        asset_class = unreal.EditorUtilityWidgetBlueprint if is_editor_utility else unreal.WidgetBlueprint

        factory = factory_class()
        factory.set_editor_property("parent_class", parent_class)
        created_asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            blueprint_name,
            package_path,
            asset_class,
            factory,
        )
        if created_asset is None:
            raise UMGCommandError("创建 Widget Blueprint 失败")

        widget_blueprint = self._cast_to_widget_blueprint(created_asset)
        if widget_blueprint is None:
            raise UMGCommandError("创建的资源不是 Widget Blueprint")

        self.ensure_root_canvas(widget_blueprint)
        self.compile_and_save(widget_blueprint)

        return {
            "success": True,
            "name": blueprint_name,
            "path": asset_path,
            "parent_class": self.class_name(parent_class),
            "blueprint_class": widget_blueprint.get_class().get_name(),
            "implementation": "local_python",
        }

    def _load_widget_blueprint(self, blueprint_reference: str) -> Optional[unreal.WidgetBlueprint]:
        candidate_paths = self._build_asset_candidates(blueprint_reference)
        for candidate_path in candidate_paths:
            widget_blueprint = self._cast_to_widget_blueprint(unreal.EditorAssetLibrary.load_asset(candidate_path))
            if widget_blueprint is not None:
                return widget_blueprint

        asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
        filter_args = unreal.ARFilter(
            class_names=["WidgetBlueprint", "EditorUtilityWidgetBlueprint"],
            package_paths=["/Game"],
            recursive_paths=True,
        )
        matched_assets = []
        for asset_data in asset_registry.get_assets(filter_args):
            asset_name = asset_data.asset_name
            package_name = asset_data.package_name
            object_path = f"{package_name}.{asset_name}"
            if (
                str(asset_name).casefold() == blueprint_reference.casefold()
                or str(package_name).casefold() == blueprint_reference.casefold()
                or str(object_path).casefold() == blueprint_reference.casefold()
            ):
                widget_blueprint = self._cast_to_widget_blueprint(asset_data.get_asset())
                if widget_blueprint is not None:
                    matched_assets.append(widget_blueprint)

        if len(matched_assets) == 1:
            return matched_assets[0]
        if len(matched_assets) > 1:
            sample_paths = ", ".join(asset.get_path_name() for asset in matched_assets[:5])
            raise UMGCommandError(f"Widget Blueprint 匹配到多个结果: {sample_paths}")
        return None

    @staticmethod
    def _build_asset_candidates(blueprint_reference: str) -> list[str]:
        normalized_reference = blueprint_reference.strip()
        candidates = []
        if normalized_reference.startswith("/"):
            candidates.append(normalized_reference)
            package_path = normalized_reference.split(".", 1)[0]
            if "." not in normalized_reference:
                asset_name = package_path.rsplit("/", 1)[-1]
                candidates.append(f"{package_path}.{asset_name}")
        return candidates

    @staticmethod
    def _cast_to_widget_blueprint(asset_object: Any) -> Optional[unreal.WidgetBlueprint]:
        if asset_object is None:
            return None
        cast_result = unreal.EditorUtilityLibrary.cast_to_widget_blueprint(asset_object)
        if not isinstance(cast_result, tuple) or len(cast_result) != 2:
            return None
        _, widget_blueprint = cast_result
        return widget_blueprint

    @staticmethod
    def _to_package_path(object_path: str) -> str:
        return object_path.split(".", 1)[0] if "." in object_path else object_path


class UMGWidgetHelper:
    """Edit Widget Blueprint source widgets through EditorUtilityLibrary."""

    def __init__(self, asset_helper: UMGAssetHelper) -> None:
        self._asset_helper = asset_helper

    def add_text_block_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        root_canvas = self._asset_helper.ensure_root_canvas(resolved_blueprint.asset)
        widget_name = self._require_widget_name(params, "text_block_name")
        initial_text = str(params.get("text", "New Text Block"))

        try:
            text_block = unreal.EditorUtilityLibrary.add_source_widget(
                resolved_blueprint.asset,
                unreal.TextBlock,
                widget_name,
                root_canvas.get_name(),
            )
        except Exception as exc:
            raise UMGCommandError("创建 Text Block 控件失败") from exc
        if text_block is None:
            raise UMGCommandError("创建 Text Block 控件失败")

        text_block.set_text(initial_text)

        font = text_block.get_editor_property("font")
        font.size = int(params.get("font_size", 12))
        text_block.set_editor_property("font", font)
        text_block.set_color_and_opacity(self._make_slate_color(params.get("color"), unreal.LinearColor.WHITE))
        self._apply_canvas_slot_layout(text_block, params)

        self._asset_helper.compile_and_save(resolved_blueprint.asset)
        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "widget_name": widget_name,
            "text": initial_text,
            "implementation": "local_python",
        }

    def add_button_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        root_canvas = self._asset_helper.ensure_root_canvas(resolved_blueprint.asset)
        widget_name = self._require_widget_name(params, "button_name")
        button_text = str(params.get("text", ""))

        try:
            button = unreal.EditorUtilityLibrary.add_source_widget(
                resolved_blueprint.asset,
                unreal.Button,
                widget_name,
                root_canvas.get_name(),
            )
        except Exception as exc:
            raise UMGCommandError("创建 Button 控件失败") from exc
        if button is None:
            raise UMGCommandError("创建 Button 控件失败")

        button.set_color_and_opacity(self._make_linear_color(params.get("color"), unreal.LinearColor.WHITE))
        button.set_background_color(
            self._make_linear_color(params.get("background_color"), unreal.LinearColor(0.1, 0.1, 0.1, 1.0))
        )
        self._apply_canvas_slot_layout(button, params)

        try:
            text_block = unreal.EditorUtilityLibrary.add_source_widget(
                resolved_blueprint.asset,
                unreal.TextBlock,
                f"{widget_name}_Text",
                widget_name,
            )
        except Exception as exc:
            raise UMGCommandError("为 Button 创建内部 TextBlock 失败") from exc
        if text_block is None:
            raise UMGCommandError("为 Button 创建内部 TextBlock 失败")

        text_block.set_text(button_text)
        font = text_block.get_editor_property("font")
        font.size = int(params.get("font_size", 12))
        text_block.set_editor_property("font", font)
        text_block.set_color_and_opacity(self._make_slate_color(params.get("color"), unreal.LinearColor.WHITE))

        self._asset_helper.compile_and_save(resolved_blueprint.asset)
        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "widget_name": widget_name,
            "text": button_text,
            "implementation": "local_python",
        }

    def add_image_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._create_widget_on_root_canvas(
            resolved_blueprint,
            params,
            "image_name",
            unreal.Image,
            "Image",
            lambda widget, widget_params: widget.set_color_and_opacity(
                self._make_linear_color(widget_params.get("color"), unreal.LinearColor.WHITE)
            ),
        )

    def add_border_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._create_widget_on_root_canvas(
            resolved_blueprint,
            params,
            "border_name",
            unreal.Border,
            "Border",
            lambda widget, widget_params: self._configure_border(widget, widget_params),
        )

    def add_canvas_panel_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._create_widget_on_root_canvas(
            resolved_blueprint,
            params,
            "canvas_panel_name",
            unreal.CanvasPanel,
            "CanvasPanel",
        )

    def add_horizontal_box_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._create_widget_on_root_canvas(
            resolved_blueprint,
            params,
            "horizontal_box_name",
            unreal.HorizontalBox,
            "HorizontalBox",
        )

    def add_vertical_box_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._create_widget_on_root_canvas(
            resolved_blueprint,
            params,
            "vertical_box_name",
            unreal.VerticalBox,
            "VerticalBox",
        )

    def add_overlay_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._create_widget_on_root_canvas(
            resolved_blueprint,
            params,
            "overlay_name",
            unreal.Overlay,
            "Overlay",
        )

    def add_scroll_box_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._create_widget_on_root_canvas(
            resolved_blueprint,
            params,
            "scroll_box_name",
            unreal.ScrollBox,
            "ScrollBox",
        )

    def add_size_box_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._create_widget_on_root_canvas(
            resolved_blueprint,
            params,
            "size_box_name",
            unreal.SizeBox,
            "SizeBox",
            lambda widget, widget_params: self._configure_size_box(widget, widget_params),
        )

    def add_spacer_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._create_widget_on_root_canvas(
            resolved_blueprint,
            params,
            "spacer_name",
            unreal.Spacer,
            "Spacer",
            lambda widget, widget_params: widget.set_size(self._make_vector2d(widget_params.get("size"), unreal.Vector2D(32.0, 32.0))),
        )

    def add_progress_bar_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        percent = self._clamp_unit_float(params.get("percent"), 0.5)
        response = self._create_widget_on_root_canvas(
            resolved_blueprint,
            params,
            "progress_bar_name",
            unreal.ProgressBar,
            "ProgressBar",
            lambda widget, widget_params: self._configure_progress_bar(widget, widget_params, percent),
        )
        response["percent"] = percent
        return response

    def add_slider_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        slider_value = self._clamp_unit_float(params.get("value"), 0.0)
        response = self._create_widget_on_root_canvas(
            resolved_blueprint,
            params,
            "slider_name",
            unreal.Slider,
            "Slider",
            lambda widget, widget_params: widget.set_value(slider_value),
        )
        response["value"] = slider_value
        return response

    def add_check_box_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        is_checked = bool(params.get("is_checked", False))
        response = self._create_widget_on_root_canvas(
            resolved_blueprint,
            params,
            "check_box_name",
            unreal.CheckBox,
            "CheckBox",
            lambda widget, widget_params: widget.set_is_checked(is_checked),
        )
        response["is_checked"] = is_checked
        return response

    def add_editable_text_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        initial_text = str(params.get("text", ""))
        hint_text = str(params.get("hint_text", ""))
        is_read_only = bool(params.get("is_read_only", False))
        response = self._create_widget_on_root_canvas(
            resolved_blueprint,
            params,
            "editable_text_name",
            unreal.EditableText,
            "EditableText",
            lambda widget, widget_params: self._configure_editable_text(widget, initial_text, hint_text, is_read_only),
        )
        response["text"] = initial_text
        response["is_read_only"] = is_read_only
        return response

    def add_rich_text_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        initial_text = str(params.get("text", ""))
        response = self._create_widget_on_root_canvas(
            resolved_blueprint,
            params,
            "rich_text_name",
            unreal.RichTextBlock,
            "RichTextBlock",
            lambda widget, widget_params: self._configure_rich_text(widget, widget_params, initial_text),
        )
        response["text"] = initial_text
        return response

    def add_multi_line_text_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        initial_text = str(params.get("text", ""))
        hint_text = str(params.get("hint_text", ""))
        is_read_only = bool(params.get("is_read_only", False))
        response = self._create_widget_on_root_canvas(
            resolved_blueprint,
            params,
            "multi_line_text_name",
            unreal.MultiLineEditableTextBox,
            "MultiLineEditableTextBox",
            lambda widget, widget_params: self._configure_multi_line_text(widget, initial_text, hint_text, is_read_only),
        )
        response["text"] = initial_text
        response["hint_text"] = hint_text
        response["is_read_only"] = is_read_only
        return response

    def add_named_slot_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._create_widget_on_root_canvas(
            resolved_blueprint,
            params,
            "named_slot_name",
            unreal.NamedSlot,
            "NamedSlot",
        )

    def add_list_view_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._create_widget_on_root_canvas(
            resolved_blueprint,
            params,
            "list_view_name",
            unreal.ListView,
            "ListView",
        )

    def add_tile_view_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._create_widget_on_root_canvas(
            resolved_blueprint,
            params,
            "tile_view_name",
            unreal.TileView,
            "TileView",
        )

    def add_tree_view_to_widget(self, resolved_blueprint: ResolvedWidgetBlueprint, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._create_widget_on_root_canvas(
            resolved_blueprint,
            params,
            "tree_view_name",
            unreal.TreeView,
            "TreeView",
        )

    def remove_widget_from_blueprint(
        self,
        resolved_blueprint: ResolvedWidgetBlueprint,
        params: Dict[str, Any],
    ) -> Dict[str, Any]:
        widget_name = self._require_existing_widget_name(params)
        if widget_name == self._asset_helper.DEFAULT_ROOT_CANVAS_NAME:
            raise UMGCommandError("不允许删除 RootCanvas")

        target_widget = self._asset_helper.resolve_source_widget(resolved_blueprint.asset, widget_name)
        parent_widget = target_widget.get_parent() if hasattr(target_widget, "get_parent") else None
        if parent_widget is None:
            raise UMGCommandError(f"控件没有父节点，无法删除: {widget_name}")

        removed = parent_widget.remove_child(target_widget) if hasattr(parent_widget, "remove_child") else False
        if not removed:
            raise UMGCommandError(f"删除控件失败: {widget_name}")

        self._asset_helper.compile_and_save(resolved_blueprint.asset)
        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "widget_name": widget_name,
            "removed": True,
            "implementation": "local_python",
        }

    def set_widget_slot_layout(
        self,
        resolved_blueprint: ResolvedWidgetBlueprint,
        params: Dict[str, Any],
    ) -> Dict[str, Any]:
        widget_name = self._require_existing_widget_name(params)
        target_widget = self._asset_helper.resolve_source_widget(resolved_blueprint.asset, widget_name)
        canvas_slot = self._require_canvas_slot(target_widget)

        self._apply_canvas_slot_layout(target_widget, params)

        if "alignment" in params:
            canvas_slot.set_alignment(
                self._make_vector2d(params.get("alignment"), canvas_slot.get_alignment())
            )

        if "anchors" in params:
            anchors_value = params.get("anchors")
            if not isinstance(anchors_value, list) or len(anchors_value) < 4:
                raise UMGCommandError("'anchors' 必须是长度为 4 的数组")
            anchors = unreal.Anchors()
            anchors.minimum = unreal.Vector2D(float(anchors_value[0]), float(anchors_value[1]))
            anchors.maximum = unreal.Vector2D(float(anchors_value[2]), float(anchors_value[3]))
            canvas_slot.set_anchors(anchors)

        if "offsets" in params:
            offsets_value = params.get("offsets")
            if not isinstance(offsets_value, list) or len(offsets_value) < 4:
                raise UMGCommandError("'offsets' 必须是长度为 4 的数组")
            offsets = unreal.Margin()
            offsets.left = float(offsets_value[0])
            offsets.top = float(offsets_value[1])
            offsets.right = float(offsets_value[2])
            offsets.bottom = float(offsets_value[3])
            canvas_slot.set_offsets(offsets)

        if "auto_size" in params:
            canvas_slot.set_auto_size(bool(params.get("auto_size")))

        if "z_order" in params:
            try:
                z_order = int(params.get("z_order"))
            except (TypeError, ValueError) as exc:
                raise UMGCommandError("'z_order' 必须是整数") from exc
            canvas_slot.set_z_order(z_order)

        self._asset_helper.compile_and_save(resolved_blueprint.asset)
        current_position = canvas_slot.get_position()
        current_size = canvas_slot.get_size()
        current_alignment = canvas_slot.get_alignment()
        current_anchors = canvas_slot.get_anchors()
        current_offsets = canvas_slot.get_offsets()
        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "widget_name": widget_name,
            "position": [current_position.x, current_position.y],
            "size": [current_size.x, current_size.y],
            "alignment": [current_alignment.x, current_alignment.y],
            "anchors": [
                current_anchors.minimum.x,
                current_anchors.minimum.y,
                current_anchors.maximum.x,
                current_anchors.maximum.y,
            ],
            "offsets": [
                current_offsets.left,
                current_offsets.top,
                current_offsets.right,
                current_offsets.bottom,
            ],
            "auto_size": canvas_slot.get_auto_size(),
            "z_order": canvas_slot.get_z_order(),
            "implementation": "local_python",
        }

    def set_widget_visibility(
        self,
        resolved_blueprint: ResolvedWidgetBlueprint,
        params: Dict[str, Any],
    ) -> Dict[str, Any]:
        widget_name = self._require_existing_widget_name(params)
        visibility_name = str(params.get("visibility", "")).strip()
        if not visibility_name:
            raise UMGCommandError("缺少 'visibility' 参数")

        target_widget = self._asset_helper.resolve_source_widget(resolved_blueprint.asset, widget_name)
        resolved_visibility = self._parse_visibility(visibility_name)
        target_widget.set_visibility(resolved_visibility)

        self._asset_helper.compile_and_save(resolved_blueprint.asset)
        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "widget_name": widget_name,
            "visibility": self._visibility_to_name(target_widget.get_visibility()),
            "implementation": "local_python",
        }

    def set_widget_brush(
        self,
        resolved_blueprint: ResolvedWidgetBlueprint,
        params: Dict[str, Any],
    ) -> Dict[str, Any]:
        widget_name = self._require_existing_widget_name(params)
        target_widget = self._asset_helper.resolve_source_widget(resolved_blueprint.asset, widget_name)

        resource_type, resource_object = self._resolve_brush_resource(params)
        if resource_object is None and "tint_color" not in params and "image_size" not in params:
            raise UMGCommandError(
                "至少需要提供 brush_asset_path、texture_asset_path、material_asset_path、tint_color、image_size 之一"
            )

        if isinstance(target_widget, unreal.Image):
            self._apply_image_brush(target_widget, resource_type, resource_object, params)
        elif isinstance(target_widget, unreal.Border):
            self._apply_border_brush(target_widget, resource_type, resource_object, params)
        else:
            raise UMGCommandError(
                f"控件类型暂不支持 set_widget_brush: {target_widget.get_class().get_name()}，当前仅支持 Image、Border"
            )

        self._asset_helper.compile_and_save(resolved_blueprint.asset)

        result = {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "widget_name": widget_name,
            "widget_class": target_widget.get_class().get_name(),
            "implementation": "local_python",
        }
        if resource_type:
            result["resource_type"] = resource_type
        if resource_object is not None and hasattr(resource_object, "get_path_name"):
            result["resource_path"] = resource_object.get_path_name()
        if "tint_color" in params:
            tint_color = self._make_linear_color(params.get("tint_color"), unreal.LinearColor.WHITE)
            result["tint_color"] = [tint_color.r, tint_color.g, tint_color.b, tint_color.a]
        if isinstance(target_widget, unreal.Image):
            if "image_size" in params:
                result["image_size"] = self._normalize_image_size_param(params.get("image_size"))
            else:
                current_brush = target_widget.get_editor_property("brush")
                result["image_size"] = self._slate_vector2d_to_list(current_brush.image_size)
        return result

    def set_widget_style(
        self,
        resolved_blueprint: ResolvedWidgetBlueprint,
        params: Dict[str, Any],
    ) -> Dict[str, Any]:
        widget_name = self._require_existing_widget_name(params)
        target_widget = self._asset_helper.resolve_source_widget(resolved_blueprint.asset, widget_name)
        style_params = params.get("style")
        if not isinstance(style_params, dict) or not style_params:
            raise UMGCommandError("'style' 必须是非空对象")

        updated_fields: list[str] = []
        if isinstance(target_widget, unreal.Button):
            updated_fields = self._apply_button_style(target_widget, style_params)
        elif isinstance(target_widget, unreal.ProgressBar):
            updated_fields = self._apply_progress_bar_style(target_widget, style_params)
        elif isinstance(target_widget, unreal.CheckBox):
            updated_fields = self._apply_check_box_style(target_widget, style_params)
        elif isinstance(target_widget, unreal.Border):
            updated_fields = self._apply_border_style(target_widget, style_params)
        elif isinstance(target_widget, unreal.Image):
            updated_fields = self._apply_image_style(target_widget, style_params)
        else:
            raise UMGCommandError(
                f"控件类型暂不支持 set_widget_style: {target_widget.get_class().get_name()}，当前支持 Button、ProgressBar、CheckBox、Border、Image"
            )

        self._asset_helper.compile_and_save(resolved_blueprint.asset)
        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "widget_name": widget_name,
            "widget_class": target_widget.get_class().get_name(),
            "updated_fields": updated_fields,
            "implementation": "local_python",
        }

    def add_widget_to_viewport(
        self,
        resolved_blueprint: ResolvedWidgetBlueprint,
        params: Dict[str, Any],
    ) -> Dict[str, Any]:
        game_world = self._get_game_world()
        generated_class = self._asset_helper.resolve_generated_class(resolved_blueprint.asset)

        try:
            z_order = int(params.get("z_order", 0))
        except (TypeError, ValueError) as exc:
            raise UMGCommandError("'z_order' 必须是整数") from exc

        instance_name = str(params.get("instance_name", "")).strip()
        requested_player_index = params.get("player_index", None)
        widget_instance = (
            unreal.new_object(generated_class, outer=game_world, name=instance_name)
            if instance_name
            else unreal.new_object(generated_class, outer=game_world)
        )
        if widget_instance is None or not isinstance(widget_instance, unreal.UserWidget):
            raise UMGCommandError(f"创建 Widget 实例失败: {resolved_blueprint.asset_name}")

        owning_player = None
        screen_mode = "viewport"
        resolved_player_index = None
        if requested_player_index is not None:
            resolved_player_index = self._normalize_player_index(requested_player_index)
            owning_player = self._resolve_player_controller(game_world, resolved_player_index)
            widget_instance.set_owning_player(owning_player)
            widget_instance.add_to_player_screen(z_order)
            screen_mode = "player_screen"
        else:
            widget_instance.add_to_viewport(z_order)

        if not widget_instance.is_in_viewport():
            raise UMGCommandError(f"Widget 未成功添加到视口: {resolved_blueprint.asset_name}")

        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "asset_path": resolved_blueprint.asset_path,
            "class_path": generated_class.get_path_name(),
            "instance_name": widget_instance.get_name(),
            "instance_path": widget_instance.get_path_name(),
            "world_name": game_world.get_name(),
            "z_order": z_order,
            "screen_mode": screen_mode,
            "player_index": resolved_player_index,
            "owning_player_name": owning_player.get_name() if owning_player else "",
            "owning_player_path": owning_player.get_path_name() if owning_player else "",
            "implementation": "local_python",
        }

    def remove_widget_from_viewport(self, params: Dict[str, Any]) -> Dict[str, Any]:
        game_world = self._get_game_world()
        widget_instance = self._resolve_runtime_widget_instance(game_world, params)
        widget_path = widget_instance.get_path_name()
        widget_name = widget_instance.get_name()
        widget_class = widget_instance.get_class().get_name() if hasattr(widget_instance, "get_class") else "UserWidget"
        was_in_viewport = bool(widget_instance.is_in_viewport()) if hasattr(widget_instance, "is_in_viewport") else False

        widget_instance.remove_from_parent()

        removed = not widget_instance.is_in_viewport() if hasattr(widget_instance, "is_in_viewport") else True
        if not removed:
            raise UMGCommandError(f"移除 Widget 实例失败: {widget_name}")

        return {
            "success": True,
            "instance_name": widget_name,
            "instance_path": widget_path,
            "widget_class": widget_class,
            "world_name": game_world.get_name(),
            "was_in_viewport": was_in_viewport,
            "removed": True,
            "implementation": "local_python",
        }

    @staticmethod
    def _normalize_player_index(player_index_value: Any) -> int:
        try:
            player_index = int(player_index_value)
        except (TypeError, ValueError) as exc:
            raise UMGCommandError("'player_index' 必须是整数") from exc

        if player_index < 0:
            raise UMGCommandError("'player_index' 不能小于 0")
        return player_index

    @staticmethod
    def _resolve_player_controller(game_world: unreal.World, player_index: int) -> unreal.PlayerController:
        owning_player = unreal.GameplayStatics.get_player_controller(game_world, player_index)
        if owning_player is None:
            raise UMGCommandError(f"未找到本地 PlayerController: {player_index}")
        return owning_player

    def _resolve_runtime_widget_instance(self, game_world: unreal.World, params: Dict[str, Any]) -> unreal.UserWidget:
        instance_path = str(params.get("instance_path", "")).strip()
        instance_name = str(params.get("instance_name", "")).strip()
        blueprint_name = str(params.get("blueprint_name", "") or params.get("widget_name", "")).strip()
        generated_class = self._try_resolve_runtime_widget_class(blueprint_name)

        if instance_path:
            widget_instance = self._find_widget_by_path(instance_path)
            if widget_instance is None:
                raise UMGCommandError(f"未找到 Widget 实例: {instance_path}")
            self._validate_runtime_widget(widget_instance, game_world, generated_class)
            return widget_instance

        if instance_name:
            matches: list[unreal.UserWidget] = []
            for widget_instance in unreal.ObjectIterator(unreal.UserWidget):
                if widget_instance is None:
                    continue
                if widget_instance.get_name() != instance_name:
                    continue
                if not self._is_widget_in_world(widget_instance, game_world):
                    continue
                if generated_class is not None and not self._is_widget_of_class(widget_instance, generated_class):
                    continue
                matches.append(widget_instance)

            if not matches:
                raise UMGCommandError(f"未找到 Widget 实例: {instance_name}")
            if len(matches) > 1:
                sample_paths = ", ".join(widget.get_path_name() for widget in matches[:5])
                raise UMGCommandError(f"找到多个同名 Widget 实例，请改传 instance_path: {sample_paths}")
            return matches[0]

        raise UMGCommandError("缺少 'instance_path' 或 'instance_name' 参数")

    def _try_resolve_runtime_widget_class(self, blueprint_name: str) -> Optional[Any]:
        if not blueprint_name:
            return None
        try:
            resolved_blueprint = self._asset_helper.resolve_widget_blueprint(blueprint_name)
            return self._asset_helper.resolve_generated_class(resolved_blueprint.asset)
        except Exception:
            return None

    @staticmethod
    def _find_widget_by_path(instance_path: str) -> Optional[unreal.UserWidget]:
        find_object = getattr(unreal, "find_object", None)
        if callable(find_object):
            try:
                widget_instance = find_object(None, instance_path)
                if isinstance(widget_instance, unreal.UserWidget):
                    return widget_instance
            except Exception:
                pass

        load_object = getattr(unreal, "load_object", None)
        if callable(load_object):
            try:
                widget_instance = load_object(None, instance_path)
                if isinstance(widget_instance, unreal.UserWidget):
                    return widget_instance
            except Exception:
                pass
        return None

    def _validate_runtime_widget(
        self,
        widget_instance: unreal.UserWidget,
        game_world: unreal.World,
        generated_class: Optional[Any],
    ) -> None:
        if not self._is_widget_in_world(widget_instance, game_world):
            raise UMGCommandError(f"Widget 不属于当前运行世界: {widget_instance.get_path_name()}")
        if generated_class is not None and not self._is_widget_of_class(widget_instance, generated_class):
            raise UMGCommandError(f"Widget 实例不属于目标 Widget Blueprint: {widget_instance.get_path_name()}")

    @staticmethod
    def _is_widget_in_world(widget_instance: unreal.UserWidget, game_world: unreal.World) -> bool:
        widget_world = widget_instance.get_world() if hasattr(widget_instance, "get_world") else None
        if widget_world is not None:
            return widget_world == game_world

        widget_outer = widget_instance.get_outer() if hasattr(widget_instance, "get_outer") else None
        return widget_outer == game_world

    @staticmethod
    def _is_widget_of_class(widget_instance: unreal.UserWidget, generated_class: Any) -> bool:
        widget_class = widget_instance.get_class() if hasattr(widget_instance, "get_class") else None
        return widget_class == generated_class

    def _create_widget_on_root_canvas(
        self,
        resolved_blueprint: ResolvedWidgetBlueprint,
        params: Dict[str, Any],
        widget_field_name: str,
        widget_class: type,
        widget_type_name: str,
        configure_widget: Optional[Any] = None,
    ) -> Dict[str, Any]:
        root_canvas = self._asset_helper.ensure_root_canvas(resolved_blueprint.asset)
        widget_name = self._require_widget_name(params, widget_field_name)

        try:
            created_widget = unreal.EditorUtilityLibrary.add_source_widget(
                resolved_blueprint.asset,
                widget_class,
                widget_name,
                root_canvas.get_name(),
            )
        except Exception as exc:
            raise UMGCommandError(f"创建 {widget_type_name} 控件失败") from exc
        if created_widget is None:
            raise UMGCommandError(f"创建 {widget_type_name} 控件失败")

        if configure_widget is not None:
            configure_widget(created_widget, params)
        self._apply_canvas_slot_layout(created_widget, params)
        self._asset_helper.compile_and_save(resolved_blueprint.asset)

        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "widget_name": widget_name,
            "widget_type": widget_type_name,
            "implementation": "local_python",
        }

    @staticmethod
    def _require_widget_name(params: Dict[str, Any], legacy_field: str) -> str:
        widget_name = ""
        if "blueprint_name" in params:
            widget_name = str(params.get("widget_name", "") or params.get(legacy_field, "")).strip()
        else:
            widget_name = str(params.get(legacy_field, "")).strip()
        if not widget_name:
            raise UMGCommandError(f"缺少 '{legacy_field}' 参数")
        return widget_name

    def _require_existing_widget_name(self, params: Dict[str, Any]) -> str:
        field_candidates = [
            "widget_name",
            "target_widget_name",
            "child_widget_name",
            "widget_component_name",
        ]
        for field_name in field_candidates:
            widget_name = str(params.get(field_name, "")).strip()
            if widget_name:
                return widget_name
        raise UMGCommandError("缺少控件名称参数")

    @staticmethod
    def _require_canvas_slot(widget: unreal.Widget) -> unreal.CanvasPanelSlot:
        panel_slot = getattr(widget, "slot", None)
        if panel_slot is None or not isinstance(panel_slot, unreal.CanvasPanelSlot):
            raise UMGCommandError("目标控件没有可写的 CanvasPanelSlot")
        return panel_slot

    @staticmethod
    def _apply_canvas_slot_layout(widget: unreal.Widget, params: Dict[str, Any]) -> None:
        panel_slot = UMGWidgetHelper._require_canvas_slot(widget)

        position = params.get("position")
        if isinstance(position, list) and len(position) >= 2:
            panel_slot.set_position(unreal.Vector2D(float(position[0]), float(position[1])))
        else:
            panel_slot.set_position(unreal.Vector2D(0.0, 0.0))

        if "size" in params:
            size = params.get("size")
            if not isinstance(size, list) or len(size) < 2:
                raise UMGCommandError("'size' 必须是长度为 2 的数组")
            panel_slot.set_size(unreal.Vector2D(float(size[0]), float(size[1])))

    @staticmethod
    def _make_vector2d(value: Any, default_value: unreal.Vector2D) -> unreal.Vector2D:
        if not isinstance(value, list) or len(value) < 2:
            return default_value
        return unreal.Vector2D(float(value[0]), float(value[1]))

    @staticmethod
    def _make_linear_color(value: Any, default_value: unreal.LinearColor) -> unreal.LinearColor:
        if not isinstance(value, list) or len(value) < 4:
            return default_value
        return unreal.LinearColor(float(value[0]), float(value[1]), float(value[2]), float(value[3]))

    @staticmethod
    def _clamp_unit_float(value: Any, default_value: float) -> float:
        try:
            numeric_value = float(value)
        except (TypeError, ValueError):
            numeric_value = default_value
        return max(0.0, min(1.0, numeric_value))

    def _make_slate_color(self, value: Any, default_value: unreal.LinearColor) -> unreal.SlateColor:
        return unreal.SlateColor(specified_color=self._make_linear_color(value, default_value))

    @staticmethod
    def _slate_vector2d_to_list(value: Any) -> list[float]:
        if hasattr(value, "to_tuple"):
            tuple_value = value.to_tuple()
            if isinstance(tuple_value, tuple) and len(tuple_value) >= 2:
                return [float(tuple_value[0]), float(tuple_value[1])]
        x_value = value.get_editor_property("x") if hasattr(value, "get_editor_property") else None
        y_value = value.get_editor_property("y") if hasattr(value, "get_editor_property") else None
        if x_value is not None and y_value is not None:
            return [float(x_value), float(y_value)]
        return [0.0, 0.0]

    @staticmethod
    def _normalize_image_size_param(value: Any) -> list[float]:
        if not isinstance(value, list) or len(value) < 2:
            raise UMGCommandError("'image_size' 必须是长度为 2 的数组")
        return [float(value[0]), float(value[1])]

    @staticmethod
    def _parse_visibility(visibility_name: str) -> unreal.SlateVisibility:
        normalized_name = visibility_name.strip().casefold().replace("-", "").replace("_", "").replace(" ", "")
        visibility_mapping = {
            "visible": unreal.SlateVisibility.VISIBLE,
            "collapsed": unreal.SlateVisibility.COLLAPSED,
            "hidden": unreal.SlateVisibility.HIDDEN,
            "hittestinvisible": unreal.SlateVisibility.HIT_TEST_INVISIBLE,
            "nothittestableselfandallchildren": unreal.SlateVisibility.HIT_TEST_INVISIBLE,
            "selfhittestinvisible": unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE,
            "nothittestableselfonly": unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE,
        }
        resolved_visibility = visibility_mapping.get(normalized_name)
        if resolved_visibility is None:
            raise UMGCommandError(
                "不支持的 'visibility'，当前支持 Visible、Collapsed、Hidden、HitTestInvisible、SelfHitTestInvisible"
            )
        return resolved_visibility

    @staticmethod
    def _visibility_to_name(visibility: Any) -> str:
        visibility_name = getattr(visibility, "name", None)
        if isinstance(visibility_name, str) and visibility_name:
            return visibility_name
        if callable(visibility_name):
            try:
                return str(visibility_name())
            except TypeError:
                pass
        raw_value = str(visibility)
        if raw_value.startswith("<SlateVisibility.") and ":" in raw_value:
            return raw_value.split(".", 1)[1].split(":", 1)[0]
        return raw_value

    @staticmethod
    def _load_brush_resource(asset_path: str, field_name: str) -> Any:
        normalized_asset_path = str(asset_path or "").strip()
        if not normalized_asset_path:
            raise UMGCommandError(f"缺少 '{field_name}' 参数")

        candidate_paths = [normalized_asset_path]
        if normalized_asset_path.startswith("/") and "." not in normalized_asset_path:
            asset_name = normalized_asset_path.rsplit("/", 1)[-1]
            if asset_name:
                candidate_paths.append(f"{normalized_asset_path}.{asset_name}")

        asset_object = None
        for candidate_path in candidate_paths:
            asset_object = unreal.EditorAssetLibrary.load_asset(candidate_path)
            if asset_object is None and hasattr(unreal, "load_asset"):
                asset_object = unreal.load_asset(candidate_path)
            if asset_object is None and hasattr(unreal, "load_object"):
                asset_object = unreal.load_object(None, candidate_path)
            if asset_object is not None:
                break
        if asset_object is None:
            raise UMGCommandError(f"未找到资源: {normalized_asset_path}")
        return asset_object

    def _resolve_brush_resource(self, params: Dict[str, Any]) -> tuple[str, Any]:
        resource_fields = [
            ("brush_asset", "brush_asset_path"),
            ("texture", "texture_asset_path"),
            ("material", "material_asset_path"),
        ]
        resolved_entries = []
        for resource_type, field_name in resource_fields:
            if str(params.get(field_name, "")).strip():
                resolved_entries.append((resource_type, self._load_brush_resource(params.get(field_name), field_name)))

        if len(resolved_entries) > 1:
            raise UMGCommandError("brush_asset_path、texture_asset_path、material_asset_path 只能三选一")
        if len(resolved_entries) == 1:
            return resolved_entries[0]
        return "", None

    def _apply_image_brush(
        self,
        image_widget: unreal.Image,
        resource_type: str,
        resource_object: Any,
        params: Dict[str, Any],
    ) -> None:
        match_size = bool(params.get("match_size", True))
        if resource_type == "texture":
            try:
                image_widget.set_brush_from_texture(resource_object, match_size)
            except TypeError:
                image_widget.set_brush_from_texture(resource_object)
        elif resource_type == "material":
            image_widget.set_brush_from_material(resource_object)
        elif resource_type == "brush_asset":
            image_widget.set_brush_from_asset(resource_object)

        if "tint_color" in params:
            image_widget.set_brush_tint_color(
                self._make_slate_color(params.get("tint_color"), unreal.LinearColor.WHITE)
            )

        if "image_size" in params:
            image_size = self._normalize_image_size_param(params.get("image_size"))
            image_widget.set_desired_size_override(unreal.Vector2D(image_size[0], image_size[1]))

    def _apply_border_brush(
        self,
        border_widget: unreal.Border,
        resource_type: str,
        resource_object: Any,
        params: Dict[str, Any],
    ) -> None:
        if resource_type == "texture":
            border_widget.set_brush_from_texture(resource_object)
        elif resource_type == "material":
            border_widget.set_brush_from_material(resource_object)
        elif resource_type == "brush_asset":
            border_widget.set_brush_from_asset(resource_object)

        if "tint_color" in params:
            border_widget.set_brush_color(
                self._make_linear_color(params.get("tint_color"), unreal.LinearColor.WHITE)
            )
        if "image_size" in params:
            raise UMGCommandError("Border 当前不支持 'image_size'，如需样式尺寸请后续走 set_widget_style")

    def _apply_button_style(self, button_widget: unreal.Button, style_params: Dict[str, Any]) -> list[str]:
        widget_style = button_widget.get_editor_property("widget_style").copy()
        updated_fields: list[str] = []
        brush_fields = ["normal", "hovered", "pressed", "disabled"]
        color_fields = [
            "normal_foreground",
            "hovered_foreground",
            "pressed_foreground",
            "disabled_foreground",
        ]

        for field_name in brush_fields:
            if field_name in style_params:
                brush_value = self._update_slate_brush(
                    widget_style.get_editor_property(field_name),
                    style_params.get(field_name),
                    field_name,
                )
                widget_style.set_editor_property(field_name, brush_value)
                updated_fields.append(field_name)

        for field_name in color_fields:
            if field_name in style_params:
                widget_style.set_editor_property(
                    field_name,
                    self._make_slate_color(style_params.get(field_name), unreal.LinearColor.WHITE),
                )
                updated_fields.append(field_name)

        if "normal_padding" in style_params:
            widget_style.set_editor_property(
                "normal_padding",
                self._make_margin(style_params.get("normal_padding"), "normal_padding"),
            )
            updated_fields.append("normal_padding")
        if "pressed_padding" in style_params:
            widget_style.set_editor_property(
                "pressed_padding",
                self._make_margin(style_params.get("pressed_padding"), "pressed_padding"),
            )
            updated_fields.append("pressed_padding")

        if not updated_fields:
            raise UMGCommandError("Button 的 'style' 没有可写字段")

        button_widget.set_style(widget_style)
        return updated_fields

    def _apply_progress_bar_style(
        self,
        progress_bar_widget: unreal.ProgressBar,
        style_params: Dict[str, Any],
    ) -> list[str]:
        widget_style = progress_bar_widget.get_editor_property("widget_style").copy()
        updated_fields: list[str] = []
        brush_fields = ["background_image", "fill_image", "marquee_image"]

        for field_name in brush_fields:
            if field_name in style_params:
                brush_value = self._update_slate_brush(
                    widget_style.get_editor_property(field_name),
                    style_params.get(field_name),
                    field_name,
                )
                widget_style.set_editor_property(field_name, brush_value)
                updated_fields.append(field_name)

        if "enable_fill_animation" in style_params:
            widget_style.set_editor_property("enable_fill_animation", bool(style_params.get("enable_fill_animation")))
            updated_fields.append("enable_fill_animation")

        if not updated_fields:
            raise UMGCommandError("ProgressBar 的 'style' 没有可写字段")

        progress_bar_widget.set_editor_property("widget_style", widget_style)
        return updated_fields

    def _apply_check_box_style(self, check_box_widget: unreal.CheckBox, style_params: Dict[str, Any]) -> list[str]:
        widget_style = check_box_widget.get_editor_property("widget_style").copy()
        updated_fields: list[str] = []
        brush_fields = [
            "background_image",
            "background_hovered_image",
            "background_pressed_image",
            "unchecked_image",
            "unchecked_hovered_image",
            "unchecked_pressed_image",
            "checked_image",
            "checked_hovered_image",
            "checked_pressed_image",
            "undetermined_image",
            "undetermined_hovered_image",
            "undetermined_pressed_image",
        ]
        color_fields = [
            "foreground_color",
            "border_background_color",
            "checked_foreground",
            "checked_hovered_foreground",
            "checked_pressed_foreground",
            "hovered_foreground",
            "pressed_foreground",
            "undetermined_foreground",
        ]

        for field_name in brush_fields:
            if field_name in style_params:
                brush_value = self._update_slate_brush(
                    widget_style.get_editor_property(field_name),
                    style_params.get(field_name),
                    field_name,
                )
                widget_style.set_editor_property(field_name, brush_value)
                updated_fields.append(field_name)

        for field_name in color_fields:
            if field_name in style_params:
                widget_style.set_editor_property(
                    field_name,
                    self._make_slate_color(style_params.get(field_name), unreal.LinearColor.WHITE),
                )
                updated_fields.append(field_name)

        if "padding" in style_params:
            widget_style.set_editor_property(
                "padding",
                self._make_margin(style_params.get("padding"), "padding"),
            )
            updated_fields.append("padding")

        if "check_box_type" in style_params:
            widget_style.set_editor_property(
                "check_box_type",
                self._parse_check_box_type(str(style_params.get("check_box_type", ""))),
            )
            updated_fields.append("check_box_type")

        if not updated_fields:
            raise UMGCommandError("CheckBox 的 'style' 没有可写字段")

        check_box_widget.set_editor_property("widget_style", widget_style)
        return updated_fields

    def _apply_border_style(self, border_widget: unreal.Border, style_params: Dict[str, Any]) -> list[str]:
        updated_fields: list[str] = []
        if "brush" in style_params:
            border_widget.set_brush(self._update_slate_brush(border_widget.get_editor_property("brush"), style_params.get("brush"), "brush"))
            updated_fields.append("brush")
        if "brush_color" in style_params:
            border_widget.set_brush_color(
                self._make_linear_color(style_params.get("brush_color"), unreal.LinearColor.WHITE)
            )
            updated_fields.append("brush_color")
        if "content_color" in style_params:
            border_widget.set_content_color_and_opacity(
                self._make_linear_color(style_params.get("content_color"), unreal.LinearColor.WHITE)
            )
            updated_fields.append("content_color")
        if "padding" in style_params:
            border_widget.set_padding(self._make_margin(style_params.get("padding"), "padding"))
            updated_fields.append("padding")
        if not updated_fields:
            raise UMGCommandError("Border 的 'style' 没有可写字段")
        return updated_fields

    def _apply_image_style(self, image_widget: unreal.Image, style_params: Dict[str, Any]) -> list[str]:
        updated_fields: list[str] = []
        if "brush" in style_params:
            image_widget.set_brush(self._update_slate_brush(image_widget.get_editor_property("brush"), style_params.get("brush"), "brush"))
            updated_fields.append("brush")
        if "color_and_opacity" in style_params:
            image_widget.set_color_and_opacity(
                self._make_linear_color(style_params.get("color_and_opacity"), unreal.LinearColor.WHITE)
            )
            updated_fields.append("color_and_opacity")
        if not updated_fields:
            raise UMGCommandError("Image 的 'style' 没有可写字段")
        return updated_fields

    def _update_slate_brush(self, base_brush: Any, brush_params: Any, field_name: str) -> unreal.SlateBrush:
        if not isinstance(brush_params, dict) or not brush_params:
            raise UMGCommandError(f"'{field_name}' 必须是非空对象")

        updated_brush = base_brush.copy() if hasattr(base_brush, "copy") else unreal.SlateBrush()
        resource_type, resource_object = self._resolve_brush_resource(brush_params)
        if resource_object is not None:
            updated_brush.set_editor_property("resource_object", resource_object)
            if resource_type == "texture" and hasattr(updated_brush, "set_editor_property"):
                try:
                    updated_brush.set_editor_property("texture_object", resource_object)
                except Exception:
                    pass
        if "tint_color" in brush_params:
            updated_brush.set_editor_property(
                "tint_color",
                self._make_slate_color(brush_params.get("tint_color"), unreal.LinearColor.WHITE),
            )
        if "image_size" in brush_params:
            image_size = self._normalize_image_size_param(brush_params.get("image_size"))
            self._set_slate_brush_image_size(updated_brush, image_size)
        if "draw_as" in brush_params:
            updated_brush.set_editor_property("draw_as", self._parse_brush_draw_type(str(brush_params.get("draw_as", ""))))
        if "margin" in brush_params:
            updated_brush.set_editor_property("margin", self._make_margin(brush_params.get("margin"), "margin"))
        return updated_brush

    @staticmethod
    def _get_game_world() -> unreal.World:
        editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        if editor_subsystem is None:
            raise UMGCommandError("UnrealEditorSubsystem 不可用")

        game_world = editor_subsystem.get_game_world() if hasattr(editor_subsystem, "get_game_world") else None
        if game_world is None:
            raise UMGCommandError("当前没有运行中的 PIE/Game 世界，无法将 Widget 添加到视口")
        return game_world

    def _configure_border(self, border: unreal.Border, params: Dict[str, Any]) -> None:
        border.set_brush_color(self._make_linear_color(params.get("brush_color"), unreal.LinearColor.WHITE))
        border.set_content_color_and_opacity(
            self._make_linear_color(params.get("content_color"), unreal.LinearColor.WHITE)
        )

    def _configure_size_box(self, size_box: unreal.SizeBox, params: Dict[str, Any]) -> None:
        requested_size = self._make_vector2d(params.get("size"), unreal.Vector2D(200.0, 50.0))
        width_override = params.get("width_override", requested_size.x)
        height_override = params.get("height_override", requested_size.y)
        size_box.set_width_override(float(width_override))
        size_box.set_height_override(float(height_override))

    def _configure_progress_bar(
        self,
        progress_bar: unreal.ProgressBar,
        params: Dict[str, Any],
        percent: float,
    ) -> None:
        progress_bar.set_percent(percent)
        progress_bar.set_fill_color_and_opacity(
            self._make_linear_color(params.get("fill_color"), unreal.LinearColor.WHITE)
        )

    @staticmethod
    def _configure_editable_text(
        editable_text: unreal.EditableText,
        initial_text: str,
        hint_text: str,
        is_read_only: bool,
    ) -> None:
        editable_text.set_text(initial_text)
        if hint_text:
            editable_text.set_hint_text(hint_text)
        editable_text.set_is_read_only(is_read_only)

    def _configure_rich_text(
        self,
        rich_text: unreal.RichTextBlock,
        params: Dict[str, Any],
        initial_text: str,
    ) -> None:
        rich_text.set_text(initial_text)
        rich_text.set_default_color_and_opacity(
            self._make_slate_color(params.get("color"), unreal.LinearColor.WHITE)
        )

    @staticmethod
    def _configure_multi_line_text(
        multi_line_text: unreal.MultiLineEditableTextBox,
        initial_text: str,
        hint_text: str,
        is_read_only: bool,
    ) -> None:
        multi_line_text.set_text(initial_text)
        multi_line_text.set_hint_text(hint_text)
        multi_line_text.set_is_read_only(is_read_only)

    @staticmethod
    def _make_margin(value: Any, field_name: str) -> unreal.Margin:
        if not isinstance(value, list) or len(value) < 4:
            raise UMGCommandError(f"'{field_name}' 必须是长度为 4 的数组")
        return unreal.Margin(
            left=float(value[0]),
            top=float(value[1]),
            right=float(value[2]),
            bottom=float(value[3]),
        )

    @staticmethod
    def _parse_brush_draw_type(draw_as_name: str) -> unreal.SlateBrushDrawType:
        normalized_name = draw_as_name.strip().casefold().replace("-", "").replace("_", "").replace(" ", "")
        draw_type_mapping = {
            "noimage": unreal.SlateBrushDrawType.NO_DRAW_TYPE,
            "nodrawtype": unreal.SlateBrushDrawType.NO_DRAW_TYPE,
            "image": unreal.SlateBrushDrawType.IMAGE,
            "box": unreal.SlateBrushDrawType.BOX,
            "border": unreal.SlateBrushDrawType.BORDER,
            "roundedbox": unreal.SlateBrushDrawType.ROUNDED_BOX,
        }
        resolved_draw_type = draw_type_mapping.get(normalized_name)
        if resolved_draw_type is None:
            raise UMGCommandError("不支持的 'draw_as'，当前支持 NoDrawType、Image、Box、Border、RoundedBox")
        return resolved_draw_type

    @staticmethod
    def _parse_check_box_type(check_box_type_name: str) -> unreal.CheckBoxType:
        normalized_name = check_box_type_name.strip().casefold().replace("-", "").replace("_", "").replace(" ", "")
        check_box_type_mapping = {
            "checkbox": unreal.CheckBoxType.CHECK_BOX,
            "check": unreal.CheckBoxType.CHECK_BOX,
            "togglebutton": unreal.CheckBoxType.TOGGLE_BUTTON,
            "toggle": unreal.CheckBoxType.TOGGLE_BUTTON,
        }
        resolved_check_box_type = check_box_type_mapping.get(normalized_name)
        if resolved_check_box_type is None:
            raise UMGCommandError("不支持的 'check_box_type'，当前支持 CheckBox、ToggleButton")
        return resolved_check_box_type

    @staticmethod
    def _set_slate_brush_image_size(brush: unreal.SlateBrush, image_size: list[float]) -> None:
        size_struct = brush.get_editor_property("image_size").copy()
        size_struct.set_editor_property("x", float(image_size[0]))
        size_struct.set_editor_property("y", float(image_size[1]))
        brush.set_editor_property("image_size", size_struct)


class UMGCommandDispatcher:
    """Dispatch supported local Widget Blueprint commands."""

    def __init__(self) -> None:
        self._asset_helper = UMGAssetHelper()
        self._widget_helper = UMGWidgetHelper(self._asset_helper)

    def handle(self, command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        if command_name == "create_umg_widget_blueprint":
            return self._handle_create_umg_widget_blueprint(params)
        if command_name == "add_text_block_to_widget":
            return self._handle_add_text_block_to_widget(params)
        if command_name == "add_button_to_widget":
            return self._handle_add_button_to_widget(params)
        if command_name == "add_widget_to_viewport":
            return self._handle_add_widget_to_viewport(params)
        if command_name == "remove_widget_from_viewport":
            return self._handle_remove_widget_from_viewport(params)
        if command_name == "add_image_to_widget":
            return self._handle_add_image_to_widget(params)
        if command_name == "add_border_to_widget":
            return self._handle_add_border_to_widget(params)
        if command_name == "add_canvas_panel_to_widget":
            return self._handle_add_canvas_panel_to_widget(params)
        if command_name == "add_horizontal_box_to_widget":
            return self._handle_add_horizontal_box_to_widget(params)
        if command_name == "add_vertical_box_to_widget":
            return self._handle_add_vertical_box_to_widget(params)
        if command_name == "add_overlay_to_widget":
            return self._handle_add_overlay_to_widget(params)
        if command_name == "add_scroll_box_to_widget":
            return self._handle_add_scroll_box_to_widget(params)
        if command_name == "add_size_box_to_widget":
            return self._handle_add_size_box_to_widget(params)
        if command_name == "add_spacer_to_widget":
            return self._handle_add_spacer_to_widget(params)
        if command_name == "add_progress_bar_to_widget":
            return self._handle_add_progress_bar_to_widget(params)
        if command_name == "add_slider_to_widget":
            return self._handle_add_slider_to_widget(params)
        if command_name == "add_check_box_to_widget":
            return self._handle_add_check_box_to_widget(params)
        if command_name == "add_editable_text_to_widget":
            return self._handle_add_editable_text_to_widget(params)
        if command_name == "add_rich_text_to_widget":
            return self._handle_add_rich_text_to_widget(params)
        if command_name == "add_multi_line_text_to_widget":
            return self._handle_add_multi_line_text_to_widget(params)
        if command_name == "add_named_slot_to_widget":
            return self._handle_add_named_slot_to_widget(params)
        if command_name == "add_list_view_to_widget":
            return self._handle_add_list_view_to_widget(params)
        if command_name == "add_tile_view_to_widget":
            return self._handle_add_tile_view_to_widget(params)
        if command_name == "add_tree_view_to_widget":
            return self._handle_add_tree_view_to_widget(params)
        if command_name == "remove_widget_from_blueprint":
            return self._handle_remove_widget_from_blueprint(params)
        if command_name == "set_widget_slot_layout":
            return self._handle_set_widget_slot_layout(params)
        if command_name == "set_widget_visibility":
            return self._handle_set_widget_visibility(params)
        if command_name == "set_widget_style":
            return self._handle_set_widget_style(params)
        if command_name == "set_widget_brush":
            return self._handle_set_widget_brush(params)
        if command_name == "open_widget_blueprint_editor":
            return self._handle_open_widget_blueprint_editor(params)
        raise UMGCommandError(f"不支持的本地 UMG 命令: {command_name}")

    def _handle_create_umg_widget_blueprint(self, params: Dict[str, Any]) -> Dict[str, Any]:
        blueprint_name = str(params.get("name", "") or params.get("widget_name", "")).strip()
        if not blueprint_name:
            raise UMGCommandError("缺少 'name' 参数")

        parent_class = self._asset_helper.resolve_parent_class(str(params.get("parent_class", "")))
        package_path = self._asset_helper.normalize_package_path(str(params.get("path", "")))
        return self._asset_helper.create_widget_blueprint(blueprint_name, parent_class, package_path)

    def _handle_add_text_block_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_text_block_to_widget(resolved_blueprint, params)

    def _handle_add_button_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_button_to_widget(resolved_blueprint, params)

    def _handle_add_widget_to_viewport(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_widget_to_viewport(resolved_blueprint, params)

    def _handle_remove_widget_from_viewport(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return self._widget_helper.remove_widget_from_viewport(params)

    def _handle_add_image_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_image_to_widget(resolved_blueprint, params)

    def _handle_add_border_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_border_to_widget(resolved_blueprint, params)

    def _handle_add_canvas_panel_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_canvas_panel_to_widget(resolved_blueprint, params)

    def _handle_add_horizontal_box_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_horizontal_box_to_widget(resolved_blueprint, params)

    def _handle_add_vertical_box_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_vertical_box_to_widget(resolved_blueprint, params)

    def _handle_add_overlay_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_overlay_to_widget(resolved_blueprint, params)

    def _handle_add_scroll_box_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_scroll_box_to_widget(resolved_blueprint, params)

    def _handle_add_size_box_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_size_box_to_widget(resolved_blueprint, params)

    def _handle_add_spacer_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_spacer_to_widget(resolved_blueprint, params)

    def _handle_add_progress_bar_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_progress_bar_to_widget(resolved_blueprint, params)

    def _handle_add_slider_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_slider_to_widget(resolved_blueprint, params)

    def _handle_add_check_box_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_check_box_to_widget(resolved_blueprint, params)

    def _handle_add_editable_text_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_editable_text_to_widget(resolved_blueprint, params)

    def _handle_add_rich_text_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_rich_text_to_widget(resolved_blueprint, params)

    def _handle_add_multi_line_text_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_multi_line_text_to_widget(resolved_blueprint, params)

    def _handle_add_named_slot_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_named_slot_to_widget(resolved_blueprint, params)

    def _handle_add_list_view_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_list_view_to_widget(resolved_blueprint, params)

    def _handle_add_tile_view_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_tile_view_to_widget(resolved_blueprint, params)

    def _handle_add_tree_view_to_widget(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.add_tree_view_to_widget(resolved_blueprint, params)

    def _handle_remove_widget_from_blueprint(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.remove_widget_from_blueprint(resolved_blueprint, params)

    def _handle_set_widget_slot_layout(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.set_widget_slot_layout(resolved_blueprint, params)

    def _handle_set_widget_visibility(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.set_widget_visibility(resolved_blueprint, params)

    def _handle_set_widget_style(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.set_widget_style(resolved_blueprint, params)

    def _handle_set_widget_brush(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        return self._widget_helper.set_widget_brush(resolved_blueprint, params)

    def _handle_open_widget_blueprint_editor(self, params: Dict[str, Any]) -> Dict[str, Any]:
        resolved_blueprint = self._resolve_target_widget_blueprint(params)
        asset_editor_subsystem = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
        if asset_editor_subsystem is None:
            raise UMGCommandError("AssetEditorSubsystem 不可用")
        opened = asset_editor_subsystem.open_editor_for_assets([resolved_blueprint.asset])
        if not opened:
            raise UMGCommandError(f"打开 Widget Blueprint 编辑器失败: {resolved_blueprint.asset_name}")

        return {
            "success": True,
            "blueprint_name": resolved_blueprint.asset_name,
            "asset_path": resolved_blueprint.asset_path,
            "implementation": "local_python",
        }

    def _resolve_target_widget_blueprint(self, params: Dict[str, Any]) -> ResolvedWidgetBlueprint:
        if "blueprint_name" in params:
            blueprint_name = str(params.get("blueprint_name", "")).strip()
        else:
            blueprint_name = str(params.get("widget_name", "")).strip()
        if not blueprint_name:
            raise UMGCommandError("缺少 'blueprint_name' 参数")
        return self._asset_helper.resolve_widget_blueprint(blueprint_name)


def handle_umg_command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
    """Module entry point used by the C++ local Python bridge."""
    dispatcher = UMGCommandDispatcher()
    try:
        return dispatcher.handle(command_name, params)
    except UMGCommandError as exc:
        return {
            "success": False,
            "error": str(exc),
        }
