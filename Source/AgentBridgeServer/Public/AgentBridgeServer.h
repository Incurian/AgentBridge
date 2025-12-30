#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class AGENTBRIDGESERVER_API FAgentBridgeServerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void StartServer(int32 Port = 50051);
	void StopServer();
	bool IsServerRunning() const;
	int32 GetPort() const;

private:
	int32 CurrentPort = 50051;
	bool bServerRunning = false;
};
