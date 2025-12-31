# AgentBridge Plugin

> UE 5.7 plugin exposing editor/runtime state to external AI agents via gRPC + MCP.
> Primary use case: "Build me a level" - agents need full read/write/discover capabilities.

## Project Phases

### Phase 1: Core Implementation (COMPLETE)
- [x] AgentBridgeCore - Reflection primitives (PropertyAccessor, FunctionInvoker, TypeDiscovery)
- [x] AgentBridgeRuntime - World context, actor ops, property paths
- [x] AgentBridgeScripting - Command layer, JSON serialization
- [x] AgentBridgeServer - HTTP server (temporary, will be replaced by gRPC)
- [x] Debug console commands (DumpActor, DumpClass, ListWorlds, QueryActors, etc.)
- [x] Design AgentBridge.proto service definition
- [x] Python client for testing

### Phase 2: Tempo Integration (REQUIRES EXPLICIT PERMISSION)
**DO NOT BEGIN WITHOUT USER APPROVAL**

**IMPORTANT:** Tempo currently only supports up to UE 5.6. Phase 2 requires either:
- Waiting for Tempo 5.7 support, or
- Using a UE 5.6 project for integration work

Phase 2 requires:
1. **Separate project** - New test project for integration work (protects main project)
2. **Engine modifications** - Install TempoModuleRules.cs to UBT
3. **Tempo plugin** - Copy TempoCore plugin (TempoScripting, gRPC, TempoCoreShared)

**Tempo integration will provide:**
- Pre-built gRPC libraries (Windows/Mac/Linux)
- FTempoScriptingServer - async gRPC server with completion queue
- Proto code generation via GenProtos.sh
- ITempoScriptable interface for service registration

### Phase 3: MCP Integration (Future)
- MCP server wrapping gRPC client
- Tool definitions for Claude/LLM agents

---

## Important Paths

### Engine
- **Engine root:** `D:\UE571`
- **Engine source:** `D:\UE571\Engine\Source\Runtime` (and `/Editor`, `/Developer`)
- **Engine logs:** `D:\UE571\Engine\Saved\Logs\Unreal.log`

### Project
- **Project root:** `E:\UnrealProjects\VR_Project`
- **Project logs:** `E:\UnrealProjects\VR_Project\Saved\Logs\VR_Project.log` (most recent)
- **Crash logs:** `E:\UnrealProjects\VR_Project\Saved\Crashes\`
- **UBT config:** `E:\UnrealProjects\VR_Project\Saved\UnrealBuildTool\BuildConfiguration.xml`

### Build
- **UBT log:** `D:\UE571\Engine\Programs\UnrealBuildTool\Log.txt` (compile errors, linker errors, build config)

### Tempo Reference (Read-Only)
- **Tempo plugin:** `D:\tempo\TempoSample\Plugins\Tempo`
- **TempoScripting:** `D:\tempo\TempoSample\Plugins\Tempo\TempoCore\Source\TempoScripting`
- **gRPC libraries:** `D:\tempo\TempoSample\Plugins\Tempo\TempoCore\Source\ThirdParty\gRPC`
- **Proto examples:** `D:\tempo\TempoSample\Plugins\Tempo\TempoWorld\Source\TempoWorld\Public\ActorControl.proto`

### When to Check Each Log
| Log | When to Check |
|-----|---------------|
| `VR_Project.log` | Runtime errors, PIE issues, Blueprint errors, plugin load failures |
| `Engine\Saved\Logs\Unreal.log` | Editor crashes, low-level engine issues |
| `Saved\Crashes\` | Hard crashes with minidumps |
| **UBT Log.txt** | **Compile errors, linker errors, missing includes** (persisted!) |

---

## Current Implementation Status

### AgentBridgeCore (COMPLETE)
| Component | Status | Notes |
|-----------|--------|-------|
| AgentBridgeTypes.h | Done | FAgentPropertyValue with TSharedPtr for self-referential containers |
| PropertyAccessor | Done | Read/write all FProperty types recursively |
| FunctionInvoker | Done | Dynamic UFunction invocation (note: struct return values have a known issue) |
| TypeDiscovery | Done | Class/struct/enum discovery, BP normalization |

### AgentBridgeRuntime (COMPLETE)
| Component | Status | Notes |
|-----------|--------|-------|
| WorldContextManager | Done | Multi-world support, PIE handling |
| ActorOperations | Done | Query, spawn, delete, modify actors |
| AgentPropertyPath | Done | Nested property resolution ("Mesh.Materials[0].Color") |
| DebugCommands | Done | Console commands for testing |

### AgentBridgeScripting (COMPLETE)
| Component | Status | Notes |
|-----------|--------|-------|
| AgentCommands.h | Done | Command/response structures for all operations |
| CommandExecutor | Done | JSON dispatch to Runtime layer, full serialization |

### AgentBridgeServer (COMPLETE for Phase 1)
| Component | Status | Notes |
|-----------|--------|-------|
| HTTP Server | Done | Uses UE HTTPServer module, port 8080 |
| gRPC Server | Pending | Will use TempoScripting in Phase 2 |

### Python Client (COMPLETE)
| Component | Status | Notes |
|-----------|--------|-------|
| agentbridge package | Done | Full API coverage |
| test_client.py | Done | All tests passing |

---

## Known Issues

### FunctionInvoker Return Values
Function calls via `CallFunction` command return default values (0 for structs, "" for strings) instead of actual return values. **Workaround:** Use `QueryActors` or property paths instead of function calls when possible. The Python client's `get_actor_location()` uses this workaround.

---

## Architecture

```
External Agents (Claude, LLMs)
         |
         v
MCP Server (Python) - Tools: spawn, modify, query
         |
         v
Python HTTP Client - agentbridge.AgentBridgeClient
         |
         v (HTTP/JSON over localhost:8080)
AgentBridgeServer (UE Module) - HTTP server (Phase 1) / gRPC (Phase 2)
         |
         v
AgentBridgeScripting (UE Module) - High-level commands, JSON serialization
         |
         v
AgentBridgeRuntime (UE Module) - World context, actor ops, property paths
         |
         v
AgentBridgeCore (UE Module) - FProperty access, UFunction invoke, type discovery
         |
         v
Unreal Engine 5.7 - Reflection System, World, Actors
```

---

## Module Structure

```
Plugins/AgentBridge/
├── AgentBridge.uplugin
├── CLAUDE.md                    # This file
├── AgentBridge_Handover.md      # Detailed implementation reference
├── Protos/
│   └── AgentBridge.proto        # gRPC service definition (Phase 2)
├── Python/
│   ├── agentbridge/             # Python client package
│   │   ├── __init__.py
│   │   ├── client.py
│   │   └── types.py
│   └── test_client.py           # Test script
└── Source/
    ├── AgentBridgeCore/         # Reflection primitives
    ├── AgentBridgeRuntime/      # Abstraction & helpers
    ├── AgentBridgeScripting/    # High-level operations
    └── AgentBridgeServer/       # HTTP/gRPC server
```

---

## Python Client Usage

```python
from agentbridge import AgentBridgeClient

client = AgentBridgeClient()  # localhost:8080

# Health check
if client.health_check():
    print("Server running!")

# List worlds
worlds = client.list_worlds()

# Query actors
actors = client.query_actors(name_pattern="Light", limit=10)

# Spawn actor
actor = client.spawn_actor("PointLight", location=(100, 200, 300), label="MyLight")

# Move actor
client.set_actor_transform("MyLight", location=(500, 500, 500))

# Delete actor
client.delete_actor("MyLight")
```

---

## Critical Technical Gotchas

### Blueprint vs C++ Reflection

**The `_C` suffix:** Blueprint classes have TWO objects:
- `BP_MyActor` - the `UBlueprint` asset (editor-only)
- `BP_MyActor_C` - the `UBlueprintGeneratedClass` (runtime class)

```cpp
// WRONG - references the asset
LoadObject<UClass>(nullptr, TEXT("/Game/BP_MyActor.BP_MyActor"));
// CORRECT - references the generated class
UClass* Class = LoadClass<AActor>(nullptr, TEXT("/Game/BP_MyActor.BP_MyActor_C"));
```

### HTTP Body Parsing
HTTP request bodies may not be null-terminated. Use explicit length conversion:
```cpp
FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()), Request.Body.Num());
FString BodyString(Converter.Length(), Converter.Get());
```

### UObject Pointer Types

| Type | Reflection Class | GC Behavior |
|------|------------------|-------------|
| `UObject*` / `TObjectPtr<>` | `FObjectProperty` | Prevents GC |
| `TSoftObjectPtr<>` | `FSoftObjectProperty` | Path-based, no GC prevention |
| `TWeakObjectPtr<>` | `FWeakObjectProperty` | Auto-nulls when target GC'd |
| `TSubclassOf<>` | `FClassProperty` | Prevents GC of UClass |

---

## Build Commands

```bash
# Compile plugin (from bash/terminal)
"D:/UE571/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" \
  VR_ProjectEditor Win64 Development \
  -Project="E:/UnrealProjects/VR_Project/VR_Project.uproject" -WaitMutex

# Or with editor running, use Live Coding: Ctrl+Alt+F11
```

## Testing

```bash
# Run Python test suite
cd Plugins/AgentBridge/Python
python test_client.py
```

---

## Tempo Integration Notes (Phase 2 Reference)

**Tempo only supports UE 5.6 currently.**

### Required Tempo Modules
- `gRPC` - ThirdParty module with pre-built libraries
- `TempoScripting` - gRPC server infrastructure
- `TempoCoreShared` - Settings and utilities

### Engine Modification Required
Copy `TempoModuleRules.cs` to:
```
<Engine>/Source/Programs/UnrealBuildTool/Configuration/TempoModuleRules.cs
```

### Proto Generation
Tempo uses `GenProtos.sh` which:
1. Runs `protoc` with `grpc_cpp_plugin` and `grpc_python_plugin`
2. Generates code to `Public/ProtobufGenerated/` and `Private/ProtobufGenerated/`
3. Auto-generates Python client stubs

---

*Document Version: 3.0*
*Last Updated: December 2024*
*Phase 1 Complete*
