// Copyright SimFlow. All Rights Reserved.

#include "SimFlowSampleBuilder.h"
#include "SimFlowEditorModule.h"
#include "SimFlowGraph.h"

#include "SimFlowAsset.h"
#include "SimFlowNode.h"
#include "SimFlowNodes.h"
#include "SimFlowTask.h"
#include "SimFlowTasks.h"
#include "SimFlowCondition.h"
#include "SimFlowConditions.h"
#include "SimFlowGameplayTags.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#define LOCTEXT_NAMESPACE "SimFlowSampleBuilder"

namespace
{
	/** Creates a node of the given class, positions it and returns it typed. */
	template <typename TNode>
	TNode* MakeNode(USimFlowAsset* Asset, float X, float Y, const FString& Comment = FString())
	{
		TNode* Node = Cast<TNode>(Asset->AddNode(TNode::StaticClass()));
		if (Node)
		{
#if WITH_EDITORONLY_DATA
			Node->GraphPosition = FVector2D(X, Y);
#endif
			Node->NodeComment = Comment;
		}
		return Node;
	}

	/** Creates an instanced task on a task node. */
	template <typename TTask>
	TTask* MakeTask(USimFlowNode_Task* Node)
	{
		TTask* Task = NewObject<TTask>(Node, TTask::StaticClass(), NAME_None, RF_Transactional);
		Node->Task = Task;
		return Task;
	}
}

void FSimFlowSampleBuilder::PopulateSampleFlow(USimFlowAsset* Asset)
{
	if (!Asset)
	{
		return;
	}

	Asset->Modify();
	Asset->Nodes.Reset();

	Asset->FlowDisplayName = LOCTEXT("SampleName", "Sample VR Safety Tutorial");
	Asset->FlowDescription = LOCTEXT("SampleDesc",
		"Demonstrates every SimFlow feature: sequential tasks, a checkpoint with auto-save, "
		"a parallel section raced against a timeout, a quiz with a remediation loop, "
		"score based branching and two different endings.");

	// ---------------------------------------------------------------- Nodes

	USimFlowNode_Entry* Entry = MakeNode<USimFlowNode_Entry>(Asset, -700.f, 0.f,
		TEXT("Entry point. The flow component starts here."));

	USimFlowNode_SetBlackboard* Init = MakeNode<USimFlowNode_SetBlackboard>(Asset, -470.f, 0.f,
		TEXT("Reset the score at the start of every run."));
	if (Init)
	{
		Init->Key = TEXT("Score");
		Init->Value = FSimFlowValue::MakeFloat(0.f);
	}

	USimFlowNode_Task* Welcome = MakeNode<USimFlowNode_Task>(Asset, -240.f, 0.f,
		TEXT("Any task can be a Blueprint subclass of SimFlowTask."));
	if (Welcome)
	{
		USimFlowTask_Log* Task = MakeTask<USimFlowTask_Log>(Welcome);
		Task->DisplayName = LOCTEXT("WelcomeName", "Welcome");
		Task->Instruction = LOCTEXT("WelcomeInstruction", "Welcome to the fire safety tutorial.");
		Task->Message = TEXT("Welcome to the fire safety tutorial.");
	}

	USimFlowNode_Task* GoTo = MakeNode<USimFlowNode_Task>(Asset, -10.f, 0.f,
		TEXT("Times out after 60s. The TimedOut pin is wired forward so the run continues either way."));
	if (GoTo)
	{
		USimFlowTask_GoToLocation* Task = MakeTask<USimFlowTask_GoToLocation>(GoTo);
		Task->DisplayName = LOCTEXT("GoToName", "Walk to the equipment station");
		Task->Instruction = LOCTEXT("GoToInstruction", "Teleport over to the red equipment cabinet.");
		Task->AcceptanceRadius = 200.f;
		Task->bRelativeToFlowOwner = true;
		Task->TargetLocation = FVector(500.f, 0.f, 0.f);
		Task->bDrawDebugSphere = true;
		Task->ScoreOnSuccess = 25.f;
		GoTo->TimeLimit = 60.f;
	}

	USimFlowNode_Checkpoint* Checkpoint = MakeNode<USimFlowNode_Checkpoint>(Asset, 230.f, 0.f,
		TEXT("Auto-saves here. Loading with 'From Last Checkpoint' resumes from this node."));
	if (Checkpoint)
	{
		Checkpoint->CheckpointId = TEXT("ReachedStation");
		Checkpoint->bAutoSave = true;
	}

	USimFlowNode_Parallel* Parallel = MakeNode<USimFlowNode_Parallel>(Asset, 450.f, 0.f,
		TEXT("Fires both branches at once."));
	if (Parallel)
	{
		Parallel->NumOutputs = 2;
		Parallel->RebuildPins();
	}

	USimFlowNode_Task* Grab = MakeNode<USimFlowNode_Task>(Asset, 680.f, -110.f,
		TEXT("Waits for a gameplay tag. Raise it from your grab component with Broadcast Flow Event."));
	if (Grab)
	{
		USimFlowTask_WaitForEvent* Task = MakeTask<USimFlowTask_WaitForEvent>(Grab);
		Task->DisplayName = LOCTEXT("GrabName", "Pick up the extinguisher");
		Task->Instruction = LOCTEXT("GrabInstruction", "Grab the extinguisher with either hand.");
		Task->EventTag = SimFlowTags::Sample_GrabExtinguisher.GetTag();
		Task->ScoreOnSuccess = 25.f;
	}

	USimFlowNode_Delay* TimeoutArm = MakeNode<USimFlowNode_Delay>(Asset, 680.f, 110.f,
		TEXT("The other half of the race: after 45s the Join fires regardless."));
	if (TimeoutArm)
	{
		TimeoutArm->Duration = 45.f;
	}

	USimFlowNode_Join* Join = MakeNode<USimFlowNode_Join>(Asset, 910.f, 0.f,
		TEXT("Wait For Any = whichever branch finishes first wins. This is the timeout pattern."));
	if (Join)
	{
		Join->NumInputs = 2;
		Join->Mode = ESimFlowJoinMode::WaitForAny;
		Join->RebuildPins();
	}

	USimFlowNode_Task* Quiz = MakeNode<USimFlowNode_Task>(Asset, 1140.f, 0.f,
		TEXT("Bind On Quiz Presented on the flow component to drive your VR widget."));
	if (Quiz)
	{
		USimFlowTask_Quiz* Task = MakeTask<USimFlowTask_Quiz>(Quiz);
		Task->DisplayName = LOCTEXT("QuizName", "Extinguisher class quiz");
		Task->Instruction = LOCTEXT("QuizInstruction", "Which extinguisher class is used on electrical fires?");
		Task->Question = LOCTEXT("QuizQuestion", "Which extinguisher class is used on electrical fires?");
		Task->Options = {
			LOCTEXT("QuizOptA", "Class A - water"),
			LOCTEXT("QuizOptB", "Class C - carbon dioxide"),
			LOCTEXT("QuizOptC", "Class K - wet chemical")
		};
		Task->CorrectOptionIndex = 1;
		Task->bFailOnWrongAnswer = true;
		Task->ScoreOnSuccess = 50.f;
		Task->MaxRetries = 3;
	}

	USimFlowNode_Task* Remediation = MakeNode<USimFlowNode_Task>(Asset, 1140.f, 220.f,
		TEXT("Wrong answers land here, then loop straight back into the quiz."));
	if (Remediation)
	{
		USimFlowTask_Log* Task = MakeTask<USimFlowTask_Log>(Remediation);
		Task->DisplayName = LOCTEXT("RemediationName", "Review");
		Task->Instruction = LOCTEXT("RemediationInstruction", "Not quite - check the colour band on the cylinder.");
		Task->Message = TEXT("Not quite - check the colour band on the cylinder, then try again.");
		Task->ScoreOnFailure = 0.f;
	}

	USimFlowNode_Branch* Branch = MakeNode<USimFlowNode_Branch>(Asset, 1390.f, 0.f,
		TEXT("Condition based execution: pass if the score reached 75."));
	if (Branch)
	{
		Branch->Cases.Reset();
		FSimFlowBranchCase PassCase;
		PassCase.Label = TEXT("Passed");
		USimFlowCondition_Score* ScoreCondition = NewObject<USimFlowCondition_Score>(Branch, NAME_None, RF_Transactional);
		ScoreCondition->Operation = ESimFlowCompareOp::GreaterOrEqual;
		ScoreCondition->Threshold = 75.f;
		PassCase.Condition = ScoreCondition;
		Branch->Cases.Add(PassCase);
		Branch->bHasDefaultPin = true;
		Branch->RebuildPins();
	}

	USimFlowNode_Finish* Pass = MakeNode<USimFlowNode_Finish>(Asset, 1640.f, -90.f);
	if (Pass)
	{
		Pass->FinishMode = ESimFlowFinishMode::Complete;
	}

	USimFlowNode_Finish* Fail = MakeNode<USimFlowNode_Finish>(Asset, 1640.f, 120.f);
	if (Fail)
	{
		Fail->FinishMode = ESimFlowFinishMode::Fail;
	}

	// ---------------------------------------------------------------- Wiring

	Asset->ConnectNodes(Entry, SimFlowPins::Out, Init, SimFlowPins::In);
	Asset->ConnectNodes(Init, SimFlowPins::Out, Welcome, SimFlowPins::In);
	Asset->ConnectNodes(Welcome, SimFlowPins::Completed, GoTo, SimFlowPins::In);

	Asset->ConnectNodes(GoTo, SimFlowPins::Completed, Checkpoint, SimFlowPins::In);
	Asset->ConnectNodes(GoTo, SimFlowPins::TimedOut, Checkpoint, SimFlowPins::In);

	Asset->ConnectNodes(Checkpoint, SimFlowPins::Out, Parallel, SimFlowPins::In);
	Asset->ConnectNodes(Parallel, TEXT("Out_0"), Grab, SimFlowPins::In);
	Asset->ConnectNodes(Parallel, TEXT("Out_1"), TimeoutArm, SimFlowPins::In);

	Asset->ConnectNodes(Grab, SimFlowPins::Completed, Join, TEXT("In_0"));
	Asset->ConnectNodes(TimeoutArm, SimFlowPins::Out, Join, TEXT("In_1"));

	Asset->ConnectNodes(Join, SimFlowPins::Out, Quiz, SimFlowPins::In);

	Asset->ConnectNodes(Quiz, SimFlowPins::Completed, Branch, SimFlowPins::In);
	Asset->ConnectNodes(Quiz, SimFlowPins::Failed, Remediation, SimFlowPins::In);
	Asset->ConnectNodes(Remediation, SimFlowPins::Completed, Quiz, SimFlowPins::In);

	Asset->ConnectNodes(Branch, USimFlowNode_Branch::MakeCasePinName(0), Pass, SimFlowPins::In);
	Asset->ConnectNodes(Branch, SimFlowPins::Default, Fail, SimFlowPins::In);

	Asset->SanitizeLinks();
}

USimFlowAsset* FSimFlowSampleBuilder::CreateSampleFlowAsset(const FString& PackagePath, const FString& AssetName)
{
	const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	FString UniquePackageName;
	FString UniqueAssetName;
	AssetToolsModule.Get().CreateUniqueAssetName(PackagePath / AssetName, TEXT(""), UniquePackageName, UniqueAssetName);

	UPackage* Package = CreatePackage(*UniquePackageName);
	if (!Package)
	{
		UE_LOG(LogSimFlowEditor, Error, TEXT("Could not create package '%s'."), *UniquePackageName);
		return nullptr;
	}
	Package->FullyLoad();

	USimFlowAsset* Asset = NewObject<USimFlowAsset>(
		Package, USimFlowAsset::StaticClass(), FName(*UniqueAssetName),
		RF_Public | RF_Standalone | RF_Transactional);

	if (!Asset)
	{
		return nullptr;
	}

	PopulateSampleFlow(Asset);

#if WITH_EDITORONLY_DATA
	USimFlowGraph* Graph = USimFlowGraph::CreateGraphForAsset(Asset);
	Asset->EdGraph = Graph;
	if (Graph)
	{
		Graph->RebuildFromAsset();
	}
#endif

	FAssetRegistryModule::AssetCreated(Asset);
	Package->MarkPackageDirty();

	const FString FileName = FPackageName::LongPackageNameToFilename(UniquePackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	UPackage::SavePackage(Package, Asset, *FileName, SaveArgs);

	if (GEditor)
	{
		if (UAssetEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			Subsystem->OpenEditorForAsset(Asset);
		}
	}

	UE_LOG(LogSimFlowEditor, Log, TEXT("Created sample flow '%s'."), *UniquePackageName);
	return Asset;
}

#undef LOCTEXT_NAMESPACE
