// Copyright InventoryBubble. All Rights Reserved.

#include "Adapters/InventoryBubbleGrabAdapter.h"

#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "InventoryBubble.h"
#include "InventoryBubbleActor.h"
#include "UObject/UnrealType.h"
#include "UObject/Class.h"

#if WITH_VR_EXPANSION_PLUGIN
#include "VRGripInterface.h"
#endif

namespace InventoryBubbleGrabAdapterNames
{
	static const FName OnGrabbed(TEXT("OnGrabbed"));
	static const FName OnDropped(TEXT("OnDropped"));
}

UActorComponent* UInventoryBubbleGrabAdapter::FindGrabComponent(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return nullptr;
	}

	// The UE5 VR Template's GrabComponent is a Blueprint class, so there is no
	// native type to cast to. Matching on the class name is the only option, and
	// it is why this whole path is optional and backed up by Layer B.
	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (!IsValid(Component))
		{
			continue;
		}

		const FString ClassName = Component->GetClass()->GetName();
		if (ClassName.Contains(TEXT("GrabComponent")))
		{
			return Component;
		}
	}

	return nullptr;
}

bool UInventoryBubbleGrabAdapter::ImplementsVRExpansionGripInterface(const AActor* Actor)
{
#if WITH_VR_EXPANSION_PLUGIN
	if (!IsValid(Actor))
	{
		return false;
	}
	return Actor->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass());
#else
	(void)Actor;
	return false;
#endif
}

bool UInventoryBubbleGrabAdapter::BindToItem(AActor* Item, AInventoryBubbleActor* InBubble)
{
	Unbind();

	if (!IsValid(Item) || !IsValid(InBubble))
	{
		return false;
	}

	Bubble = InBubble;
	BoundItem = Item;

	// Prefer a GrabComponent, fall back to the actor itself - VRE items and many
	// custom systems put the delegates straight on the actor.
	UObject* Target = FindGrabComponent(Item);
	if (Target == nullptr)
	{
		Target = Item;
	}
	BoundTarget = Target;

	bool bBoundAny = false;
	bBoundAny |= TryBindParameterlessDelegate(Target, InventoryBubbleGrabAdapterNames::OnGrabbed);
	bBoundAny |= TryBindParameterlessDelegate(Target, InventoryBubbleGrabAdapterNames::OnDropped);

	if (bBoundAny)
	{
		UE_LOG(LogInventoryBubble, Verbose,
			TEXT("Grab adapter bound to '%s' on '%s' (%d delegate(s))."),
			*Target->GetName(), *Item->GetName(), BoundDelegateNames.Num());
	}
	else
	{
		UE_LOG(LogInventoryBubble, Verbose,
			TEXT("No parameterless grab delegate on '%s' - relying on Layer B attachment detection."),
			*Item->GetName());
	}

	return bBoundAny;
}

void UInventoryBubbleGrabAdapter::Unbind()
{
	if (UObject* Target = BoundTarget.Get())
	{
		for (const FName& DelegateName : BoundDelegateNames)
		{
			UnbindParameterlessDelegate(Target, DelegateName);
		}
	}

	BoundDelegateNames.Reset();
	BoundTarget = nullptr;
	BoundItem = nullptr;
	Bubble = nullptr;
}

bool UInventoryBubbleGrabAdapter::IsBoundToGrabComponent() const
{
	const UObject* Target = BoundTarget.Get();
	// BindToItem falls back to the item actor when it finds no GrabComponent,
	// so "target is not the item" is exactly "we found a component".
	return Target != nullptr
		&& BoundDelegateNames.Num() > 0
		&& Target != static_cast<const UObject*>(BoundItem.Get());
}

bool UInventoryBubbleGrabAdapter::TryBindParameterlessDelegate(UObject* Target, FName DelegatePropertyName)
{
	if (!IsValid(Target))
	{
		return false;
	}

	FMulticastDelegateProperty* DelegateProp =
		FindFProperty<FMulticastDelegateProperty>(Target->GetClass(), DelegatePropertyName);

	if (DelegateProp == nullptr)
	{
		return false;
	}

	// Binding a handler with the wrong arity would corrupt the stack when the
	// delegate fires, so only bind when the signature genuinely takes nothing.
	const UFunction* Signature = DelegateProp->SignatureFunction;
	if (Signature == nullptr || Signature->NumParms != 0)
	{
		UE_LOG(LogInventoryBubble, Verbose,
			TEXT("Delegate '%s' on '%s' takes parameters - skipping bind, Layer B will cover it."),
			*DelegatePropertyName.ToString(), *Target->GetName());
		return false;
	}

	FScriptDelegate ScriptDelegate;
	ScriptDelegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UInventoryBubbleGrabAdapter, HandleGrabNotification));

	void* PropertyValue = DelegateProp->ContainerPtrToValuePtr<void>(Target);
	DelegateProp->AddDelegate(ScriptDelegate, Target, PropertyValue);

	BoundDelegateNames.AddUnique(DelegatePropertyName);
	return true;
}

void UInventoryBubbleGrabAdapter::UnbindParameterlessDelegate(UObject* Target, FName DelegatePropertyName)
{
	if (!IsValid(Target))
	{
		return;
	}

	FMulticastDelegateProperty* DelegateProp =
		FindFProperty<FMulticastDelegateProperty>(Target->GetClass(), DelegatePropertyName);

	if (DelegateProp == nullptr)
	{
		return;
	}

	FScriptDelegate ScriptDelegate;
	ScriptDelegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UInventoryBubbleGrabAdapter, HandleGrabNotification));

	void* PropertyValue = DelegateProp->ContainerPtrToValuePtr<void>(Target);
	DelegateProp->RemoveDelegate(ScriptDelegate, Target, PropertyValue);
}

void UInventoryBubbleGrabAdapter::HandleGrabNotification()
{
	AInventoryBubbleActor* OwningBubble = Bubble.Get();
	AActor* Item = BoundItem.Get();

	if (!IsValid(OwningBubble) || !IsValid(Item))
	{
		return;
	}

	// Layer C never manipulates state itself - it routes into Layer A.
	OwningBubble->NotifyItemGrabbed(Item);
}
