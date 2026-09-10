// Copyright InventoryBubble. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InventoryBubbleFunctionLibrary.generated.h"

class AActor;
class AInventoryBubbleActor;
class UObject;

/**
 * Blueprint helpers. Everything here is a convenience over the actor's own API -
 * none of it holds state, so it is safe to call from anywhere.
 */
UCLASS()
class INVENTORYBUBBLE_API UInventoryBubbleFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Nearest bubble to Origin that can currently accept Item.
	 * Pass a null Item to find the nearest empty bubble regardless of eligibility.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory Bubble|Utility",
		meta = (WorldContext = "WorldContextObject"))
	static AInventoryBubbleActor* FindNearestFreeBubble(const UObject* WorldContextObject,
		FVector Origin, float MaxDistance = 500.f, AActor* Item = nullptr);

	/** Nearest bubble to Origin regardless of whether it is occupied. */
	UFUNCTION(BlueprintCallable, Category = "Inventory Bubble|Utility",
		meta = (WorldContext = "WorldContextObject"))
	static AInventoryBubbleActor* FindNearestBubble(const UObject* WorldContextObject,
		FVector Origin, float MaxDistance = 500.f);

	/** The bubble currently holding Item, or null. */
	UFUNCTION(BlueprintCallable, Category = "Inventory Bubble|Utility",
		meta = (WorldContext = "WorldContextObject"))
	static AInventoryBubbleActor* GetBubbleHoldingItem(const UObject* WorldContextObject, AActor* Item);

	UFUNCTION(BlueprintPure, Category = "Inventory Bubble|Utility",
		meta = (WorldContext = "WorldContextObject"))
	static bool IsActorStored(const UObject* WorldContextObject, AActor* Item);

	/**
	 * Convenience for the non-VR test path: store Item in the nearest bubble that
	 * will take it. Returns the bubble that accepted, or null.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory Bubble|Utility",
		meta = (WorldContext = "WorldContextObject"))
	static AInventoryBubbleActor* StoreInNearestBubble(const UObject* WorldContextObject,
		AActor* Item, float MaxDistance = 500.f);

	/**
	 * Convenience for the non-VR test path: release from the nearest occupied
	 * bubble. Returns the released actor, or null.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory Bubble|Utility",
		meta = (WorldContext = "WorldContextObject"))
	static AActor* ReleaseFromNearestBubble(const UObject* WorldContextObject,
		FVector Origin, float MaxDistance = 500.f);

	/** Every bubble in the level. */
	UFUNCTION(BlueprintCallable, Category = "Inventory Bubble|Utility",
		meta = (WorldContext = "WorldContextObject"))
	static void GetAllBubbles(const UObject* WorldContextObject, TArray<AInventoryBubbleActor*>& OutBubbles);
};
