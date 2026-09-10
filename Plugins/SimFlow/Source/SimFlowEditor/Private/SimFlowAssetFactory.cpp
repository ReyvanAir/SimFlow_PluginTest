// Copyright SimFlow. All Rights Reserved.

#include "SimFlowAssetFactory.h"
#include "SimFlowAsset.h"
#include "SimFlowGraph.h"
#include "SimFlowGraphSchema.h"
#include "AssetTypeCategories.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "SimFlowAssetFactory"

USimFlowAssetFactory::USimFlowAssetFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = USimFlowAsset::StaticClass();
}

UObject* USimFlowAssetFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	UObject* /*Context*/, FFeedbackContext* /*Warn*/)
{
	USimFlowAsset* NewAsset = NewObject<USimFlowAsset>(InParent, InClass, InName, Flags | RF_Transactional);

#if WITH_EDITORONLY_DATA
	USimFlowGraph* Graph = USimFlowGraph::CreateGraphForAsset(NewAsset);
	NewAsset->EdGraph = Graph;

	if (Graph)
	{
		Graph->GetSchema()->CreateDefaultNodesForGraph(*Graph);
		Graph->CompileToAsset();
	}
#endif

	return NewAsset;
}

FText USimFlowAssetFactory::GetDisplayName() const
{
	return LOCTEXT("SimFlowAssetName", "SimFlow Graph");
}

uint32 USimFlowAssetFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Gameplay;
}

#undef LOCTEXT_NAMESPACE
