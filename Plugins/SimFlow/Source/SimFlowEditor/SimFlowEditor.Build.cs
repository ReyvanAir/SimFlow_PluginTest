// Copyright SimFlow. All Rights Reserved.

using UnrealBuildTool;

public class SimFlowEditor : ModuleRules
{
	public SimFlowEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"SimFlowRuntime"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"EditorFramework",
			"EditorSubsystem",
			"ToolMenus",
			"GraphEditor",
			"KismetWidgets",
			"PropertyEditor",
			"AssetDefinition",
			"AssetTools",
			"ApplicationCore",
			"Projects",
			"GameplayTags",
			"WorkspaceMenuStructure"
		});
	}
}
