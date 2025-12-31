#include "AgentHttpServer.h"
#include "CommandExecutor.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpPath.h"
#include "Async/Async.h"

DEFINE_LOG_CATEGORY(LogAgentBridgeServer);

//~==============================================================================
// Singleton
//~==============================================================================

FAgentHttpServer& FAgentHttpServer::Get()
{
	static FAgentHttpServer Instance;
	return Instance;
}

FAgentHttpServer::FAgentHttpServer()
{
}

FAgentHttpServer::~FAgentHttpServer()
{
	Stop();
}

//~==============================================================================
// Server Lifecycle
//~==============================================================================

bool FAgentHttpServer::Start(int32 Port)
{
	if (bIsRunning)
	{
		UE_LOG(LogAgentBridgeServer, Warning, TEXT("Server already running on port %d"), ServerPort);
		return true;
	}

	ServerPort = Port;

	// Get or create HTTP router
	FHttpServerModule& HttpModule = FHttpServerModule::Get();
	HttpRouter = HttpModule.GetHttpRouter(ServerPort);

	if (!HttpRouter.IsValid())
	{
		UE_LOG(LogAgentBridgeServer, Error, TEXT("Failed to create HTTP router on port %d"), ServerPort);
		return false;
	}

	// Register routes

	// POST /agentbridge/execute
	FHttpRouteHandle ExecuteHandle = HttpRouter->BindRoute(
		FHttpPath(TEXT("/agentbridge/execute")),
		EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateRaw(this, &FAgentHttpServer::HandleExecute)
	);
	if (ExecuteHandle.IsValid())
	{
		RouteHandles.Add(ExecuteHandle);
	}

	// POST /agentbridge/batch
	FHttpRouteHandle BatchHandle = HttpRouter->BindRoute(
		FHttpPath(TEXT("/agentbridge/batch")),
		EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateRaw(this, &FAgentHttpServer::HandleBatch)
	);
	if (BatchHandle.IsValid())
	{
		RouteHandles.Add(BatchHandle);
	}

	// GET /agentbridge/health
	FHttpRouteHandle HealthHandle = HttpRouter->BindRoute(
		FHttpPath(TEXT("/agentbridge/health")),
		EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FAgentHttpServer::HandleHealth)
	);
	if (HealthHandle.IsValid())
	{
		RouteHandles.Add(HealthHandle);
	}

	// GET /agentbridge/schema
	FHttpRouteHandle SchemaHandle = HttpRouter->BindRoute(
		FHttpPath(TEXT("/agentbridge/schema")),
		EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FAgentHttpServer::HandleSchema)
	);
	if (SchemaHandle.IsValid())
	{
		RouteHandles.Add(SchemaHandle);
	}

	// Start listening
	HttpModule.StartAllListeners();

	bIsRunning = true;
	UE_LOG(LogAgentBridgeServer, Log, TEXT("AgentBridge HTTP server started on port %d"), ServerPort);
	UE_LOG(LogAgentBridgeServer, Log, TEXT("  POST http://localhost:%d/agentbridge/execute"), ServerPort);
	UE_LOG(LogAgentBridgeServer, Log, TEXT("  POST http://localhost:%d/agentbridge/batch"), ServerPort);
	UE_LOG(LogAgentBridgeServer, Log, TEXT("  GET  http://localhost:%d/agentbridge/health"), ServerPort);
	UE_LOG(LogAgentBridgeServer, Log, TEXT("  GET  http://localhost:%d/agentbridge/schema"), ServerPort);

	return true;
}

void FAgentHttpServer::Stop()
{
	if (!bIsRunning)
	{
		return;
	}

	// Unbind routes
	if (HttpRouter.IsValid())
	{
		for (const FHttpRouteHandle& Handle : RouteHandles)
		{
			HttpRouter->UnbindRoute(Handle);
		}
	}
	RouteHandles.Empty();

	// Stop the listener for our port
	FHttpServerModule& HttpModule = FHttpServerModule::Get();
	HttpModule.StopAllListeners();

	HttpRouter.Reset();
	bIsRunning = false;

	UE_LOG(LogAgentBridgeServer, Log, TEXT("AgentBridge HTTP server stopped"));
}

//~==============================================================================
// Request Handlers
//~==============================================================================

bool FAgentHttpServer::HandleExecute(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Get request body - use explicit length since body may not be null-terminated
	FString BodyString;
	if (Request.Body.Num() > 0)
	{
		FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()), Request.Body.Num());
		BodyString = FString(Converter.Length(), Converter.Get());
	}

	if (BodyString.IsEmpty())
	{
		OnComplete(CreateErrorResponse(400, TEXT("Empty request body")));
		return true;
	}

	UE_LOG(LogAgentBridgeServer, Verbose, TEXT("Execute request: %s"), *BodyString);

	// Execute on Game Thread
	TWeakPtr<FAgentHttpServer> WeakThis = AsShared();

	// For now, execute synchronously since we're already handling thread safety in CommandExecutor
	// In production, this should use AsyncTask(ENamedThreads::GameThread, ...) for safety
	FString ResponseJson = FCommandExecutor::ExecuteJson(BodyString);

	UE_LOG(LogAgentBridgeServer, Verbose, TEXT("Execute response: %s"), *ResponseJson);
	OnComplete(CreateJsonResponse(ResponseJson));

	return true;
}

bool FAgentHttpServer::HandleBatch(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Get request body - use explicit length since body may not be null-terminated
	FString BodyString;
	if (Request.Body.Num() > 0)
	{
		FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()), Request.Body.Num());
		BodyString = FString(Converter.Length(), Converter.Get());
	}

	if (BodyString.IsEmpty())
	{
		OnComplete(CreateErrorResponse(400, TEXT("Empty request body")));
		return true;
	}

	UE_LOG(LogAgentBridgeServer, Verbose, TEXT("Batch request: %s"), *BodyString);

	// Execute batch
	FString ResponseJson = FCommandExecutor::ExecuteBatchJson(BodyString, true);

	UE_LOG(LogAgentBridgeServer, Verbose, TEXT("Batch response: %s"), *ResponseJson);
	OnComplete(CreateJsonResponse(ResponseJson));

	return true;
}

bool FAgentHttpServer::HandleHealth(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	FString ResponseJson = TEXT("{\"status\":\"ok\",\"version\":\"1.0.0\",\"plugin\":\"AgentBridge\"}");
	OnComplete(CreateJsonResponse(ResponseJson));
	return true;
}

bool FAgentHttpServer::HandleSchema(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Return API schema documentation
	FString SchemaJson = TEXT("{")
		TEXT("\"name\":\"AgentBridge API\",")
		TEXT("\"version\":\"1.0.0\",")
		TEXT("\"description\":\"HTTP JSON API for AI agent interaction with Unreal Engine\",")
		TEXT("\"endpoints\":{")
			TEXT("\"/agentbridge/execute\":{\"method\":\"POST\",\"description\":\"Execute a single command\"},")
			TEXT("\"/agentbridge/batch\":{\"method\":\"POST\",\"description\":\"Execute multiple commands\"},")
			TEXT("\"/agentbridge/health\":{\"method\":\"GET\",\"description\":\"Health check\"},")
			TEXT("\"/agentbridge/schema\":{\"method\":\"GET\",\"description\":\"This schema\"}")
		TEXT("},")
		TEXT("\"commands\":[")
			TEXT("\"ListWorlds\",\"SetTargetWorld\",\"QueryActors\",\"GetActor\",")
			TEXT("\"SpawnActor\",\"DeleteActor\",\"SetActorProperties\",\"SetActorTransform\",")
			TEXT("\"GetPropertyPath\",\"SetPropertyPath\",\"CallFunction\",")
			TEXT("\"FindClass\",\"GetClassSchema\",\"ListClasses\"")
		TEXT("]")
	TEXT("}");

	OnComplete(CreateJsonResponse(SchemaJson));
	return true;
}

//~==============================================================================
// Response Helpers
//~==============================================================================

TUniquePtr<FHttpServerResponse> FAgentHttpServer::CreateErrorResponse(int32 Code, const FString& Message)
{
	FString Json = FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\",\"code\":%d}"),
		*Message.ReplaceCharWithEscapedChar(), Code);

	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(Json, TEXT("application/json"));
	return Response;
}

TUniquePtr<FHttpServerResponse> FAgentHttpServer::CreateJsonResponse(const FString& Json)
{
	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(Json, TEXT("application/json"));
	return Response;
}

// Helper for weak pointer (needed by HandleExecute's async pattern)
TSharedPtr<FAgentHttpServer> FAgentHttpServer::AsShared()
{
	// This is a singleton so we return a shared pointer to self
	// In a real implementation, this would be properly managed
	return TSharedPtr<FAgentHttpServer>(&Get(), [](FAgentHttpServer*) {});
}
