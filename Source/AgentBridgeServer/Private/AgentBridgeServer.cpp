#include "AgentBridgeServer.h"

#define LOCTEXT_NAMESPACE "FAgentBridgeServerModule"

void FAgentBridgeServerModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("AgentBridgeServer: Module started"));

	// TODO: Auto-start server based on settings
}

void FAgentBridgeServerModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("AgentBridgeServer: Module shutdown"));
	StopServer();
}

void FAgentBridgeServerModule::StartServer(int32 Port)
{
	if (bServerRunning)
	{
		UE_LOG(LogTemp, Warning, TEXT("AgentBridgeServer: Server already running on port %d"), CurrentPort);
		return;
	}

	CurrentPort = Port;
	bServerRunning = true;

	UE_LOG(LogTemp, Log, TEXT("AgentBridgeServer: Started on port %d"), CurrentPort);
	// TODO: Actual gRPC server implementation
}

void FAgentBridgeServerModule::StopServer()
{
	if (!bServerRunning)
	{
		return;
	}

	bServerRunning = false;
	UE_LOG(LogTemp, Log, TEXT("AgentBridgeServer: Stopped"));
	// TODO: Actual gRPC server shutdown
}

bool FAgentBridgeServerModule::IsServerRunning() const
{
	return bServerRunning;
}

int32 FAgentBridgeServerModule::GetPort() const
{
	return CurrentPort;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAgentBridgeServerModule, AgentBridgeServer)
