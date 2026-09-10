// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SimFlowNetTypes.h"
#include "SimFlowPlayerComponent.generated.h"

class USimFlowComponent;

/**
 * Client -> server control channel for flows.
 *
 * A flow usually lives on a level actor or the Game State, which has no owning
 * connection, so a client cannot send RPCs to it directly. This component sits
 * on the PlayerController - which always has one - and forwards control requests
 * to whichever flow the server says owns that save id.
 *
 * Add it to your PlayerController Blueprint. It is only needed when clients need
 * to drive the flow (answer quizzes, press retry, raise interaction events).
 * A server-only or single-player flow does not need it at all.
 */
UCLASS(ClassGroup = "SimFlow", meta = (BlueprintSpawnableComponent, DisplayName = "SimFlow Player Component"))
class SIMFLOWRUNTIME_API USimFlowPlayerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USimFlowPlayerComponent();

	/**
	 * Refuse control requests from this client for flows it does not own.
	 * Leave on unless you are deliberately building instructor controls, and
	 * see IsRequestAuthorised for how to gate those properly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimFlow|Network")
	bool bRestrictToOwnedFlows = false;

	/** Finds the local player's SimFlow player component, if one exists. */
	UFUNCTION(BlueprintPure, Category = "SimFlow|Network", meta = (WorldContext = "WorldContextObject"))
	static USimFlowPlayerComponent* GetLocal(const UObject* WorldContextObject);

	// ------------------------------------------------------- Client entry points

	/** Asks the server to perform a control action on the named flow. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Network")
	void RequestControl(FName FlowSaveId, ESimFlowControlRequest Request);

	/** Asks the server to submit a quiz answer on the named flow. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Network")
	void RequestQuizAnswer(FName FlowSaveId, FGuid QuizNodeGuid, int32 OptionIndex);

	/** Asks the server to raise an event tag. Leave FlowSaveId as None to hit every flow. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Network")
	void RequestEvent(FName FlowSaveId, FGameplayTag EventTag, UObject* Payload = nullptr);

	// ------------------------------------------------------------- Server RPCs

	UFUNCTION(Server, Reliable)
	void ServerRequestControl(FName FlowSaveId, ESimFlowControlRequest Request);

	UFUNCTION(Server, Reliable)
	void ServerRequestQuizAnswer(FName FlowSaveId, FGuid QuizNodeGuid, int32 OptionIndex);

	UFUNCTION(Server, Reliable)
	void ServerRequestEvent(FName FlowSaveId, FGameplayTag EventTag, UObject* Payload);

protected:
	/**
	 * Server-side gate for every incoming request. Override this in a subclass to
	 * implement your own rules - for example, only the instructor may skip a task,
	 * or a trainee may only answer their own quiz.
	 *
	 * Clients are not trusted: everything they send arrives here first.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "SimFlow|Network")
	bool IsRequestAuthorised(USimFlowComponent* TargetFlow, ESimFlowControlRequest Request) const;
	virtual bool IsRequestAuthorised_Implementation(USimFlowComponent* TargetFlow, ESimFlowControlRequest Request) const;

private:
	/** Resolves a save id to a live flow on the server. */
	USimFlowComponent* ResolveFlow(FName FlowSaveId) const;

	void ApplyControl(USimFlowComponent* Flow, ESimFlowControlRequest Request) const;
};
