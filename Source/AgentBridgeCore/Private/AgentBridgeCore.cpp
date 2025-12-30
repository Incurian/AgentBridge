#include "AgentBridgeCore.h"

#define LOCTEXT_NAMESPACE "FAgentBridgeCoreModule"

void FAgentBridgeCoreModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("AgentBridgeCore: Module started"));
}

void FAgentBridgeCoreModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("AgentBridgeCore: Module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAgentBridgeCoreModule, AgentBridgeCore)
