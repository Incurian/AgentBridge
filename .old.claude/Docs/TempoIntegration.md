# Tempo gRPC Integration Guide

> Reference document for integrating AgentBridge with Tempo's gRPC infrastructure.

## Architecture Overview

Tempo provides a complete gRPC infrastructure via the `TempoScripting` module:

```
┌─────────────────────────────────────────────────────────────────┐
│                    External Client (Python/gRPC)                │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼ (gRPC over localhost:50051)
┌─────────────────────────────────────────────────────────────────┐
│                    FTempoScriptingServer                        │
│         Async gRPC server with CompletionQueue                  │
│         Ticks each frame on game thread                         │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    ITempoScriptable                             │
│         Interface for registering gRPC services                 │
│         Implemented by UWorldSubsystem or any UObject           │
└─────────────────────────────────────────────────────────────────┘
```

## Key Components

### 1. ITempoScriptable Interface

```cpp
// TempoScriptable.h
class TEMPOSCRIPTING_API ITempoScriptable
{
    GENERATED_BODY()
public:
    virtual void RegisterScriptingServices(FTempoScriptingServer& ScriptingServer) = 0;
};
```

Any UObject can implement this to register gRPC services.

### 2. FTempoScriptingServer

Core singleton managing the gRPC server:
- `FTempoScriptingServer::Get()` - Access the singleton
- `RegisterService<ServiceType>(...)` - Register RPC handlers
- `ActivateService<ServiceType>(this)` - Activate for current world
- `DeactivateService<ServiceType>()` - Deactivate on shutdown

### 3. Request Handler Templates

```cpp
// For unary RPC (request → response)
SimpleRequestHandler(
    &MyAsyncService::RequestMyMethod,
    &UMySubsystem::HandleMyMethod
)

// For streaming RPC (request → stream of responses)
StreamingRequestHandler(
    &MyAsyncService::RequestMyStream,
    &UMySubsystem::HandleMyStream
)
```

### 4. Response Delegate Pattern

```cpp
template <class ResponseType>
using TResponseDelegate = TDelegate<void(const ResponseType&, grpc::Status)>;

// Handler signature
void HandleMyMethod(
    const MyRequest& Request,
    const TResponseDelegate<MyResponse>& ResponseContinuation
) {
    MyResponse Response;
    // ... populate response ...
    ResponseContinuation.ExecuteIfBound(Response, grpc::Status::OK);
}
```

## Implementation Pattern

### Step 1: Create Proto File

Place in `Source/AgentBridgeServer/Public/AgentBridge.proto`:

```proto
syntax = "proto3";
import "TempoScripting/Empty.proto";
import "TempoScripting/Geometry.proto";

message QueryActorsRequest {
    string class_filter = 1;
    string name_pattern = 2;
    int32 limit = 3;
}

message ActorInfo {
    string name = 1;
    string label = 2;
    string class_name = 3;
    TempoScripting.Transform transform = 4;
}

message QueryActorsResponse {
    repeated ActorInfo actors = 1;
}

service AgentBridgeService {
    rpc QueryActors(QueryActorsRequest) returns (QueryActorsResponse);
    // ... more RPCs ...
}
```

### Step 2: Create Subsystem Header

```cpp
// AgentBridgeServiceSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TempoScriptable.h"
#include "ProtobufGenerated/AgentBridgeServer/AgentBridge.pb.h"
#include "ProtobufGenerated/AgentBridgeServer/AgentBridge.grpc.pb.h"
#include "AgentBridgeServiceSubsystem.generated.h"

UCLASS()
class AGENTBRIDGESERVER_API UAgentBridgeServiceSubsystem :
    public UWorldSubsystem,
    public ITempoScriptable
{
    GENERATED_BODY()

public:
    virtual void RegisterScriptingServices(FTempoScriptingServer& ScriptingServer) override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // RPC Handlers
    void QueryActors(
        const AgentBridgeServer::QueryActorsRequest& Request,
        const TResponseDelegate<AgentBridgeServer::QueryActorsResponse>& ResponseContinuation);
};
```

### Step 3: Implement Subsystem

```cpp
// AgentBridgeServiceSubsystem.cpp
#include "AgentBridgeServiceSubsystem.h"
#include "TempoScriptingServer.h"

using namespace AgentBridgeServer;
using AgentBridgeAsyncService = AgentBridgeService::AsyncService;

void UAgentBridgeServiceSubsystem::RegisterScriptingServices(
    FTempoScriptingServer& ScriptingServer)
{
    ScriptingServer.RegisterService<AgentBridgeService>(
        SimpleRequestHandler(
            &AgentBridgeAsyncService::RequestQueryActors,
            &UAgentBridgeServiceSubsystem::QueryActors)
        // ... more handlers ...
    );
}

void UAgentBridgeServiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    FTempoScriptingServer::Get().ActivateService<AgentBridgeService>(this);
}

void UAgentBridgeServiceSubsystem::Deinitialize()
{
    FTempoScriptingServer::Get().DeactivateService<AgentBridgeService>();
    Super::Deinitialize();
}

void UAgentBridgeServiceSubsystem::QueryActors(
    const QueryActorsRequest& Request,
    const TResponseDelegate<QueryActorsResponse>& ResponseContinuation)
{
    QueryActorsResponse Response;

    // Use existing AgentBridge infrastructure
    FActorQueryParams Params;
    Params.NamePattern = UTF8_TO_TCHAR(Request.name_pattern().c_str());
    Params.Limit = Request.limit();

    TArray<FActorReference> Results = FActorOperations::QueryActors(Params);

    for (const FActorReference& ActorRef : Results)
    {
        ActorInfo* Info = Response.add_actors();
        Info->set_name(TCHAR_TO_UTF8(*ActorRef.Name));
        Info->set_label(TCHAR_TO_UTF8(*ActorRef.Label));
        Info->set_class_name(TCHAR_TO_UTF8(*ActorRef.ClassName));
        // ... set transform ...
    }

    ResponseContinuation.ExecuteIfBound(Response, grpc::Status::OK);
}
```

### Step 4: Update Build.cs

```csharp
// AgentBridgeServer.Build.cs
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "AgentBridgeCore",
    "AgentBridgeRuntime",
    "AgentBridgeScripting",
    "TempoScripting",  // Add this
});
```

## Proto Generation

Tempo's `GenProtos.sh` automatically:
1. Discovers `.proto` files in module `Public/` and `Private/` directories
2. Adds `package ModuleName;` to each proto
3. Runs `protoc` with C++ and Python plugins
4. Places generated code in `ProtobufGenerated/` subdirectories

**Headers** → `Public/ProtobufGenerated/ModuleName/`
**Implementations** → `Private/ProtobufGenerated/ModuleName/`

The script runs automatically via PreBuildSteps defined in Tempo's .uplugin.

## Type Conversions

### String
```cpp
// Proto to UE
FString UEString = UTF8_TO_TCHAR(proto_string.c_str());

// UE to Proto
response.set_name(TCHAR_TO_UTF8(*UEString));
```

### Transform
```cpp
// Use Tempo's helpers from TempoGeometry.h
#include "TempoGeometry.h"

FTransform UETransform = ToUnrealTransform(proto_transform);
*response.mutable_transform() = FromUnrealTransform(UETransform);
```

### Arrays
```cpp
// Reading repeated field
for (const auto& Item : request.items()) {
    // process Item
}

// Writing repeated field
for (const FString& Name : Names) {
    response.add_names(TCHAR_TO_UTF8(*Name));
}
```

## Error Handling

```cpp
// Success
ResponseContinuation.ExecuteIfBound(Response, grpc::Status::OK);

// Not found
ResponseContinuation.ExecuteIfBound(Response,
    grpc::Status(grpc::NOT_FOUND, "Actor not found"));

// Invalid input
ResponseContinuation.ExecuteIfBound(Response,
    grpc::Status(grpc::INVALID_ARGUMENT, "Class name required"));
```

## Streaming Responses

For watch/subscribe patterns:

```cpp
// Store delegates for later
TMap<RequestType, TArray<TResponseDelegate<ResponseType>>> PendingRequests;

void HandleStreamRequest(
    const RequestType& Request,
    const TResponseDelegate<ResponseType>& Continuation)
{
    PendingRequests.FindOrAdd(Request).Add(Continuation);
}

// In Tick(), send updates
void Tick(float DeltaTime)
{
    for (auto& [Request, Delegates] : PendingRequests)
    {
        ResponseType Update = GetCurrentState(Request);
        for (auto& Delegate : Delegates)
        {
            Delegate.ExecuteIfBound(Update, grpc::Status::OK);
        }
    }
}
```

## Key Files Reference

| File | Purpose |
|------|---------|
| `TempoScripting/Public/TempoScriptingServer.h` | Server infrastructure |
| `TempoScripting/Public/TempoScriptable.h` | Interface to implement |
| `TempoScripting/Public/TempoRequestHandlers.h` | Handler templates |
| `TempoWorld/Public/ActorControl.proto` | Example proto |
| `TempoWorld/Public/TempoActorControlServiceSubsystem.h` | Example implementation |
| `TempoCore/Scripts/GenProtos.sh` | Proto generation |

---

*Document Version: 1.0*
*Created: December 2024*
