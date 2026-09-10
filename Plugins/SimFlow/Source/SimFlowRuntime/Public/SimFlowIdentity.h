// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SimFlowTypes.h"
#include "SimFlowIdentity.generated.h"

class USimFlowInstance;

/**
 * Marks an actor as something a flow can recognise.
 *
 * Drop it on the item Blueprint - not on each level instance - and every copy
 * you place or spawn carries the same tags. A flow then asks for
 * "Item.Extinguisher" and any foam or CO2 extinguisher answers, or asks for
 * "Item.Extinguisher.Foam" and only the foam one does.
 *
 * Gameplay tags rather than AActor::Tags on purpose: they are validated at
 * author time, they autocomplete, and they nest - which is what lets a task
 * tell a near miss apart from a completely wrong object.
 */
UCLASS(ClassGroup = SimFlow, meta = (BlueprintSpawnableComponent, DisplayName = "SimFlow Identity"))
class SIMFLOWRUNTIME_API USimFlowIdentityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USimFlowIdentityComponent();

	/** What this object is, e.g. Item.Extinguisher.Foam. Several tags are fine. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FGameplayTagContainer IdentityTags;

	/** Shown in mistake text and tutorial UI. Falls back to the actor label. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FText DisplayName;

	/** Optional stable id, handy for analytics and for addressing this actor from script. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FName IdentityId = NAME_None;

	UFUNCTION(BlueprintPure, Category = "SimFlow|Identity")
	FText GetDisplayNameText() const;

	/**
	 * True while something is holding this object.
	 *
	 * A Zone cannot reliably work this out for itself. Attachment is only one of the
	 * ways a VR framework can hold an object - VRExpansion, for one, holds most grip
	 * types with a physics constraint and never reparents the actor - so an object in
	 * the player's hand can look perfectly detached from the outside.
	 *
	 * Set it from wherever your grab succeeds and clear it on release, and placement
	 * checks become exact regardless of how the grabbing is implemented.
	 */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Identity")
	void SetHeld(bool bInIsHeld);

	UFUNCTION(BlueprintPure, Category = "SimFlow|Identity")
	bool IsHeld() const { return bIsHeld; }

	/** Returns the identity component on Actor, or null. */
	static USimFlowIdentityComponent* FindOn(const AActor* Actor);

	/** True when Actor carries an identity component that is currently flagged held. */
	static bool IsActorHeld(const AActor* Actor);

private:
	UPROPERTY(Transient)
	bool bIsHeld = false;
};

/**
 * "Which object does this task mean?"
 *
 * Fill in exactly one field in the common case. The resolver checks them in
 * order - specific actor, blackboard key, tags, class - and the first one that
 * is set decides the answer.
 *
 * Use SpecificActor for "that button, the one on the left wall". Use
 * RequiredTags for "any foam extinguisher", which is the only option that works
 * for objects spawned at runtime.
 */
USTRUCT(BlueprintType)
struct SIMFLOWRUNTIME_API FSimFlowActorQuery
{
	GENERATED_BODY()

	/** One particular actor placed in the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query")
	TSoftObjectPtr<AActor> SpecificActor;

	/** Reads the target from a blackboard object key - use when it is chosen at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query")
	FName BlackboardKey = NAME_None;

	/** Matches any actor whose SimFlow Identity carries these tags. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query")
	FGameplayTagContainer RequiredTags;

	/** When true every tag above must be present; when false any one of them is enough. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query")
	bool bRequireAllTags = true;

	/** Optional extra narrowing by class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query", AdvancedDisplay)
	TSubclassOf<AActor> RequiredClass;

	/** Fallback for actors you do not own and cannot add an identity component to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query", AdvancedDisplay)
	FName RequiredActorTag = NAME_None;

	/**
	 * How many leading tag nodes two tags must share before a failed match counts
	 * as Related rather than NoMatch. At the default of 2, Item.Extinguisher.CO2
	 * is a near miss for Item.Extinguisher.Foam but Item.Wrench is not.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query", AdvancedDisplay, meta = (ClampMin = "1"))
	int32 MinRelatedTagDepth = 2;

	/** True when at least one field has been filled in. */
	bool IsSet() const;

	/**
	 * Resolves the query to a single actor. Only meaningful for the SpecificActor
	 * and BlackboardKey forms; tag queries describe a set, so this returns the
	 * first matching actor in the world and is mainly useful for finding zones.
	 */
	AActor* Resolve(const USimFlowInstance* Instance) const;

	/** Grades how well Actor answers this query. */
	ESimFlowMatchQuality MatchActor(const AActor* Actor, const USimFlowInstance* Instance) const;

	/** As MatchActor, but unwraps a component payload to its owning actor first. */
	ESimFlowMatchQuality MatchObject(const UObject* Object, const USimFlowInstance* Instance) const;

	/** Human readable form of what this query is asking for, used in mistake text. */
	FText Describe() const;
};

/** Blueprint access to identity tags and query matching. */
UCLASS()
class SIMFLOWRUNTIME_API USimFlowIdentityStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Identity tags for an actor. Reads the SimFlow Identity component first, then
	 * falls back to IGameplayTagAssetInterface so actors from a GAS project work
	 * without a second component.
	 */
	UFUNCTION(BlueprintPure, Category = "SimFlow|Identity")
	static FGameplayTagContainer GetIdentityTags(const AActor* Actor);

	/** Display name from the identity component, falling back to the actor's name. */
	UFUNCTION(BlueprintPure, Category = "SimFlow|Identity")
	static FText GetIdentityDisplayName(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "SimFlow|Identity")
	static bool ActorHasIdentityTag(const AActor* Actor, FGameplayTag Tag);

	/** Grades an actor against a query. Instance may be null when the query uses no blackboard key. */
	UFUNCTION(BlueprintPure, Category = "SimFlow|Identity")
	static ESimFlowMatchQuality MatchActorAgainstQuery(const AActor* Actor, const FSimFlowActorQuery& Query, const USimFlowInstance* Instance);

	/** Convenience for the common "did this pass?" check. */
	UFUNCTION(BlueprintPure, Category = "SimFlow|Identity")
	static bool ActorSatisfiesQuery(const AActor* Actor, const FSimFlowActorQuery& Query, const USimFlowInstance* Instance);
};
