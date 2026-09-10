// Copyright SimFlow. All Rights Reserved.

#include "SimFlowRuntimeModule.h"

DEFINE_LOG_CATEGORY(LogSimFlow);

#define LOCTEXT_NAMESPACE "FSimFlowRuntimeModule"

void FSimFlowRuntimeModule::StartupModule()
{
	UE_LOG(LogSimFlow, Log, TEXT("SimFlowRuntime module started."));
}

void FSimFlowRuntimeModule::ShutdownModule()
{
	UE_LOG(LogSimFlow, Log, TEXT("SimFlowRuntime module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSimFlowRuntimeModule, SimFlowRuntime)
