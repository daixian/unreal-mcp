#pragma once

#include "Commandlets/Commandlet.h"
#include "CleanupBlueprintForReparentCommandlet.generated.h"

/**
 * @brief 清理 Blueprint 重设父类后残留节点的命令行工具。
 */
UCLASS()
class UNREALMCP_API UCleanupBlueprintForReparentCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UCleanupBlueprintForReparentCommandlet();

    virtual int32 Main(const FString& Params) override;
};
