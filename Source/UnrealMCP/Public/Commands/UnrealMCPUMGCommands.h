#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * @brief 处理 UMG（Widget Blueprint）相关 MCP 命令。
 * @note 负责创建/修改 Widget Blueprint、添加控件以及管理视口中的 Widget 实例。
 */
class UNREALMCP_API FUnrealMCPUMGCommands
{
public:
    /**
     * @brief 构造函数。
     */
    FUnrealMCPUMGCommands();

    /**
     * @brief 分发并执行 UMG 相关命令。
     * @param [in] CommandType 命令类型字符串。
     * @param [in] Params 命令参数 JSON 对象。
     * @return TSharedPtr<FJsonObject> 执行结果或错误信息。
     */
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    /**
     * @brief 创建 UMG Widget Blueprint。
     * @param [in] Params 需包含目标 Widget 的创建参数。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleCreateUMGWidgetBlueprint(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加 Text Block 控件。
     * @param [in] Params 需包含蓝图名、控件名和可选布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddTextBlockToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 将 Widget 实例添加到游戏视口。
     * @param [in] Params 需包含蓝图名以及可选 ZOrder。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddWidgetToViewport(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 从游戏视口移除运行中的 Widget 实例。
     * @param [in] Params 需包含 instance_path 或 instance_name。
     * @return TSharedPtr<FJsonObject> 移除结果。
     */
    TSharedPtr<FJsonObject> HandleRemoveWidgetFromViewport(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加 Button 控件。
     * @param [in] Params 需包含蓝图名、按钮名、文本与布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddButtonToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 为控件绑定事件（如按钮点击）。
     * @param [in] Params 需包含蓝图名、控件名和事件名。
     * @return TSharedPtr<FJsonObject> 绑定结果。
     */
    TSharedPtr<FJsonObject> HandleBindWidgetEvent(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 配置 Text Block 的数据绑定，用于动态文本更新。
     * @param [in] Params 需包含蓝图名、控件名和绑定配置。
     * @return TSharedPtr<FJsonObject> 绑定配置结果。
     */
    TSharedPtr<FJsonObject> HandleSetTextBlockBinding(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 为任意已支持属性的控件配置属性绑定。
     * @param [in] Params 需包含蓝图名、控件名和绑定配置。
     * @return TSharedPtr<FJsonObject> 绑定配置结果。
     */
    TSharedPtr<FJsonObject> HandleBindWidgetProperty(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加 Image 控件。
     * @param [in] Params 需包含蓝图名、控件名和可选布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddImageToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加 Border 控件。
     * @param [in] Params 需包含蓝图名、控件名和可选布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddBorderToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加 Canvas Panel 控件。
     * @param [in] Params 需包含蓝图名、控件名和可选布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddCanvasPanelToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加 Horizontal Box 控件。
     * @param [in] Params 需包含蓝图名、控件名和可选布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddHorizontalBoxToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加 Vertical Box 控件。
     * @param [in] Params 需包含蓝图名、控件名和可选布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddVerticalBoxToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加 Overlay 控件。
     * @param [in] Params 需包含蓝图名、控件名和可选布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddOverlayToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加 Scroll Box 控件。
     * @param [in] Params 需包含蓝图名、控件名和可选布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddScrollBoxToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加 Size Box 控件。
     * @param [in] Params 需包含蓝图名、控件名和可选布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddSizeBoxToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加 Spacer 控件。
     * @param [in] Params 需包含蓝图名、控件名和可选布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddSpacerToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加 Progress Bar 控件。
     * @param [in] Params 需包含蓝图名、控件名和可选布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddProgressBarToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加 Slider 控件。
     * @param [in] Params 需包含蓝图名、控件名和可选布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddSliderToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加 Check Box 控件。
     * @param [in] Params 需包含蓝图名、控件名和可选布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddCheckBoxToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加 Editable Text 控件。
     * @param [in] Params 需包含蓝图名、控件名和可选布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddEditableTextToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加 Rich Text 控件。
     * @param [in] Params 需包含蓝图名、控件名和可选布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddRichTextToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加多行文本控件。
     * @param [in] Params 需包含蓝图名、控件名和可选布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddMultiLineTextToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加 Named Slot 控件。
     * @param [in] Params 需包含蓝图名、控件名和可选布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddNamedSlotToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加 List View 控件。
     * @param [in] Params 需包含蓝图名、控件名和可选布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddListViewToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加 Tile View 控件。
     * @param [in] Params 需包含蓝图名、控件名和可选布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddTileViewToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 向 UMG Widget Blueprint 添加 Tree View 控件。
     * @param [in] Params 需包含蓝图名、控件名和可选布局参数。
     * @return TSharedPtr<FJsonObject> 添加结果。
     */
    TSharedPtr<FJsonObject> HandleAddTreeViewToWidget(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 从 Widget Blueprint 中删除指定控件。
     * @param [in] Params 需包含蓝图名和目标控件名。
     * @return TSharedPtr<FJsonObject> 删除结果。
     */
    TSharedPtr<FJsonObject> HandleRemoveWidgetFromBlueprint(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置控件的 Canvas Slot 布局参数。
     * @param [in] Params 需包含蓝图名、控件名和布局参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetWidgetSlotLayout(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置控件可见性。
     * @param [in] Params 需包含蓝图名、控件名和 visibility。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetWidgetVisibility(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置控件样式结构体中的稳定字段。
     * @param [in] Params 需包含蓝图名、控件名和 style 对象。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetWidgetStyle(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 设置控件 Brush 资源与基础显示参数。
     * @param [in] Params 需包含蓝图名、控件名以及 Brush 资源参数。
     * @return TSharedPtr<FJsonObject> 设置结果。
     */
    TSharedPtr<FJsonObject> HandleSetWidgetBrush(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 在 Widget Blueprint 中创建新的 Widget 动画。
     * @param [in] Params 需包含蓝图名以及可选动画名、时间范围和显示帧率。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleCreateWidgetAnimation(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 给 Widget 动画添加关键帧。
     * @param [in] Params 需包含动画名、目标控件、属性名、时间和数值。
     * @return TSharedPtr<FJsonObject> 写入结果。
     */
    TSharedPtr<FJsonObject> HandleAddWidgetAnimationKeyframe(const TSharedPtr<FJsonObject>& Params);

    /**
     * @brief 打开 Widget Blueprint 编辑器。
     * @param [in] Params 需包含蓝图名。
     * @return TSharedPtr<FJsonObject> 打开结果。
     */
    TSharedPtr<FJsonObject> HandleOpenWidgetBlueprintEditor(const TSharedPtr<FJsonObject>& Params);
};
