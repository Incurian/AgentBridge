using UnrealBuildTool;

public class AgentBridgeScripting : ModuleRules
{
	public AgentBridgeScripting(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Json",
			"JsonUtilities",
			"AgentBridgeCore",
			"AgentBridgeRuntime",
			"AssetRegistry",
			"ImageWrapper",
			"RenderCore",
			"RHI",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",
				"BlueprintGraph",    // K2Node classes for Blueprint node manipulation
				"KismetCompiler",    // Blueprint compilation utilities
			});
		}
	}
}
