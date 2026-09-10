// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

SIMFLOWEDITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogSimFlowEditor, Log, All);

class FSimFlowEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FSimFlowEditorModule& Get();

	/** Shared toolkit app identifier. */
	static const FName SimFlowEditorAppIdentifier;

private:
	void RegisterMenus();
	void OnCreateSampleFlowClicked();
};
