# AgentBridgeServer - Network Layer

**Plugin:** `AgentBridgeServer/`
**Dependencies:** AgentBridgeCore, AgentBridgeRuntime, AgentBridgeScripting, TempoCore
**Depended on by:** None (top of C++ stack)

AgentBridgeServer provides the gRPC and HTTP interfaces. It contains only thin handler
methods and proto definitions - all business logic is in Scripting to avoid gRPC header
conflicts.

## Class Diagram

```mermaid
classDiagram
    direction TB

    class FAgentBridgeServerModule {
        <<IModuleInterface>>
        -CurrentPort: int32
        -bServerRunning: bool
        +StartupModule()
        +ShutdownModule()
        +StartServer(Port)
        +StopServer()
        +IsServerRunning() bool
        +GetPort() int32
    }

    class UAgentBridgeServiceSubsystem {
        <<UWorldSubsystem, ITempoScriptable>>
        +Initialize(Collection)
        +Deinitialize()
        +ShouldCreateSubsystem(Outer) bool
        +RegisterScriptingServices(Server)
        -HandleListWorlds(Req, Resp)
        -HandleSetTargetWorld(Req, Resp)
        -HandleQueryActors(Req, Resp)
        -HandleGetActor(Req, Resp)
        -HandleSpawnActor(Req, Resp)
        -HandleDeleteActor(Req, Resp)
        -HandleDuplicateActor(Req, Resp)
        -HandleGetPropertyPath(Req, Resp)
        -HandleSetPropertyPath(Req, Resp)
        -HandleCallFunction(Req, Resp)
        -HandleCallAssetFunction(Req, Resp)
        -HandleFindClass(Req, Resp)
        -HandleGetClassSchema(Req, Resp)
        -HandleListClasses(Req, Resp)
        -HandleCreateBlueprintNode(Req, Resp)
        -HandleConnectBlueprintPins(Req, Resp)
        -HandleListBlueprintNodes(Req, Resp)
        -HandleListBlueprintPins(Req, Resp)
        - 51 handlers total
    }

    class FAgentHttpServer {
        <<singleton>>
        -HttpRouter: TSharedPtr~IHttpRouter~
        -RouteHandles: TArray~FHttpRouteHandle~
        -bIsRunning: bool
        -ServerPort: int32
        +Get()$ FAgentHttpServer
        +Start(Port) bool
        +Stop()
        +IsRunning() bool
        +GetPort() int32
        -HandleExecute(Req, Resp) bool
        -HandleBatch(Req, Resp) bool
        -HandleHealth(Req, Resp) bool
        -HandleSchema(Req, Resp) bool
    }

    class ProtoMessages["AgentBridge.proto (888 lines)"] {
        <<protobuf definitions>>
        PropertyValue  PropertyKeyValue  PropertyInfo
        ClassInfo  ClassSchema  FunctionSignature
        ActorDescriptor  ActorTransform  Scale  Color
        BlueprintNodeInfo  BlueprintPinInfo
        StreamingActorInfo  BoundingBox
        51 Request/Response message pairs
    }

    class AgentBridgeService["AgentBridgeService (gRPC)"] {
        <<protobuf service, 51 RPCs>>
        rpc ListWorlds(Req) returns (Resp)
        rpc QueryActors(Req) returns (Resp)
        rpc SpawnActor(Req) returns (Resp)
        rpc GetPropertyPath(Req) returns (Resp)
        rpc SetPropertyPath(Req) returns (Resp)
        rpc CallFunction(Req) returns (Resp)
        rpc CreateBlueprintNode(Req) returns (Resp)
        rpc ListBlueprintNodes(Req) returns (Resp)
        ...
    }

    class ValueConverters["Value Conversion Helpers (anonymous namespace)"] {
        +SetProtoVector(FVector, proto)$
        +SetProtoRotation(FRotator, proto)$
        +SetProtoTransform(FTransform, proto)$
        +FromProtoVector(proto) FVector$
        +FromProtoRotation(proto) FRotator$
        +FromProtoTransform(proto) FTransform$
        +FillActorDescriptor(FActorInfo, proto)$
        +JsonToProtoPropertyValue(json, type) proto$
        +ProtoPropertyValueToJson(proto) FString$
    }

    FAgentBridgeServerModule --> FAgentHttpServer : starts
    UAgentBridgeServiceSubsystem --> AgentBridgeService : registers via Tempo
    UAgentBridgeServiceSubsystem ..> ProtoMessages : converts to/from
    UAgentBridgeServiceSubsystem ..> ValueConverters : uses
    FAgentHttpServer ..> ValueConverters : uses
    AgentBridgeService ..> ProtoMessages : defined by
```

## Handler Pattern

All 51 gRPC handlers follow identical structure:

```cpp
void UAgentBridgeServiceSubsystem::HandleXxx(
    const XxxRequest& Request,
    const TResponseDelegate<XxxResponse>& ResponseContinuation)
{
    // Step 1: Proto -> Command struct
    FXxxCommand Cmd;
    Cmd.Field = UTF8_TO_TCHAR(Request.field().c_str());

    // Step 2: Execute via CommandExecutor
    FXxxResponse CmdResponse;
    FCommandExecutor::Execute(Cmd, CmdResponse);

    // Step 3: Response struct -> Proto
    XxxResponse Response;
    Response.set_success(CmdResponse.bSuccess);

    // Step 4: Return via callback
    ResponseContinuation.ExecuteIfBound(Response, grpc::Status::OK);
}
```

## Registration

All handlers are registered in a single `RegisterScriptingServices()` call using Tempo's
`SimpleRequestHandler` macro. **Forgetting to register** is a common bug - the code
compiles fine but the RPC returns "unimplemented" at runtime.

## Header Conflict Warning

AgentBridgeServer **cannot include** certain UE headers due to Windows SDK conflicts
with gRPC (`TempoScriptingServer.h` includes `<grpcpp/grpcpp.h>` before `CoreMinimal.h`):

- `AssetRegistry/AssetRegistryModule.h`
- `Editor.h`, `LevelEditor.h`
- `IImageWrapper.h`
- Any header transitively including `IoBuffer.h`

This is why all business logic lives in Scripting's `CommandExecutor.cpp` instead.

## Initialization Sequence

1. `FAgentBridgeServerModule::StartupModule()` starts HTTP server on port 8080
2. UWorld creation triggers `UAgentBridgeServiceSubsystem::Initialize()`
3. Initialize calls `ActivateService<AgentBridgeService>(this)` on Tempo
4. Tempo calls `RegisterScriptingServices()` which registers all 51 handlers
5. gRPC server listens on port 10001 (managed by Tempo)

## Tempo Proto Gotcha

Tempo's `TempoScripting::Rotation` proto uses SHORT field names:
- `.r` = roll, `.p` = pitch, `.y` = yaw (NOT `.roll`, `.pitch`, `.yaw`)

## Files

| File | Contents |
|------|----------|
| `Public/AgentBridgeServer.h` | `FAgentBridgeServerModule` |
| `Public/AgentBridgeServiceSubsystem.h` | `UAgentBridgeServiceSubsystem` (51 handler declarations) |
| `Public/AgentHttpServer.h` | `FAgentHttpServer` |
| `Public/AgentBridge.proto` | Service definition (888 lines, 51 RPCs) |
| `Public/ProtobufGenerated/` | Generated C++ from proto |
| `Private/AgentBridgeServiceSubsystem.cpp` | Handler implementations + value converters (2544 lines) |
