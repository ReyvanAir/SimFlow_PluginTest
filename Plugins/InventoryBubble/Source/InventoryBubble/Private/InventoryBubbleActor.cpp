// Copyright InventoryBubble. All Rights Reserved.

#include "InventoryBubbleActor.h"

#include "Adapters/InventoryBubbleGrabAdapter.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "InventoryBubble.h"
#include "InventoryBubbleItemInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"

namespace InventoryBubbleConstants
{
	/** The engine's basic sphere is 100 uu across, so radius 50 at scale 1. */
	static constexpr float BasicSphereRadius = 50.f;
}

namespace
{
	int32 GInventoryBubbleIdleMotion = 1;

	FAutoConsoleVariableRef CVarInventoryBubbleIdleMotion(
		TEXT("InventoryBubble.IdleMotion"),
		GInventoryBubbleIdleMotion,
		TEXT("Global switch for inventory bubble idle spin / bob.\n")
		TEXT("  1: per-bubble bIdleSpin / bIdleBob apply (default)\n")
		TEXT("  0: idle motion is off everywhere, whatever the per-bubble settings say.\n")
		TEXT("Bubbles stop ticking entirely when nothing else needs them, so this is\n")
		TEXT("a real performance switch and not just a visual one."),
		ECVF_Scalability);

	/**
	 * Toggling the switch has to wake or sleep bubbles that already exist -
	 * without this they keep whatever tick state they had when it changed.
	 *
	 * The sink fires for any console variable, so the last value is cached and
	 * the sweep only runs when this one actually moved.
	 */
	int32 GInventoryBubbleIdleMotionApplied = 1;

	void OnInventoryBubbleIdleMotionChanged()
	{
		if (GInventoryBubbleIdleMotion == GInventoryBubbleIdleMotionApplied)
		{
			return;
		}
		GInventoryBubbleIdleMotionApplied = GInventoryBubbleIdleMotion;

		for (TObjectIterator<AInventoryBubbleActor> It; It; ++It)
		{
			AInventoryBubbleActor* Bubble = *It;
			if (IsValid(Bubble) && !Bubble->HasAnyFlags(RF_ClassDefaultObject) && Bubble->GetWorld())
			{
				Bubble->RefreshIdleMotionState();
			}
		}
	}

	/**
	 * True when Item is currently sitting in a bubble's anchor.
	 *
	 * Capture always attaches the item under the bubble holding it, so one hop
	 * up the attachment answers this - cheap enough for a timer callback.
	 */
	bool IsItemStoredInBubble(const AActor* Item)
	{
		const USceneComponent* Root = IsValid(Item) ? Item->GetRootComponent() : nullptr;
		const USceneComponent* AttachParent = Root ? Root->GetAttachParent() : nullptr;
		const AInventoryBubbleActor* Holder = AttachParent
			? Cast<AInventoryBubbleActor>(AttachParent->GetOwner())
			: nullptr;

		return Holder != nullptr && Holder->GetStoredItem() == Item;
	}

	FAutoConsoleVariableSink CVarInventoryBubbleIdleMotionSink(
		FConsoleCommandDelegate::CreateStatic(&OnInventoryBubbleIdleMotionChanged));
}

#if WITH_VR_EXPANSION_PLUGIN
#include "VRBPDatatypes.h"
#include "GripMotionControllerComponent.h"
#include "VRGripInterface.h"

namespace
{
	/**
	 * Ask VRExpansionPlugin directly whether it is holding this actor.
	 *
	 * This cannot be inferred from attachment. VRE's physics-constraint grips
	 * drive the object through a physics handle and never reparent it, so an
	 * attachment walk sees a free-floating actor and concludes it is not held.
	 * IsHeld is the grip system's own answer, and it is correct for every grip
	 * type VRE supports.
	 */
	bool IsGrippedByVRExpansion(const AActor* Actor)
	{
		if (!IsValid(Actor) || !Actor->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
		{
			return false;
		}

		TArray<FBPGripPair> HoldingControllers;
		bool bIsHeld = false;
		IVRGripInterface::Execute_IsHeld(const_cast<AActor*>(Actor), HoldingControllers, bIsHeld);
		return bIsHeld;
	}
}
#endif


AInventoryBubbleActor::AInventoryBubbleActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	BubbleRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BubbleRoot"));
	SetRootComponent(BubbleRoot);

	CaptureVolume = CreateDefaultSubobject<USphereComponent>(TEXT("CaptureVolume"));
	CaptureVolume->SetupAttachment(BubbleRoot);
	CaptureVolume->SetSphereRadius(CaptureRadius);
	CaptureVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CaptureVolume->SetCollisionObjectType(ECC_WorldDynamic);
	CaptureVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	CaptureVolume->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	CaptureVolume->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	CaptureVolume->SetGenerateOverlapEvents(true);
	CaptureVolume->SetCanEverAffectNavigation(false);

	BubbleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BubbleMesh"));
	BubbleMesh->SetupAttachment(BubbleRoot);
	// Never collide - the sphere must not block hands or physics items.
	BubbleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BubbleMesh->SetCollisionProfileName(TEXT("NoCollision"));
	BubbleMesh->SetGenerateOverlapEvents(false);
	BubbleMesh->SetCastShadow(false);
	BubbleMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshFinder.Succeeded())
	{
		BubbleMesh->SetStaticMesh(SphereMeshFinder.Object);
	}

	ItemAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("ItemAnchor"));
	ItemAnchor->SetupAttachment(BubbleRoot);
	ItemAnchor->SetCanEverAffectNavigation(false);
}

// ---------------------------------------------------------------------------
//  Construction / lifecycle
// ---------------------------------------------------------------------------

void AInventoryBubbleActor::ApplyCaptureRadius()
{
	if (CaptureVolume)
	{
		CaptureVolume->SetSphereRadius(CaptureRadius, /*bUpdateOverlaps*/ true);
	}

	// One value drives both, so a designer never resizes the mesh separately.
	if (BubbleMesh)
	{
		const float MeshScale = CaptureRadius / InventoryBubbleConstants::BasicSphereRadius;
		BubbleMesh->SetRelativeScale3D(FVector(MeshScale));
	}

	if (ItemAnchor)
	{
		ItemAnchor->SetRelativeLocation(AnchorOffset);
	}
}

void AInventoryBubbleActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyCaptureRadius();
}

void AInventoryBubbleActor::BeginPlay()
{
	Super::BeginPlay();

	ApplyCaptureRadius();

	if (CaptureVolume)
	{
		CaptureVolume->OnComponentBeginOverlap.AddDynamic(this, &AInventoryBubbleActor::HandleCaptureOverlapBegin);
		CaptureVolume->OnComponentEndOverlap.AddDynamic(this, &AInventoryBubbleActor::HandleCaptureOverlapEnd);
	}

	// Drive appearance through a dynamic instance so state changes are parameter
	// writes, never asset swaps.
	if (BubbleMesh)
	{
		UMaterialInterface* SourceMaterial = BubbleMaterial
			? ToRawPtr(BubbleMaterial)
			: BubbleMesh->GetMaterial(0);

		if (SourceMaterial)
		{
			BubbleMID = UMaterialInstanceDynamic::Create(SourceMaterial, this);
			if (BubbleMID)
			{
				BubbleMesh->SetMaterial(0, BubbleMID);
			}
		}
		else
		{
			UE_LOG(LogInventoryBubble, Verbose,
				TEXT("'%s' has no bubble material - appearance parameters will be skipped."), *GetName());
		}
	}

	SetBubbleState(EInventoryBubbleState::Empty);
	RefreshMaterialParameters();
	RefreshTickEnabled();
}

void AInventoryBubbleActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Never leave an item attached with its physics disabled.
	if (StoredItem.IsValid())
	{
		ForceEject();
	}

	if (GrabAdapter)
	{
		GrabAdapter->Unbind();
		GrabAdapter = nullptr;
	}
	bGrabEventsBound = false;

	if (CaptureVolume)
	{
		CaptureVolume->OnComponentBeginOverlap.RemoveDynamic(this, &AInventoryBubbleActor::HandleCaptureOverlapBegin);
		CaptureVolume->OnComponentEndOverlap.RemoveDynamic(this, &AInventoryBubbleActor::HandleCaptureOverlapEnd);
	}

	PendingCandidates.Empty();
	DeferredPhysicsItem = nullptr;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecheckTimer);
		World->GetTimerManager().ClearTimer(DeferredPhysicsTimer);
	}

	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void AInventoryBubbleActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ApplyCaptureRadius();
	RefreshMaterialParameters();
}
#endif

// ---------------------------------------------------------------------------
//  Tick
// ---------------------------------------------------------------------------

void AInventoryBubbleActor::SetIdleMotionEnabled(bool bEnabled)
{
	if (IConsoleVariable* Variable =
		IConsoleManager::Get().FindConsoleVariable(TEXT("InventoryBubble.IdleMotion")))
	{
		// Routed through the console variable rather than a separate flag so the
		// sink is the single thing that refreshes live bubbles.
		Variable->Set(bEnabled ? TEXT("1") : TEXT("0"), ECVF_SetByGameSetting);
	}
}

bool AInventoryBubbleActor::IsIdleMotionEnabled()
{
	return GInventoryBubbleIdleMotion != 0;
}

bool AInventoryBubbleActor::IsIdleMotionActive() const
{
	return (bIdleSpin || bIdleBob) && GInventoryBubbleIdleMotion != 0;
}

bool AInventoryBubbleActor::NeedsGrabPolling() const
{
	// A GrabComponent binding reports grabs as events, so the poll adds nothing.
	return bAutoDetectGrab && !bGrabEventsBound;
}

void AInventoryBubbleActor::RefreshIdleMotionState()
{
	RefreshTickEnabled();
}

void AInventoryBubbleActor::RefreshTickEnabled()
{
	// Anything the player is actively watching runs at full rate. Idle motion
	// and grab polling do not - they are sampled, and can afford to be coarse.
	const bool bNeedsFullRate =
		BubbleState == EInventoryBubbleState::Capturing ||
		BubbleState == EInventoryBubbleState::Releasing ||
		RejectFlashRemaining > 0.f ||
		bShowDebug;

	const bool bNeedsSampledRate =
		BubbleState == EInventoryBubbleState::Occupied &&
		(IsIdleMotionActive() || NeedsGrabPolling());

	SetActorTickEnabled(bNeedsFullRate || bNeedsSampledRate);
	SetActorTickInterval((bNeedsSampledRate && !bNeedsFullRate)
		? FMath::Max(0.f, IdleTickInterval)
		: 0.f);
}

void AInventoryBubbleActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (RejectFlashRemaining > 0.f)
	{
		RejectFlashRemaining = FMath::Max(0.f, RejectFlashRemaining - DeltaSeconds);
		RefreshMaterialParameters();
	}

	AActor* Item = StoredItem.Get();

	switch (BubbleState)
	{
	case EInventoryBubbleState::Capturing:
	{
		if (!IsValid(Item))
		{
			SetBubbleState(EInventoryBubbleState::Empty);
			break;
		}

		BlendElapsed += DeltaSeconds;
		const float Alpha = (BlendDuration > KINDA_SMALL_NUMBER)
			? FMath::Clamp(BlendElapsed / BlendDuration, 0.f, 1.f)
			: 1.f;
		const float Eased = EvaluateEase(Alpha);

		FTransform Current;
		Current.SetLocation(FMath::Lerp(BlendStartWorld.GetLocation(), BlendTargetWorld.GetLocation(), Eased));
		Current.SetRotation(FQuat::Slerp(BlendStartWorld.GetRotation(), BlendTargetWorld.GetRotation(), Eased));
		Current.SetScale3D(FMath::Lerp(BlendStartWorld.GetScale3D(), BlendTargetWorld.GetScale3D(), Eased));
		Item->SetActorTransform(Current, false, nullptr, ETeleportType::TeleportPhysics);

		if (Alpha >= 1.f)
		{
			FinishCapture();
		}
		break;
	}

	case EInventoryBubbleState::Releasing:
	{
		if (!IsValid(Item))
		{
			FinishRelease();
			break;
		}

		BlendElapsed += DeltaSeconds;
		const float Alpha = (BlendDuration > KINDA_SMALL_NUMBER)
			? FMath::Clamp(BlendElapsed / BlendDuration, 0.f, 1.f)
			: 1.f;
		const float Eased = EvaluateEase(Alpha);

		FTransform Current;
		Current.SetLocation(FMath::Lerp(BlendStartWorld.GetLocation(), BlendTargetWorld.GetLocation(), Eased));
		Current.SetRotation(FQuat::Slerp(BlendStartWorld.GetRotation(), BlendTargetWorld.GetRotation(), Eased));
		Current.SetScale3D(FMath::Lerp(BlendStartWorld.GetScale3D(), BlendTargetWorld.GetScale3D(), Eased));
		Item->SetActorTransform(Current, false, nullptr, ETeleportType::TeleportPhysics);

		if (Alpha >= 1.f)
		{
			FinishRelease();
		}
		break;
	}

	case EInventoryBubbleState::Occupied:
	{
		if (!IsValid(Item))
		{
			// The stored actor was destroyed underneath us - go back to empty.
			StoredItem = nullptr;
			StoredState.Reset();
			SetBubbleState(EInventoryBubbleState::Empty);
			break;
		}

		if (NeedsGrabPolling())
		{
			CheckForExternalGrab();
			// CheckForExternalGrab may have emptied the bubble.
			if (BubbleState != EInventoryBubbleState::Occupied)
			{
				break;
			}
			Item = StoredItem.Get();
			if (!IsValid(Item))
			{
				break;
			}
		}

		if (IsIdleMotionActive())
		{
			IdleTime += DeltaSeconds;

			const FTransform AnchorTransform = GetStoredItemTransform(Item);
			FTransform Desired = AnchorTransform;

			if (bIdleSpin)
			{
				const FQuat Spin(FVector::UpVector, FMath::DegreesToRadians(IdleSpinRate * IdleTime));
				Desired.SetRotation(AnchorTransform.GetRotation() * Spin);
			}

			if (bIdleBob)
			{
				const float Offset = FMath::Sin(IdleTime * IdleBobSpeed * 2.f * PI) * IdleBobAmplitude;
				Desired.SetLocation(AnchorTransform.GetLocation() + FVector(0.f, 0.f, Offset));
			}

			Item->SetActorTransform(Desired, false, nullptr, ETeleportType::TeleportPhysics);
		}
		break;
	}

	case EInventoryBubbleState::Empty:
	default:
		break;
	}

	PruneCooldowns();

	if (bShowDebug)
	{
		const FColor DebugColor =
			BubbleState == EInventoryBubbleState::Empty ? FColor::Green :
			BubbleState == EInventoryBubbleState::Occupied ? FColor::Cyan : FColor::Yellow;

		DrawDebugSphere(GetWorld(), GetActorLocation(), CaptureRadius, 16, DebugColor, false, -1.f, 0, 0.5f);
	}

	RefreshTickEnabled();
}

// ---------------------------------------------------------------------------
//  Eligibility
// ---------------------------------------------------------------------------

bool AInventoryBubbleActor::IsOccupied() const
{
	return BubbleState == EInventoryBubbleState::Occupied
		|| BubbleState == EInventoryBubbleState::Capturing;
}

AActor* AInventoryBubbleActor::GetStoredItem() const
{
	return StoredItem.Get();
}

bool AInventoryBubbleActor::IsOnCooldown(const AActor* Item) const
{
	if (!IsValid(Item) || !GetWorld())
	{
		return false;
	}

	const double* Until = CooldownUntil.Find(TWeakObjectPtr<AActor>(const_cast<AActor*>(Item)));
	return Until != nullptr && GetWorld()->GetTimeSeconds() < *Until;
}

bool AInventoryBubbleActor::IsActorHeldByPlayer(const AActor* Actor) const
{
	if (!bRejectItemsHeldByPlayer || !IsValid(Actor))
	{
		return false;
	}

#if WITH_VR_EXPANSION_PLUGIN
	if (IsGrippedByVRExpansion(Actor))
	{
		return true;
	}
#endif

	const USceneComponent* Root = Actor->GetRootComponent();
	const USceneComponent* AttachParent = Root ? Root->GetAttachParent() : nullptr;

	if (AttachParent == nullptr || AttachParent == ItemAnchor)
	{
		// Unattached, or already ours - not held.
		return false;
	}

	// Walk the WHOLE attachment chain, and each link's ownership chain.
	//
	// A single-level owner test is not enough. The VR Template parents a held
	// item directly under a component of the pawn, which one level finds, but
	// VRExpansionPlugin parents it under its grasping-hand ACTOR - a plain
	// AActor that is itself attached to the pawn. That misses the pawn, the
	// bubble concludes the item is free, and it swallows an item that is still
	// in the player's hand - which then fights the grip and drops on release.
	for (const USceneComponent* Ancestor = AttachParent; Ancestor; Ancestor = Ancestor->GetAttachParent())
	{
		for (const AActor* OwningActor = Ancestor->GetOwner(); IsValid(OwningActor); OwningActor = OwningActor->GetOwner())
		{
			if (OwningActor->IsA(APawn::StaticClass()))
			{
				return true;
			}
		}
	}

	return false;
}

EInventoryBubbleRejectReason AInventoryBubbleActor::DetermineRejectReason(AActor* Candidate) const
{
	if (!IsValid(Candidate) || Candidate == this || Candidate->IsActorBeingDestroyed())
	{
		return EInventoryBubbleRejectReason::Invalid;
	}

	if (BubbleState == EInventoryBubbleState::Releasing)
	{
		return EInventoryBubbleRejectReason::Busy;
	}

	if (IsOccupied())
	{
		return EInventoryBubbleRejectReason::AlreadyOccupied;
	}

	if (!ItemTag.IsNone() && !Candidate->ActorHasTag(ItemTag))
	{
		return EInventoryBubbleRejectReason::TagMismatch;
	}

	if (IsOnCooldown(Candidate))
	{
		return EInventoryBubbleRejectReason::OnCooldown;
	}

	if (IsActorHeldByPlayer(Candidate))
	{
		return EInventoryBubbleRejectReason::HeldByPlayer;
	}

	if (Candidate->GetClass()->ImplementsInterface(UInventoryBubbleItemInterface::StaticClass()))
	{
		if (!IInventoryBubbleItemInterface::Execute_CanBeStored(Candidate, this))
		{
			return EInventoryBubbleRejectReason::InterfaceRefused;
		}
	}

	return EInventoryBubbleRejectReason::None;
}

bool AInventoryBubbleActor::CanAcceptItem_Implementation(AActor* Candidate)
{
	return DetermineRejectReason(Candidate) == EInventoryBubbleRejectReason::None;
}

FTransform AInventoryBubbleActor::GetStoredItemTransform_Implementation(AActor* Item) const
{
	FTransform Result = ItemAnchor ? ItemAnchor->GetComponentTransform() : GetActorTransform();
	Result.SetRotation((Result.Rotator() + StoredRotation).Quaternion());

	float Multiplier = 1.f;
	if (IsValid(Item) && Item->GetClass()->ImplementsInterface(UInventoryBubbleItemInterface::StaticClass()))
	{
		Multiplier = IInventoryBubbleItemInterface::Execute_GetStoredScaleMultiplier(Item);
		if (Multiplier <= 0.f)
		{
			Multiplier = 1.f;
		}
	}

	const FVector BaseScale = StoredState.bValid ? StoredState.OriginalWorldScale : FVector::OneVector;
	Result.SetScale3D(BaseScale * ShrinkScale * Multiplier);
	return Result;
}

// ---------------------------------------------------------------------------
//  Capture
// ---------------------------------------------------------------------------

void AInventoryBubbleActor::HandleCaptureOverlapBegin(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (!IsValid(OtherActor) || OtherActor == this)
	{
		return;
	}

	// While occupied, other items must be left completely alone - no physics
	// change, no attach, no shrink. Only the rejection event fires.
	const EInventoryBubbleRejectReason Reason = DetermineRejectReason(OtherActor);
	if (Reason != EInventoryBubbleRejectReason::None)
	{
		// A tag mismatch is the common case (walls, floors, the player). Staying
		// quiet there keeps the reject event meaningful.
		UE_LOG(LogInventoryBubble, Verbose, TEXT("'%s' refused '%s' (reason %d)."),
			*GetName(), *OtherActor->GetName(), static_cast<int32>(Reason));

		if (Reason != EInventoryBubbleRejectReason::TagMismatch && Reason != EInventoryBubbleRejectReason::Invalid)
		{
			RejectCandidate(OtherActor, Reason);

			// Held / on cooldown / bubble busy are all temporary. Keep watching the
			// candidate while it stays inside, so letting go of it in the bubble
			// stores it. BeginOverlap alone fires only on entry, so without this a
			// refused item must leave and re-enter to ever be captured.
			if (bCaptureOnRelease && Reason != EInventoryBubbleRejectReason::InterfaceRefused)
			{
				PendingCandidates.Add(TWeakObjectPtr<AActor>(OtherActor));
				UpdateRecheckTimer();
			}
		}
		return;
	}

	if (!CanAcceptItem(OtherActor))
	{
		RejectCandidate(OtherActor, EInventoryBubbleRejectReason::BlueprintRefused);
		return;
	}

	PendingCandidates.Remove(TWeakObjectPtr<AActor>(OtherActor));
	UpdateRecheckTimer();
	TryStoreItem(OtherActor);
}

void AInventoryBubbleActor::HandleCaptureOverlapEnd(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	if (!IsValid(OtherActor))
	{
		return;
	}

	// A multi-body item fires one EndOverlap per component, so only forget the
	// actor once none of it overlaps any more.
	if (CaptureVolume && CaptureVolume->IsOverlappingActor(OtherActor))
	{
		return;
	}

	PendingCandidates.Remove(TWeakObjectPtr<AActor>(OtherActor));
	UpdateRecheckTimer();
}

void AInventoryBubbleActor::RecheckPendingCandidates()
{
	// Forget anything that died or left while it was waiting.
	for (auto It = PendingCandidates.CreateIterator(); It; ++It)
	{
		AActor* Candidate = It->Get();
		if (!IsValid(Candidate) || (CaptureVolume && !CaptureVolume->IsOverlappingActor(Candidate)))
		{
			It.RemoveCurrent();
		}
	}

	if (BubbleState == EInventoryBubbleState::Empty)
	{
		for (auto It = PendingCandidates.CreateIterator(); It; ++It)
		{
			AActor* Candidate = It->Get();

			// Silent test on purpose: an item waiting to be let go must not fire a
			// reject event on every tick of this timer.
			if (!IsValid(Candidate) || DetermineRejectReason(Candidate) != EInventoryBubbleRejectReason::None)
			{
				continue;
			}

			if (!CanAcceptItem(Candidate))
			{
				continue;
			}

			UE_LOG(LogInventoryBubble, Verbose,
				TEXT("'%s' capturing '%s' after it was released inside the volume."),
				*GetName(), *Candidate->GetName());

			It.RemoveCurrent();
			TryStoreItem(Candidate);
			break;
		}
	}

	UpdateRecheckTimer();
}

void AInventoryBubbleActor::UpdateRecheckTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const bool bWanted = bCaptureOnRelease && PendingCandidates.Num() > 0;
	const bool bRunning = World->GetTimerManager().IsTimerActive(RecheckTimer);

	if (bWanted && !bRunning)
	{
		World->GetTimerManager().SetTimer(RecheckTimer, this,
			&AInventoryBubbleActor::RecheckPendingCandidates,
			FMath::Max(RecheckInterval, 0.02f), /*bLoop*/ true);
	}
	else if (!bWanted && bRunning)
	{
		World->GetTimerManager().ClearTimer(RecheckTimer);
	}
}


bool AInventoryBubbleActor::TryStoreItem(AActor* Item)
{
	const EInventoryBubbleRejectReason Reason = DetermineRejectReason(Item);
	if (Reason != EInventoryBubbleRejectReason::None)
	{
		RejectCandidate(Item, Reason);
		return false;
	}

	if (!CanAcceptItem(Item))
	{
		RejectCandidate(Item, EInventoryBubbleRejectReason::BlueprintRefused);
		return false;
	}

	CaptureItemState(Item);

	if (ItemAnchor)
	{
		Item->AttachToComponent(ItemAnchor, FAttachmentTransformRules::KeepWorldTransform);
	}

	StoredItem = Item;
	Item->OnDestroyed.AddDynamic(this, &AInventoryBubbleActor::HandleStoredItemDestroyed);

	BlendStartWorld = Item->GetActorTransform();
	BlendTargetWorld = GetStoredItemTransform(Item);
	BlendElapsed = 0.f;
	BlendDuration = FMath::Max(0.f, CaptureBlendTime);
	IdleTime = 0.f;

	SetBubbleState(EInventoryBubbleState::Capturing);
	OnCaptureStarted(Item);

	UE_LOG(LogInventoryBubble, Log, TEXT("'%s' capturing '%s'."), *GetName(), *Item->GetName());

	if (BlendDuration <= KINDA_SMALL_NUMBER)
	{
		FinishCapture();
	}

	return true;
}

void AInventoryBubbleActor::FinishCapture()
{
	AActor* Item = StoredItem.Get();
	if (!IsValid(Item))
	{
		StoredItem = nullptr;
		StoredState.Reset();
		SetBubbleState(EInventoryBubbleState::Empty);
		return;
	}

	Item->SetActorTransform(BlendTargetWorld, false, nullptr, ETeleportType::TeleportPhysics);
	SetBubbleState(EInventoryBubbleState::Occupied);

	// Layer C: bind to whatever grab framework this item uses, if any.
	if (bUseGrabComponentAdapter)
	{
		if (!GrabAdapter)
		{
			GrabAdapter = NewObject<UInventoryBubbleGrabAdapter>(this);
		}

		// Only a GrabComponent binding is trusted enough to switch the Layer B
		// poll off. VRE items always keep polling: a physics grip never detaches
		// the item and never touches these delegates, so IsGrippedByVRExpansion
		// inside CheckForExternalGrab is the only thing that ever notices it.
		bGrabEventsBound =
			GrabAdapter->BindToItem(Item, this)
			&& GrabAdapter->IsBoundToGrabComponent()
			&& !UInventoryBubbleGrabAdapter::ImplementsVRExpansionGripInterface(Item);

		if (bGrabEventsBound)
		{
			UE_LOG(LogInventoryBubble, Verbose,
				TEXT("'%s' is taking grab notifications from '%s' as events - Layer B polling off."),
				*GetName(), *Item->GetName());
		}
	}

	// After the state change above, so the tick gate sees the bind result.
	RefreshTickEnabled();

	if (Item->GetClass()->ImplementsInterface(UInventoryBubbleItemInterface::StaticClass()))
	{
		IInventoryBubbleItemInterface::Execute_OnStoredInBubble(Item, this);
	}

	OnCaptureFinished(Item);
	OnItemStored.Broadcast(Item);

	UE_LOG(LogInventoryBubble, Log, TEXT("'%s' stored '%s'."), *GetName(), *Item->GetName());
}

void AInventoryBubbleActor::CaptureItemState(AActor* Item)
{
	StoredState.Reset();

	if (!IsValid(Item))
	{
		return;
	}

	// An item taken by a live grip keeps its simulation switched off until that
	// grip lets go, so what sits on its components right now is the suppressed
	// state and not the item's own. Take that pending restore over: it stops the
	// restore landing underneath the item we are about to store - which tears it
	// off the anchor and drops it - and keeps the real physics values for the
	// next release.
	const FInventoryBubbleStoredItemState PendingRestore = TakePendingPhysicsRestore(Item);

	StoredState.bValid = true;
	StoredState.OriginalWorldTransform = Item->GetActorTransform();
	StoredState.OriginalWorldScale = Item->GetActorScale3D();

	if (USceneComponent* Root = Item->GetRootComponent())
	{
		StoredState.PreviousAttachParent = Root->GetAttachParent();
		StoredState.PreviousAttachSocket = Root->GetAttachSocketName();
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Item->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (!IsValid(Primitive))
		{
			continue;
		}

		const FInventoryBubbleComponentState* Pending = PendingRestore.bValid
			? PendingRestore.ComponentStates.FindByPredicate(
				[Primitive](const FInventoryBubbleComponentState& Candidate)
				{
					return Candidate.Component.Get() == Primitive;
				})
			: nullptr;

		// Simulation is the only thing a deferred restore still holds back, but
		// gravity travels in the same snapshot, so both come from it when one is
		// pending.
		const bool bSimulatingNow = Primitive->IsSimulatingPhysics();

		FInventoryBubbleComponentState State;
		State.Component = Primitive;
		State.bSimulatePhysics = Pending ? Pending->bSimulatePhysics : bSimulatingNow;
		State.bGravityEnabled = Pending ? Pending->bGravityEnabled : Primitive->IsGravityEnabled();
		State.CollisionProfileName = Primitive->GetCollisionProfileName();
		State.CollisionEnabled = Primitive->GetCollisionEnabled();
		State.LinearDamping = Primitive->GetLinearDamping();
		State.AngularDamping = Primitive->GetAngularDamping();
		StoredState.ComponentStates.Add(State);

		if (bDisablePhysicsWhileStored)
		{
			if (bSimulatingNow)
			{
				Primitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
				Primitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
				Primitive->SetSimulatePhysics(false);
			}
			Primitive->SetEnableGravity(false);
			// Query-only so a stored item cannot punch the player or other actors.
			Primitive->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}
	}
}

// ---------------------------------------------------------------------------
//  Release
// ---------------------------------------------------------------------------

void AInventoryBubbleActor::RestoreItemState(AActor* Item, bool bRestorePhysics)
{
	if (!IsValid(Item) || !StoredState.bValid)
	{
		return;
	}

	// Scale first: the item must come out exactly the size it went in.
	Item->SetActorScale3D(StoredState.OriginalWorldScale);

	for (const FInventoryBubbleComponentState& State : StoredState.ComponentStates)
	{
		UPrimitiveComponent* Primitive = State.Component.Get();
		if (!IsValid(Primitive))
		{
			continue;
		}

		Primitive->SetCollisionProfileName(State.CollisionProfileName);
		Primitive->SetCollisionEnabled(State.CollisionEnabled);
		Primitive->SetLinearDamping(State.LinearDamping);
		Primitive->SetAngularDamping(State.AngularDamping);
		Primitive->SetEnableGravity(State.bGravityEnabled);

		if (bRestorePhysics && State.bSimulatePhysics)
		{
			Primitive->SetSimulatePhysics(true);
		}
	}
}

AActor* AInventoryBubbleActor::ReleaseItem(bool bRestorePhysics)
{
	AActor* Item = StoredItem.Get();
	if (!IsValid(Item) || BubbleState == EInventoryBubbleState::Empty)
	{
		return nullptr;
	}

	// A grab must be instant - blending would fight the hand holding the item.
	const bool bHeld = IsActorHeldByPlayer(Item);
	const bool bShouldBlend = !bHeld && ReleaseBlendTime > KINDA_SMALL_NUMBER
		&& BubbleState == EInventoryBubbleState::Occupied;

	Item->OnDestroyed.RemoveDynamic(this, &AInventoryBubbleActor::HandleStoredItemDestroyed);

	if (GrabAdapter)
	{
		GrabAdapter->Unbind();
	}
	bGrabEventsBound = false;

	Item->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	OnReleaseStarted(Item);

	if (bShouldBlend)
	{
		// Physics stays off for the blend, then comes back in FinishRelease.
		bRestorePhysicsOnReleaseFinish = bRestorePhysics;
		BlendStartWorld = Item->GetActorTransform();
		BlendTargetWorld = Item->GetActorTransform();
		BlendTargetWorld.SetScale3D(StoredState.OriginalWorldScale);
		BlendElapsed = 0.f;
		BlendDuration = ReleaseBlendTime;
		SetBubbleState(EInventoryBubbleState::Releasing);
		RefreshTickEnabled();
		return Item;
	}

	RestoreItemState(Item, bRestorePhysics);

	if (Item->GetClass()->ImplementsInterface(UInventoryBubbleItemInterface::StaticClass()))
	{
		IInventoryBubbleItemInterface::Execute_OnRemovedFromBubble(Item, this);
	}

	// Stop the item we just handed over from being swallowed again immediately.
	if (GetWorld() && ReCaptureCooldown > 0.f)
	{
		CooldownUntil.Add(TWeakObjectPtr<AActor>(Item), GetWorld()->GetTimeSeconds() + ReCaptureCooldown);
	}

	StoredItem = nullptr;
	StoredState.Reset();
	SetBubbleState(EInventoryBubbleState::Empty);

	OnReleaseFinished(Item);
	OnItemRemoved.Broadcast(Item);

	UE_LOG(LogInventoryBubble, Log, TEXT("'%s' released '%s'."), *GetName(), *Item->GetName());
	return Item;
}

void AInventoryBubbleActor::FinishRelease()
{
	AActor* Item = StoredItem.Get();

	if (IsValid(Item))
	{
		Item->SetActorTransform(BlendTargetWorld, false, nullptr, ETeleportType::TeleportPhysics);
		RestoreItemState(Item, bRestorePhysicsOnReleaseFinish);

		if (Item->GetClass()->ImplementsInterface(UInventoryBubbleItemInterface::StaticClass()))
		{
			IInventoryBubbleItemInterface::Execute_OnRemovedFromBubble(Item, this);
		}

		if (GetWorld() && ReCaptureCooldown > 0.f)
		{
			CooldownUntil.Add(TWeakObjectPtr<AActor>(Item), GetWorld()->GetTimeSeconds() + ReCaptureCooldown);
		}
	}

	StoredItem = nullptr;
	StoredState.Reset();
	SetBubbleState(EInventoryBubbleState::Empty);

	if (IsValid(Item))
	{
		OnReleaseFinished(Item);
		OnItemRemoved.Broadcast(Item);
		UE_LOG(LogInventoryBubble, Log, TEXT("'%s' finished releasing '%s'."), *GetName(), *Item->GetName());
	}
}

AActor* AInventoryBubbleActor::ForceEject()
{
	const float SavedBlend = ReleaseBlendTime;
	ReleaseBlendTime = 0.f;
	AActor* Item = ReleaseItem(true);
	ReleaseBlendTime = SavedBlend;
	return Item;
}

void AInventoryBubbleActor::NotifyItemGrabbed(AActor* Item)
{
	// Only react to the item we are actually holding.
	if (!IsValid(Item) || StoredItem.Get() != Item)
	{
		return;
	}

	if (BubbleState == EInventoryBubbleState::Empty)
	{
		return;
	}

	UE_LOG(LogInventoryBubble, Verbose, TEXT("'%s' notified that '%s' was grabbed."), *GetName(), *Item->GetName());
	ForceEject();
}

void AInventoryBubbleActor::RestoreVREGripScale(AActor* Item, const FVector& OriginalScale)
{
#if WITH_VR_EXPANSION_PLUGIN
	if (!IsValid(Item) || !Item->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
	{
		return;
	}

	TArray<FBPGripPair> HoldingControllers;
	bool bIsHeld = false;
	IVRGripInterface::Execute_IsHeld(Item, HoldingControllers, bIsHeld);
	if (!bIsHeld)
	{
		return;
	}

	for (const FBPGripPair& Pair : HoldingControllers)
	{
		UGripMotionControllerComponent* Controller = Pair.HoldingController;
		if (!IsValid(Controller))
		{
			continue;
		}

		FBPActorGripInformation Grip;
		EBPVRResultSwitch GetResult = EBPVRResultSwitch::OnFailed;
		Controller->GetGripByID(Grip, Pair.GripID, GetResult);
		if (GetResult != EBPVRResultSwitch::OnSucceeded)
		{
			continue;
		}

		// The grip cached the shrunken size when it was made. Put the item's real
		// size back into it, or VRE re-applies the small scale on its next tick.
		FTransform NewRelative = Grip.RelativeTransform;
		NewRelative.SetScale3D(OriginalScale);

		EBPVRResultSwitch SetResult = EBPVRResultSwitch::OnFailed;
		Controller->SetGripRelativeTransform(Grip, SetResult, NewRelative);

		UE_LOG(LogInventoryBubble, Verbose,
			TEXT("'%s' rewrote the VRE grip scale for '%s' to %s."),
			*GetName(), *Item->GetName(), *OriginalScale.ToCompactString());
	}
#else
	(void)Item;
	(void)OriginalScale;
#endif
}


void AInventoryBubbleActor::ScheduleDeferredPhysicsRestore(AActor* Item, const FInventoryBubbleStoredItemState& State)
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(Item))
	{
		return;
	}

	DeferredPhysicsItem = Item;
	DeferredPhysicsState = State;

	UE_LOG(LogInventoryBubble, Verbose,
		TEXT("'%s' deferred the physics restore of '%s' until its grip releases."),
		*GetName(), *Item->GetName());

	World->GetTimerManager().SetTimer(DeferredPhysicsTimer, this,
		&AInventoryBubbleActor::TickDeferredPhysicsRestore, 0.1f, /*bLoop*/ true);
}

void AInventoryBubbleActor::TickDeferredPhysicsRestore()
{
	UWorld* World = GetWorld();
	AActor* Item = DeferredPhysicsItem.Get();

	if (IsValid(Item))
	{
#if WITH_VR_EXPANSION_PLUGIN
		if (IsGrippedByVRExpansion(Item))
		{
			// Still in a hand. The grip system stays in charge of the transform.
			return;
		}
#endif

		if (IsItemStoredInBubble(Item))
		{
			// Back in a bubble while we were waiting. Switching simulation on now
			// tears the item off the anchor and drops it on the floor; the capture
			// that took it owns this state instead.
			UE_LOG(LogInventoryBubble, Verbose,
				TEXT("'%s' dropped the deferred physics restore of '%s' - it is in a bubble again."),
				*GetName(), *Item->GetName());
		}
		else
		{
			for (const FInventoryBubbleComponentState& State : DeferredPhysicsState.ComponentStates)
			{
				UPrimitiveComponent* Primitive = State.Component.Get();
				if (!IsValid(Primitive))
				{
					continue;
				}

				Primitive->SetEnableGravity(State.bGravityEnabled);
				if (State.bSimulatePhysics)
				{
					Primitive->SetSimulatePhysics(true);
				}
			}

			UE_LOG(LogInventoryBubble, Log,
				TEXT("'%s' restored physics on '%s' now that its grip has released."),
				*GetName(), *Item->GetName());
		}
	}

	if (World)
	{
		World->GetTimerManager().ClearTimer(DeferredPhysicsTimer);
	}
	DeferredPhysicsItem = nullptr;
	DeferredPhysicsState.Reset();
}


bool AInventoryBubbleActor::TakeOwnPendingPhysicsRestore(AActor* Item, FInventoryBubbleStoredItemState& OutState)
{
	if (!IsValid(Item) || DeferredPhysicsItem.Get() != Item)
	{
		return false;
	}

	OutState = DeferredPhysicsState;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeferredPhysicsTimer);
	}
	DeferredPhysicsItem = nullptr;
	DeferredPhysicsState.Reset();

	return OutState.bValid;
}

FInventoryBubbleStoredItemState AInventoryBubbleActor::TakePendingPhysicsRestore(AActor* Item)
{
	FInventoryBubbleStoredItemState Pending;

	if (TakeOwnPendingPhysicsRestore(Item, Pending))
	{
		return Pending;
	}

	// Pulling an item out of one bubble and pushing it straight into another is
	// rare, but the pending restore belongs to the bubble that let go of it, so
	// the new holder has to go and find it.
	for (TObjectIterator<AInventoryBubbleActor> It; It; ++It)
	{
		AInventoryBubbleActor* Other = *It;
		if (Other != this
			&& IsValid(Other)
			&& !Other->HasAnyFlags(RF_ClassDefaultObject)
			&& Other->GetWorld() == GetWorld()
			&& Other->TakeOwnPendingPhysicsRestore(Item, Pending))
		{
			return Pending;
		}
	}

	Pending.Reset();
	return Pending;
}

void AInventoryBubbleActor::CheckForExternalGrab()
{
	AActor* Item = StoredItem.Get();
	if (!IsValid(Item) || !ItemAnchor)
	{
		return;
	}

	const USceneComponent* Root = Item->GetRootComponent();
	const USceneComponent* AttachParent = Root ? Root->GetAttachParent() : nullptr;

	// Layer B: any grab system that re-attaches the item to a hand breaks this
	// link, and that is all we need to detect a grab with zero integration.
#if WITH_VR_EXPANSION_PLUGIN
	// A VRE physics grip takes the item without ever detaching it, so the
	// attachment test alone would never notice the player took it back.
	const bool bTakenByGrip = AttachParent != ItemAnchor || IsGrippedByVRExpansion(Item);
#else
	const bool bTakenByGrip = AttachParent != ItemAnchor;
#endif

	if (bTakenByGrip)
	{
		UE_LOG(LogInventoryBubble, Log,
			TEXT("'%s' detected '%s' was taken by an external grab."), *GetName(), *Item->GetName());

		Item->OnDestroyed.RemoveDynamic(this, &AInventoryBubbleActor::HandleStoredItemDestroyed);

		if (GrabAdapter)
		{
			GrabAdapter->Unbind();
		}
		bGrabEventsBound = false;

		// Snapshot before RestoreItemState, because StoredState is reset below.
		// Snapshot before RestoreItemState, because StoredState is reset below.
		const FVector OriginalScale = StoredState.bValid
			? StoredState.OriginalWorldScale
			: Item->GetActorScale3D();
		const FInventoryBubbleStoredItemState StateForLater = StoredState;

#if WITH_VR_EXPANSION_PLUGIN
		const bool bStillGripped = IsGrippedByVRExpansion(Item);
#else
		const bool bStillGripped = false;
#endif

		// Scale and collision go back now. Simulation must not: turning it on
		// under a live grip recreates the body beneath the grip's constraint.
		RestoreItemState(Item, !bStillGripped);
		RestoreVREGripScale(Item, OriginalScale);

		if (bStillGripped)
		{
			ScheduleDeferredPhysicsRestore(Item, StateForLater);
		}

		if (Item->GetClass()->ImplementsInterface(UInventoryBubbleItemInterface::StaticClass()))
		{
			IInventoryBubbleItemInterface::Execute_OnRemovedFromBubble(Item, this);
		}

		if (GetWorld() && ReCaptureCooldown > 0.f)
		{
			CooldownUntil.Add(TWeakObjectPtr<AActor>(Item), GetWorld()->GetTimeSeconds() + ReCaptureCooldown);
		}

		StoredItem = nullptr;
		StoredState.Reset();
		SetBubbleState(EInventoryBubbleState::Empty);

		OnReleaseFinished(Item);
		OnItemRemoved.Broadcast(Item);
	}
}

void AInventoryBubbleActor::HandleStoredItemDestroyed(AActor* DestroyedActor)
{
	if (StoredItem.Get() != DestroyedActor)
	{
		return;
	}

	UE_LOG(LogInventoryBubble, Log,
		TEXT("'%s' had its stored item destroyed - resetting to empty."), *GetName());

	if (GrabAdapter)
	{
		GrabAdapter->Unbind();
	}
	bGrabEventsBound = false;

	StoredItem = nullptr;
	StoredState.Reset();
	SetBubbleState(EInventoryBubbleState::Empty);
}

// ---------------------------------------------------------------------------
//  Presentation helpers
// ---------------------------------------------------------------------------

void AInventoryBubbleActor::SetBubbleState(EInventoryBubbleState NewState)
{
	if (BubbleState == NewState)
	{
		return;
	}

	BubbleState = NewState;
	RefreshMaterialParameters();
	RefreshTickEnabled();
	OnBubbleStateChanged.Broadcast(NewState);
}

void AInventoryBubbleActor::RefreshMaterialParameters()
{
	if (!BubbleMID)
	{
		return;
	}

	FLinearColor Color = (BubbleState == EInventoryBubbleState::Empty)
		? BubbleColorEmpty
		: BubbleColorOccupied;

	float Emissive = 1.f;

	if (RejectFlashRemaining > 0.f && RejectFlashTime > KINDA_SMALL_NUMBER)
	{
		const float FlashAlpha = FMath::Clamp(RejectFlashRemaining / RejectFlashTime, 0.f, 1.f);
		Color = FMath::Lerp(Color, BubbleColorRejecting, FlashAlpha);
		Emissive = FMath::Lerp(1.f, 4.f, FlashAlpha);
	}

	BubbleMID->SetVectorParameterValue(BaseColorParameterName, Color);
	BubbleMID->SetScalarParameterValue(OpacityParameterName, BubbleOpacity);
	BubbleMID->SetScalarParameterValue(EmissiveParameterName, Emissive);
}

void AInventoryBubbleActor::RejectCandidate(AActor* Candidate, EInventoryBubbleRejectReason Reason)
{
	if (!IsValid(Candidate))
	{
		return;
	}

	RejectFlashRemaining = RejectFlashTime;
	RefreshMaterialParameters();
	RefreshTickEnabled();

	UE_LOG(LogInventoryBubble, Verbose, TEXT("'%s' rejected '%s' (reason %d)."),
		*GetName(), *Candidate->GetName(), static_cast<int32>(Reason));

	OnItemRejected.Broadcast(Candidate, Reason);
}

float AInventoryBubbleActor::EvaluateEase(float Alpha) const
{
	const float Clamped = FMath::Clamp(Alpha, 0.f, 1.f);

	if (EaseCurve)
	{
		return EaseCurve->GetFloatValue(Clamped);
	}

	// Smoothstep - a sane default so the curve is genuinely optional.
	return Clamped * Clamped * (3.f - 2.f * Clamped);
}

void AInventoryBubbleActor::ClearCooldowns()
{
	CooldownUntil.Reset();
}

void AInventoryBubbleActor::PruneCooldowns()
{
	if (CooldownUntil.IsEmpty() || !GetWorld())
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	for (auto It = CooldownUntil.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || Now >= It.Value())
		{
			It.RemoveCurrent();
		}
	}
}
