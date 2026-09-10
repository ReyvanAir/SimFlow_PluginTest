// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * Native tags shipped with the plugin.
 *
 * Declare your project's own event tags the same way (or in the Gameplay Tags
 * project settings) and raise them with USimFlowStatics::BroadcastFlowEvent.
 */
namespace SimFlowTags
{
	SIMFLOWRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event);
	SIMFLOWRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Generic);
	SIMFLOWRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Interact);
	SIMFLOWRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Grab);
	SIMFLOWRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Release);
	SIMFLOWRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_ButtonPressed);
	SIMFLOWRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Placed);
	SIMFLOWRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Removed);

	/** Kinds of mistake recorded on USimFlowInstance. */
	SIMFLOWRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mistake);
	SIMFLOWRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mistake_WrongItem);
	SIMFLOWRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mistake_WrongTarget);
	SIMFLOWRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mistake_WrongOrder);
	SIMFLOWRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mistake_WrongAnswer);

	SIMFLOWRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sample_GrabExtinguisher);
	SIMFLOWRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sample_PullPin);
}
