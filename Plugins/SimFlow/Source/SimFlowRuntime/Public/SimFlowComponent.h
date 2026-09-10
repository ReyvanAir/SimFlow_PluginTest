// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SimFlowTypes.h"
#include "SimFlowNetTypes.h"
#include "SimFlowInstance.h"
#include "SimFlowComponent.generated.h"

class USimFlowAsset;
class USimFlowSaveGame;
class USimFlowTask_Quiz;

/**
 * Drop this on any actor - a Game Mode, a level actor, the Game State, or the VR
 * pawn - to run a flow.
 *
 * This is the whole designer-facing runtime API: start, pause, resume, retry,
 * skip, fail, save and load.
 *
 * NETWORKING
 * Turn on bReplicateFlow and the flow becomes server-authoritative: only the
 * server runs nodes and tasks, and clients receive a compact state summary they
 * mirror locally so their UI and events stay in step. Control calls made on a
 * client are forwarded to the server automatically, provided the PlayerController
 * carries a USimFlowPlayerComponent.
 *
 * With bReplicateFlow off - the default - nothing changes from single player.
 */
UCLASS(ClassGroup = "SimFlow", meta = (BlueprintSpawnableComponent, DisplayName = "SimFlow Component"))
class SIMFLOWRUNTIME_API USimFlowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USimFlowComponent();

	// ------------------------------------------------------------- Authoring

	/** The flow this component runs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimFlow")
	TObjectPtr<USimFlowAsset> FlowAsset = nullptr;

	/** Which Start node to begin from. Leave as Default for single-entry flows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimFlow")
	FName EntryName = TEXT("Default");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimFlow|Start")
	ESimFlowStartMode StartMode = ESimFlowStartMode::Manual;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimFlow|Start",
		meta = (ClampMin = "0.0", Units = "s", EditCondition = "StartMode == ESimFlowStartMode::AutoAfterDelay", EditConditionHides))
	float AutoStartDelay = 1.f;

	// ------------------------------------------------------------- Networking

	/**
	 * Run this flow server-authoritatively and mirror it to clients.
	 *
	 * Requires the owning actor to replicate. Good homes are the Game State (one
	 * shared scenario everyone sees) or the PlayerState (a flow per trainee).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SimFlow|Network")
	bool bReplicateFlow = false;

	/** How often the server refreshes the elapsed-time field. Structural changes replicate immediately. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimFlow|Network",
		meta = (ClampMin = "0.1", Units = "s", EditCondition = "bReplicateFlow"))
	float NetRefreshInterval = 1.f;

	/** Replicate the blackboard to clients so their UI can read score, answers and custom keys. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimFlow|Network", meta = (EditCondition = "bReplicateFlow"))
	bool bReplicateBlackboard = true;

	// ------------------------------------------------------------------ Save

	/** Identifies this flow inside a save file, and addresses it over the network. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimFlow|Save")
	FName FlowSaveId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimFlow|Save")
	FString DefaultSaveSlotName = TEXT("SimFlowSave");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimFlow|Save")
	int32 DefaultSaveUserIndex = 0;

	/** When a save exists on BeginPlay, resume from it instead of starting fresh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimFlow|Save")
	bool bAutoResumeFromSaveOnBeginPlay = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimFlow|Save")
	ESimFlowLoadMode DefaultLoadMode = ESimFlowLoadMode::ExactState;

	/** Draw this flow's status on screen. Also toggled globally by SimFlow.Debug. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimFlow|Debug")
	bool bShowDebugHUD = false;

	/** Pause the flow automatically when the game itself is paused. Authority only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimFlow|Advanced")
	bool bFollowGamePause = true;

	// ------------------------------------------------------------- Delegates

	UPROPERTY(BlueprintAssignable, Category = "SimFlow") FSimFlowSimpleSignature		OnFlowStarted;
	UPROPERTY(BlueprintAssignable, Category = "SimFlow") FSimFlowSimpleSignature		OnFlowPaused;
	UPROPERTY(BlueprintAssignable, Category = "SimFlow") FSimFlowSimpleSignature		OnFlowResumed;
	UPROPERTY(BlueprintAssignable, Category = "SimFlow") FSimFlowFinishedSignature	OnFlowFinished;
	UPROPERTY(BlueprintAssignable, Category = "SimFlow") FSimFlowTaskSignature		OnTaskStarted;
	UPROPERTY(BlueprintAssignable, Category = "SimFlow") FSimFlowTaskResultSignature	OnTaskFinished;
	UPROPERTY(BlueprintAssignable, Category = "SimFlow") FSimFlowTaskSignature		OnTaskRetried;
	UPROPERTY(BlueprintAssignable, Category = "SimFlow") FSimFlowCheckpointSignature	OnCheckpointReached;
	UPROPERTY(BlueprintAssignable, Category = "SimFlow") FSimFlowQuizSignature		OnQuizPresented;

	/** Fired on clients whenever the replicated state changes. Server fires it too. */
	UPROPERTY(BlueprintAssignable, Category = "SimFlow|Network")
	FSimFlowSimpleSignature OnNetStateChanged;

	// ------------------------------------------------------------- Controls
	//
	// Safe to call from either side. On a client with bReplicateFlow on, these
	// forward to the server through the local SimFlow Player Component.

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control")
	bool StartFlow();

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control")
	bool StartFlowFromEntry(FName InEntryName);

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control")
	void StopFlow();

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control")
	bool RestartFlow();

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control")
	void PauseFlow();

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control")
	void ResumeFlow();

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control")
	void TogglePause();

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control")
	bool RetryCurrentTask();

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control")
	bool SkipCurrentTask();

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control")
	bool FailCurrentTask();

	/** Raises an event tag on this flow. Forwarded to the server from clients. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control")
	void SendEvent(FGameplayTag EventTag, UObject* Payload = nullptr);

	/** Answers the quiz that is currently on screen. Forwarded to the server from clients. */
	UFUNCTION(BlueprintCallable, Category = "SimFlow|Control")
	void SubmitQuizAnswer(int32 OptionIndex);

	/** Server-side quiz submission, addressed by node. Called by the player component. */
	void AuthoritySubmitQuizAnswer(const FGuid& QuizNodeGuid, int32 OptionIndex);

	// ------------------------------------------------------------- Save/load
	//
	// Save and load are authority-only. On a client these log and do nothing.

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Save")
	FSimFlowSaveState SaveFlowState() const;

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Save")
	bool LoadFlowState(const FSimFlowSaveState& State, ESimFlowLoadMode LoadMode = ESimFlowLoadMode::ExactState);

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Save")
	bool SaveFlowToSlot(const FString& SlotName, int32 UserIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Save")
	bool LoadFlowFromSlot(const FString& SlotName, int32 UserIndex = 0, ESimFlowLoadMode LoadMode = ESimFlowLoadMode::ExactState);

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Save")
	bool QuickSave();

	UFUNCTION(BlueprintCallable, Category = "SimFlow|Save")
	bool QuickLoad();

	UFUNCTION(BlueprintPure, Category = "SimFlow|Save")
	bool HasSaveInSlot(const FString& SlotName, int32 UserIndex = 0) const;

	// ------------------------------------------------------------- Queries
	//
	// These all work on clients too, reading the replicated state and resolving
	// names, instructions and quiz content from the flow asset every machine has.

	UFUNCTION(BlueprintPure, Category = "SimFlow") USimFlowInstance* GetFlowInstance() const { return Instance; }
	UFUNCTION(BlueprintPure, Category = "SimFlow") USimFlowBlackboard* GetBlackboard() const;
	UFUNCTION(BlueprintPure, Category = "SimFlow") ESimFlowRunState GetRunState() const;
	UFUNCTION(BlueprintPure, Category = "SimFlow") bool IsFlowRunning() const;
	UFUNCTION(BlueprintPure, Category = "SimFlow") bool IsFlowPaused() const;
	UFUNCTION(BlueprintPure, Category = "SimFlow") USimFlowTask* GetCurrentTask() const;
	UFUNCTION(BlueprintPure, Category = "SimFlow") FText GetCurrentTaskName() const;
	UFUNCTION(BlueprintPure, Category = "SimFlow") FText GetCurrentInstruction() const;
	UFUNCTION(BlueprintPure, Category = "SimFlow") float GetProgress() const;
	UFUNCTION(BlueprintPure, Category = "SimFlow") float GetScore() const;

	/** Seconds left on the current task's time limit, or -1 when it has none. */
	UFUNCTION(BlueprintPure, Category = "SimFlow") float GetCurrentTaskRemainingTime() const;

	/** The quiz currently being asked, or null. On clients this is the authored template. */
	UFUNCTION(BlueprintPure, Category = "SimFlow") USimFlowTask_Quiz* GetCurrentQuiz() const;

	/** True on a machine that is only mirroring a flow the server runs. */
	UFUNCTION(BlueprintPure, Category = "SimFlow|Network") bool IsClientMirror() const;

	/** True when this machine is the one actually executing the flow. */
	UFUNCTION(BlueprintPure, Category = "SimFlow|Network") bool HasFlowAuthority() const;

	UFUNCTION(BlueprintPure, Category = "SimFlow|Network") FSimFlowNetState GetNetState() const { return NetState; }

	/** Multi-line status text, ready to drop into a world-space VR debug widget. */
	UFUNCTION(BlueprintPure, Category = "SimFlow|Debug")
	FString GetDebugText() const;

	UFUNCTION(BlueprintPure, Category = "SimFlow")
	FName GetEffectiveSaveId() const;

	// ------------------------------------------------------------- UActorComponent

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void EnsureInstance();
	void BindInstanceDelegates();

	/** Sends a control request to the server. Returns false when there is no route. */
	bool ForwardToServer(ESimFlowControlRequest Request) const;

	/** Rebuilds NetState from the live instance. Authority only. */
	void UpdateNetState(bool bForce);
	void PushBlackboardToClients();

	/** Finds a task node by guid, preferring the live instance over the template. */
	USimFlowNode_Task* ResolveTaskNode(const FGuid& NodeGuid) const;

	UFUNCTION() void OnRep_NetState();
	UFUNCTION() void OnRep_Blackboard();

	UFUNCTION() void HandleFlowStarted();
	UFUNCTION() void HandleFlowPaused();
	UFUNCTION() void HandleFlowResumed();
	UFUNCTION() void HandleFlowFinished(ESimFlowRunState FinalState);
	UFUNCTION() void HandleTaskStarted(USimFlowNode_Task* Node, USimFlowTask* Task);
	UFUNCTION() void HandleTaskFinished(USimFlowNode_Task* Node, USimFlowTask* Task, ESimFlowResult Result);
	UFUNCTION() void HandleTaskRetried(USimFlowNode_Task* Node, USimFlowTask* Task);
	UFUNCTION() void HandleCheckpointReached(USimFlowNode_Checkpoint* Checkpoint);
	UFUNCTION() void HandleQuizPresented(USimFlowTask_Quiz* Quiz);
	UFUNCTION() void HandleBlackboardChanged(FName Key, const FSimFlowValue& NewValue);

	/** True when presentation events should go out as multicasts rather than fire locally. */
	bool ShouldMulticastEvents() const;

	// Presentation events. Node guids only - clients resolve the rest themselves.
	UFUNCTION(NetMulticast, Reliable) void MulticastFlowStarted();
	UFUNCTION(NetMulticast, Reliable) void MulticastFlowPaused();
	UFUNCTION(NetMulticast, Reliable) void MulticastFlowResumed();
	UFUNCTION(NetMulticast, Reliable) void MulticastFlowFinished(ESimFlowRunState FinalState);
	UFUNCTION(NetMulticast, Reliable) void MulticastTaskStarted(FGuid NodeGuid);
	UFUNCTION(NetMulticast, Reliable) void MulticastTaskFinished(FGuid NodeGuid, ESimFlowResult Result);
	UFUNCTION(NetMulticast, Reliable) void MulticastTaskRetried(FGuid NodeGuid);
	UFUNCTION(NetMulticast, Reliable) void MulticastCheckpointReached(FGuid NodeGuid);
	UFUNCTION(NetMulticast, Reliable) void MulticastQuizPresented(FGuid NodeGuid);

	UPROPERTY(ReplicatedUsing = OnRep_NetState)
	FSimFlowNetState NetState;

	UPROPERTY(ReplicatedUsing = OnRep_Blackboard)
	TArray<FSimFlowBlackboardEntry> ReplicatedBlackboard;

	UPROPERTY(Transient)
	TObjectPtr<USimFlowInstance> Instance = nullptr;

	/** Stand-in blackboard on clients, fed from ReplicatedBlackboard. */
	UPROPERTY(Transient)
	TObjectPtr<USimFlowBlackboard> ClientBlackboard = nullptr;

	UPROPERTY(Transient) bool bAutoStartPending = false;
	UPROPERTY(Transient) float AutoStartTimer = 0.f;
	UPROPERTY(Transient) bool bPausedByGamePause = false;
	UPROPERTY(Transient) bool bBlackboardDirty = false;
	UPROPERTY(Transient) float NetRefreshTimer = 0.f;
};
