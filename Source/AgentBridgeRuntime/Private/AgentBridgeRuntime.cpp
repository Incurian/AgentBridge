#include "AgentBridgeRuntime.h"
#include "AgentBridgeDebug.h"

#define LOCTEXT_NAMESPACE "FAgentBridgeRuntimeModule"

void FAgentBridgeRuntimeModule::StartupModule()
{
	UE_LOG(LogAgentBridge, Log, TEXT("AgentBridgeRuntime: Module started"));
	FAgentBridgeDebug::RegisterCommands();
}

void FAgentBridgeRuntimeModule::ShutdownModule()
{
	FAgentBridgeDebug::UnregisterCommands();
	UE_LOG(LogAgentBridge, Log, TEXT("AgentBridgeRuntime: Module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAgentBridgeRuntimeModule, AgentBridgeRuntime)
