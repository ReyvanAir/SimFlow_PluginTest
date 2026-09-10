// Copyright SimFlow. All Rights Reserved.

#include "SimFlowStatusWidget.h"
#include "SimFlowComponent.h"
#include "SimFlowStatics.h"
#include "SimFlowNodes.h"
#include "SimFlowTask.h"
#include "SimFlowTasks.h"
#include "SimFlowInstance.h"

void USimFlowStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (bAutoBind && !BoundFlow)
	{
		USimFlowComponent* Component = FlowSaveId.IsNone()
			? USimFlowStatics::GetPrimaryFlow(this)
			: USimFlowStatics::FindFlowById(this, FlowSaveId);

		if (Component)
		{
			BindToFlow(Component);
		}
	}
}

void USimFlowStatusWidget::NativeDestruct()
{
	UnbindFromFlow();
	Super::NativeDestruct();
}

void USimFlowStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bAutoBind && !BoundFlow)
	{
		USimFlowComponent* Component = FlowSaveId.IsNone()
			? USimFlowStatics::GetPrimaryFlow(this)
			: USimFlowStatics::FindFlowById(this, FlowSaveId);

		if (Component)
		{
			BindToFlow(Component);
		}
	}
}

void USimFlowStatusWidget::BindToFlow(USimFlowComponent* Component)
{
	if (BoundFlow == Component)
	{
		return;
	}

	UnbindFromFlow();

	BoundFlow = Component;
	if (!BoundFlow)
	{
		return;
	}

	BoundFlow->OnFlowStarted.AddDynamic(this, &USimFlowStatusWidget::HandleFlowStarted);
	BoundFlow->OnFlowPaused.AddDynamic(this, &USimFlowStatusWidget::HandleFlowPaused);
	BoundFlow->OnFlowResumed.AddDynamic(this, &USimFlowStatusWidget::HandleFlowResumed);
	BoundFlow->OnFlowFinished.AddDynamic(this, &USimFlowStatusWidget::HandleFlowFinished);
	BoundFlow->OnTaskStarted.AddDynamic(this, &USimFlowStatusWidget::HandleTaskStarted);
	BoundFlow->OnTaskFinished.AddDynamic(this, &USimFlowStatusWidget::HandleTaskFinished);
	BoundFlow->OnQuizPresented.AddDynamic(this, &USimFlowStatusWidget::HandleQuizPresented);
	BoundFlow->OnNetStateChanged.AddDynamic(this, &USimFlowStatusWidget::HandleNetStateChanged);

	OnFlowBound(BoundFlow);

	// Catch up with whatever is already running.
	if (USimFlowTask* Current = BoundFlow->GetCurrentTask())
	{
		OnTaskStarted(Current);
	}
}

void USimFlowStatusWidget::UnbindFromFlow()
{
	if (!BoundFlow)
	{
		return;
	}

	BoundFlow->OnFlowStarted.RemoveDynamic(this, &USimFlowStatusWidget::HandleFlowStarted);
	BoundFlow->OnFlowPaused.RemoveDynamic(this, &USimFlowStatusWidget::HandleFlowPaused);
	BoundFlow->OnFlowResumed.RemoveDynamic(this, &USimFlowStatusWidget::HandleFlowResumed);
	BoundFlow->OnFlowFinished.RemoveDynamic(this, &USimFlowStatusWidget::HandleFlowFinished);
	BoundFlow->OnTaskStarted.RemoveDynamic(this, &USimFlowStatusWidget::HandleTaskStarted);
	BoundFlow->OnTaskFinished.RemoveDynamic(this, &USimFlowStatusWidget::HandleTaskFinished);
	BoundFlow->OnQuizPresented.RemoveDynamic(this, &USimFlowStatusWidget::HandleQuizPresented);
	BoundFlow->OnNetStateChanged.RemoveDynamic(this, &USimFlowStatusWidget::HandleNetStateChanged);

	BoundFlow = nullptr;
}

FText USimFlowStatusWidget::GetTaskName() const
{
	return BoundFlow ? BoundFlow->GetCurrentTaskName() : FText::GetEmpty();
}

FText USimFlowStatusWidget::GetInstruction() const
{
	return BoundFlow ? BoundFlow->GetCurrentInstruction() : FText::GetEmpty();
}

float USimFlowStatusWidget::GetProgress() const
{
	return BoundFlow ? BoundFlow->GetProgress() : 0.f;
}

float USimFlowStatusWidget::GetScore() const
{
	return BoundFlow ? BoundFlow->GetScore() : 0.f;
}

FText USimFlowStatusWidget::GetTimeRemainingText() const
{
	if (!BoundFlow)
	{
		return FText::GetEmpty();
	}

	// Net-aware: on a client this is computed from the replicated start time.
	const float Remaining = BoundFlow->GetCurrentTaskRemainingTime();
	return (Remaining >= 0.f) ? USimFlowStatics::FormatSeconds(Remaining) : FText::GetEmpty();
}

USimFlowTask_Quiz* USimFlowStatusWidget::GetCurrentQuiz() const
{
	return BoundFlow ? BoundFlow->GetCurrentQuiz() : nullptr;
}

void USimFlowStatusWidget::SubmitQuizAnswer(int32 OptionIndex)
{
	if (BoundFlow)
	{
		BoundFlow->SubmitQuizAnswer(OptionIndex);
	}
}

bool USimFlowStatusWidget::IsFlowPaused() const
{
	return BoundFlow && BoundFlow->IsFlowPaused();
}

bool USimFlowStatusWidget::CanRetry() const
{
	const USimFlowTask* Task = BoundFlow ? BoundFlow->GetCurrentTask() : nullptr;
	return Task && Task->CanRetry();
}

bool USimFlowStatusWidget::CanSkip() const
{
	const USimFlowTask* Task = BoundFlow ? BoundFlow->GetCurrentTask() : nullptr;
	return Task && Task->bAllowSkip;
}

void USimFlowStatusWidget::RequestPauseToggle()	{ if (BoundFlow) { BoundFlow->TogglePause(); } }
void USimFlowStatusWidget::RequestRetry()		{ if (BoundFlow) { BoundFlow->RetryCurrentTask(); } }
void USimFlowStatusWidget::RequestSkip()			{ if (BoundFlow) { BoundFlow->SkipCurrentTask(); } }
void USimFlowStatusWidget::RequestRestart()		{ if (BoundFlow) { BoundFlow->RestartFlow(); } }

void USimFlowStatusWidget::HandleFlowStarted()									{ OnFlowStarted(); }
void USimFlowStatusWidget::HandleFlowPaused()									{ OnFlowPaused(); }
void USimFlowStatusWidget::HandleFlowResumed()									{ OnFlowResumed(); }
void USimFlowStatusWidget::HandleFlowFinished(ESimFlowRunState FinalState)		{ OnFlowFinished(FinalState); }
void USimFlowStatusWidget::HandleQuizPresented(USimFlowTask_Quiz* Quiz)			{ OnQuizPresented(Quiz); }
void USimFlowStatusWidget::HandleNetStateChanged()								{ OnFlowStateReplicated(); }

void USimFlowStatusWidget::HandleTaskStarted(USimFlowNode_Task* /*Node*/, USimFlowTask* Task)
{
	OnTaskStarted(Task);
}

void USimFlowStatusWidget::HandleTaskFinished(USimFlowNode_Task* /*Node*/, USimFlowTask* Task, ESimFlowResult Result)
{
	OnTaskFinished(Task, Result);
}
