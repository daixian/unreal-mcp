#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * @brief 处理项目级 MCP 命令的处理器。
 */
class UNREALMCP_API FUnrealMCPProjectCommands
{
public:
    /**
     * @brief 构造函数。
     */
    FUnrealMCPProjectCommands();

    /**
     * @brief 分发并执行项目级命令。
     * @param [in] CommandType 命令类型字符串。
     * @param [in] Params 命令参数 JSON 对象。
     * @return TSharedPtr<FJsonObject> 执行结果或错误信息。
     */
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    /**
     * @brief 创建输入映射（Action/Axis）配置。
     * @param [in] Params 输入映射参数。
     * @return TSharedPtr<FJsonObject> 创建结果。
     */
    TSharedPtr<FJsonObject> HandleCreateInputMapping(const TSharedPtr<FJsonObject>& Params);
};
