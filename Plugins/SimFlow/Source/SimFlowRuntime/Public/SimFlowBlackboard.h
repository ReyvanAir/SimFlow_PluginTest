// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SimFlowTypes.h"
#include "SimFlowBlackboard.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSimFlowBlackboardChanged, FName, Key, const FSimFlowValue&, NewValue);

/**
 * Per-flow key/value store. Holds score, quiz answers, timers, arbitrary designer
 * state, and everything conditions and branches read from. Fully save/load-able.
 */
UCLASS(BlueprintType)
class SIMFLOWRUNTIME_API USimFlowBlackboard : public UObject
{
	GENERATED_BODY()

public:
	/** Fired whenever any key is written. */
	UPROPERTY(BlueprintAssignable, Category = "SimFlow|Blackboard")
	FSimFlowBlackboardChanged OnValueChanged;

	// ---- Generic access -------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Blackboard")
	void SetValue(FName Key, const FSimFlowValue& Value);

	UFUNCTION(BlueprintPure, Category = "SimFlow|Blackboard")
	FSimFlowValue GetValue(FName Key) const;

	UFUNCTION(BlueprintPure, Category = "SimFlow|Blackboard")
	bool HasValue(FName Key) const;

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Blackboard")
	void RemoveValue(FName Key);

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Blackboard")
	void ClearAll();

	/** Adds Delta to an existing value (numeric, string or vector). Creates the key if missing. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Blackboard")
	void AddToValue(FName Key, const FSimFlowValue& Delta);

	// ---- Typed convenience ----------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Blackboard") void SetBool(FName Key, bool Value);
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Blackboard") void SetInt(FName Key, int32 Value);
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Blackboard") void SetFloat(FName Key, float Value);
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Blackboard") void SetString(FName Key, const FString& Value);
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Blackboard") void SetName(FName Key, FName Value);
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Blackboard") void SetVector(FName Key, FVector Value);
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Blackboard") void SetObject(FName Key, UObject* Value);

	UFUNCTION(BlueprintPure, Category = "SimFlow|Blackboard") bool GetBool(FName Key, bool DefaultValue = false) const;
	UFUNCTION(BlueprintPure, Category = "SimFlow|Blackboard") int32 GetInt(FName Key, int32 DefaultValue = 0) const;
	UFUNCTION(BlueprintPure, Category = "SimFlow|Blackboard") float GetFloat(FName Key, float DefaultValue = 0.f) const;
	UFUNCTION(BlueprintPure, Category = "SimFlow|Blackboard") FString GetString(FName Key, const FString& DefaultValue = FString(TEXT(""))) const;
	UFUNCTION(BlueprintPure, Category = "SimFlow|Blackboard") FName GetName(FName Key, FName DefaultValue = NAME_None) const;
	UFUNCTION(BlueprintPure, Category = "SimFlow|Blackboard") FVector GetVector(FName Key, FVector DefaultValue = FVector::ZeroVector) const;
	UFUNCTION(BlueprintPure, Category = "SimFlow|Blackboard") UObject* GetObject(FName Key) const;

	/** Score helpers - "Score" is a well known key used by the built-in tasks. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Blackboard") void AddScore(float Delta);
	UFUNCTION(BlueprintPure, Category = "SimFlow|Blackboard") float GetScore() const;

	// ---- Serialisation ---------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SimFlow|Blackboard")
	TArray<FSimFlowBlackboardEntry> ToEntries(bool bStripObjectReferences = true) const;

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Blackboard")
	void FromEntries(const TArray<FSimFlowBlackboardEntry>& Entries, bool bClearFirst = true);

	/** Copies every entry from Other into this blackboard (used by sub-flows). */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Blackboard")
	void MergeFrom(const USimFlowBlackboard* Other, bool bOverwriteExisting = true);

	/** Debug dump, one key per line. */
	FString ToDebugString() const;

	const TMap<FName, FSimFlowValue>& GetAllValues() const { return Values; }

private:
	UPROPERTY()
	TMap<FName, FSimFlowValue> Values;
};
