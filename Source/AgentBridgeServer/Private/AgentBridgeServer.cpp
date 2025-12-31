#include "AgentBridgeServer.h"
#include "AgentHttpServer.h"

#define LOCTEXT_NAMESPACE "FAgentBridgeServerModule"

void FAgentBridgeServerModule::StartupModule()
{
	UE_LOG(LogAgentBridgeServer, Log, TEXT("AgentBridgeServer: Module started"));

	// Auto-start HTTP server on default port
	// Can be disabled via console or config in the future
	StartServer(8080);
}

void FAgentBridgeServerModule::ShutdownModule()
{
	UE_LOG(LogAgentBridgeServer, Log, TEXT("AgentBridgeServer: Module shutdown"));
	StopServer();
}

void FAgentBridgeServerModule::StartServer(int32 Port)
{
	if (FAgentHttpServer::Get().IsRunning())
	{
		UE_LOG(LogAgentBridgeServer, Warning, TEXT("Server already running on port %d"),
			FAgentHttpServer::Get().GetPort());
		return;
	}

	CurrentPort = Port;
	bServerRunning = FAgentHttpServer::Get().Start(Port);
}

void FAgentBridgeServerModule::StopServer()
{
	if (!FAgentHttpServer::Get().IsRunning())
	{
		return;
	}

	FAgentHttpServer::Get().Stop();
	bServerRunning = false;
}

bool FAgentBridgeServerModule::IsServerRunning() const
{
	return FAgentHttpServer::Get().IsRunning();
}

int32 FAgentBridgeServerModule::GetPort() const
{
	return FAgentHttpServer::Get().GetPort();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAgentBridgeServerModule, AgentBridgeServer)
