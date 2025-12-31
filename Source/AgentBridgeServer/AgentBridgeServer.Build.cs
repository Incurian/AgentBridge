using UnrealBuildTool;

public class AgentBridgeServer : ModuleRules
{
	public AgentBridgeServer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"HTTPServer",
			"AgentBridgeCore",
			"AgentBridgeRuntime",
			"AgentBridgeScripting",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});

		// Only available in editor
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
