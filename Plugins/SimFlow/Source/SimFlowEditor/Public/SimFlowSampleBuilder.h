// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class USimFlowAsset;

/**
 * Builds a fully wired example flow in code.
 *
 * Invoked from Tools > SimFlow > Create Sample VR Tutorial Flow. Because the
 * whole graph is authored through the public asset API, this file also doubles
 * as a worked example of building flows procedurally.
 */
class SIMFLOWEDITOR_API FSimFlowSampleBuilder
{
public:
	/** Creates, saves and opens the sample asset. Returns it, or null on failure. */
	static USimFlowAsset* CreateSampleFlowAsset(const FString& PackagePath = TEXT("/Game/SimFlow/Samples"),
		const FString& AssetName = TEXT("SF_SampleVRTutorial"));

	/** Fills an existing (empty) asset with the sample graph. */
	static void PopulateSampleFlow(USimFlowAsset* Asset);
};
