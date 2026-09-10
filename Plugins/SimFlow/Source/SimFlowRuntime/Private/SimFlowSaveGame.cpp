// Copyright SimFlow. All Rights Reserved.

#include "SimFlowSaveGame.h"

FSimFlowSaveState USimFlowSaveGame::GetFlow(FName FlowSaveId) const
{
	if (const FSimFlowSaveState* Found = Flows.Find(FlowSaveId))
	{
		return *Found;
	}
	return FSimFlowSaveState();
}
