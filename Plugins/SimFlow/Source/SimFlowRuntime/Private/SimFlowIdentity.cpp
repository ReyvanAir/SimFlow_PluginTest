// Copyright SimFlow. All Rights Reserved.

#include "SimFlowIdentity.h"
#include "SimFlowInstance.h"
#include "SimFlowBlackboard.h"
#include "SimFlowRuntimeModule.h"
#include "GameplayTagAssetInterface.h"
#include "Components/ActorComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "SimFlowIdentity"

// ------------------------------------------------------------------ Component

USimFlowIdentityComponent::USimFlowIdentityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = false;
}

FText USimFlowIdentityComponent::GetDisplayNameText() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}
	if (const AActor* Owner = GetOwner())
	{
		return FText::FromString(Owner->GetName());
	}
	return LOCTEXT("UnknownObject", "Unknown Object");
}

void USimFlowIdentityComponent::SetHeld(bool bInIsHeld)
{
	bIsHeld = bInIsHeld;
}

USimFlowIdentityComponent* USimFlowIdentityComponent::FindOn(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<USimFlowIdentityComponent>() : nullptr;
}

bool USimFlowIdentityComponent::IsActorHeld(const AActor* Actor)
{
	const USimFlowIdentityComponent* Identity = FindOn(Actor);
	return Identity && Identity->IsHeld();
}

// ---------------------------------------------------------------------- Query

namespace
{
	/** Highest number of leading tag nodes shared between any pair drawn from the two containers. */
	int32 BestSharedDepth(const FGameplayTagContainer& A, const FGameplayTagContainer& B)
	{
		int32 Best = 0;
		for (const FGameplayTag& TagA : A)
		{
			for (const FGameplayTag& TagB : B)
			{
				Best = FMath::Max(Best, TagA.MatchesTagDepth(TagB));
			}
		}
		return Best;
	}
}

bool FSimFlowActorQuery::IsSet() const
{
	return !SpecificActor.IsNull()
		|| !BlackboardKey.IsNone()
		|| !RequiredTags.IsEmpty()
		|| RequiredClass != nullptr
		|| !RequiredActorTag.IsNone();
}

AActor* FSimFlowActorQuery::Resolve(const USimFlowInstance* Instance) const
{
	if (!SpecificActor.IsNull())
	{
		// Already-loaded level actors resolve without a synchronous load.
		if (AActor* Direct = SpecificActor.Get())
		{
			return Direct;
		}
	}

	if (!BlackboardKey.IsNone() && Instance)
	{
		if (const USimFlowBlackboard* Blackboard = Instance->GetBlackboard())
		{
			if (AActor* FromKey = Cast<AActor>(Blackboard->GetObject(BlackboardKey)))
			{
				return FromKey;
			}
		}
	}

	// Tag / class forms describe a set. Scan for the first member, which is what
	// callers that need a single actor (a zone, for instance) are after.
	if (!RequiredTags.IsEmpty() || RequiredClass || !RequiredActorTag.IsNone())
	{
		const UWorld* World = Instance ? Instance->GetWorld() : nullptr;
		if (World)
		{
			const TSubclassOf<AActor> ScanClass = RequiredClass ? RequiredClass : TSubclassOf<AActor>(AActor::StaticClass());
			for (TActorIterator<AActor> It(World, ScanClass); It; ++It)
			{
				if (MatchActor(*It, Instance) == ESimFlowMatchQuality::Exact)
				{
					return *It;
				}
			}
		}
	}

	return nullptr;
}

ESimFlowMatchQuality FSimFlowActorQuery::MatchActor(const AActor* Actor, const USimFlowInstance* Instance) const
{
	if (!Actor || !IsSet())
	{
		return ESimFlowMatchQuality::NoMatch;
	}

	// 1. A named actor is an exact identity check.
	if (!SpecificActor.IsNull() && SpecificActor.Get() == Actor)
	{
		return ESimFlowMatchQuality::Exact;
	}

	// 2. A target chosen at runtime.
	if (!BlackboardKey.IsNone() && Instance)
	{
		if (const USimFlowBlackboard* Blackboard = Instance->GetBlackboard())
		{
			if (Blackboard->GetObject(BlackboardKey) == Actor)
			{
				return ESimFlowMatchQuality::Exact;
			}
		}
	}

	const bool bClassOk = !RequiredClass || Actor->IsA(RequiredClass);
	const bool bActorTagOk = RequiredActorTag.IsNone() || Actor->ActorHasTag(RequiredActorTag);

	// 3. Identity tags - the form that works for spawned and duplicated objects.
	if (!RequiredTags.IsEmpty())
	{
		const FGameplayTagContainer ActorTags = USimFlowIdentityStatics::GetIdentityTags(Actor);

		const bool bTagsOk = bRequireAllTags ? ActorTags.HasAll(RequiredTags) : ActorTags.HasAny(RequiredTags);
		if (bTagsOk && bClassOk && bActorTagOk)
		{
			return ESimFlowMatchQuality::Exact;
		}

		// Wrong, but how wrong? Sharing enough leading nodes means right family.
		if (BestSharedDepth(ActorTags, RequiredTags) >= FMath::Max(1, MinRelatedTagDepth))
		{
			return ESimFlowMatchQuality::Related;
		}
		return ESimFlowMatchQuality::NoMatch;
	}

	// 4. Class / actor tag only.
	if ((RequiredClass || !RequiredActorTag.IsNone()) && bClassOk && bActorTagOk)
	{
		return ESimFlowMatchQuality::Exact;
	}

	return ESimFlowMatchQuality::NoMatch;
}

ESimFlowMatchQuality FSimFlowActorQuery::MatchObject(const UObject* Object, const USimFlowInstance* Instance) const
{
	if (!Object)
	{
		return ESimFlowMatchQuality::NoMatch;
	}

	if (const AActor* AsActor = Cast<AActor>(Object))
	{
		return MatchActor(AsActor, Instance);
	}

	// A button Blueprint often sends the pressed component rather than the actor.
	if (const UActorComponent* AsComponent = Cast<UActorComponent>(Object))
	{
		return MatchActor(AsComponent->GetOwner(), Instance);
	}

	// Anything else has no actor behind it. The common case is a UMG widget graph
	// broadcasting `self` from a button OnClicked: a UUserWidget is neither an Actor
	// nor an ActorComponent, so it can never satisfy a query.
	//
	// Deliberately not walking GetTypedOuter<AActor>() to find one: a widget made with
	// CreateWidget(PlayerController, ...) outers to the controller, so that would report
	// a confident match against the wrong actor. Failing is better than lying.
	//
	// Warn rather than fail quietly - a query that silently scores NoMatch every time
	// looks exactly like a task that mysteriously never completes.
	UE_LOG(LogSimFlow, Warning,
		TEXT("Event payload '%s' is a %s, which is neither an Actor nor an ActorComponent, so it cannot ")
		TEXT("match this query (which asks for %s). If it came from a widget's OnClicked, broadcast from ")
		TEXT("the owning actor with Payload = self instead of from the widget."),
		*Object->GetName(), *Object->GetClass()->GetName(), *Describe().ToString());

	return ESimFlowMatchQuality::NoMatch;
}

FText FSimFlowActorQuery::Describe() const
{
	if (!SpecificActor.IsNull())
	{
		return FText::FromString(SpecificActor.GetAssetName());
	}
	if (!RequiredTags.IsEmpty())
	{
		return FText::FromString(RequiredTags.ToStringSimple());
	}
	if (!BlackboardKey.IsNone())
	{
		return FText::FromName(BlackboardKey);
	}
	if (RequiredClass)
	{
		return FText::FromString(RequiredClass->GetName());
	}
	if (!RequiredActorTag.IsNone())
	{
		return FText::FromName(RequiredActorTag);
	}
	return LOCTEXT("AnythingQuery", "anything");
}

// -------------------------------------------------------------------- Statics

FGameplayTagContainer USimFlowIdentityStatics::GetIdentityTags(const AActor* Actor)
{
	FGameplayTagContainer Out;
	if (!Actor)
	{
		return Out;
	}

	if (const USimFlowIdentityComponent* Identity = USimFlowIdentityComponent::FindOn(Actor))
	{
		Out.AppendTags(Identity->IdentityTags);
	}

	// Interop with projects that already tag their actors through GAS.
	if (const IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Actor))
	{
		FGameplayTagContainer Owned;
		TagInterface->GetOwnedGameplayTags(Owned);
		Out.AppendTags(Owned);
	}

	return Out;
}

FText USimFlowIdentityStatics::GetIdentityDisplayName(const AActor* Actor)
{
	if (!Actor)
	{
		return LOCTEXT("NothingObject", "nothing");
	}
	if (const USimFlowIdentityComponent* Identity = USimFlowIdentityComponent::FindOn(Actor))
	{
		return Identity->GetDisplayNameText();
	}
	return FText::FromString(Actor->GetName());
}

bool USimFlowIdentityStatics::ActorHasIdentityTag(const AActor* Actor, FGameplayTag Tag)
{
	return Tag.IsValid() && GetIdentityTags(Actor).HasTag(Tag);
}

ESimFlowMatchQuality USimFlowIdentityStatics::MatchActorAgainstQuery(const AActor* Actor, const FSimFlowActorQuery& Query, const USimFlowInstance* Instance)
{
	return Query.MatchActor(Actor, Instance);
}

bool USimFlowIdentityStatics::ActorSatisfiesQuery(const AActor* Actor, const FSimFlowActorQuery& Query, const USimFlowInstance* Instance)
{
	return Query.MatchActor(Actor, Instance) == ESimFlowMatchQuality::Exact;
}

#undef LOCTEXT_NAMESPACE
