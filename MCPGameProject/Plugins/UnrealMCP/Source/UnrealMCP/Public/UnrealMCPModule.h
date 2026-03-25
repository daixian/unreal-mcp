#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * @brief UnrealMCP 插件模块入口，负责模块生命周期管理。
 */
class FUnrealMCPModule : public IModuleInterface
{
public:
	/**
	 * @brief 模块启动时调用，用于初始化模块资源。
	 */
	virtual void StartupModule() override;

	/**
	 * @brief 模块关闭时调用，用于释放模块资源。
	 */
	virtual void ShutdownModule() override;

	/**
	 * @brief 获取模块单例实例。
	 * @return FUnrealMCPModule& 已加载模块的引用。
	 */
	static inline FUnrealMCPModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FUnrealMCPModule>("UnrealMCP");
	}

	/**
	 * @brief 判断模块是否已被加载。
	 * @return bool 已加载返回 true，否则返回 false。
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("UnrealMCP");
	}
};
