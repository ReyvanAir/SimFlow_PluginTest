// Copyright InventoryBubble. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InventoryBubbleGrabAdapter.generated.h"

class AActor;
class UActorComponent;
class AInventoryBubbleActor;

/**
 * Layer C. Optional glue to grab frameworks. Never owns state - it only calls
 * AInventoryBubbleActor::NotifyItemGrabbed, which routes into Layer A.
 *
 * UE5 VR Template: the template's GrabComponent is content-only, so there is no
 * C++ type to link against. We find it by class name and bind to its OnGrabbed /
 * OnDropped multicast delegates through reflection - but only when the delegate
 * signature takes no parameters, so a template revision that changes the
 * signature degrades to Layer B instead of crashing.
 *
 * VR Expansion Plugin: compiled in only when WITH_VR_EXPANSION_PLUGIN is 1.
 */
UCLASS()
class INVENTORYBUBBLE_API UInventoryBubbleGrabAdapter : public UObject
{
	GENERATED_BODY()

public:
	/** Returns true when at least one grab notification was successfully bound. */
	bool BindToItem(AActor* Item, AInventoryBubbleActor* InBubble);

	/** Always safe to call, including when nothing was ever bound. */
	void Unbind();

	/**
	 * True only when the bind landed on a real GrabComponent, not on the item
	 * actor itself.
	 *
	 * The bubble uses this to decide whether it can stop polling for grabs. A
	 * GrabComponent's OnGrabbed / OnDropped are the grab system's own signals
	 * and are authoritative. The actor-level fallback is not: any actor can
	 * happen to expose a parameterless delegate of that name without it firing
	 * on every grab path, and trusting it would silently strand the item.
	 */
	bool IsBoundToGrabComponent() const;

	/** True when the actor carries something that looks like a VR Template GrabComponent. */
	static UActorComponent* FindGrabComponent(const AActor* Actor);

	/** True when the actor implements the VRE grip interface (always false without VRE). */
	static bool ImplementsVRExpansionGripInterface(const AActor* Actor);

protected:
	/** Bound by reflection to a parameterless OnGrabbed / OnDropped delegate. */
	UFUNCTION()
	void HandleGrabNotification();

	bool TryBindParameterlessDelegate(UObject* Target, FName DelegatePropertyName);
	void UnbindParameterlessDelegate(UObject* Target, FName DelegatePropertyName);

private:
	TWeakObjectPtr<AInventoryBubbleActor> Bubble;
	TWeakObjectPtr<AActor> BoundItem;
	TWeakObjectPtr<UObject> BoundTarget;

	/** Delegate property names we actually bound, so Unbind is exact. */
	TArray<FName> BoundDelegateNames;
};
