// Copyright SimFlow. All Rights Reserved.

#include "AssetDefinition_SimFlowAsset.h"
#include "SimFlowAsset.h"
#include "SimFlowAssetEditor.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_SimFlowAsset"

FText UAssetDefinition_SimFlowAsset::GetAssetDisplayName() const
{
	return LOCTEXT("SimFlowAssetDisplayName", "SimFlow Graph");
}

FLinearColor UAssetDefinition_SimFlowAsset::GetAssetColor() const
{
	return FLinearColor(FColor(52, 122, 196));
}

TSoftClassPtr<UObject> UAssetDefinition_SimFlowAsset::GetAssetClass() const
{
	return USimFlowAsset::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_SimFlowAsset::GetAssetCategories() const
{
	static const auto Categories = { FAssetCategoryPath(LOCTEXT("SimFlowCategory", "SimFlow")) };
	return Categories;
}

EAssetCommandResult UAssetDefinition_SimFlowAsset::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (USimFlowAsset* Asset : OpenArgs.LoadObjects<USimFlowAsset>())
	{
		const TSharedRef<FSimFlowAssetEditor> Editor = MakeShared<FSimFlowAssetEditor>();
		Editor->InitEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Asset);
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
