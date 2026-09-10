// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SimFlow_PluginTest : ModuleRules
{
	public SimFlow_PluginTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"VRExpansionPlugin"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"SimFlow_PluginTest",
			"SimFlow_PluginTest/Variant_Platforming",
			"SimFlow_PluginTest/Variant_Platforming/Animation",
			"SimFlow_PluginTest/Variant_Combat",
			"SimFlow_PluginTest/Variant_Combat/AI",
			"SimFlow_PluginTest/Variant_Combat/Animation",
			"SimFlow_PluginTest/Variant_Combat/Gameplay",
			"SimFlow_PluginTest/Variant_Combat/Interfaces",
			"SimFlow_PluginTest/Variant_Combat/UI",
			"SimFlow_PluginTest/Variant_SideScrolling",
			"SimFlow_PluginTest/Variant_SideScrolling/AI",
			"SimFlow_PluginTest/Variant_SideScrolling/Gameplay",
			"SimFlow_PluginTest/Variant_SideScrolling/Interfaces",
			"SimFlow_PluginTest/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
