// Copyright SimFlow. All Rights Reserved.

#include "SimFlowGameplayTags.h"

namespace SimFlowTags
{
	UE_DEFINE_GAMEPLAY_TAG(Event, "SimFlow.Event");
	UE_DEFINE_GAMEPLAY_TAG(Event_Generic, "SimFlow.Event.Generic");
	UE_DEFINE_GAMEPLAY_TAG(Event_Interact, "SimFlow.Event.Interact");
	UE_DEFINE_GAMEPLAY_TAG(Event_Grab, "SimFlow.Event.Grab");
	UE_DEFINE_GAMEPLAY_TAG(Event_Release, "SimFlow.Event.Release");
	UE_DEFINE_GAMEPLAY_TAG(Event_ButtonPressed, "SimFlow.Event.ButtonPressed");
	UE_DEFINE_GAMEPLAY_TAG(Event_Placed, "SimFlow.Event.Placed");
	UE_DEFINE_GAMEPLAY_TAG(Event_Removed, "SimFlow.Event.Removed");

	UE_DEFINE_GAMEPLAY_TAG(Mistake, "SimFlow.Mistake");
	UE_DEFINE_GAMEPLAY_TAG(Mistake_WrongItem, "SimFlow.Mistake.WrongItem");
	UE_DEFINE_GAMEPLAY_TAG(Mistake_WrongTarget, "SimFlow.Mistake.WrongTarget");
	UE_DEFINE_GAMEPLAY_TAG(Mistake_WrongOrder, "SimFlow.Mistake.WrongOrder");
	UE_DEFINE_GAMEPLAY_TAG(Mistake_WrongAnswer, "SimFlow.Mistake.WrongAnswer");

	UE_DEFINE_GAMEPLAY_TAG(Sample_GrabExtinguisher, "SimFlow.Sample.GrabExtinguisher");
	UE_DEFINE_GAMEPLAY_TAG(Sample_PullPin, "SimFlow.Sample.PullPin");
}
