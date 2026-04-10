#pragma once

#include "Modules/ModuleManager.h"

class FVPTrackerReceiverModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
