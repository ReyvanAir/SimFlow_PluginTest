// Copyright InventoryBubble. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class InventoryBubble : ModuleRules
{
	public InventoryBubble(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects"
		});

		// The VR Expansion Plugin is optional. Detect it at build time so this
		// module compiles cleanly in a project that has it, does not have it, or
		// has only the UE5 VR Template. Every VRE call site is #if-guarded on
		// WITH_VR_EXPANSION_PLUGIN, which is always defined either way.
		bool bHasVRExpansion = IsVRExpansionPluginAvailable(Target);

		if (bHasVRExpansion)
		{
			PrivateDependencyModuleNames.Add("VRExpansionPlugin");
			PublicDefinitions.Add("WITH_VR_EXPANSION_PLUGIN=1");
			System.Console.WriteLine("[InventoryBubble] VRExpansionPlugin found - VRE adapter enabled.");
		}
		else
		{
			PublicDefinitions.Add("WITH_VR_EXPANSION_PLUGIN=0");
			System.Console.WriteLine("[InventoryBubble] VRExpansionPlugin not found - VRE adapter compiled out.");
		}
	}

	/// <summary>
	/// Looks for VRExpansionPlugin.uplugin under the host project's Plugins folder
	/// and under the engine's Plugins folder. Purely a file probe, so it costs
	/// nothing and never throws on a project that has no Plugins directory.
	/// </summary>
	private bool IsVRExpansionPluginAvailable(ReadOnlyTargetRules Target)
	{
		if (Target.ProjectFile != null)
		{
			string ProjectPluginsDir = Path.Combine(Target.ProjectFile.Directory.FullName, "Plugins");
			if (ContainsVRExpansionUPlugin(ProjectPluginsDir))
			{
				return true;
			}
		}

		string EnginePluginsDir = Path.Combine(EngineDirectory, "Plugins");
		return ContainsVRExpansionUPlugin(EnginePluginsDir);
	}

	private bool ContainsVRExpansionUPlugin(string SearchRoot)
	{
		try
		{
			if (!Directory.Exists(SearchRoot))
			{
				return false;
			}

			string[] Found = Directory.GetFiles(SearchRoot, "VRExpansionPlugin.uplugin", SearchOption.AllDirectories);
			return Found.Length > 0;
		}
		catch (System.Exception)
		{
			// A permissions or reparse-point error while probing must never fail
			// the build - treat it as "not installed" and compile the VRE path out.
			return false;
		}
	}
}
