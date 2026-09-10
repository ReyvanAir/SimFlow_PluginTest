// Copyright SimFlow. All Rights Reserved.

#include "SimFlowPlayerComponent.h"
#include "SimFlowComponent.h"
#include "SimFlowSubsystem.h"
#include "SimFlowRuntimeModule.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

USimFlowPlayerComponent::USimFlowPlayerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

USimFlowPlayerComponent* USimFlowPlayerComponent::GetLocal(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}

	APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0);
	return Controller ? Controller->FindComponentByClass<USimFlowPlayerComponent>() : nullptr;
}

// --------------------------------------------------------- Client entry points

void USimFlowPlayerComponent::RequestControl(FName FlowSaveId, ESimFlowControlRequest Request)
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		// Already on the server (listen server host, or a server-side call).
		if (USimFlowComponent* Flow = ResolveFlow(FlowSaveId))
		{
			if (IsRequestAuthorised(Flow, Request))
			{
				ApplyControl(Flow, Request);
			}
		}
		return;
	}

	ServerRequestControl(FlowSaveId, Request);
}

void USimFlowPlayerComponent::RequestQuizAnswer(FName FlowSaveId, FGuid QuizNodeGuid, int32 OptionIndex)
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		if (USimFlowComponent* Flow = ResolveFlow(FlowSaveId))
		{
			Flow->AuthoritySubmitQuizAnswer(QuizNodeGuid, OptionIndex);
		}
		return;
	}

	ServerRequestQuizAnswer(FlowSaveId, QuizNodeGuid, OptionIndex);
}

void USimFlowPlayerComponent::RequestEvent(FName FlowSaveId, FGameplayTag EventTag, UObject* Payload)
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		ServerRequestEvent_Implementation(FlowSaveId, EventTag, Payload);
		return;
	}

	ServerRequestEvent(FlowSaveId, EventTag, Payload);
}

// ------------------------------------------------------------------ Server RPCs

void USimFlowPlayerComponent::ServerRequestControl_Implementation(FName FlowSaveId, ESimFlowControlRequest Request)
{
	USimFlowComponent* Flow = ResolveFlow(FlowSaveId);
	if (!Flow)
	{
		UE_LOG(LogSimFlow, Warning, TEXT("Client asked for control of flow '%s', which the server does not have."),
			*FlowSaveId.ToString());
		return;
	}

	if (!IsRequestAuthorised(Flow, Request))
	{
		UE_LOG(LogSimFlow, Warning, TEXT("Refused control request '%s' on flow '%s' from %s."),
			*StaticEnum<ESimFlowControlRequest>()->GetNameStringByValue(static_cast<int64>(Request)),
			*FlowSaveId.ToString(),
			*GetNameSafe(GetOwner()));
		return;
	}

	ApplyControl(Flow, Request);
}

void USimFlowPlayerComponent::ServerRequestQuizAnswer_Implementation(FName FlowSaveId, FGuid QuizNodeGuid, int32 OptionIndex)
{
	if (USimFlowComponent* Flow = ResolveFlow(FlowSaveId))
	{
		Flow->AuthoritySubmitQuizAnswer(QuizNodeGuid, OptionIndex);
	}
}

void USimFlowPlayerComponent::ServerRequestEvent_Implementation(FName FlowSaveId, FGameplayTag EventTag, UObject* Payload)
{
	if (!EventTag.IsValid())
	{
		return;
	}

	if (!FlowSaveId.IsNone())
	{
		if (USimFlowComponent* Flow = ResolveFlow(FlowSaveId))
		{
			Flow->SendEvent(EventTag, Payload);
		}
		return;
	}

	// No id given: raise it on every flow the server is running.
	if (USimFlowSubsystem* Subsystem = USimFlowSubsystem::Get(this))
	{
		for (USimFlowComponent* Flow : Subsystem->GetAllFlowComponents())
		{
			Flow->SendEvent(EventTag, Payload);
		}
	}
}

// ----------------------------------------------------------------- Internals

bool USimFlowPlayerComponent::IsRequestAuthorised_Implementation(USimFlowComponent* TargetFlow, ESimFlowControlRequest /*Request*/) const
{
	if (!TargetFlow)
	{
		return false;
	}

	if (!bRestrictToOwnedFlows)
	{
		return true;
	}

	// Only allow control of a flow that belongs to this player's own actors.
	const AActor* FlowOwner = TargetFlow->GetOwner();
	const AActor* MyOwner = GetOwner();
	if (!FlowOwner || !MyOwner)
	{
		return false;
	}

	return FlowOwner == MyOwner
		|| FlowOwner->GetOwner() == MyOwner
		|| FlowOwner->GetInstigatorController() == MyOwner;
}

USimFlowComponent* USimFlowPlayerComponent::ResolveFlow(FName FlowSaveId) const
{
	USimFlowSubsystem* Subsystem = USimFlowSubsystem::Get(this);
	if (!Subsystem)
	{
		return nullptr;
	}

	if (FlowSaveId.IsNone())
	{
		return Subsystem->GetPrimaryFlow();
	}

	return Subsystem->FindFlowById(FlowSaveId);
}

void USimFlowPlayerComponent::ApplyControl(USimFlowComponent* Flow, ESimFlowControlRequest Request) const
{
	if (!Flow)
	{
		return;
	}

	switch (Request)
	{
	case ESimFlowControlRequest::Start:			Flow->StartFlow();			break;
	case ESimFlowControlRequest::Stop:			Flow->StopFlow();			break;
	case ESimFlowControlRequest::Restart:		Flow->RestartFlow();		break;
	case ESimFlowControlRequest::Pause:			Flow->PauseFlow();			break;
	case ESimFlowControlRequest::Resume:		Flow->ResumeFlow();			break;
	case ESimFlowControlRequest::TogglePause:	Flow->TogglePause();		break;
	case ESimFlowControlRequest::Retry:			Flow->RetryCurrentTask();	break;
	case ESimFlowControlRequest::Skip:			Flow->SkipCurrentTask();	break;
	case ESimFlowControlRequest::Fail:			Flow->FailCurrentTask();	break;
	default:																break;
	}
}
