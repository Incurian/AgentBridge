#pragma once

#include "CoreMinimal.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAgentBridgeServer, Log, All);

/**
 * FAgentHttpServer - HTTP JSON API server for agent communication.
 *
 * This server exposes AgentBridge functionality via a REST-like HTTP API.
 * External agents (Python clients, MCP servers) can send JSON commands
 * and receive JSON responses.
 *
 * Endpoints:
 *
 * POST /agentbridge/execute
 *   - Executes a single command
 *   - Body: JSON command object
 *   - Returns: JSON response object
 *
 * POST /agentbridge/batch
 *   - Executes multiple commands
 *   - Body: JSON array of commands
 *   - Returns: JSON array of responses
 *
 * GET /agentbridge/health
 *   - Health check endpoint
 *   - Returns: {"status": "ok", "version": "1.0"}
 *
 * GET /agentbridge/schema
 *   - Returns API schema/documentation
 *   - Returns: JSON schema object
 *
 * Configuration:
 *   - Default port: 8080
 *   - Can be changed via Start() parameter
 *
 * Security:
 *   - Only binds to localhost by default
 *   - No authentication (trusted local environment)
 *
 * Thread Safety:
 *   - HTTP requests are received on worker threads
 *   - Commands are dispatched to Game Thread for execution
 *   - Responses are sent back asynchronously
 *
 * @see CommandExecutor for command handling
 * @see AgentCommands.h for command/response structures
 */
class AGENTBRIDGESERVER_API FAgentHttpServer
{
public:
	/**
	 * Gets the singleton instance.
	 */
	static FAgentHttpServer& Get();

	/**
	 * Starts the HTTP server.
	 *
	 * @param Port	Port to listen on (default: 8080).
	 * @return		True if server started successfully.
	 */
	bool Start(int32 Port = 8080);

	/**
	 * Stops the HTTP server.
	 */
	void Stop();

	/**
	 * Checks if the server is running.
	 */
	bool IsRunning() const { return bIsRunning; }

	/**
	 * Gets the port the server is listening on.
	 */
	int32 GetPort() const { return ServerPort; }

private:
	FAgentHttpServer();
	~FAgentHttpServer();

	// Non-copyable
	FAgentHttpServer(const FAgentHttpServer&) = delete;
	FAgentHttpServer& operator=(const FAgentHttpServer&) = delete;

	/** Helper for weak pointer pattern. */
	TSharedPtr<FAgentHttpServer> AsShared();

	/**
	 * Handles POST /agentbridge/execute
	 */
	bool HandleExecute(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/**
	 * Handles POST /agentbridge/batch
	 */
	bool HandleBatch(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/**
	 * Handles GET /agentbridge/health
	 */
	bool HandleHealth(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/**
	 * Handles GET /agentbridge/schema
	 */
	bool HandleSchema(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/**
	 * Creates a JSON error response.
	 */
	TUniquePtr<FHttpServerResponse> CreateErrorResponse(int32 Code, const FString& Message);

	/**
	 * Creates a JSON success response.
	 */
	TUniquePtr<FHttpServerResponse> CreateJsonResponse(const FString& Json);

	/** HTTP router for handling requests. */
	TSharedPtr<IHttpRouter> HttpRouter;

	/** Route handles for cleanup. */
	TArray<FHttpRouteHandle> RouteHandles;

	/** Whether server is running. */
	bool bIsRunning = false;

	/** Port number. */
	int32 ServerPort = 8080;
};
