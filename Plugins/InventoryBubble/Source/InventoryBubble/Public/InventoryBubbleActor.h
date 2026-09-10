// Copyright InventoryBubble. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InventoryBubbleTypes.h"
#include "InventoryBubbleActor.generated.h"

class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;
class UCurveFloat;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UInventoryBubbleGrabAdapter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryBubbleItemStoredSignature, AActor*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryBubbleItemRemovedSignature, AActor*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInventoryBubbleItemRejectedSignature, AActor*, Item, EInventoryBubbleRejectReason, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryBubbleStateChangedSignature, EInventoryBubbleState, NewState);

/**
 * A translucent sphere that captures, shrinks and stores exactly one grabbable item.
 *
 * Retrieval is layered so the plugin works with any grab system:
 *   Layer A  ReleaseItem / TryStoreItem - the only code that actually moves state.
 *   Layer B  attachment-change detection - notices any grab system stealing the item.
 *   Layer C  optional VR Template / VR Expansion adapters that just call Layer A.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = "Inventory Bubble", meta = (DisplayName = "Inventory Bubble Actor"))
class INVENTORYBUBBLE_API AInventoryBubbleActor : public AActor
{
	GENERATED_BODY()

public:
	AInventoryBubbleActor();

	// ---------------------------------------------------------------- Components

	/** Root. Everything else hangs off this. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory Bubble|Components")
	TObjectPtr<USceneComponent> BubbleRoot;

	/** The visible sphere. Never collides, so it cannot block hands or physics items. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory Bubble|Components")
	TObjectPtr<UStaticMeshComponent> BubbleMesh;

	/** Query-only overlap trigger. Its radius drives the mesh scale. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory Bubble|Components")
	TObjectPtr<USphereComponent> CaptureVolume;

	/** Where a stored item snaps to. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory Bubble|Components")
	TObjectPtr<USceneComponent> ItemAnchor;

	// ---------------------------------------------------------------- Eligibility

	/** An actor must carry this tag to be storable. Change it for specialised bubbles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Eligibility",
		meta = (ToolTip = "Actor tag a candidate must have. Leave as 'Item' unless you want a specialised bubble."))
	FName ItemTag = TEXT("Item");

	/** Radius of the capture volume in cm. Also drives the visual sphere's scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Eligibility",
		meta = (ClampMin = "1.0", UIMin = "5.0", UIMax = "100.0", Units = "cm"))
	float CaptureRadius = 20.f;

	/** Seconds after a release during which the same actor cannot be re-captured. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Eligibility",
		meta = (ClampMin = "0.0", UIMax = "5.0", Units = "s"))
	float ReCaptureCooldown = 0.75f;

	// ---------------------------------------------------------------- Storage

	/** Stored size as a fraction of the item's original world scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Storage",
		meta = (ClampMin = "0.01", ClampMax = "1.0", UIMin = "0.05", UIMax = "0.5"))
	float ShrinkScale = 0.35f;

	/** Seconds to blend an item in. 0 snaps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Storage",
		meta = (ClampMin = "0.0", UIMax = "2.0", Units = "s"))
	float CaptureBlendTime = 0.35f;

	/** Seconds to blend an item back out on a scripted release. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Storage",
		meta = (ClampMin = "0.0", UIMax = "2.0", Units = "s"))
	float ReleaseBlendTime = 0.2f;

	/** Ease curve sampled over 0..1. Falls back to a smooth ease-in-out when unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Storage")
	TObjectPtr<UCurveFloat> EaseCurve;

	/** Rotation a stored item settles into, relative to the anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Storage")
	FRotator StoredRotation = FRotator::ZeroRotator;

	/** Offset of the anchor from the bubble centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Storage")
	FVector AnchorOffset = FVector::ZeroVector;

	/** Disable physics simulation on the item while it is stored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Storage")
	bool bDisablePhysicsWhileStored = true;

	// ---------------------------------------------------------------- Idle motion

	/**
	 * Cosmetic spin of the stored item.
	 *
	 * Off by default. Idle motion is the only thing that makes an occupied
	 * bubble write a transform every frame, and that write - a teleport of a
	 * live query body - is by far the most expensive thing a bubble does. Turn
	 * it on for the few bubbles meant to draw the eye, not across a level of
	 * them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Idle Motion")
	bool bIdleSpin = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Idle Motion",
		meta = (EditCondition = "bIdleSpin", Units = "deg/s"))
	float IdleSpinRate = 45.f;

	/** Cosmetic vertical bob of the stored item. Off by default - see bIdleSpin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Idle Motion")
	bool bIdleBob = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Idle Motion",
		meta = (EditCondition = "bIdleBob", ClampMin = "0.0", Units = "cm"))
	float IdleBobAmplitude = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Idle Motion",
		meta = (EditCondition = "bIdleBob", ClampMin = "0.0"))
	float IdleBobSpeed = 2.f;

	/**
	 * Seconds between ticks while the bubble is only animating a stored item or
	 * polling for a grab. 0 means every frame.
	 *
	 * Idle motion is cosmetic and reads fine well below display rate, so the
	 * default trades two thirds of the per-frame cost for a sampling rate
	 * nobody notices. Capture and release blends, and the reject flash, always
	 * run at full rate regardless of this value.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Idle Motion",
		meta = (ClampMin = "0.0", UIMax = "0.2", Units = "s"))
	float IdleTickInterval = 0.033f;

	/**
	 * Global off switch for every bubble's idle motion, for a comfort or
	 * performance option in a settings menu.
	 *
	 * Mirrors the InventoryBubble.IdleMotion console variable. Per-bubble
	 * bIdleSpin / bIdleBob still apply on top: this can only take motion away,
	 * never add it. Bubbles already in the level pick the change up immediately.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory Bubble|Idle Motion")
	static void SetIdleMotionEnabled(bool bEnabled);

	/** False when idle motion has been globally switched off. */
	UFUNCTION(BlueprintPure, Category = "Inventory Bubble|Idle Motion")
	static bool IsIdleMotionEnabled();

	/**
	 * Re-evaluate whether this bubble needs to tick. Call after changing any
	 * idle motion property at runtime; the global switch does it for you.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory Bubble|Idle Motion")
	void RefreshIdleMotionState();

	// ---------------------------------------------------------------- Appearance

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Appearance")
	FLinearColor BubbleColorEmpty = FLinearColor(0.15f, 0.45f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Appearance")
	FLinearColor BubbleColorOccupied = FLinearColor(0.25f, 0.7f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Appearance")
	FLinearColor BubbleColorRejecting = FLinearColor(1.f, 0.15f, 0.1f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Appearance",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BubbleOpacity = 0.35f;

	/** How long the reject flash lasts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Appearance",
		meta = (ClampMin = "0.0", UIMax = "2.0", Units = "s"))
	float RejectFlashTime = 0.25f;

	/** Optional material for the sphere. When unset the bubble keeps whatever the mesh has. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Appearance")
	TObjectPtr<UMaterialInterface> BubbleMaterial;

	/** Scalar/vector parameter names, so a custom material can use its own naming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Appearance|Parameter Names", AdvancedDisplay)
	FName BaseColorParameterName = TEXT("BaseColor");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Appearance|Parameter Names", AdvancedDisplay)
	FName OpacityParameterName = TEXT("Opacity");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Appearance|Parameter Names", AdvancedDisplay)
	FName EmissiveParameterName = TEXT("EmissiveIntensity");

	// ---------------------------------------------------------------- Integration

	/** Layer B: treat "the item stopped being attached to us" as a grab. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Integration")
	bool bAutoDetectGrab = true;

	/** Layer C: bind to a UE5 VR Template GrabComponent when the item has one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Integration")
	bool bUseGrabComponentAdapter = true;

	/** Refuse items that are already attached to something hand-like. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Integration")
	bool bRejectItemsHeldByPlayer = true;

	/**
	 * Re-test an item that was refused on entry (held, on cooldown, bubble busy)
	 * while it stays inside the volume, so releasing it inside the bubble stores
	 * it. Without this, capture can only ever happen on the entry event.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Storage")
	bool bCaptureOnRelease = true;

	/** How often a refused-but-still-inside candidate is re-tested. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Storage",
		meta = (ClampMin = "0.02", UIMax = "1.0", Units = "s", EditCondition = "bCaptureOnRelease"))
	float RecheckInterval = 0.1f;

	// ---------------------------------------------------------------- Debug

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Bubble|Debug")
	bool bShowDebug = false;

	// ---------------------------------------------------------------- Events

	UPROPERTY(BlueprintAssignable, Category = "Inventory Bubble|Events")
	FInventoryBubbleItemStoredSignature OnItemStored;

	UPROPERTY(BlueprintAssignable, Category = "Inventory Bubble|Events")
	FInventoryBubbleItemRemovedSignature OnItemRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Inventory Bubble|Events")
	FInventoryBubbleItemRejectedSignature OnItemRejected;

	UPROPERTY(BlueprintAssignable, Category = "Inventory Bubble|Events")
	FInventoryBubbleStateChangedSignature OnBubbleStateChanged;

	// ---------------------------------------------------------------- Layer A API

	/**
	 * Store an item now, bypassing the overlap path. Returns false and fires
	 * OnItemRejected when the item is not acceptable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory Bubble|Actions")
	bool TryStoreItem(AActor* Item);

	/**
	 * Release the stored item and restore its original transform, physics and
	 * collision. Returns the released actor, or null when the bubble was empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory Bubble|Actions")
	AActor* ReleaseItem(bool bRestorePhysics = true);

	/** Release without the blend - used for teardown and for scripted ejection. */
	UFUNCTION(BlueprintCallable, Category = "Inventory Bubble|Actions")
	AActor* ForceEject();

	/** Forget every re-capture cooldown this bubble is tracking. */
	UFUNCTION(BlueprintCallable, Category = "Inventory Bubble|Actions")
	void ClearCooldowns();

	/**
	 * Layer C hook. Call this from Blueprint when your grab system takes the item
	 * (for example from a VR Template GrabComponent's On Grabbed).
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory Bubble|Actions")
	void NotifyItemGrabbed(AActor* Item);

	// ---------------------------------------------------------------- Queries

	UFUNCTION(BlueprintPure, Category = "Inventory Bubble|Queries")
	bool IsOccupied() const;

	UFUNCTION(BlueprintPure, Category = "Inventory Bubble|Queries")
	AActor* GetStoredItem() const;

	UFUNCTION(BlueprintPure, Category = "Inventory Bubble|Queries")
	EInventoryBubbleState GetBubbleState() const { return BubbleState; }

	UFUNCTION(BlueprintPure, Category = "Inventory Bubble|Queries")
	bool IsOnCooldown(const AActor* Item) const;

	// ---------------------------------------------------------------- Overridables

	/** Default rules: tag match, not occupied, not on cooldown, not held, interface allows. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory Bubble|Rules")
	bool CanAcceptItem(AActor* Candidate);
	virtual bool CanAcceptItem_Implementation(AActor* Candidate);

	/** World transform a stored item should settle into. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory Bubble|Rules")
	FTransform GetStoredItemTransform(AActor* Item) const;
	virtual FTransform GetStoredItemTransform_Implementation(AActor* Item) const;

	UFUNCTION(BlueprintNativeEvent, Category = "Inventory Bubble|Events")
	void OnCaptureStarted(AActor* Item);
	virtual void OnCaptureStarted_Implementation(AActor* Item) {}

	UFUNCTION(BlueprintNativeEvent, Category = "Inventory Bubble|Events")
	void OnCaptureFinished(AActor* Item);
	virtual void OnCaptureFinished_Implementation(AActor* Item) {}

	UFUNCTION(BlueprintNativeEvent, Category = "Inventory Bubble|Events")
	void OnReleaseStarted(AActor* Item);
	virtual void OnReleaseStarted_Implementation(AActor* Item) {}

	UFUNCTION(BlueprintNativeEvent, Category = "Inventory Bubble|Events")
	void OnReleaseFinished(AActor* Item);
	virtual void OnReleaseFinished_Implementation(AActor* Item) {}

	// ---------------------------------------------------------------- AActor

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	UFUNCTION()
	void HandleCaptureOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleCaptureOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** Re-test candidates still inside the volume that were refused on entry. */
	void RecheckPendingCandidates();

	/** Starts the recheck timer while candidates are waiting, stops it otherwise. */
	void UpdateRecheckTimer();

	UFUNCTION()
	void HandleStoredItemDestroyed(AActor* DestroyedActor);

	/** Snapshot, then neutralise, the item's physics and collision. */
	void CaptureItemState(AActor* Item);

	/** Put back everything CaptureItemState changed. */
	void RestoreItemState(AActor* Item, bool bRestorePhysics);

	void SetBubbleState(EInventoryBubbleState NewState);
	void RefreshMaterialParameters();
	void RefreshTickEnabled();
	void ApplyCaptureRadius();

	/** Reject helper - fires the event and starts the flash. */
	void RejectCandidate(AActor* Candidate, EInventoryBubbleRejectReason Reason);

	EInventoryBubbleRejectReason DetermineRejectReason(AActor* Candidate) const;

	float EvaluateEase(float Alpha) const;

	/** True when the actor looks like it is attached to a hand / controller. */
	bool IsActorHeldByPlayer(const AActor* Actor) const;

	void PruneCooldowns();

	/**
	 * True when this bubble should be animating its stored item right now.
	 * Tick and RefreshTickEnabled must agree on this or a bubble either ticks
	 * forever doing nothing or never wakes up to animate, so both go through here.
	 */
	bool IsIdleMotionActive() const;

	/** True while Layer B still has to poll because no Layer C event took over. */
	bool NeedsGrabPolling() const;

	/** Layer B: has something taken our item out from under us? */
	void CheckForExternalGrab();

	/**
	 * Rewrite a live VRE grip's cached scale after a restore.
	 *
	 * VRExpansionPlugin re-imposes the grip's RelativeTransform scale every
	 * tick, and it cached that scale at grip time - while the item was still
	 * shrunk. Without this the item is restored and instantly re-shrunk.
	 * No-op when the plugin is built without VRE.
	 */
	void RestoreVREGripScale(AActor* Item, const FVector& OriginalScale);

	/**
	 * Hand physics back to the item only once the grip system has let go.
	 *
	 * Re-enabling simulation while a grip is live recreates the body underneath
	 * the grip's constraint, which leaves the grip registered but inert - the
	 * item then hangs in the air when finally released. The bubble restores
	 * scale and collision immediately and waits for the release to restore
	 * simulation and gravity.
	 */
	void ScheduleDeferredPhysicsRestore(AActor* Item, const FInventoryBubbleStoredItemState& State);
	void TickDeferredPhysicsRestore();

	/**
	 * Take over the pending physics restore for Item - from this bubble, or from
	 * whichever one is still waiting on it - and stop it from ever firing.
	 *
	 * A deferred restore that lands after the item has been stored again switches
	 * simulation on underneath the anchor, which tears the item off the bubble and
	 * drops it. Capture consumes the pending state instead, so the physics the
	 * item is owed is recorded in the fresh snapshot and handed back at the next
	 * real release rather than being lost.
	 *
	 * Returns an invalid state when nothing was pending.
	 */
	FInventoryBubbleStoredItemState TakePendingPhysicsRestore(AActor* Item);

	/** The half of TakePendingPhysicsRestore that only looks at this bubble. */
	bool TakeOwnPendingPhysicsRestore(AActor* Item, FInventoryBubbleStoredItemState& OutState);

	void FinishCapture();
	void FinishRelease();

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Inventory Bubble|Runtime")
	EInventoryBubbleState BubbleState = EInventoryBubbleState::Empty;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> StoredItem;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BubbleMID;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryBubbleGrabAdapter> GrabAdapter;

	/**
	 * Set while the Layer C adapter holds a GrabComponent binding for the
	 * stored item, which makes the Layer B per-frame poll redundant.
	 */
	bool bGrabEventsBound = false;

	/** Snapshot taken at capture, consumed at release. */
	FInventoryBubbleStoredItemState StoredState;

	/** Item waiting on a grip release before its physics comes back. */
	TWeakObjectPtr<AActor> DeferredPhysicsItem;
	FInventoryBubbleStoredItemState DeferredPhysicsState;
	FTimerHandle DeferredPhysicsTimer;

	/** Actor -> world time at which it becomes capturable again. */
	TMap<TWeakObjectPtr<AActor>, double> CooldownUntil;

	/** Blend bookkeeping. */
	float BlendElapsed = 0.f;
	float BlendDuration = 0.f;
	FTransform BlendStartWorld = FTransform::Identity;
	FTransform BlendTargetWorld = FTransform::Identity;
	bool bRestorePhysicsOnReleaseFinish = true;

	/** Idle motion bookkeeping. */
	float IdleTime = 0.f;

	/** Inside the volume but refused on entry - re-tested while they stay. */
	TSet<TWeakObjectPtr<AActor>> PendingCandidates;

	FTimerHandle RecheckTimer;

	/** Reject flash countdown. */
	float RejectFlashRemaining = 0.f;
};
