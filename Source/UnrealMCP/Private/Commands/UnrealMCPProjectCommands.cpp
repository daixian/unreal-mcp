/**
 * @file UnrealMCPProjectCommands.cpp
 * @brief 项目级 MCP 命令处理实现。
 */
#include "Commands/UnrealMCPProjectCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "GameFramework/InputSettings.h"

/**
 * @brief 构造函数。
 */
FUnrealMCPProjectCommands::FUnrealMCPProjectCommands()
{
}

/**
 * @brief 分发项目级命令到具体处理函数。
 * @param [in] CommandType 命令类型。
 * @param [in] Params 命令参数。
 * @return TSharedPtr<FJsonObject> 命令结果或错误信息。
 */
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_input_mapping"))
    {
        return HandleCreateInputMapping(Params);
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown project command: %s"), *CommandType));
}

/**
 * @brief 创建输入映射配置（Action/Axis）。
 * @param [in] Params 输入映射参数。
 * @return TSharedPtr<FJsonObject> 执行结果。
 * @note 该实现会将配置直接写入默认输入设置并保存。
 */
TSharedPtr<FJsonObject> FUnrealMCPProjectCommands::HandleCreateInputMapping(const TSharedPtr<FJsonObject>& Params)
{
    FString MappingName;
    if (!Params->TryGetStringField(TEXT("action_name"), MappingName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'action_name' parameter"));
    }

    FString Key;
    if (!Params->TryGetStringField(TEXT("key"), Key))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'key' parameter"));
    }

    // Get the input settings
    UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();
    if (!InputSettings)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get input settings"));
    }

    FString InputType = TEXT("Action");
    Params->TryGetStringField(TEXT("input_type"), InputType);
    const bool bIsAxisMapping = InputType.Equals(TEXT("Axis"), ESearchCase::IgnoreCase);

    if (bIsAxisMapping)
    {
        float Scale = 1.0f;
        Params->TryGetNumberField(TEXT("scale"), Scale);

        FInputAxisKeyMapping AxisMapping;
        AxisMapping.AxisName = FName(*MappingName);
        AxisMapping.Key = FKey(*Key);
        AxisMapping.Scale = Scale;

        InputSettings->AddAxisMapping(AxisMapping);
    }
    else
    {
        FInputActionKeyMapping ActionMapping;
        ActionMapping.ActionName = FName(*MappingName);
        ActionMapping.Key = FKey(*Key);

        if (Params->HasField(TEXT("shift")))
        {
            ActionMapping.bShift = Params->GetBoolField(TEXT("shift"));
        }
        if (Params->HasField(TEXT("ctrl")))
        {
            ActionMapping.bCtrl = Params->GetBoolField(TEXT("ctrl"));
        }
        if (Params->HasField(TEXT("alt")))
        {
            ActionMapping.bAlt = Params->GetBoolField(TEXT("alt"));
        }
        if (Params->HasField(TEXT("cmd")))
        {
            ActionMapping.bCmd = Params->GetBoolField(TEXT("cmd"));
        }

        InputSettings->AddActionMapping(ActionMapping);
    }

    InputSettings->SaveConfig();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("action_name"), MappingName);
    ResultObj->SetStringField(TEXT("key"), Key);
    ResultObj->SetStringField(TEXT("input_type"), bIsAxisMapping ? TEXT("Axis") : TEXT("Action"));
    return ResultObj;
} 
