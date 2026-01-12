# AgentBridgeServer Module

> gRPC server (via Tempo) and HTTP fallback server.

## Purpose

This module exposes AgentBridge functionality over the network:
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

Protos are generated via Tempo's `GenProtos.sh`:

```bash
cd <PROJECT_ROOT>/Plugins/Tempo/TempoCore/Scripts
./GenProtos.sh
```

Generated files go to:
- `Public/ProtobufGenerated/AgentBridgeServer/` - Headers
- `Private/ProtobufGenerated/AgentBridgeServer/` - Sources

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

```csharp
// AgentBridgeServer.Build.cs (uses TempoModuleRules)
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

Changed from `Editor` to `Runtime` in Phase 4 to support PIE:

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
