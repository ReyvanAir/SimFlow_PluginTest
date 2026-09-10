// Copyright SimFlow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "SimFlowAssetFactory.generated.h"

/** Creates new SimFlow Graph assets from the content browser. */
UCLASS()
class SIMFLOWEDITOR_API USimFlowAssetFactory : public UFactory
{
	GENERATED_BODY()

public:
	USimFlowAssetFactory();

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
		UObject* Context, FFeedbackContext* Warn) override;
	virtual bool CanCreateNew() const override { return true; }
	virtual FText GetDisplayName() const override;
	virtual uint32 GetMenuCategories() const override;
};
