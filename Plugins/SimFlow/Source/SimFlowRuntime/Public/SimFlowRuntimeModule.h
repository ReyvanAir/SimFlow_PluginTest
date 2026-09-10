// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

SIMFLOWRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogSimFlow, Log, All);

class FSimFlowRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
