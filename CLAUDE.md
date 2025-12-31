# AgentBridge Plugin

> UE 5.6 plugin exposing editor/runtime state to external AI agents via gRPC + MCP.
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

### Phase 2: Tempo Integration (COMPLETE)
**gRPC service integrated with TempoScripting infrastructure**

Completed:
- [x] AgentBridgeServer depends on TempoScripting (uses TempoModuleRules)
- [x] AgentBridge.proto - gRPC service definition (14 RPCs)
- [x] UAgentBridgeServiceSubsystem - implements ITempoScriptable
- [x] Auto-generated code via GenProtos.sh
- [x] Build passes successfully

**gRPC Service Port:** Tempo default (typically 50051, configurable)

### Phase 3: MCP Integration (COMPLETE)
**MCP server exposing AgentBridge to Claude and LLM agents**

Completed:
- [x] MCP server package (`Python/mcp/`)
- [x] gRPC client wrapper with Pythonic API
- [x] 11 MCP tools covering all operations
- [x] Claude Code configuration example

**MCP Tools:**
| Tool | Description |
|------|-------------|
| `list_worlds` | List available world contexts |
| `set_target_world` | Switch between Editor/PIE worlds |
| `query_actors` | Search actors by class/name/tag |
| `get_actor` | Get actor details |
| `spawn_actor` | Create new actors |
| `delete_actor` | Remove actors |
| `set_actor_transform` | Move/rotate/scale actors |
| `get_property` | Read property values |
| `set_property` | Write property values |
| `list_classes` | Discover available classes |
| `get_class_schema` | Get class properties/functions |

---

## Important Paths

### Engine
- **Engine root:** `D:\EL_UE\UE_5.6`
- **Engine source:** `D:\EL_UE\UE_5.6\Engine\Source\Runtime` (and `/Editor`, `/Developer`)
- **Engine logs:** `D:\EL_UE\UE_5.6\Engine\Saved\Logs\` (if exists)

### Project
- **Project root:** `D:\tempo\TempoSample`
- **Project logs:** `D:\tempo\TempoSample\Saved\Logs\TempoSample.log` (most recent)
- **Crash logs:** `D:\tempo\TempoSample\Saved\Crashes\`
- **UBT config:** `D:\tempo\TempoSample\Saved\UnrealBuildTool\BuildConfiguration.xml`

### Build
- **UBT:** `D:\EL_UE\UE_5.6\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe`
- **Build script:** `D:\tempo\TempoSample\Scripts\Build.sh` (~1 minute build time)

### Tempo Reference
- **Tempo plugin:** `D:\tempo\TempoSample\Plugins\Tempo`
- **TempoScripting:** `D:\tempo\TempoSample\Plugins\Tempo\TempoCore\Source\TempoScripting`
- **gRPC libraries:** `D:\tempo\TempoSample\Plugins\Tempo\TempoCore\Source\ThirdParty\gRPC`
- **Proto examples:** `D:\tempo\TempoSample\Plugins\Tempo\TempoWorld\Source\TempoWorld\Public\ActorControl.proto`

### When to Check Each Log
| Log | When to Check |
|-----|---------------|
| `TempoSample.log` | Runtime errors, PIE issues, Blueprint errors, plugin load failures |
| `Engine\Saved\Logs\` | Editor crashes, low-level engine issues |
| `Saved\Crashes\` | Hard crashes with minidumps |

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

### AgentBridgeServer (COMPLETE)
| Component | Status | Notes |
|-----------|--------|-------|
| HTTP Server | Done | Uses UE HTTPServer module, port 8080 (fallback) |
| gRPC Server | Done | Via TempoScripting, UAgentBridgeServiceSubsystem |
| AgentBridge.proto | Done | 14 RPCs for all operations |

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
         +---------------------------+
         |                           |
         v                           v
   Python gRPC Client          Python HTTP Client
         |                           |
         v (gRPC/protobuf)           v (HTTP/JSON)
         |                           |
         +---------------------------+
                     |
                     v
AgentBridgeServer (UE Module)
├── UAgentBridgeServiceSubsystem (gRPC via TempoScripting)
└── FAgentHttpServer (HTTP fallback, port 8080)
                     |
                     v
AgentBridgeScripting (UE Module) - FCommandExecutor
                     |
                     v
AgentBridgeRuntime (UE Module) - World context, actor ops, property paths
                     |
                     v
AgentBridgeCore (UE Module) - FProperty access, UFunction invoke, type discovery
                     |
                     v
Unreal Engine 5.6 - Reflection System, World, Actors
```

---

## Module Structure

```
Plugins/AgentBridge/
├── AgentBridge.uplugin          # Plugin descriptor (depends on TempoCore)
├── CLAUDE.md                    # This file
├── Docs/
│   └── TempoIntegration.md      # Tempo/gRPC integration guide
├── Python/
│   ├── agentbridge/             # HTTP client package (Phase 1)
│   ├── mcp/                     # MCP server package (Phase 3)
│   │   ├── __init__.py
│   │   ├── client.py            # gRPC client wrapper
│   │   ├── tools.py             # MCP tool definitions
│   │   └── server.py            # MCP server entry point
│   ├── mcp_config.json          # Claude Code config example
│   ├── requirements.txt
│   └── test_client.py           # HTTP test script
└── Source/
    ├── AgentBridgeCore/         # Reflection primitives
    ├── AgentBridgeRuntime/      # World context, actor ops
    ├── AgentBridgeScripting/    # Command layer
    └── AgentBridgeServer/       # HTTP + gRPC server
        ├── Public/
        │   ├── AgentBridge.proto
        │   ├── AgentBridgeServiceSubsystem.h
        │   └── ProtobufGenerated/
        └── Private/
```

---

## MCP Server Usage (Claude Code)

### Setup

1. **Start Unreal Editor** with the TempoSample project
2. **Add to Claude Code settings** (`~/.claude/settings.json`):

```json
{
  "mcpServers": {
    "agentbridge": {
      "command": "python",
      "args": ["-m", "mcp"],
      "cwd": "D:/tempo/TempoSample/Plugins/AgentBridge/Python",
      "env": {
        "PYTHONPATH": "D:/tempo/TempoSample/Plugins/Tempo/TempoCore/Content/Python/API/tempo"
      }
    }
  }
}
```

3. **Restart Claude Code** - tools will be available

### Available Tools

Once configured, Claude can use commands like:
- "List all lights in the scene"
- "Spawn a PointLight at position 100, 200, 300"
- "Move the actor named 'MyLight' to 500, 500, 500"
- "What properties does a PointLight have?"

---

## Python HTTP Client Usage (Legacy)

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
# Build using Tempo's build script (~1 minute)
cd D:/tempo/TempoSample/Scripts
./Build.sh

# Or compile plugin directly (from bash/terminal)
"D:/EL_UE/UE_5.6/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" \
  TempoSampleEditor Win64 Development \
  -Project="D:/tempo/TempoSample/TempoSample.uproject" -WaitMutex

# Or with editor running, use Live Coding: Ctrl+Alt+F11
```

## Testing

```bash
# Run Python test suite
cd D:/tempo/TempoSample/Plugins/AgentBridge/Python
python test_client.py
```

---

## Tempo Integration Notes (Phase 2 Reference)

**Environment ready - UE 5.6 with Tempo mods installed.**

### Required Tempo Modules (Available)
- `gRPC` - ThirdParty module with pre-built libraries
- `TempoScripting` - gRPC server infrastructure
- `TempoCoreShared` - Settings and utilities

### Engine Modification (DONE)
TempoModuleRules.cs installed at:
```
D:\EL_UE\UE_5.6\TempoMods\
```

### Proto Generation
Tempo uses `GenProtos.sh` which:
1. Runs `protoc` with `grpc_cpp_plugin` and `grpc_python_plugin`
2. Generates code to `Public/ProtobufGenerated/` and `Private/ProtobufGenerated/`
3. Auto-generates Python client stubs

---

*Document Version: 6.0*
*Last Updated: December 2024*
*All Phases Complete - Ready for Production*
