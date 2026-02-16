# AgentBridgeServer Plugin

> Standalone UE plugin providing gRPC server (via Tempo) and HTTP fallback server.

**Note:** As of PR #3 (plugin split), this is a standalone Unreal Engine plugin with its own
`.uplugin` file, not just a module within a monolithic AgentBridge plugin. It sits at the top
of the AgentBridge dependency chain, depending on AgentBridgeCore, AgentBridgeRuntime,
AgentBridgeScripting, and TempoCore.

## Plugin Structure

```
AgentBridgeServer/
+-- AgentBridgeServer.uplugin    (depends on Core, Runtime, Scripting, TempoCore)
+-- CLAUDE.md
+-- README.md
+-- Source/AgentBridgeServer/
    +-- AgentBridgeServer.Build.cs
    +-- Public/
    |   +-- AgentBridge.proto          (gRPC service definition)
    |   +-- AgentBridgeServiceSubsystem.h
    |   +-- AgentHttpServer.h
    |   +-- ProtobufGenerated/         (auto-generated, do not edit)
    +-- Private/
        +-- AgentBridgeServiceSubsystem.cpp
        +-- AgentHttpServer.cpp
        +-- ProtobufGenerated/         (auto-generated, do not edit)
```

## Purpose

This plugin exposes AgentBridge functionality over the network:
- gRPC via TempoScripting infrastructure (port 10001)
- HTTP/JSON fallback (port 8080)

## Key Files

| File | Purpose |
|------|---------|
| `AgentBridge.proto` | gRPC service definition (38 RPCs) |
| `AgentBridgeServiceSubsystem.h/.cpp` | gRPC handlers, implements ITempoScriptable |
| `AgentHttpServer.h/.cpp` | HTTP fallback server |
| `ProtobufGenerated/` | Auto-generated proto code |

## gRPC Service

The service is registered via Tempo's scripting infrastructure:

```cpp
class UAgentBridgeServiceSubsystem : public UWorldSubsystem, public ITempoScriptable
{
    // Implements 38 RPC handlers
    void ListWorlds(const ListWorldsRequest&, const TResponseDelegate<ListWorldsResponse>&);
    void QueryActors(const QueryActorsRequest&, const TResponseDelegate<QueryActorsResponse>&);
    // ... etc
};
```

## CRITICAL: Header Restrictions

**This module CANNOT include certain UE headers** due to Windows SDK conflicts with gRPC.

### Problematic Headers (DO NOT INCLUDE)

- `AssetRegistry/AssetRegistryModule.h`
- `Editor.h`, `LevelEditor.h`
- `IImageWrapper.h`
- `ThumbnailRendering/ThumbnailManager.h`
- Any header transitively including `IoBuffer.h`

### Root Cause

`TempoScriptingServer.h` includes `<grpcpp/grpcpp.h>` BEFORE `CoreMinimal.h`, causing Windows SDK macros (`InterlockedIncrement`) to conflict with `FPlatformAtomics`.

### Solution Pattern

Put functionality requiring those headers in **AgentBridgeScripting/CommandExecutor.cpp** instead:

```cpp
// In AgentBridgeServiceSubsystem.cpp (thin handler only)
void UAgentBridgeServiceSubsystem::CreateAsset(...)
{
    FCreateAssetCommand Cmd;
    // Convert proto to command...

    FCreateAssetResponse Response;
    FCommandExecutor::Get().Execute(Cmd, Response);  // All logic in CommandExecutor

    // Convert response to proto...
}
```

## Proto Generation

Protos are generated via Tempo's `GenProtos.sh`, which calls `gen_protos.py`. The script
recursively scans the project root for `.proto` files, so no path configuration is needed
when adding or moving proto files -- it discovers them automatically during the build.

```bash
# Manual generation (rarely needed - happens automatically during build)
cd <PROJECT_ROOT>/Plugins/Tempo/TempoCore/Scripts
./GenProtos.sh
```

Generated files go to (paths relative to plugin root):
- `AgentBridgeServer/Source/AgentBridgeServer/Public/ProtobufGenerated/AgentBridgeServer/` - Headers (.grpc.pb.h, .pb.h)
- `AgentBridgeServer/Source/AgentBridgeServer/Private/ProtobufGenerated/AgentBridgeServer/` - Sources (.grpc.pb.cc, .pb.cc)

Proto generation runs automatically as part of the build pipeline via Tempo's `PreBuild.bat`.
The generated files use `REPLACE_IF_STALE` -- they are only updated when content differs.

## Value Conversions

Bidirectional JSON <-> Protobuf conversion:

```cpp
// In AgentBridgeServiceSubsystem.cpp
AgentBridge::PropertyValue JsonToProtoPropertyValue(const FString& JsonString);
FString ProtoPropertyValueToJson(const AgentBridge::PropertyValue& ProtoValue);
```

## HTTP Server

Fallback for testing without gRPC:

```cpp
// Registers on port 8080
FAgentHttpServer::Start(8080);

// Endpoints:
// GET /health
// POST /command - JSON body with command type and params
```

## Dependencies

**Plugin-level** (in `AgentBridgeServer.uplugin`):
- AgentBridgeCore, AgentBridgeRuntime, AgentBridgeScripting, TempoCore

**Module-level** (in `AgentBridgeServer.Build.cs`, uses TempoModuleRules):

```csharp
public class AgentBridgeServer : TempoModuleRules
{
    PublicDependencyModuleNames.AddRange(new string[] {
        "Core",
        "CoreUObject",
        "Engine",
        "AgentBridgeCore",
        "AgentBridgeRuntime",
        "AgentBridgeScripting",
        "TempoScripting",
        "gRPC",
        "Json",
        "HTTP",
    });
}
```

## Module Type

Module type is `Runtime` (changed from `Editor` in earlier development to support PIE):

```csharp
Type = ModuleType.Runtime;  // NOT Editor!
```

## Todos

- [x] gRPC integration via TempoScripting
- [x] HTTP fallback server
- [x] 38 RPCs implemented
- [ ] Potential: Standalone gRPC server (without Tempo dependency) for plugin release

## Stretch Goals

| Feature | Effort | Notes |
|---------|--------|-------|
| Standalone gRPC | High | Remove Tempo dependency for standalone plugin |
| WebSocket transport | Medium | Alternative to gRPC for browser clients |
