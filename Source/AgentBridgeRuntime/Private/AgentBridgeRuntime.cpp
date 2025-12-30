#include "AgentBridgeRuntime.h"

#define LOCTEXT_NAMESPACE "FAgentBridgeRuntimeModule"

void FAgentBridgeRuntimeModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("AgentBridgeRuntime: Module started"));
}

void FAgentBridgeRuntimeModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("AgentBridgeRuntime: Module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAgentBridgeRuntimeModule, AgentBridgeRuntime)
