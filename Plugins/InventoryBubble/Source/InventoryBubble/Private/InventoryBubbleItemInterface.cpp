// Copyright InventoryBubble. All Rights Reserved.

#include "InventoryBubbleItemInterface.h"

// UHT generates inline defaults for interface BlueprintNativeEvents, and those
// defaults are zero-initialised: CanBeStored would return false and
// GetStoredScaleMultiplier would return 0. Both are wrong here - an item that
// implements the interface but overrides nothing must behave exactly like a
// plain tagged actor.
//
// Declaring CanBeStored_Implementation in the header suppresses UHT's stub so we
// can supply the correct default below. GetStoredScaleMultiplier keeps UHT's 0
// and is normalised to 1.0 by the bubble, which treats any value <= 0 as "no
// override" - see AInventoryBubbleActor::GetStoredItemTransform_Implementation.

bool IInventoryBubbleItemInterface::CanBeStored_Implementation(const AInventoryBubbleActor* /*Bubble*/) const
{
	return true;
}
