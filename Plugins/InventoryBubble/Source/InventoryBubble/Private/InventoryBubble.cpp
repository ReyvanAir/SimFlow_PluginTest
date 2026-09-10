// Copyright InventoryBubble. All Rights Reserved.

#include "InventoryBubble.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogInventoryBubble);

#define LOCTEXT_NAMESPACE "FInventoryBubbleModule"

void FInventoryBubbleModule::StartupModule()
{
	UE_LOG(LogInventoryBubble, Log, TEXT("InventoryBubble module started (VRE support: %s)."),
		WITH_VR_EXPANSION_PLUGIN ? TEXT("enabled") : TEXT("disabled"));
}

void FInventoryBubbleModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FInventoryBubbleModule, InventoryBubble)
