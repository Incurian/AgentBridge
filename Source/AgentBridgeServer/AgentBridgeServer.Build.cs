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
			"AgentBridgeCore",
			"AgentBridgeRuntime",
			"AgentBridgeScripting",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
		});
	}
}
