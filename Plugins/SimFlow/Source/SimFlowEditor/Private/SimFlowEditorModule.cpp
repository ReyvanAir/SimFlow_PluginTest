// Copyright SimFlow. All Rights Reserved.

#include "SimFlowEditorModule.h"
#include "SimFlowSampleBuilder.h"
#include "ToolMenus.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

DEFINE_LOG_CATEGORY(LogSimFlowEditor);

#define LOCTEXT_NAMESPACE "FSimFlowEditorModule"

const FName FSimFlowEditorModule::SimFlowEditorAppIdentifier(TEXT("SimFlowEditorApp"));

void FSimFlowEditorModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSimFlowEditorModule::RegisterMenus));

	UE_LOG(LogSimFlowEditor, Log, TEXT("SimFlowEditor module started."));
}

void FSimFlowEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

FSimFlowEditorModule& FSimFlowEditorModule::Get()
{
	return FModuleManager::LoadModuleChecked<FSimFlowEditorModule>(TEXT("SimFlowEditor"));
}

void FSimFlowEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
	if (!ToolsMenu)
	{
		return;
	}

	FToolMenuSection& Section = ToolsMenu->FindOrAddSection(TEXT("SimFlow"));
	Section.Label = LOCTEXT("SimFlowSection", "SimFlow");

	Section.AddMenuEntry(
		TEXT("SimFlowCreateSample"),
		LOCTEXT("CreateSample", "Create Sample VR Tutorial Flow"),
		LOCTEXT("CreateSampleTooltip",
			"Creates a fully wired example flow asset that demonstrates sequential tasks, "
			"a parallel section, a timeout race, a quiz with a remediation branch, a checkpoint and branching."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FSimFlowEditorModule::OnCreateSampleFlowClicked)));
}

void FSimFlowEditorModule::OnCreateSampleFlowClicked()
{
	FSimFlowSampleBuilder::CreateSampleFlowAsset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSimFlowEditorModule, SimFlowEditor)
