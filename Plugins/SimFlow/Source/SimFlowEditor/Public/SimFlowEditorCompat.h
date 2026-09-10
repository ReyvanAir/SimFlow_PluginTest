// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h"

#define SIMFLOW_PERFORMACTION_LOCATION_MODE 0

#if SIMFLOW_PERFORMACTION_LOCATION_MODE == 0
using FSimFlowGraphLocation = const FVector2D;
#elif SIMFLOW_PERFORMACTION_LOCATION_MODE == 1
using FSimFlowGraphLocation = const UE::Slate::FDeprecateVector2DParameter&;
#elif SIMFLOW_PERFORMACTION_LOCATION_MODE == 2
using FSimFlowGraphLocation = const UE::Slate::FDeprecateVector2DParameter;
#else
#error "SIMFLOW_PERFORMACTION_LOCATION_MODE must be 0, 1 or 2 - see the comment above."
#endif

 /** Normalises whatever that type is into a plain FVector2D for our own code. */
#define SIMFLOW_GRAPH_LOCATION_TO_VECTOR2D(Loc) FVector2D((Loc).X, (Loc).Y)
