#include "AgentBridgeScripting.h"

#define LOCTEXT_NAMESPACE "FAgentBridgeScriptingModule"

void FAgentBridgeScriptingModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("AgentBridgeScripting: Module started"));
}

void FAgentBridgeScriptingModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("AgentBridgeScripting: Module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAgentBridgeScriptingModule, AgentBridgeScripting)
