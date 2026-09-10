// Copyright InventoryBubble. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

INVENTORYBUBBLE_API DECLARE_LOG_CATEGORY_EXTERN(LogInventoryBubble, Log, All);

class FInventoryBubbleModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
