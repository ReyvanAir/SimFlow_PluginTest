// Copyright SimFlow. All Rights Reserved.

#include "SimFlowComponent.h"
#include "SimFlowAsset.h"
#include "SimFlowNodes.h"
#include "SimFlowTask.h"
#include "SimFlowTasks.h"
#include "SimFlowBlackboard.h"
#include "SimFlowSaveGame.h"
#include "SimFlowSubsystem.h"
#include "SimFlowPlayerComponent.h"
#include "SimFlowRuntimeModule.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

namespace
{
	/** Server world time, which is synchronised on clients via the game state. */
	float GetSyncedTimeSeconds(const UWorld* World)
	{
		if (!World)
		{
			return 0.f;
		}
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			return GameState->GetServerWorldTimeSeconds();
		}
		return World->GetTimeSeconds();
	}
}

USimFlowComponent::USimFlowComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// Flows keep running while the game is paused so a pause menu built on top of
	// them still works. Pausing the flow itself is always explicit.
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	bAutoActivate = true;

	SetIsReplicatedByDefault(true);
}

void USimFlowComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USimFlowComponent, NetState);
	DOREPLIFETIME(USimFlowComponent, ReplicatedBlackboard);
}

// ------------------------------------------------------------------- Authority

bool USimFlowComponent::HasFlowAuthority() const
{
	if (!bReplicateFlow)
	{
		return true;
	}
	const AActor* Owner = GetOwner();
	return !Owner || Owner->HasAuthority();
}

bool USimFlowComponent::IsClientMirror() const
{
	return bReplicateFlow && GetOwner() && !GetOwner()->HasAuthority();
}

bool USimFlowComponent::ShouldMulticastEvents() const
{
	return bReplicateFlow
		&& GetOwner()
		&& GetOwner()->HasAuthority()
		&& GetNetMode() != NM_Standalone;
}

bool USimFlowComponent::ForwardToServer(ESimFlowControlRequest Request) const
{
	if (USimFlowPlayerComponent* Player = USimFlowPlayerComponent::GetLocal(this))
	{
		Player->RequestControl(GetEffectiveSaveId(), Request);
		return true;
	}

	UE_LOG(LogSimFlow, Warning,
		TEXT("Flow '%s': a client tried to control a replicated flow, but the PlayerController has no SimFlow Player Component. ")
		TEXT("Add one to your PlayerController Blueprint so control requests can reach the server."),
		*GetEffectiveSaveId().ToString());
	return false;
}

// ------------------------------------------------------------------- Lifecycle

void USimFlowComponent::BeginPlay()
{
	Super::BeginPlay();

	if (USimFlowSubsystem* Subsystem = USimFlowSubsystem::Get(this))
	{
		Subsystem->RegisterComponent(this);
	}

	if (bReplicateFlow)
	{
		// Clients read score and custom keys from here rather than a live blackboard.
		ClientBlackboard = NewObject<USimFlowBlackboard>(this);

		if (const AActor* Owner = GetOwner())
		{
			if (!Owner->GetIsReplicated() && GetNetMode() != NM_Standalone)
			{
				UE_LOG(LogSimFlow, Warning,
					TEXT("Flow '%s' has Replicate Flow enabled, but its owning actor '%s' does not replicate. ")
					TEXT("Put the component on a replicated actor such as the Game State or Player State."),
					*GetEffectiveSaveId().ToString(), *Owner->GetName());
			}
		}
	}

	if (!HasFlowAuthority())
	{
		// Clients mirror; they never start or run anything themselves.
		return;
	}

	if (bAutoResumeFromSaveOnBeginPlay && HasSaveInSlot(DefaultSaveSlotName, DefaultSaveUserIndex))
	{
		if (LoadFlowFromSlot(DefaultSaveSlotName, DefaultSaveUserIndex, DefaultLoadMode))
		{
			return;
		}
	}

	switch (StartMode)
	{
	case ESimFlowStartMode::AutoOnBeginPlay:
		StartFlow();
		break;
	case ESimFlowStartMode::AutoOnFirstTick:
		bAutoStartPending = true;
		AutoStartTimer = 0.f;
		break;
	case ESimFlowStartMode::AutoAfterDelay:
		bAutoStartPending = true;
		AutoStartTimer = AutoStartDelay;
		break;
	case ESimFlowStartMode::Manual:
	default:
		break;
	}
}

void USimFlowComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Instance && HasFlowAuthority())
	{
		Instance->StopInstance();
	}

	if (USimFlowSubsystem* Subsystem = USimFlowSubsystem::Get(this))
	{
		Subsystem->UnregisterComponent(this);
	}

	Super::EndPlay(EndPlayReason);
}

void USimFlowComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!HasFlowAuthority())
	{
		// Nothing to run on a mirror. Everything it shows comes from replication.
		return;
	}

	if (bAutoStartPending)
	{
		AutoStartTimer -= DeltaTime;
		if (AutoStartTimer <= 0.f)
		{
			bAutoStartPending = false;
			StartFlow();
		}
	}

	if (bFollowGamePause)
	{
		const UWorld* World = GetWorld();
		const bool bGamePaused = World ? World->IsPaused() : false;

		if (bGamePaused && IsFlowRunning())
		{
			PauseFlow();
			bPausedByGamePause = true;
		}
		else if (!bGamePaused && bPausedByGamePause && IsFlowPaused())
		{
			bPausedByGamePause = false;
			ResumeFlow();
		}
	}

	if (Instance)
	{
		Instance->TickInstance(DeltaTime);
	}

	if (bReplicateFlow)
	{
		NetRefreshTimer -= DeltaTime;
		const bool bPeriodic = NetRefreshTimer <= 0.f;
		if (bPeriodic)
		{
			NetRefreshTimer = FMath::Max(0.1f, NetRefreshInterval);
		}

		UpdateNetState(bPeriodic);

		if (bBlackboardDirty)
		{
			PushBlackboardToClients();
		}
	}
}

void USimFlowComponent::EnsureInstance()
{
	if (!Instance)
	{
		Instance = NewObject<USimFlowInstance>(this);
		BindInstanceDelegates();
	}

	if (Instance->GetTemplate() != FlowAsset)
	{
		Instance->InitializeInstance(FlowAsset, this, nullptr);
	}

	if (USimFlowBlackboard* Blackboard = Instance->GetBlackboard())
	{
		if (!Blackboard->OnValueChanged.IsAlreadyBound(this, &USimFlowComponent::HandleBlackboardChanged))
		{
			Blackboard->OnValueChanged.AddDynamic(this, &USimFlowComponent::HandleBlackboardChanged);
		}
	}
}

void USimFlowComponent::BindInstanceDelegates()
{
	if (!Instance)
	{
		return;
	}

	Instance->OnFlowStarted.AddDynamic(this, &USimFlowComponent::HandleFlowStarted);
	Instance->OnFlowPaused.AddDynamic(this, &USimFlowComponent::HandleFlowPaused);
	Instance->OnFlowResumed.AddDynamic(this, &USimFlowComponent::HandleFlowResumed);
	Instance->OnFlowFinished.AddDynamic(this, &USimFlowComponent::HandleFlowFinished);
	Instance->OnTaskStarted.AddDynamic(this, &USimFlowComponent::HandleTaskStarted);
	Instance->OnTaskFinished.AddDynamic(this, &USimFlowComponent::HandleTaskFinished);
	Instance->OnTaskRetried.AddDynamic(this, &USimFlowComponent::HandleTaskRetried);
	Instance->OnCheckpointReached.AddDynamic(this, &USimFlowComponent::HandleCheckpointReached);
	Instance->OnQuizPresented.AddDynamic(this, &USimFlowComponent::HandleQuizPresented);
}

// -------------------------------------------------------------------- Controls

bool USimFlowComponent::StartFlow()
{
	return StartFlowFromEntry(EntryName);
}

bool USimFlowComponent::StartFlowFromEntry(FName InEntryName)
{
	if (IsClientMirror())
	{
		if (InEntryName != EntryName)
		{
			UE_LOG(LogSimFlow, Warning,
				TEXT("A client asked to start flow '%s' from entry '%s'; the server will use its own configured entry '%s'."),
				*GetEffectiveSaveId().ToString(), *InEntryName.ToString(), *EntryName.ToString());
		}
		return ForwardToServer(ESimFlowControlRequest::Start);
	}

	if (!FlowAsset)
	{
		UE_LOG(LogSimFlow, Warning, TEXT("'%s' has no flow asset assigned."), *GetReadableName());
		return false;
	}

	bAutoStartPending = false;
	EnsureInstance();

	const bool bStarted = Instance->StartInstance(InEntryName);
	UpdateNetState(true);
	PushBlackboardToClients();
	return bStarted;
}

void USimFlowComponent::StopFlow()
{
	if (IsClientMirror())
	{
		ForwardToServer(ESimFlowControlRequest::Stop);
		return;
	}

	if (Instance)
	{
		Instance->StopInstance();
		UpdateNetState(true);
	}
}

bool USimFlowComponent::RestartFlow()
{
	if (IsClientMirror())
	{
		return ForwardToServer(ESimFlowControlRequest::Restart);
	}

	StopFlow();
	return StartFlow();
}

void USimFlowComponent::PauseFlow()
{
	if (IsClientMirror())
	{
		ForwardToServer(ESimFlowControlRequest::Pause);
		return;
	}

	if (Instance)
	{
		Instance->PauseInstance();
		UpdateNetState(true);
	}
}

void USimFlowComponent::ResumeFlow()
{
	if (IsClientMirror())
	{
		ForwardToServer(ESimFlowControlRequest::Resume);
		return;
	}

	if (Instance)
	{
		Instance->ResumeInstance();
		UpdateNetState(true);
	}
}

void USimFlowComponent::TogglePause()
{
	if (IsClientMirror())
	{
		ForwardToServer(ESimFlowControlRequest::TogglePause);
		return;
	}

	if (IsFlowPaused())
	{
		ResumeFlow();
	}
	else if (IsFlowRunning())
	{
		PauseFlow();
	}
}

bool USimFlowComponent::RetryCurrentTask()
{
	if (IsClientMirror())
	{
		return ForwardToServer(ESimFlowControlRequest::Retry);
	}

	const bool bDone = Instance ? Instance->RetryActiveTasks() > 0 : false;
	UpdateNetState(true);
	return bDone;
}

bool USimFlowComponent::SkipCurrentTask()
{
	if (IsClientMirror())
	{
		return ForwardToServer(ESimFlowControlRequest::Skip);
	}

	const bool bDone = Instance ? Instance->SkipActiveTasks() > 0 : false;
	UpdateNetState(true);
	return bDone;
}

bool USimFlowComponent::FailCurrentTask()
{
	if (IsClientMirror())
	{
		return ForwardToServer(ESimFlowControlRequest::Fail);
	}

	const bool bDone = Instance ? Instance->FailActiveTasks() > 0 : false;
	UpdateNetState(true);
	return bDone;
}

void USimFlowComponent::SendEvent(FGameplayTag EventTag, UObject* Payload)
{
	if (IsClientMirror())
	{
		if (USimFlowPlayerComponent* Player = USimFlowPlayerComponent::GetLocal(this))
		{
			Player->RequestEvent(GetEffectiveSaveId(), EventTag, Payload);
		}
		else
		{
			UE_LOG(LogSimFlow, Warning,
				TEXT("Event '%s' was raised on a client, but the PlayerController has no SimFlow Player Component to send it to the server."),
				*EventTag.ToString());
		}
		return;
	}

	if (Instance)
	{
		Instance->RaiseEvent(EventTag, Payload);
		UpdateNetState(true);
	}
}

// ------------------------------------------------------------------------ Quiz

void USimFlowComponent::SubmitQuizAnswer(int32 OptionIndex)
{
	FGuid QuizGuid;

	if (Instance)
	{
		for (const USimFlowNode_Task* Node : Instance->GetActiveTaskNodes())
		{
			if (Node && Cast<USimFlowTask_Quiz>(Node->GetTask()))
			{
				QuizGuid = Node->NodeGuid;
				break;
			}
		}
	}
	else
	{
		for (const FSimFlowNetTask& NetTask : NetState.ActiveTasks)
		{
			const USimFlowNode_Task* Node = ResolveTaskNode(NetTask.NodeGuid);
			if (Node && Cast<USimFlowTask_Quiz>(Node->GetTask()))
			{
				QuizGuid = NetTask.NodeGuid;
				break;
			}
		}
	}

	if (!QuizGuid.IsValid())
	{
		UE_LOG(LogSimFlow, Warning, TEXT("SubmitQuizAnswer called but no quiz is currently active."));
		return;
	}

	if (IsClientMirror())
	{
		if (USimFlowPlayerComponent* Player = USimFlowPlayerComponent::GetLocal(this))
		{
			Player->RequestQuizAnswer(GetEffectiveSaveId(), QuizGuid, OptionIndex);
		}
		else
		{
			UE_LOG(LogSimFlow, Warning,
				TEXT("A client answered a quiz, but the PlayerController has no SimFlow Player Component to send it to the server."));
		}
		return;
	}

	AuthoritySubmitQuizAnswer(QuizGuid, OptionIndex);
}

void USimFlowComponent::AuthoritySubmitQuizAnswer(const FGuid& QuizNodeGuid, int32 OptionIndex)
{
	if (!HasFlowAuthority() || !Instance)
	{
		return;
	}

	for (USimFlowNode_Task* Node : Instance->GetActiveTaskNodes())
	{
		if (!Node)
		{
			continue;
		}
		if (QuizNodeGuid.IsValid() && Node->NodeGuid != QuizNodeGuid)
		{
			continue;
		}
		if (USimFlowTask_Quiz* Quiz = Cast<USimFlowTask_Quiz>(Node->GetTask()))
		{
			Quiz->SubmitAnswer(OptionIndex);
			UpdateNetState(true);
			return;
		}
	}

	UE_LOG(LogSimFlow, Warning, TEXT("Quiz answer arrived for a node that is no longer running - ignoring."));
}

USimFlowTask_Quiz* USimFlowComponent::GetCurrentQuiz() const
{
	if (Instance)
	{
		for (const USimFlowNode_Task* Node : Instance->GetActiveTaskNodes())
		{
			if (Node)
			{
				if (USimFlowTask_Quiz* Quiz = Cast<USimFlowTask_Quiz>(Node->GetTask()))
				{
					return Quiz;
				}
			}
		}
		return nullptr;
	}

	for (const FSimFlowNetTask& NetTask : NetState.ActiveTasks)
	{
		if (const USimFlowNode_Task* Node = ResolveTaskNode(NetTask.NodeGuid))
		{
			if (USimFlowTask_Quiz* Quiz = Cast<USimFlowTask_Quiz>(Node->GetTask()))
			{
				return Quiz;
			}
		}
	}
	return nullptr;
}

// -------------------------------------------------------------- Net state push

void USimFlowComponent::UpdateNetState(bool bForce)
{
	if (!bReplicateFlow || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	FSimFlowNetState NewState;
	NewState.bHasEverStarted = NetState.bHasEverStarted;

	if (Instance)
	{
		NewState.RunState = Instance->GetRunState();
		NewState.EntryName = Instance->GetEntryName();
		NewState.ElapsedTime = Instance->GetElapsedTime();
		NewState.Progress = Instance->GetProgress();

		if (NewState.RunState != ESimFlowRunState::NotStarted)
		{
			NewState.bHasEverStarted = true;
		}

		for (const USimFlowNode* Node : Instance->GetActiveNodes())
		{
			if (Node)
			{
				NewState.ActiveNodeGuids.Add(Node->NodeGuid);
			}
		}

		const float ServerNow = GetSyncedTimeSeconds(GetWorld());
		for (const USimFlowNode_Task* TaskNode : Instance->GetActiveTaskNodes())
		{
			if (!TaskNode)
			{
				continue;
			}
			FSimFlowNetTask NetTask;
			NetTask.NodeGuid = TaskNode->NodeGuid;
			NetTask.TimeLimit = TaskNode->TimeLimit;
			NetTask.StartServerTime = ServerNow - TaskNode->GetNodeActiveTime();
			NetTask.RetryCount = TaskNode->GetTask() ? TaskNode->GetTask()->RetryCount : 0;
			NewState.ActiveTasks.Add(NetTask);
		}
	}

	const bool bStructuralChange = !NewState.EqualsIgnoringTime(NetState);
	if (!bStructuralChange && !bForce)
	{
		return;
	}

	NetState = NewState;

	// OnRep never fires on the authority, so tell local listeners directly.
	OnNetStateChanged.Broadcast();
}

void USimFlowComponent::PushBlackboardToClients()
{
	bBlackboardDirty = false;

	if (!bReplicateFlow || !bReplicateBlackboard)
	{
		return;
	}
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	if (!Instance || !Instance->GetBlackboard())
	{
		return;
	}

	ReplicatedBlackboard = Instance->GetBlackboard()->ToEntries(true);
}

void USimFlowComponent::OnRep_NetState()
{
	OnNetStateChanged.Broadcast();
}

void USimFlowComponent::OnRep_Blackboard()
{
	if (!ClientBlackboard)
	{
		ClientBlackboard = NewObject<USimFlowBlackboard>(this);
	}
	ClientBlackboard->FromEntries(ReplicatedBlackboard, true);
}

USimFlowNode_Task* USimFlowComponent::ResolveTaskNode(const FGuid& NodeGuid) const
{
	if (!NodeGuid.IsValid())
	{
		return nullptr;
	}

	if (Instance)
	{
		if (USimFlowNode* RuntimeNode = Instance->FindRuntimeNode(NodeGuid))
		{
			return Cast<USimFlowNode_Task>(RuntimeNode);
		}
	}

	if (FlowAsset)
	{
		return Cast<USimFlowNode_Task>(FlowAsset->FindNodeByGuid(NodeGuid));
	}

	return nullptr;
}

// ----------------------------------------------------------------- Save/load

FName USimFlowComponent::GetEffectiveSaveId() const
{
	if (!FlowSaveId.IsNone())
	{
		return FlowSaveId;
	}
	if (const AActor* Owner = GetOwner())
	{
		return FName(*Owner->GetName());
	}
	return FName(TEXT("SimFlow"));
}

FSimFlowSaveState USimFlowComponent::SaveFlowState() const
{
	if (Instance)
	{
		FSimFlowSaveState State = Instance->SaveInstanceState();
		State.FlowSaveId = GetEffectiveSaveId();
		return State;
	}
	return FSimFlowSaveState();
}

bool USimFlowComponent::LoadFlowState(const FSimFlowSaveState& State, ESimFlowLoadMode LoadMode)
{
	if (IsClientMirror())
	{
		UE_LOG(LogSimFlow, Warning, TEXT("LoadFlowState is server-only for a replicated flow."));
		return false;
	}

	if (!State.IsValid())
	{
		UE_LOG(LogSimFlow, Warning, TEXT("LoadFlowState called with an empty save state."));
		return false;
	}

	if (!FlowAsset)
	{
		FlowAsset = LoadObject<USimFlowAsset>(nullptr, *State.FlowAssetPath);
		if (!FlowAsset)
		{
			UE_LOG(LogSimFlow, Error, TEXT("Could not load flow asset '%s' from the save."), *State.FlowAssetPath);
			return false;
		}
	}

	bAutoStartPending = false;
	EnsureInstance();

	const bool bLoaded = Instance->LoadInstanceState(State, LoadMode);
	UpdateNetState(true);
	PushBlackboardToClients();
	return bLoaded;
}

bool USimFlowComponent::SaveFlowToSlot(const FString& SlotName, int32 UserIndex)
{
	if (IsClientMirror())
	{
		UE_LOG(LogSimFlow, Warning, TEXT("Saving a replicated flow is server-only."));
		return false;
	}

	if (!Instance)
	{
		UE_LOG(LogSimFlow, Warning, TEXT("SaveFlowToSlot: no running flow."));
		return false;
	}

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

	const FName SaveId = GetEffectiveSaveId();
	SaveGame->Flows.Add(SaveId, SaveFlowState());

	const bool bSaved = UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, UserIndex);
	UE_LOG(LogSimFlow, Log, TEXT("SaveFlowToSlot('%s', %d) for '%s': %s"),
		*SlotName, UserIndex, *SaveId.ToString(), bSaved ? TEXT("ok") : TEXT("FAILED"));

	return bSaved;
}

bool USimFlowComponent::LoadFlowFromSlot(const FString& SlotName, int32 UserIndex, ESimFlowLoadMode LoadMode)
{
	if (IsClientMirror())
	{
		UE_LOG(LogSimFlow, Warning, TEXT("Loading a replicated flow is server-only."));
		return false;
	}

	if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		return false;
	}

	USimFlowSaveGame* SaveGame = Cast<USimFlowSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
	if (!SaveGame)
	{
		UE_LOG(LogSimFlow, Warning, TEXT("Slot '%s' does not contain a SimFlow save."), *SlotName);
		return false;
	}

	const FName SaveId = GetEffectiveSaveId();
	if (!SaveGame->Flows.Contains(SaveId))
	{
		UE_LOG(LogSimFlow, Warning, TEXT("Slot '%s' has no state for flow id '%s'."), *SlotName, *SaveId.ToString());
		return false;
	}

	return LoadFlowState(SaveGame->Flows[SaveId], LoadMode);
}

bool USimFlowComponent::QuickSave()
{
	return SaveFlowToSlot(DefaultSaveSlotName, DefaultSaveUserIndex);
}

bool USimFlowComponent::QuickLoad()
{
	return LoadFlowFromSlot(DefaultSaveSlotName, DefaultSaveUserIndex, DefaultLoadMode);
}

bool USimFlowComponent::HasSaveInSlot(const FString& SlotName, int32 UserIndex) const
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		return false;
	}

	const USimFlowSaveGame* SaveGame = Cast<USimFlowSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
	return SaveGame && SaveGame->Flows.Contains(GetEffectiveSaveId());
}

// ------------------------------------------------------------------- Queries

USimFlowBlackboard* USimFlowComponent::GetBlackboard() const
{
	if (Instance)
	{
		return Instance->GetBlackboard();
	}
	return ClientBlackboard;
}

ESimFlowRunState USimFlowComponent::GetRunState() const
{
	return Instance ? Instance->GetRunState() : NetState.RunState;
}

bool USimFlowComponent::IsFlowRunning() const
{
	return GetRunState() == ESimFlowRunState::Running;
}

bool USimFlowComponent::IsFlowPaused() const
{
	return GetRunState() == ESimFlowRunState::Paused;
}

USimFlowTask* USimFlowComponent::GetCurrentTask() const
{
	if (Instance)
	{
		return Instance->GetCurrentTask();
	}

	if (NetState.ActiveTasks.Num() > 0)
	{
		if (const USimFlowNode_Task* Node = ResolveTaskNode(NetState.ActiveTasks[0].NodeGuid))
		{
			return Node->GetTask();
		}
	}
	return nullptr;
}

FText USimFlowComponent::GetCurrentTaskName() const
{
	const USimFlowTask* Task = GetCurrentTask();
	return Task ? Task->GetDisplayNameText() : FText::GetEmpty();
}

FText USimFlowComponent::GetCurrentInstruction() const
{
	const USimFlowTask* Task = GetCurrentTask();
	return Task ? Task->Instruction : FText::GetEmpty();
}

float USimFlowComponent::GetProgress() const
{
	return Instance ? Instance->GetProgress() : NetState.Progress;
}

float USimFlowComponent::GetScore() const
{
	const USimFlowBlackboard* Blackboard = GetBlackboard();
	return Blackboard ? Blackboard->GetScore() : 0.f;
}

float USimFlowComponent::GetCurrentTaskRemainingTime() const
{
	if (Instance)
	{
		for (const USimFlowNode_Task* Node : Instance->GetActiveTaskNodes())
		{
			if (Node)
			{
				const float Remaining = Node->GetRemainingTime();
				if (Remaining >= 0.f)
				{
					return Remaining;
				}
			}
		}
		return -1.f;
	}

	const float ServerNow = GetSyncedTimeSeconds(GetWorld());
	for (const FSimFlowNetTask& NetTask : NetState.ActiveTasks)
	{
		if (NetTask.TimeLimit > 0.f)
		{
			return FMath::Max(0.f, NetTask.TimeLimit - (ServerNow - NetTask.StartServerTime));
		}
	}
	return -1.f;
}

FString USimFlowComponent::GetDebugText() const
{
	if (Instance)
	{
		return Instance->BuildDebugString();
	}

	if (!bReplicateFlow)
	{
		return FString::Printf(TEXT("SimFlow [%s]: not started"), *GetEffectiveSaveId().ToString());
	}

	// Client mirror: describe what replication tells us.
	FString Result = FString::Printf(TEXT("SimFlow [%s] (client mirror)  [%s]  t=%.1fs  progress=%.0f%%\n"),
		*GetEffectiveSaveId().ToString(),
		*StaticEnum<ESimFlowRunState>()->GetNameStringByValue(static_cast<int64>(NetState.RunState)),
		NetState.ElapsedTime,
		NetState.Progress * 100.f);

	for (const FGuid& Guid : NetState.ActiveNodeGuids)
	{
		if (FlowAsset)
		{
			if (const USimFlowNode* Node = FlowAsset->FindNodeByGuid(Guid))
			{
				Result += FString::Printf(TEXT("  > %s\n"), *Node->GetNodeTitle().ToString());
				continue;
			}
		}
		Result += FString::Printf(TEXT("  > %s\n"), *Guid.ToString(EGuidFormats::DigitsWithHyphens));
	}

	if (ClientBlackboard)
	{
		const FString BlackboardText = ClientBlackboard->ToDebugString();
		if (!BlackboardText.IsEmpty())
		{
			Result += TEXT("  Blackboard:\n");
			Result += BlackboardText;
		}
	}

	return Result;
}

// ------------------------------------------------------ Presentation forwarding

void USimFlowComponent::HandleFlowStarted()
{
	if (ShouldMulticastEvents())
	{
		MulticastFlowStarted();
	}
	else
	{
		OnFlowStarted.Broadcast();
	}
	UpdateNetState(true);
}

void USimFlowComponent::HandleFlowPaused()
{
	if (ShouldMulticastEvents())
	{
		MulticastFlowPaused();
	}
	else
	{
		OnFlowPaused.Broadcast();
	}
	UpdateNetState(true);
}

void USimFlowComponent::HandleFlowResumed()
{
	if (ShouldMulticastEvents())
	{
		MulticastFlowResumed();
	}
	else
	{
		OnFlowResumed.Broadcast();
	}
	UpdateNetState(true);
}

void USimFlowComponent::HandleFlowFinished(ESimFlowRunState FinalState)
{
	if (ShouldMulticastEvents())
	{
		MulticastFlowFinished(FinalState);
	}
	else
	{
		OnFlowFinished.Broadcast(FinalState);
	}
	UpdateNetState(true);
	PushBlackboardToClients();
}

void USimFlowComponent::HandleTaskStarted(USimFlowNode_Task* Node, USimFlowTask* Task)
{
	if (ShouldMulticastEvents())
	{
		MulticastTaskStarted(Node ? Node->NodeGuid : FGuid());
	}
	else
	{
		OnTaskStarted.Broadcast(Node, Task);
	}
	UpdateNetState(true);
}

void USimFlowComponent::HandleTaskFinished(USimFlowNode_Task* Node, USimFlowTask* Task, ESimFlowResult Result)
{
	if (ShouldMulticastEvents())
	{
		MulticastTaskFinished(Node ? Node->NodeGuid : FGuid(), Result);
	}
	else
	{
		OnTaskFinished.Broadcast(Node, Task, Result);
	}
	UpdateNetState(true);
	PushBlackboardToClients();
}

void USimFlowComponent::HandleTaskRetried(USimFlowNode_Task* Node, USimFlowTask* Task)
{
	if (ShouldMulticastEvents())
	{
		MulticastTaskRetried(Node ? Node->NodeGuid : FGuid());
	}
	else
	{
		OnTaskRetried.Broadcast(Node, Task);
	}
	UpdateNetState(true);
}

void USimFlowComponent::HandleCheckpointReached(USimFlowNode_Checkpoint* Checkpoint)
{
	if (ShouldMulticastEvents())
	{
		MulticastCheckpointReached(Checkpoint ? Checkpoint->NodeGuid : FGuid());
	}
	else
	{
		OnCheckpointReached.Broadcast(Checkpoint);
	}
}

void USimFlowComponent::HandleQuizPresented(USimFlowTask_Quiz* Quiz)
{
	if (ShouldMulticastEvents())
	{
		const USimFlowNode_Task* Node = Quiz ? Quiz->OwningNode.Get() : nullptr;
		MulticastQuizPresented(Node ? Node->NodeGuid : FGuid());
	}
	else
	{
		OnQuizPresented.Broadcast(Quiz);
	}
}

void USimFlowComponent::HandleBlackboardChanged(FName /*Key*/, const FSimFlowValue& /*NewValue*/)
{
	bBlackboardDirty = true;
}

// ------------------------------------------------------------- Multicast impls

void USimFlowComponent::MulticastFlowStarted_Implementation()
{
	OnFlowStarted.Broadcast();
}

void USimFlowComponent::MulticastFlowPaused_Implementation()
{
	OnFlowPaused.Broadcast();
}

void USimFlowComponent::MulticastFlowResumed_Implementation()
{
	OnFlowResumed.Broadcast();
}

void USimFlowComponent::MulticastFlowFinished_Implementation(ESimFlowRunState FinalState)
{
	OnFlowFinished.Broadcast(FinalState);
}

void USimFlowComponent::MulticastTaskStarted_Implementation(FGuid NodeGuid)
{
	USimFlowNode_Task* Node = ResolveTaskNode(NodeGuid);
	OnTaskStarted.Broadcast(Node, Node ? Node->GetTask() : nullptr);
}

void USimFlowComponent::MulticastTaskFinished_Implementation(FGuid NodeGuid, ESimFlowResult Result)
{
	USimFlowNode_Task* Node = ResolveTaskNode(NodeGuid);
	OnTaskFinished.Broadcast(Node, Node ? Node->GetTask() : nullptr, Result);
}

void USimFlowComponent::MulticastTaskRetried_Implementation(FGuid NodeGuid)
{
	USimFlowNode_Task* Node = ResolveTaskNode(NodeGuid);
	OnTaskRetried.Broadcast(Node, Node ? Node->GetTask() : nullptr);
}

void USimFlowComponent::MulticastCheckpointReached_Implementation(FGuid NodeGuid)
{
	USimFlowNode_Checkpoint* Checkpoint = nullptr;

	if (Instance)
	{
		Checkpoint = Cast<USimFlowNode_Checkpoint>(Instance->FindRuntimeNode(NodeGuid));
	}
	if (!Checkpoint && FlowAsset)
	{
		Checkpoint = Cast<USimFlowNode_Checkpoint>(FlowAsset->FindNodeByGuid(NodeGuid));
	}

	OnCheckpointReached.Broadcast(Checkpoint);
}

void USimFlowComponent::MulticastQuizPresented_Implementation(FGuid NodeGuid)
{
	const USimFlowNode_Task* Node = ResolveTaskNode(NodeGuid);
	USimFlowTask_Quiz* Quiz = Node ? Cast<USimFlowTask_Quiz>(Node->GetTask()) : nullptr;
	OnQuizPresented.Broadcast(Quiz);
}
