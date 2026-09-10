// Copyright InventoryBubble. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/ObjectMacros.h"
#include "InventoryBubbleTypes.generated.h"

class UPrimitiveComponent;
class USceneComponent;

/** Lifecycle of a bubble. Drives ticking, material parameters and what input is accepted. */
UENUM(BlueprintType)
enum class EInventoryBubbleState : uint8
{
	/** No item stored, ready to capture. */
	Empty		UMETA(DisplayName = "Empty"),
	/** An item is blending in toward the anchor. */
	Capturing	UMETA(DisplayName = "Capturing"),
	/** An item is fully stored. */
	Occupied	UMETA(DisplayName = "Occupied"),
	/** An item is blending back out to its original transform. */
	Releasing	UMETA(DisplayName = "Releasing")
};

/** Why a candidate item was not accepted. Passed to OnItemRejected. */
UENUM(BlueprintType)
enum class EInventoryBubbleRejectReason : uint8
{
	None			UMETA(DisplayName = "None"),
	/** The bubble already holds an item. */
	AlreadyOccupied	UMETA(DisplayName = "Already Occupied"),
	/** The actor does not carry ItemTag. */
	TagMismatch		UMETA(DisplayName = "Tag Mismatch"),
	/** The actor was released from this bubble too recently. */
	OnCooldown		UMETA(DisplayName = "On Cooldown"),
	/** The actor is currently attached to something that looks like a hand. */
	HeldByPlayer	UMETA(DisplayName = "Held By Player"),
	/** IInventoryBubbleItemInterface::CanBeStored returned false. */
	InterfaceRefused UMETA(DisplayName = "Interface Refused"),
	/** A Blueprint override of CanAcceptItem returned false. */
	BlueprintRefused UMETA(DisplayName = "Blueprint Refused"),
	/** The actor was null, being destroyed, or had no primitive component. */
	Invalid			UMETA(DisplayName = "Invalid"),
	/** The bubble is mid-capture or mid-release. */
	Busy			UMETA(DisplayName = "Busy")
};

/**
 * Everything we changed on one primitive component, captured before we change it.
 * Restoring these exactly is the contract that makes a released item behave as if
 * it had never been stored.
 */
USTRUCT()
struct INVENTORYBUBBLE_API FInventoryBubbleComponentState
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UPrimitiveComponent> Component = nullptr;

	UPROPERTY()
	bool bSimulatePhysics = false;

	UPROPERTY()
	bool bGravityEnabled = false;

	UPROPERTY()
	FName CollisionProfileName = NAME_None;

	UPROPERTY()
	TEnumAsByte<ECollisionEnabled::Type> CollisionEnabled = ECollisionEnabled::NoCollision;

	UPROPERTY()
	float LinearDamping = 0.f;

	UPROPERTY()
	float AngularDamping = 0.f;
};

/**
 * The full pre-capture snapshot of a stored actor.
 *
 * Scale is stored as the actor's world scale so a released item is restored to the
 * size it actually had in the level, independent of whatever it was attached to.
 */
USTRUCT()
struct INVENTORYBUBBLE_API FInventoryBubbleStoredItemState
{
	GENERATED_BODY()

	/** True once a snapshot has actually been taken. */
	UPROPERTY()
	bool bValid = false;

	UPROPERTY()
	FVector OriginalWorldScale = FVector::OneVector;

	UPROPERTY()
	FTransform OriginalWorldTransform = FTransform::Identity;

	/** Where the actor was attached before we took it, so we can put it back. */
	UPROPERTY()
	TWeakObjectPtr<USceneComponent> PreviousAttachParent = nullptr;

	UPROPERTY()
	FName PreviousAttachSocket = NAME_None;

	UPROPERTY()
	TArray<FInventoryBubbleComponentState> ComponentStates;

	void Reset()
	{
		bValid = false;
		OriginalWorldScale = FVector::OneVector;
		OriginalWorldTransform = FTransform::Identity;
		PreviousAttachParent = nullptr;
		PreviousAttachSocket = NAME_None;
		ComponentStates.Reset();
	}
};
