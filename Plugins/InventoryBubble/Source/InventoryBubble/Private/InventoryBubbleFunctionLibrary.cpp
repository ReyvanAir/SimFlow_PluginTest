// Copyright InventoryBubble. All Rights Reserved.

#include "InventoryBubbleFunctionLibrary.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "InventoryBubbleActor.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	/** Shared scan. Predicate decides which bubbles are candidates. */
	template <typename PredicateType>
	AInventoryBubbleActor* FindNearestBubbleWhere(const UObject* WorldContextObject, const FVector& Origin,
		float MaxDistance, PredicateType&& Predicate)
	{
		if (WorldContextObject == nullptr)
		{
			return nullptr;
		}

		UWorld* World = GEngine
			? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
			: nullptr;

		if (World == nullptr)
		{
			return nullptr;
		}

		const float MaxDistanceSq = (MaxDistance > 0.f) ? (MaxDistance * MaxDistance) : TNumericLimits<float>::Max();

		AInventoryBubbleActor* Best = nullptr;
		float BestDistanceSq = TNumericLimits<float>::Max();

		for (TActorIterator<AInventoryBubbleActor> It(World); It; ++It)
		{
			AInventoryBubbleActor* Bubble = *It;
			if (!IsValid(Bubble) || !Predicate(Bubble))
			{
				continue;
			}

			const float DistanceSq = FVector::DistSquared(Origin, Bubble->GetActorLocation());
			if (DistanceSq <= MaxDistanceSq && DistanceSq < BestDistanceSq)
			{
				Best = Bubble;
				BestDistanceSq = DistanceSq;
			}
		}

		return Best;
	}
}

AInventoryBubbleActor* UInventoryBubbleFunctionLibrary::FindNearestFreeBubble(const UObject* WorldContextObject,
	FVector Origin, float MaxDistance, AActor* Item)
{
	return FindNearestBubbleWhere(WorldContextObject, Origin, MaxDistance,
		[Item](AInventoryBubbleActor* Bubble)
		{
			if (Bubble->IsOccupied())
			{
				return false;
			}
			// With no item to test against, "free" is enough.
			return Item == nullptr || Bubble->CanAcceptItem(Item);
		});
}

AInventoryBubbleActor* UInventoryBubbleFunctionLibrary::FindNearestBubble(const UObject* WorldContextObject,
	FVector Origin, float MaxDistance)
{
	return FindNearestBubbleWhere(WorldContextObject, Origin, MaxDistance,
		[](AInventoryBubbleActor*) { return true; });
}

AInventoryBubbleActor* UInventoryBubbleFunctionLibrary::GetBubbleHoldingItem(const UObject* WorldContextObject, AActor* Item)
{
	if (!IsValid(Item))
	{
		return nullptr;
	}

	return FindNearestBubbleWhere(WorldContextObject, Item->GetActorLocation(), 0.f,
		[Item](AInventoryBubbleActor* Bubble) { return Bubble->GetStoredItem() == Item; });
}

bool UInventoryBubbleFunctionLibrary::IsActorStored(const UObject* WorldContextObject, AActor* Item)
{
	return GetBubbleHoldingItem(WorldContextObject, Item) != nullptr;
}

AInventoryBubbleActor* UInventoryBubbleFunctionLibrary::StoreInNearestBubble(const UObject* WorldContextObject,
	AActor* Item, float MaxDistance)
{
	if (!IsValid(Item))
	{
		return nullptr;
	}

	AInventoryBubbleActor* Bubble = FindNearestFreeBubble(WorldContextObject, Item->GetActorLocation(), MaxDistance, Item);
	if (Bubble && Bubble->TryStoreItem(Item))
	{
		return Bubble;
	}

	return nullptr;
}

AActor* UInventoryBubbleFunctionLibrary::ReleaseFromNearestBubble(const UObject* WorldContextObject,
	FVector Origin, float MaxDistance)
{
	AInventoryBubbleActor* Bubble = FindNearestBubbleWhere(WorldContextObject, Origin, MaxDistance,
		[](AInventoryBubbleActor* Candidate) { return Candidate->IsOccupied(); });

	return Bubble ? Bubble->ReleaseItem(true) : nullptr;
}

void UInventoryBubbleFunctionLibrary::GetAllBubbles(const UObject* WorldContextObject,
	TArray<AInventoryBubbleActor*>& OutBubbles)
{
	OutBubbles.Reset();

	if (WorldContextObject == nullptr || GEngine == nullptr)
	{
		return;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<AInventoryBubbleActor> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			OutBubbles.Add(*It);
		}
	}
}
