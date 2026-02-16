# AgentBridgeServer Plugin

> Standalone UE plugin providing gRPC server (via Tempo) and HTTP fallback server.
> This plugin sits at the TOP of the AgentBridge dependency chain. It contains ONLY
> thin handlers that convert between protobuf messages and command structs. All business
> logic lives in AgentBridgeScripting.

## Plugin Structure

```
AgentBridgeServer/
|-- AgentBridgeServer.uplugin    (depends on Core, Runtime, Scripting, TempoCore)
|-- CLAUDE.md
|-- README.md
|-- Source/AgentBridgeServer/
    |-- AgentBridgeServer.Build.cs   (extends TempoModuleRules, not ModuleRules)
    |-- Public/
    |   |-- AgentBridge.proto          gRPC service definition (888 lines, 105 messages, 51 RPCs)
    |   |-- AgentBridgeServer.h        Module interface (FAgentBridgeServerModule)
    |   |-- AgentBridgeServiceSubsystem.h   gRPC handler declarations
    |   |-- AgentHttpServer.h          HTTP server interface
    |   |-- ProtobufGenerated/         Auto-generated, do not edit
    |       |-- AgentBridgeServer/
    |           |-- AgentBridge.pb.h       Generated message classes
    |           |-- AgentBridge.grpc.pb.h  Generated service stubs
    |-- Private/
        |-- AgentBridgeServer.cpp          Module startup, starts HTTP server on port 8080
        |-- AgentBridgeServiceSubsystem.cpp  51 gRPC handlers + value conversion helpers
        |-- AgentHttpServer.cpp            HTTP/JSON server (4 endpoints)
        |-- ProtobufGenerated/             Auto-generated, do not edit
            |-- AgentBridgeServer/
                |-- AgentBridge.pb.cc      Generated message implementations
                |-- AgentBridge.grpc.pb.cc Generated service implementations
```

## Purpose

This plugin exposes AgentBridge functionality over the network:
- gRPC via TempoScripting infrastructure (port 10001)
- HTTP/JSON fallback (port 8080)

## Key Files

| File | Purpose |
|------|---------|
| `AgentBridge.proto` | gRPC service definition (51 RPCs, 105 messages) |
| `AgentBridgeServiceSubsystem.h/.cpp` | gRPC handlers, implements ITempoScriptable |
| `AgentHttpServer.h/.cpp` | HTTP fallback server |
| `AgentBridgeServer.h/.cpp` | Module interface, auto-starts HTTP on load |
| `ProtobufGenerated/` | Auto-generated proto code (do not edit) |

---

## CRITICAL: Header Restrictions

**This module CANNOT include certain UE headers** due to Windows SDK conflicts with gRPC.

### Problematic Headers (DO NOT INCLUDE)

- `AssetRegistry/AssetRegistryModule.h`
- `Editor.h`, `LevelEditor.h`
- `IImageWrapper.h`
- `ThumbnailRendering/ThumbnailManager.h`
- Any header transitively including `IoBuffer.h`

### Root Cause

`TempoScriptingServer.h` includes `<grpcpp/grpcpp.h>` BEFORE `CoreMinimal.h`, causing Windows
SDK macros (`InterlockedIncrement`) to conflict with `FPlatformAtomics`.

### Solution Pattern

Put functionality requiring those headers in **AgentBridgeScripting/CommandExecutor.cpp** instead.
The handler in this plugin should be a thin wrapper:

```cpp
// In AgentBridgeServiceSubsystem.cpp (thin handler only)
void UAgentBridgeServiceSubsystem::CreateAsset(
    const CreateAssetRequest& Request,
    const TResponseDelegate<CreateAssetResponse>& ResponseContinuation)
{
    // 1. Convert proto -> command struct
    FCreateAssetCommand Cmd;
    Cmd.AssetClass = UTF8_TO_TCHAR(Request.asset_class().c_str());
    Cmd.PackagePath = UTF8_TO_TCHAR(Request.package_path().c_str());
    Cmd.AssetName = UTF8_TO_TCHAR(Request.asset_name().c_str());

    // 2. Execute via CommandExecutor (all logic lives there)
    FCreateAssetResponse CmdResponse;
    FCommandExecutor::Execute(Cmd, CmdResponse);

    // 3. Convert response -> proto
    CreateAssetResponse Response;
    Response.set_success(CmdResponse.bSuccess);
    Response.set_asset_path(TCHAR_TO_UTF8(*CmdResponse.AssetPath));

    if (CmdResponse.bSuccess)
    {
        ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
    }
    else
    {
        ResponseContinuation.ExecuteIfBound(Response,
            grpc::Status(grpc::INTERNAL, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
    }
}
```

**Every handler follows this same three-step pattern:**
1. Convert proto request fields to command struct fields
2. Call `FCommandExecutor::Execute(Cmd, Response)`
3. Convert response struct fields to proto response fields

---

## gRPC Service Registration

The subsystem uses Tempo's scripting infrastructure:

```cpp
class UAgentBridgeServiceSubsystem : public UWorldSubsystem, public ITempoScriptable
{
    // ITempoScriptable interface
    void RegisterScriptingServices(FTempoScriptingServer& ScriptingServer) override;

    // 51 RPC handler methods...
};
```

### Initialization Flow

```cpp
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
```

### ShouldCreateSubsystem

The subsystem is created for Editor, PIE, and Game worlds:

```cpp
bool UAgentBridgeServiceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    if (UWorld* World = Cast<UWorld>(Outer))
    {
        return World->WorldType == EWorldType::Editor ||
               World->WorldType == EWorldType::PIE ||
               World->WorldType == EWorldType::Game;
    }
    return false;
}
```

### Registration Pattern

All 51 RPCs are registered in a single `RegisterService` call using `SimpleRequestHandler`:

```cpp
void UAgentBridgeServiceSubsystem::RegisterScriptingServices(FTempoScriptingServer& ScriptingServer)
{
    ScriptingServer.RegisterService<AgentBridgeService>(
        // World Operations
        SimpleRequestHandler(&AgentBridgeAsyncService::RequestListWorlds,
            &UAgentBridgeServiceSubsystem::ListWorlds),
        SimpleRequestHandler(&AgentBridgeAsyncService::RequestSetTargetWorld,
            &UAgentBridgeServiceSubsystem::SetTargetWorld),

        // Actor Discovery
        SimpleRequestHandler(&AgentBridgeAsyncService::RequestQueryActors,
            &UAgentBridgeServiceSubsystem::QueryActors),
        // ... 47 more handlers ...
    );
}
```

**WARNING: Forgetting to register a handler is a common mistake.** The code will compile fine,
but the RPC will return "unimplemented" at runtime. The `AsyncService` type alias is:

```cpp
using AgentBridgeAsyncService = AgentBridgeService::AsyncService;
```

### RPC Categories (51 total)

| Category | Count | RPCs |
|----------|-------|------|
| World Operations | 2 | ListWorlds, SetTargetWorld |
| Actor Discovery | 2 | QueryActors, GetActor |
| Actor Manipulation | 5 | SpawnActor, DeleteActor, DuplicateActor, SetActorTransform, SetActorProperties |
| Property Paths | 2 | GetPropertyPath, SetPropertyPath |
| Function Invocation | 2 | CallFunction, CallAssetFunction |
| Type Discovery | 3 | FindClass, GetClassSchema, ListClasses |
| World Partition | 7 | IsWorldPartitioned, QueryAllActors, GetStreamingState, QueryLandscape, GetLandscapeBounds, GetDataLayers, GetActorsInDataLayer |
| Console Commands | 2 | ExecuteConsoleCommand, SearchConsoleCommands |
| Asset Operations | 5 | CreateAsset, SaveAsset, SaveActorAsBlueprint, DuplicateAsset, GetAssetThumbnail |
| Component Operations | 6 | GetComponentTransform, SetComponentTransform, AttachComponent, AttachActor, DetachComponent, DetachActor |
| Unified Transform/Attachment | 4 | SetTransform, GetTransform, Attach, Detach |
| File Operations | 5 | ReadProjectFile, WriteProjectFile, ListProjectDirectory, CopyProjectFile, DeleteProjectFile |
| Blueprint Nodes | 6 | CreateBlueprintNode, ConnectBlueprintPins, DisconnectBlueprintPins, DeleteBlueprintNode, ListBlueprintNodes, ListBlueprintPins |

---

## Proto Generation

Protos are generated via Tempo's `GenProtos.sh`, which calls `gen_protos.py`. The script
recursively scans the project root for `.proto` files, so no path configuration is needed
when adding or moving proto files - it discovers them automatically during the build.

```bash
# Manual generation (rarely needed - happens automatically during build)
cd <PROJECT_ROOT>/Plugins/Tempo/TempoCore/Scripts
./GenProtos.sh
```

### Generated File Locations

| Type | Location (relative to plugin root) |
|------|-----------------------------------|
| Headers (.pb.h, .grpc.pb.h) | `Source/AgentBridgeServer/Public/ProtobufGenerated/AgentBridgeServer/` |
| Sources (.pb.cc, .grpc.pb.cc) | `Source/AgentBridgeServer/Private/ProtobufGenerated/AgentBridgeServer/` |

### Build Pipeline

Proto generation runs automatically as part of the build pipeline:

1. `Build.sh` calls `PreBuild.bat`
2. `PreBuild.bat` calls Tempo's `GenProtos.sh`
3. `GenProtos.sh` recursively discovers `.proto` files
4. `protoc` compiles each proto into C++ headers and sources
5. Generated files use `REPLACE_IF_STALE` - only updated when content differs

### Proto File Line Ending Warning

**Proto files MUST use LF line endings (not CRLF).** The protoc compiler silently fails on
CRLF files, and `GenProtos.sh` suppresses errors with `|| true`, so the failure is invisible.
Old generated headers persist unchanged, making it look like nothing happened.

After editing proto files (especially on Windows or WSL with the Edit tool):
1. Check with `file AgentBridge.proto` - should say "ASCII text", not "CRLF line terminators"
2. Fix with `sed -i 's/\r$//' AgentBridge.proto` if needed

### Proto Imports

The `AgentBridge.proto` file imports two Tempo proto files:

```protobuf
import "TempoScripting/Empty.proto";    // Empty message for void returns
import "TempoScripting/Geometry.proto";  // Vector and Rotation types
```

**Tempo proto field name gotcha:** `TempoScripting::Rotation` uses SHORT field names:
- `.r` = roll, `.p` = pitch, `.y` = yaw (NOT `.roll`, `.pitch`, `.yaw`)

---

## Value Conversions

The subsystem includes bidirectional conversion helpers in an anonymous namespace.

### Type Conversion Helpers

```cpp
// UE <-> Proto geometry conversions
void SetProtoVector(TempoScripting::Vector* Proto, const FVector& V);
void SetProtoRotation(TempoScripting::Rotation* Proto, const FRotator& R);
void SetProtoScale(Scale* Proto, const FVector& S);
void SetProtoTransform(ActorTransform* Proto, const FTransform& T);

FVector FromProtoVector(const TempoScripting::Vector& V);
FRotator FromProtoRotation(const TempoScripting::Rotation& R);
FVector FromProtoScale(const Scale& S);
FTransform FromProtoTransform(const ActorTransform& T);

// Actor/Component descriptor filling
void FillActorDescriptor(ActorDescriptor* Desc, const FActorInfo& Info);
void FillComponentDescriptor(ComponentDescriptor* Desc, const FString& Name, const FString& ClassName);
```

### PropertyValue Conversions

The most complex conversion logic handles the bidirectional mapping between JSON strings
(from CommandExecutor) and proto PropertyValue messages:

```cpp
// JSON string -> proto PropertyValue
// Uses TypeName hint + JSON structure detection
void JsonToProtoPropertyValue(const FString& JsonStr, const FString& TypeName, PropertyValue* OutValue);

// Proto PropertyValue -> JSON string
// Recursive for nested structs/arrays
FString ProtoPropertyValueToJson(const PropertyValue& Value);
```

**JsonToProtoPropertyValue** type detection priority:
1. TypeName contains "Bool" OR value is "true"/"false" -> PROPERTY_TYPE_BOOL
2. TypeName contains "Int"/"Byte" -> PROPERTY_TYPE_INT
3. TypeName contains "Float"/"Double" -> PROPERTY_TYPE_FLOAT
4. TypeName contains "Vector" + JSON object -> PROPERTY_TYPE_VECTOR
5. TypeName contains "Rotator" + JSON object -> PROPERTY_TYPE_ROTATOR
6. TypeName contains "Transform" + JSON object -> PROPERTY_TYPE_TRANSFORM
7. TypeName contains "Color" -> PROPERTY_TYPE_COLOR (handles both JSON and UE native format)
8. Starts with `[` -> PROPERTY_TYPE_ARRAY (recursive)
9. Starts with `{` -> PROPERTY_TYPE_STRUCT (recursive key-value pairs)
10. Quoted string -> PROPERTY_TYPE_STRING
11. Numeric -> PROPERTY_TYPE_INT or PROPERTY_TYPE_FLOAT (checks for decimal point)
12. Default -> PROPERTY_TYPE_STRING

---

## HTTP Server

Fallback for testing without gRPC. Implemented as a singleton (`FAgentHttpServer::Get()`).

### Lifecycle

The HTTP server auto-starts in `FAgentBridgeServerModule::StartupModule()` on port 8080:

```cpp
void FAgentBridgeServerModule::StartupModule()
{
    StartServer(8080);  // Auto-starts HTTP fallback
    // gRPC service is handled by UAgentBridgeServiceSubsystem (via Tempo)
}
```

### Endpoints

| Method | Path | Handler |
|--------|------|---------|
| POST | `/agentbridge/execute` | `HandleExecute` - passes JSON to `FCommandExecutor::ExecuteJson()` |
| POST | `/agentbridge/batch` | `HandleBatch` - passes JSON array to `FCommandExecutor::ExecuteBatchJson()` |
| GET | `/agentbridge/health` | `HandleHealth` - returns `{"status":"ok","version":"1.0.0"}` |
| GET | `/agentbridge/schema` | `HandleSchema` - returns API schema/command list |

### Implementation Details

- Uses UE's `FHttpServerModule` and `IHttpRouter`
- Binds to localhost only (no authentication)
- HTTP requests arrive on worker threads
- JSON commands are passed directly to `FCommandExecutor::ExecuteJson()` which handles
  game thread dispatch internally

---

## Dependencies

**Plugin-level** (in `AgentBridgeServer.uplugin`):
- AgentBridgeCore, AgentBridgeRuntime, AgentBridgeScripting, TempoCore

**Module-level** (in `AgentBridgeServer.Build.cs`, extends TempoModuleRules):

```csharp
public class AgentBridgeServer : TempoModuleRules
{
    PublicDependencyModuleNames.AddRange(new string[] {
        "Core", "CoreUObject", "Engine",
        "HTTPServer", "Json",
        "AgentBridgeCore", "AgentBridgeRuntime", "AgentBridgeScripting",
        "TempoScripting",
    });

    PrivateDependencyModuleNames.AddRange(new string[] {
        "TempoCoreShared",
    });

    // Editor-only dependency (conditional)
    if (Target.bBuildEditor)
    {
        PrivateDependencyModuleNames.Add("UnrealEd");
    }
}
```

**Note:** The Build.cs extends `TempoModuleRules`, which adds gRPC/protobuf dependencies
automatically. This is NOT standard `ModuleRules`.

## Module Type

- **Type:** Runtime (supports Editor, PIE, and Game worlds)
- **Loading Phase:** Default
- **Enabled by Default:** true

---

## Adding New RPCs Checklist

1. Add proto message + RPC to `AgentBridge.proto`
2. Regenerate proto files (`GenProtos.sh` or just build)
3. Add forward declarations in `AgentBridgeServiceSubsystem.h`
4. Add handler method declaration in `AgentBridgeServiceSubsystem.h`
5. Implement thin handler in `AgentBridgeServiceSubsystem.cpp`
6. **Register in `RegisterScriptingServices()`** - EASY TO FORGET!
7. Add command struct in `AgentBridgeScripting/AgentCommands.h`
8. Implement logic in `AgentBridgeScripting/CommandExecutor.cpp`
9. Add Python client method in `mcp/services/agentbridge.py`
10. Add MCP tool wrapper
11. Add to `MODULES` dict in `mcp/__init__.py`

---

## Includes in AgentBridgeServiceSubsystem.cpp

For reference, the current includes in the main implementation file:

```cpp
#include "AgentBridgeServiceSubsystem.h"
#include "CommandExecutor.h"
#include "AgentCommands.h"
#include "WorldContextManager.h"
#include "ActorOperations.h"
#include "WorldPartitionOps.h"
#include "TempoScriptingServer.h"
#include "HAL/IConsoleManager.h"

// JSON includes for property value conversion
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Math/Color.h"

// gRPC includes
#include <grpcpp/grpcpp.h>

// Generated proto headers
#include "AgentBridgeServer/AgentBridge.pb.h"
#include "AgentBridgeServer/AgentBridge.grpc.pb.h"
#include "TempoScripting/Empty.pb.h"
```

Note: `WorldContextManager.h`, `ActorOperations.h`, and `WorldPartitionOps.h` are from
AgentBridgeRuntime. `CommandExecutor.h` and `AgentCommands.h` are from AgentBridgeScripting.
These are safe to include because they do not pull in conflicting headers.
