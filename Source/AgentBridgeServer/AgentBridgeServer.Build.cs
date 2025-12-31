using UnrealBuildTool;

public class AgentBridgeServer : TempoModuleRules
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
			"TempoScripting",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"TempoCoreShared",
		});

		// Only available in editor
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
