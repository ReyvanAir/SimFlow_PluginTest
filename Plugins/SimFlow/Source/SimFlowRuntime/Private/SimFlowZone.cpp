// Copyright SimFlow. All Rights Reserved.

#include "SimFlowZone.h"
#include "SimFlowStatics.h"
#include "SimFlowGameplayTags.h"
#include "SimFlowRuntimeModule.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "SimFlowZone"

ASimFlowZone::ASimFlowZone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetBoxExtent(FVector(50.f, 50.f, 50.f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Box->SetGenerateOverlapEvents(true);
	SetRootComponent(Box);

	Identity = CreateDefaultSubobject<USimFlowIdentityComponent>(TEXT("Identity"));
}

void ASimFlowZone::BeginPlay()
{
	Super::BeginPlay();

	Box->OnComponentBeginOverlap.AddDynamic(this, &ASimFlowZone::HandleBeginOverlap);
	Box->OnComponentEndOverlap.AddDynamic(this, &ASimFlowZone::HandleEndOverlap);

	// Anything already sitting in the zone at BeginPlay should be tracked too.
	TArray<AActor*> Overlapping;
	Box->GetOverlappingActors(Overlapping);
	for (AActor* Actor : Overlapping)
	{
		if (ShouldTrack(Actor) && IndexOf(Actor) == INDEX_NONE)
		{
			FTrackedActor Entry;
			Entry.Actor = Actor;
			Tracked.Add(Entry);
			OnActorEntered.Broadcast(this, Actor);
		}
	}
}

void ASimFlowZone::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Box)
	{
		Box->OnComponentBeginOverlap.RemoveDynamic(this, &ASimFlowZone::HandleBeginOverlap);
		Box->OnComponentEndOverlap.RemoveDynamic(this, &ASimFlowZone::HandleEndOverlap);
	}
	Tracked.Reset();
	Super::EndPlay(Reason);
}

bool ASimFlowZone::ShouldTrack(const AActor* Actor) const
{
	if (!Actor || Actor == this)
	{
		return false;
	}

	if (TrackFilter.IsSet())
	{
		return TrackFilter.MatchActor(Actor, nullptr) == ESimFlowMatchQuality::Exact;
	}

	if (bRequireIdentityComponent)
	{
		return USimFlowIdentityComponent::FindOn(Actor) != nullptr;
	}

	return true;
}

int32 ASimFlowZone::IndexOf(const AActor* Actor) const
{
	return Tracked.IndexOfByPredicate([Actor](const FTrackedActor& Entry)
	{
		return Entry.Actor.Get() == Actor;
	});
}

void ASimFlowZone::HandleBeginOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*Sweep*/)
{
	if (!ShouldTrack(OtherActor) || IndexOf(OtherActor) != INDEX_NONE)
	{
		return;
	}

	FTrackedActor Entry;
	Entry.Actor = OtherActor;
	Tracked.Add(Entry);

	OnActorEntered.Broadcast(this, OtherActor);
}

void ASimFlowZone::HandleEndOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	const int32 Index = IndexOf(OtherActor);
	if (Index == INDEX_NONE)
	{
		return;
	}

	const bool bWasSettled = Tracked[Index].bSettled;
	Tracked.RemoveAt(Index);

	OnActorExited.Broadcast(this, OtherActor);

	if (bWasSettled && bBroadcastFlowEvents)
	{
		USimFlowStatics::BroadcastFlowEvent(this, SimFlowTags::Event_Removed, OtherActor);
	}
}

bool ASimFlowZone::IsActorAtRest(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	// Ask the object first. Attachment is only one of the ways a framework can hold
	// something - VRExpansion holds most grip types with a physics constraint and never
	// reparents - so an object in the player's hand can look detached from out here.
	if (USimFlowIdentityComponent::IsActorHeld(Actor))
	{
		return false;
	}

	// Fall back to attachment for objects that never set the flag.
	if (GetEffectiveRequireDetached() && Actor->GetAttachParentActor() != nullptr)
	{
		return false;
	}

	const float SpeedLimit = GetEffectiveSettleSpeed();
	if (SpeedLimit > 0.f)
	{
		if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
		{
			if (Root->IsSimulatingPhysics())
			{
				return Root->GetPhysicsLinearVelocity().Size() <= SpeedLimit;
			}
		}
		// Non-simulating actors fall back to the actor's own velocity.
		if (Actor->GetVelocity().Size() > SpeedLimit)
		{
			return false;
		}
	}

	return true;
}

float ASimFlowZone::GetEffectiveSettleTime() const
{
	switch (SettleMode)
	{
	case ESimFlowSettleMode::Instant: return 0.f;
	case ESimFlowSettleMode::Custom:  return SettleTime;
	default:                          return StandardSettleTime;
	}
}

float ASimFlowZone::GetEffectiveSettleSpeed() const
{
	switch (SettleMode)
	{
	case ESimFlowSettleMode::Instant: return 0.f;
	case ESimFlowSettleMode::Custom:  return SettleSpeedThreshold;
	default:                          return StandardSettleSpeed;
	}
}

bool ASimFlowZone::GetEffectiveRequireDetached() const
{
	// Only Custom gets to turn this off - the held flag still applies either way.
	return SettleMode != ESimFlowSettleMode::Custom || bRequireDetached;
}

void ASimFlowZone::PostLoad()
{
	Super::PostLoad();

	// Zones saved before Settle Mode existed carry their own numbers. Anything left
	// at the old defaults is Standard by definition; anything tuned means Custom.
	if (SettleMode == ESimFlowSettleMode::Standard
		&& (!FMath::IsNearlyEqual(SettleTime, StandardSettleTime)
			|| !FMath::IsNearlyEqual(SettleSpeedThreshold, StandardSettleSpeed)
			|| !bRequireDetached))
	{
		SettleMode = ESimFlowSettleMode::Custom;
	}
}

#if WITH_EDITOR
void ASimFlowZone::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);

	// Keep the hidden numbers honest, so switching to Custom starts from what the
	// zone was actually doing rather than from whatever was last typed.
	const FName Changed = Event.GetPropertyName();
	if (Changed == GET_MEMBER_NAME_CHECKED(ASimFlowZone, SettleMode) && SettleMode != ESimFlowSettleMode::Custom)
	{
		SettleTime = GetEffectiveSettleTime();
		SettleSpeedThreshold = GetEffectiveSettleSpeed();
		bRequireDetached = true;
	}
}
#endif

void ASimFlowZone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	for (int32 Index = Tracked.Num() - 1; Index >= 0; --Index)
	{
		FTrackedActor& Entry = Tracked[Index];
		AActor* Actor = Entry.Actor.Get();
		if (!Actor)
		{
			Tracked.RemoveAt(Index);
			continue;
		}

		if (!IsActorAtRest(Actor))
		{
			// Picked back up - it has to settle again from scratch.
			Entry.StillTime = 0.f;
			Entry.bSettled = false;
			continue;
		}

		if (Entry.bSettled)
		{
			continue;
		}

		Entry.StillTime += DeltaTime;
		if (Entry.StillTime >= GetEffectiveSettleTime())
		{
			Entry.bSettled = true;

			OnActorSettled.Broadcast(this, Actor);

			if (bBroadcastFlowEvents)
			{
				USimFlowStatics::BroadcastFlowEvent(this, SimFlowTags::Event_Placed, Actor);
			}
		}
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug && Box)
	{
		DrawDebugBox(GetWorld(), Box->GetComponentLocation(), Box->GetScaledBoxExtent(),
			Box->GetComponentQuat(), Tracked.Num() > 0 ? FColor::Green : FColor::Silver, false, -1.f, 0, 1.5f);
	}
#endif
}

TArray<AActor*> ASimFlowZone::GetContainedActors() const
{
	TArray<AActor*> Out;
	Out.Reserve(Tracked.Num());
	for (const FTrackedActor& Entry : Tracked)
	{
		if (AActor* Actor = Entry.Actor.Get())
		{
			Out.Add(Actor);
		}
	}
	return Out;
}

TArray<AActor*> ASimFlowZone::GetSettledActors() const
{
	TArray<AActor*> Out;
	for (const FTrackedActor& Entry : Tracked)
	{
		if (Entry.bSettled)
		{
			if (AActor* Actor = Entry.Actor.Get())
			{
				Out.Add(Actor);
			}
		}
	}
	return Out;
}

bool ASimFlowZone::ContainsActor(const AActor* Actor) const
{
	return IndexOf(Actor) != INDEX_NONE;
}

bool ASimFlowZone::IsActorSettled(const AActor* Actor) const
{
	const int32 Index = IndexOf(Actor);
	return Index != INDEX_NONE && Tracked[Index].bSettled;
}

FText ASimFlowZone::GetDisplayNameText() const
{
	return Identity ? Identity->GetDisplayNameText() : FText::FromString(GetName());
}

#undef LOCTEXT_NAMESPACE
