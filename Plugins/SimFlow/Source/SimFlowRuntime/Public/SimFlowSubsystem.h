// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "SimFlowTypes.h"
#include "SimFlowSubsystem.generated.h"

class USimFlowComponent;
class USimFlowSaveGame;

/**
 * Tracks every flow running in the game instance.
 *
 * Use it to pause every flow at once, to find a flow by id from UI code,
 * and to save or load all flows in a single call.
 * It also owns the on-screen debug HUD (console: SimFlow.Debug 1).
 */
UCLASS()
class SIMFLOWRUNTIME_API USimFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Convenience accessor from any world context object. */
	static USimFlowSubsystem* Get(const UObject* WorldContextObject);

	void RegisterComponent(USimFlowComponent* Component);
	void UnregisterComponent(USimFlowComponent* Component);

	UFUNCTION(BlueprintPure, Category = "SimFlow")
	TArray<USimFlowComponent*> GetAllFlowComponents() const;

	UFUNCTION(BlueprintPure, Category = "SimFlow")
	USimFlowComponent* FindFlowById(FName FlowSaveId) const;

	/** The first running flow. Handy for single-scenario VR apps. */
	UFUNCTION(BlueprintPure, Category = "SimFlow")
	USimFlowComponent* GetPrimaryFlow() const;

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control")
	void PauseAllFlows();

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control")
	void ResumeAllFlows();

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control")
	void StopAllFlows();

	/** Writes every registered flow into one slot. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Save")
	bool SaveAllFlowsToSlot(const FString& SlotName, int32 UserIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Save")
	int32 LoadAllFlowsFromSlot(const FString& SlotName, int32 UserIndex = 0, ESimFlowLoadMode LoadMode = ESimFlowLoadMode::ExactState);

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Save")
	static bool DeleteSaveSlot(const FString& SlotName, int32 UserIndex = 0);

	/** Turns the on-screen debug overlay on or off for every flow. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Debug")
	void SetDebugHUDEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "SimFlow|Debug")
	bool IsDebugHUDEnabled() const { return bDebugHUDEnabled; }

	/** Combined debug text for every registered flow. */
	UFUNCTION(BlueprintPure, Category = "SimFlow|Debug")
	FString BuildGlobalDebugText() const;

private:
	bool TickDebugHUD(float DeltaTime);

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<USimFlowComponent>> RegisteredComponents;

	FTSTicker::FDelegateHandle DebugTickHandle;
	bool bDebugHUDEnabled = false;
};
