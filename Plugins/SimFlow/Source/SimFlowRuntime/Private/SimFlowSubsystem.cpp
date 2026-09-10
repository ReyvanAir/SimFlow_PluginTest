// Copyright SimFlow. All Rights Reserved.

#include "SimFlowSubsystem.h"
#include "SimFlowComponent.h"
#include "SimFlowSaveGame.h"
#include "SimFlowRuntimeModule.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarSimFlowDebug(
	TEXT("SimFlow.Debug"),
	0,
	TEXT("Draws the SimFlow status overlay on screen.\n0: off\n1: on"),
	ECVF_Cheat);

void USimFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	DebugTickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &USimFlowSubsystem::TickDebugHUD), 0.f);
}

void USimFlowSubsystem::Deinitialize()
{
	if (DebugTickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(DebugTickHandle);
		DebugTickHandle.Reset();
	}

	RegisteredComponents.Reset();

	Super::Deinitialize();
}

USimFlowSubsystem* USimFlowSubsystem::Get(const UObject* WorldContextObject)
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

	UGameInstance* GameInstance = World->GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<USimFlowSubsystem>() : nullptr;
}

void USimFlowSubsystem::RegisterComponent(USimFlowComponent* Component)
{
	if (!Component)
	{
		return;
	}
	RegisteredComponents.RemoveAll([](const TWeakObjectPtr<USimFlowComponent>& Weak) { return !Weak.IsValid(); });
	RegisteredComponents.AddUnique(Component);
}

void USimFlowSubsystem::UnregisterComponent(USimFlowComponent* Component)
{
	RegisteredComponents.RemoveAll([Component](const TWeakObjectPtr<USimFlowComponent>& Weak)
	{
		return !Weak.IsValid() || Weak.Get() == Component;
	});
}

TArray<USimFlowComponent*> USimFlowSubsystem::GetAllFlowComponents() const
{
	TArray<USimFlowComponent*> Result;
	for (const TWeakObjectPtr<USimFlowComponent>& Weak : RegisteredComponents)
	{
		if (USimFlowComponent* Component = Weak.Get())
		{
			Result.Add(Component);
		}
	}
	return Result;
}

USimFlowComponent* USimFlowSubsystem::FindFlowById(FName FlowSaveId) const
{
	for (const TWeakObjectPtr<USimFlowComponent>& Weak : RegisteredComponents)
	{
		USimFlowComponent* Component = Weak.Get();
		if (Component && Component->GetEffectiveSaveId() == FlowSaveId)
		{
			return Component;
		}
	}
	return nullptr;
}

USimFlowComponent* USimFlowSubsystem::GetPrimaryFlow() const
{
	USimFlowComponent* FirstAny = nullptr;
	for (const TWeakObjectPtr<USimFlowComponent>& Weak : RegisteredComponents)
	{
		USimFlowComponent* Component = Weak.Get();
		if (!Component)
		{
			continue;
		}
		if (!FirstAny)
		{
			FirstAny = Component;
		}
		if (Component->IsFlowRunning() || Component->IsFlowPaused())
		{
			return Component;
		}
	}
	return FirstAny;
}

void USimFlowSubsystem::PauseAllFlows()
{
	for (USimFlowComponent* Component : GetAllFlowComponents())
	{
		Component->PauseFlow();
	}
}

void USimFlowSubsystem::ResumeAllFlows()
{
	for (USimFlowComponent* Component : GetAllFlowComponents())
	{
		Component->ResumeFlow();
	}
}

void USimFlowSubsystem::StopAllFlows()
{
	for (USimFlowComponent* Component : GetAllFlowComponents())
	{
		Component->StopFlow();
	}
}

bool USimFlowSubsystem::SaveAllFlowsToSlot(const FString& SlotName, int32 UserIndex)
{
	USimFlowSaveGame* SaveGame = nullptr;

	if (UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		SaveGame = Cast<USimFlowSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
	}
	if (!SaveGame)
	{
		SaveGame = Cast<USimFlowSaveGame>(UGameplayStatics::CreateSaveGameObject(USimFlowSaveGame::StaticClass()));
	}
	if (!SaveGame)
	{
		return false;
	}

	for (USimFlowComponent* Component : GetAllFlowComponents())
	{
		if (Component->GetFlowInstance())
		{
			SaveGame->Flows.Add(Component->GetEffectiveSaveId(), Component->SaveFlowState());
		}
	}

	return UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, UserIndex);
}

int32 USimFlowSubsystem::LoadAllFlowsFromSlot(const FString& SlotName, int32 UserIndex, ESimFlowLoadMode LoadMode)
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		return 0;
	}

	const USimFlowSaveGame* SaveGame = Cast<USimFlowSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
	if (!SaveGame)
	{
		return 0;
	}

	int32 Loaded = 0;
	for (USimFlowComponent* Component : GetAllFlowComponents())
	{
		const FName SaveId = Component->GetEffectiveSaveId();
		if (const FSimFlowSaveState* State = SaveGame->Flows.Find(SaveId))
		{
			if (Component->LoadFlowState(*State, LoadMode))
			{
				Loaded++;
			}
		}
	}

	return Loaded;
}

bool USimFlowSubsystem::DeleteSaveSlot(const FString& SlotName, int32 UserIndex)
{
	return UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
}

void USimFlowSubsystem::SetDebugHUDEnabled(bool bEnabled)
{
	bDebugHUDEnabled = bEnabled;
}

FString USimFlowSubsystem::BuildGlobalDebugText() const
{
	FString Result;
	for (const TWeakObjectPtr<USimFlowComponent>& Weak : RegisteredComponents)
	{
		if (const USimFlowComponent* Component = Weak.Get())
		{
			Result += Component->GetDebugText();
			Result += TEXT("\n");
		}
	}
	return Result;
}

bool USimFlowSubsystem::TickDebugHUD(float /*DeltaTime*/)
{
	if (!GEngine)
	{
		return true;
	}

	const bool bGlobalOn = bDebugHUDEnabled || CVarSimFlowDebug.GetValueOnGameThread() != 0;

	FString Text;
	for (const TWeakObjectPtr<USimFlowComponent>& Weak : RegisteredComponents)
	{
		const USimFlowComponent* Component = Weak.Get();
		if (!Component)
		{
			continue;
		}
		if (bGlobalOn || Component->bShowDebugHUD)
		{
			Text += Component->GetDebugText();
			Text += TEXT("\n");
		}
	}

	if (!Text.IsEmpty())
	{
		// Negative key so the message is replaced every frame rather than stacking.
		GEngine->AddOnScreenDebugMessage(0x51F10, 0.2f, FColor::White, Text, false);
	}

	return true;
}
