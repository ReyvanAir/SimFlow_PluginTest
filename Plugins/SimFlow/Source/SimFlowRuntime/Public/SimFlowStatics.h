// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "SimFlowTypes.h"
#include "SimFlowNetTypes.h"
#include "SimFlowStatics.generated.h"

class USimFlowComponent;
class USimFlowInstance;
class USimFlowTask;
class AActor;

/** Blueprint helpers so gameplay code rarely needs a direct component reference. */
UCLASS()
class SIMFLOWRUNTIME_API USimFlowStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The first running flow in the game instance. */
	UFUNCTION(BlueprintPure, Category = "SimFlow", meta = (WorldContext = "WorldContextObject"))
	static USimFlowComponent* GetPrimaryFlow(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "SimFlow", meta = (WorldContext = "WorldContextObject"))
	static USimFlowComponent* FindFlowById(const UObject* WorldContextObject, FName FlowSaveId);

	/** Finds the flow component on an actor, if it has one. */
	UFUNCTION(BlueprintPure, Category = "SimFlow")
	static USimFlowComponent* GetFlowFromActor(AActor* Actor);

	/**
	 * Raises an event on every running flow. This is the usual way for a grabbable
	 * object, a button or an animation notify to talk to whatever flow is running.
	 */
	UFUNCTION(BlueprintCallable, Category = "SimFlow", meta = (WorldContext = "WorldContextObject"))
	static void BroadcastFlowEvent(const UObject* WorldContextObject, FGameplayTag EventTag, UObject* Payload = nullptr);

	/** Raises an event on one specific flow. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow", meta = (WorldContext = "WorldContextObject"))
	static void SendFlowEvent(const UObject* WorldContextObject, FName FlowSaveId, FGameplayTag EventTag, UObject* Payload = nullptr);

	/**
	 * Asks for a control action on a flow, taking the right route automatically:
	 * straight through on the server or in single player, via a Server RPC on a
	 * client. Ideal for instructor panels and pause menus.
	 */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control", meta = (WorldContext = "WorldContextObject"))
	static void RequestFlowControl(const UObject* WorldContextObject, FName FlowSaveId, ESimFlowControlRequest Request);

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control", meta = (WorldContext = "WorldContextObject"))
	static void PauseAllFlows(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control", meta = (WorldContext = "WorldContextObject"))
	static void ResumeAllFlows(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Save", meta = (WorldContext = "WorldContextObject"))
	static bool SaveAllFlows(const UObject* WorldContextObject, const FString& SlotName, int32 UserIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Save", meta = (WorldContext = "WorldContextObject"))
	static int32 LoadAllFlows(const UObject* WorldContextObject, const FString& SlotName, int32 UserIndex = 0,
		ESimFlowLoadMode LoadMode = ESimFlowLoadMode::ExactState);

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Save")
	static bool DeleteFlowSave(const FString& SlotName, int32 UserIndex = 0);

	// ------------------------------------------------------------ Value makers

	UFUNCTION(BlueprintPure, Category = "SimFlow|Value", meta = (DisplayName = "Make SimFlow Value (Bool)"))	static FSimFlowValue MakeFlowBool(bool Value);
	UFUNCTION(BlueprintPure, Category = "SimFlow|Value", meta = (DisplayName = "Make SimFlow Value (Int)"))	static FSimFlowValue MakeFlowInt(int32 Value);
	UFUNCTION(BlueprintPure, Category = "SimFlow|Value", meta = (DisplayName = "Make SimFlow Value (Float)"))	static FSimFlowValue MakeFlowFloat(float Value);
	UFUNCTION(BlueprintPure, Category = "SimFlow|Value", meta = (DisplayName = "Make SimFlow Value (String)"))	static FSimFlowValue MakeFlowString(const FString& Value);
	UFUNCTION(BlueprintPure, Category = "SimFlow|Value", meta = (DisplayName = "Make SimFlow Value (Name)"))	static FSimFlowValue MakeFlowName(FName Value);
	UFUNCTION(BlueprintPure, Category = "SimFlow|Value", meta = (DisplayName = "Make SimFlow Value (Vector)"))	static FSimFlowValue MakeFlowVector(FVector Value);
	UFUNCTION(BlueprintPure, Category = "SimFlow|Value", meta = (DisplayName = "Make SimFlow Value (Object)"))	static FSimFlowValue MakeFlowObject(UObject* Value);

	UFUNCTION(BlueprintPure, Category = "SimFlow|Value") static bool		FlowValueToBool(const FSimFlowValue& Value);
	UFUNCTION(BlueprintPure, Category = "SimFlow|Value") static int32	FlowValueToInt(const FSimFlowValue& Value);
	UFUNCTION(BlueprintPure, Category = "SimFlow|Value") static float	FlowValueToFloat(const FSimFlowValue& Value);
	UFUNCTION(BlueprintPure, Category = "SimFlow|Value") static FString	FlowValueToString(const FSimFlowValue& Value);

	/** Formats a result enum for UI, e.g. "Timed Out". */
	UFUNCTION(BlueprintPure, Category = "SimFlow")
	static FText ResultToText(ESimFlowResult Result);

	UFUNCTION(BlueprintPure, Category = "SimFlow")
	static FText RunStateToText(ESimFlowRunState State);

	/** Seconds -> "01:23", for countdown widgets. */
	UFUNCTION(BlueprintPure, Category = "SimFlow")
	static FText FormatSeconds(float Seconds);
};
