// Copyright InventoryBubble. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InventoryBubbleItemInterface.generated.h"

class AInventoryBubbleActor;

UINTERFACE(MinimalAPI, Blueprintable, meta = (DisplayName = "Inventory Bubble Item"))
class UInventoryBubbleItemInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Optional interface for items that need custom behaviour inside a bubble.
 *
 * Implementing this is never required - an actor tagged with the bubble's ItemTag
 * works on its own. Implement it only when an item needs to veto storage, store at
 * a different size, or react to being taken.
 */
class INVENTORYBUBBLE_API IInventoryBubbleItemInterface
{
	GENERATED_BODY()

public:
	/** Return false to refuse storage in this particular bubble. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory Bubble|Item")
	bool CanBeStored(const AInventoryBubbleActor* Bubble) const;
	virtual bool CanBeStored_Implementation(const AInventoryBubbleActor* Bubble) const;

	/**
	 * Multiplier applied on top of the bubble's ShrinkScale for this item.
	 * Return 1.0 to accept the bubble's value unchanged.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory Bubble|Item")
	float GetStoredScaleMultiplier() const;

	/** Called once the capture blend has finished and the item is fully stored. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory Bubble|Item")
	void OnStoredInBubble(AInventoryBubbleActor* Bubble);

	/** Called once the item has been detached and its original state restored. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory Bubble|Item")
	void OnRemovedFromBubble(AInventoryBubbleActor* Bubble);
};
