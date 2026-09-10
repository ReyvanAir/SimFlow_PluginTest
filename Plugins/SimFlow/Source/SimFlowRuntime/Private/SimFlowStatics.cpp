// Copyright SimFlow. All Rights Reserved.

#include "SimFlowStatics.h"
#include "SimFlowComponent.h"
#include "SimFlowSubsystem.h"
#include "SimFlowPlayerComponent.h"
#include "SimFlowRuntimeModule.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

USimFlowComponent* USimFlowStatics::GetPrimaryFlow(const UObject* WorldContextObject)
{
	USimFlowSubsystem* Subsystem = USimFlowSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->GetPrimaryFlow() : nullptr;
}

USimFlowComponent* USimFlowStatics::FindFlowById(const UObject* WorldContextObject, FName FlowSaveId)
{
	USimFlowSubsystem* Subsystem = USimFlowSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->FindFlowById(FlowSaveId) : nullptr;
}

USimFlowComponent* USimFlowStatics::GetFlowFromActor(AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<USimFlowComponent>() : nullptr;
}

void USimFlowStatics::BroadcastFlowEvent(const UObject* WorldContextObject, FGameplayTag EventTag, UObject* Payload)
{
	if (!EventTag.IsValid())
	{
		return;
	}

	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;

	// On a client one request covers every flow the server is running, so send a
	// single RPC rather than one per local mirror.
	if (World && World->GetNetMode() == NM_Client)
	{
		if (USimFlowPlayerComponent* Player = USimFlowPlayerComponent::GetLocal(WorldContextObject))
		{
			Player->RequestEvent(NAME_None, EventTag, Payload);
		}
		else
		{
			UE_LOG(LogSimFlow, Warning,
				TEXT("Event '%s' was raised on a client with no SimFlow Player Component on the PlayerController, so it never reached the server."),
				*EventTag.ToString());
		}
		return;
	}

	USimFlowSubsystem* Subsystem = USimFlowSubsystem::Get(WorldContextObject);
	if (!Subsystem)
	{
		return;
	}

	for (USimFlowComponent* Component : Subsystem->GetAllFlowComponents())
	{
		Component->SendEvent(EventTag, Payload);
	}
}

void USimFlowStatics::RequestFlowControl(const UObject* WorldContextObject, FName FlowSaveId, ESimFlowControlRequest Request)
{
	if (USimFlowPlayerComponent* Player = USimFlowPlayerComponent::GetLocal(WorldContextObject))
	{
		Player->RequestControl(FlowSaveId, Request);
		return;
	}

	// No player component: fall back to acting directly, which is correct in
	// single player and on the server.
	if (USimFlowComponent* Component = FindFlowById(WorldContextObject, FlowSaveId))
	{
		switch (Request)
		{
		case ESimFlowControlRequest::Start:			Component->StartFlow();			break;
		case ESimFlowControlRequest::Stop:			Component->StopFlow();			break;
		case ESimFlowControlRequest::Restart:		Component->RestartFlow();		break;
		case ESimFlowControlRequest::Pause:			Component->PauseFlow();			break;
		case ESimFlowControlRequest::Resume:		Component->ResumeFlow();		break;
		case ESimFlowControlRequest::TogglePause:	Component->TogglePause();		break;
		case ESimFlowControlRequest::Retry:			Component->RetryCurrentTask();	break;
		case ESimFlowControlRequest::Skip:			Component->SkipCurrentTask();	break;
		case ESimFlowControlRequest::Fail:			Component->FailCurrentTask();	break;
		default:																	break;
		}
	}
}

void USimFlowStatics::SendFlowEvent(const UObject* WorldContextObject, FName FlowSaveId, FGameplayTag EventTag, UObject* Payload)
{
	if (USimFlowComponent* Component = FindFlowById(WorldContextObject, FlowSaveId))
	{
		Component->SendEvent(EventTag, Payload);
	}
}

void USimFlowStatics::PauseAllFlows(const UObject* WorldContextObject)
{
	if (USimFlowSubsystem* Subsystem = USimFlowSubsystem::Get(WorldContextObject))
	{
		Subsystem->PauseAllFlows();
	}
}

void USimFlowStatics::ResumeAllFlows(const UObject* WorldContextObject)
{
	if (USimFlowSubsystem* Subsystem = USimFlowSubsystem::Get(WorldContextObject))
	{
		Subsystem->ResumeAllFlows();
	}
}

bool USimFlowStatics::SaveAllFlows(const UObject* WorldContextObject, const FString& SlotName, int32 UserIndex)
{
	USimFlowSubsystem* Subsystem = USimFlowSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->SaveAllFlowsToSlot(SlotName, UserIndex) : false;
}

int32 USimFlowStatics::LoadAllFlows(const UObject* WorldContextObject, const FString& SlotName, int32 UserIndex, ESimFlowLoadMode LoadMode)
{
	USimFlowSubsystem* Subsystem = USimFlowSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->LoadAllFlowsFromSlot(SlotName, UserIndex, LoadMode) : 0;
}

bool USimFlowStatics::DeleteFlowSave(const FString& SlotName, int32 UserIndex)
{
	return USimFlowSubsystem::DeleteSaveSlot(SlotName, UserIndex);
}

FSimFlowValue USimFlowStatics::MakeFlowBool(bool Value)					{ return FSimFlowValue::MakeBool(Value); }
FSimFlowValue USimFlowStatics::MakeFlowInt(int32 Value)					{ return FSimFlowValue::MakeInt(Value); }
FSimFlowValue USimFlowStatics::MakeFlowFloat(float Value)				{ return FSimFlowValue::MakeFloat(Value); }
FSimFlowValue USimFlowStatics::MakeFlowString(const FString& Value)		{ return FSimFlowValue::MakeString(Value); }
FSimFlowValue USimFlowStatics::MakeFlowName(FName Value)					{ return FSimFlowValue::MakeName(Value); }
FSimFlowValue USimFlowStatics::MakeFlowVector(FVector Value)				{ return FSimFlowValue::MakeVector(Value); }
FSimFlowValue USimFlowStatics::MakeFlowObject(UObject* Value)				{ return FSimFlowValue::MakeObject(Value); }

bool USimFlowStatics::FlowValueToBool(const FSimFlowValue& Value)			{ return Value.AsBool(); }
int32 USimFlowStatics::FlowValueToInt(const FSimFlowValue& Value)			{ return FMath::RoundToInt(Value.AsNumber()); }
float USimFlowStatics::FlowValueToFloat(const FSimFlowValue& Value)		{ return static_cast<float>(Value.AsNumber()); }
FString USimFlowStatics::FlowValueToString(const FSimFlowValue& Value)	{ return Value.AsString(); }

FText USimFlowStatics::ResultToText(ESimFlowResult Result)
{
	return StaticEnum<ESimFlowResult>()->GetDisplayNameTextByValue(static_cast<int64>(Result));
}

FText USimFlowStatics::RunStateToText(ESimFlowRunState State)
{
	return StaticEnum<ESimFlowRunState>()->GetDisplayNameTextByValue(static_cast<int64>(State));
}

FText USimFlowStatics::FormatSeconds(float Seconds)
{
	const int32 Total = FMath::Max(0, FMath::CeilToInt(Seconds));
	const int32 Minutes = Total / 60;
	const int32 Rest = Total % 60;
	return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minutes, Rest));
}
