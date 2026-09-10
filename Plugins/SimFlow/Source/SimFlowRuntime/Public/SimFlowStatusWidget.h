// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SimFlowTypes.h"
#include "SimFlowStatusWidget.generated.h"

class USimFlowComponent;
class USimFlowTask;
class USimFlowTask_Quiz;
class USimFlowNode_Task;

/**
 * Base class for a tutorial / status panel.
 *
 * Reparent your VR widget Blueprint to this, and the events below fire as the
 * flow progresses. Works equally well as a screen-space HUD or a world-space
 * panel attached to the player's wrist.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "SimFlow Status Widget"))
class SIMFLOWRUNTIME_API USimFlowStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Which flow to watch. Leave as None to follow the primary running flow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimFlow")
	FName FlowSaveId = NAME_None;

	/** Re-check for a flow every frame until one is found. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimFlow")
	bool bAutoBind = true;

	UFUNCTION(BlueprintCallable, Category = "SimFlow")
	void BindToFlow(USimFlowComponent* Component);

	UFUNCTION(BlueprintCallable, Category = "SimFlow")
	void UnbindFromFlow();

	UFUNCTION(BlueprintPure, Category = "SimFlow")
	USimFlowComponent* GetBoundFlow() const { return BoundFlow; }

	// --------------------------------------------------------------- Queries

	UFUNCTION(BlueprintPure, Category = "SimFlow") FText	GetTaskName() const;
	UFUNCTION(BlueprintPure, Category = "SimFlow") FText	GetInstruction() const;
	UFUNCTION(BlueprintPure, Category = "SimFlow") float	GetProgress() const;
	UFUNCTION(BlueprintPure, Category = "SimFlow") float	GetScore() const;
	UFUNCTION(BlueprintPure, Category = "SimFlow") FText	GetTimeRemainingText() const;
	UFUNCTION(BlueprintPure, Category = "SimFlow") bool	IsFlowPaused() const;
	UFUNCTION(BlueprintPure, Category = "SimFlow") bool	CanRetry() const;
	UFUNCTION(BlueprintPure, Category = "SimFlow") bool	CanSkip() const;

	// --------------------------------------------------- Controls for buttons

	UFUNCTION(BlueprintCallable, Category = "SimFlow") void RequestPauseToggle();
	UFUNCTION(BlueprintCallable, Category = "SimFlow") void RequestRetry();
	UFUNCTION(BlueprintCallable, Category = "SimFlow") void RequestSkip();
	UFUNCTION(BlueprintCallable, Category = "SimFlow") void RequestRestart();

	/**
	 * Answers the quiz currently on screen. Wire this to your answer buttons.
	 * In multiplayer it is forwarded to the server for you.
	 */
	UFUNCTION(BlueprintCallable, Category = "SimFlow") void SubmitQuizAnswer(int32 OptionIndex);

	/** The quiz being asked, or null. Read Question and Options straight off it. */
	UFUNCTION(BlueprintPure, Category = "SimFlow") USimFlowTask_Quiz* GetCurrentQuiz() const;

	// ---------------------------------------------------------- Design events

	UFUNCTION(BlueprintImplementableEvent, Category = "SimFlow") void OnFlowBound(USimFlowComponent* Component);
	UFUNCTION(BlueprintImplementableEvent, Category = "SimFlow") void OnFlowStarted();
	UFUNCTION(BlueprintImplementableEvent, Category = "SimFlow") void OnFlowPaused();
	UFUNCTION(BlueprintImplementableEvent, Category = "SimFlow") void OnFlowResumed();
	UFUNCTION(BlueprintImplementableEvent, Category = "SimFlow") void OnFlowFinished(ESimFlowRunState FinalState);
	UFUNCTION(BlueprintImplementableEvent, Category = "SimFlow") void OnTaskStarted(USimFlowTask* Task);
	UFUNCTION(BlueprintImplementableEvent, Category = "SimFlow") void OnTaskFinished(USimFlowTask* Task, ESimFlowResult Result);
	UFUNCTION(BlueprintImplementableEvent, Category = "SimFlow") void OnQuizPresented(USimFlowTask_Quiz* Quiz);

	/** Fires on clients whenever the server's flow state arrives. Refresh your bindings here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "SimFlow") void OnFlowStateReplicated();

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION() void HandleFlowStarted();
	UFUNCTION() void HandleFlowPaused();
	UFUNCTION() void HandleFlowResumed();
	UFUNCTION() void HandleFlowFinished(ESimFlowRunState FinalState);
	UFUNCTION() void HandleTaskStarted(USimFlowNode_Task* Node, USimFlowTask* Task);
	UFUNCTION() void HandleTaskFinished(USimFlowNode_Task* Node, USimFlowTask* Task, ESimFlowResult Result);
	UFUNCTION() void HandleQuizPresented(USimFlowTask_Quiz* Quiz);
	UFUNCTION() void HandleNetStateChanged();

	UPROPERTY(Transient)
	TObjectPtr<USimFlowComponent> BoundFlow = nullptr;
};
