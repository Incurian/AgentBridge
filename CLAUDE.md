# AgentBridge Plugin

> UE 5.6 plugin exposing editor/runtime state to external AI agents via gRPC + MCP.
> Primary use case: "Build me a level" - agents need full read/write/discover capabilities.

---

## Quick Links

| Document | Purpose |
|----------|---------|
| [HANDOVER.md](Docs/HANDOVER.md) | **Start here** - Session context and next steps |
| [TestingStrategy.md](Docs/TestingStrategy.md) | Manual testing guide for all features |
| [StretchGoals.md](Docs/StretchGoals.md) | Future features and research notes |
| [TempoIntegration.md](Docs/TempoIntegration.md) | gRPC/Tempo setup reference |

---

## Project Phases

### Phase 1: Core Implementation (COMPLETE)
- [x] AgentBridgeCore - Reflection primitives (PropertyAccessor, FunctionInvoker, TypeDiscovery)
- [x] AgentBridgeRuntime - World context, actor ops, property paths
- [x] AgentBridgeScripting - Command layer, JSON serialization
- [x] AgentBridgeServer - HTTP server (temporary, will be replaced by gRPC)
- [x] Debug console commands (DumpActor, DumpClass, ListWorlds, QueryActors, etc.)
- [x] Design AgentBridge.proto service definition
- [x] Python client for testing
- [x] DataAsset support (list, inspect, query data tables)
- [x] Viewport/SceneCapture support (capture images, depth, normals)
- [x] Audio capture and analysis support
- [x] Material operations (list, inspect, create instances, set parameters)
- [x] PCG operations (list actors, regenerate, set parameters)

### Phase 2: Tempo Integration (COMPLETE)
**gRPC service integrated with TempoScripting infrastructure**

Completed:
- [x] AgentBridgeServer depends on TempoScripting (uses TempoModuleRules)
- [x] AgentBridge.proto - gRPC service definition (22 RPCs)
- [x] UAgentBridgeServiceSubsystem - implements ITempoScriptable
- [x] Auto-generated code via GenProtos.sh
- [x] Build passes successfully

**gRPC Service Port:** 10001 (Tempo default, configurable in TempoCoreSettings)

### Phase 3: MCP Integration (COMPLETE)
**MCP server exposing AgentBridge + Tempo to Claude and LLM agents**

Completed:
- [x] Modular service architecture (`Python/mcp/services/`)
- [x] Auto-discovery and registration of service modules
- [x] 12 services with 72 total MCP tools
- [x] Proto-to-MCP generator script
- [x] Claude Code configuration example

**Service Modules (12 services, 74 tools):**
| Service | Tools | Description |
|---------|-------|-------------|
| `agentbridge` | 21 | World/actor manipulation, World Partition, console commands, help system |
| `tempo_time` | 6 | Simulation time control (play/pause/step) |
| `tempo_actor_control` | 17 | Typed property setters and transforms |
| `tempo_core` | 6 | Level loading, control mode, quit |
| `tempo_core_editor` | 6 | PIE, simulate, save/open levels |
| `tempo_geographic` | 5 | Date/time, geographic coordinates |
| `tempo_movement` | 5 | Vehicle/pawn commands, navigation |
| `tempo_world_state` | 2 | Actor state queries (velocity, bounds) |
| `tempo_labels` | 1 | Semantic label mapping |
| `tempo_sensors` | 1 | Sensor/camera discovery |
| `tempo_map_query` | 3 | Lane and zone queries |
| `tempo_agents_editor` | 1 | Zone graph builder |

### Phase 4: PIE/Runtime Support (COMPLETE)
**Transparent handling of different world contexts**

Completed:
- [x] `FWorldContextCapabilities` struct for reporting context-specific features
- [x] `GetCapabilities` command for agents to query available features
- [x] Proper detection of Editor/PIE/Game contexts via `World->WorldType`
- [x] AgentBridgeServer module type changed from Editor to Runtime
- [x] Python client `ContextCapabilities` type
- [x] `AgentBridge.Capabilities` console command

**Context Capability Matrix:**
| Feature | Editor | PIE | Packaged |
|---------|--------|-----|----------|
| Property iteration | ✓ | ✓ | ✓ |
| Function invocation | ✓ | ✓ | ✓ |
| Spawn/Destroy actors | ✓ | ✓ | ✓ |
| SetActorLabel/Folder | ✓ | ✓ | ✗ |
| Transactions (Undo) | ✓ | ✗ | ✗ |
| Property metadata | ✓ | ✓ | ✗ |

**Critical:** `GIsEditor` remains TRUE during PIE! Use `World->WorldType` for accurate context detection.

### Phase 5: World Partition & Landscape Streaming (COMPLETE)
**Streaming-aware actor queries for large worlds**

Completed:
- [x] `FWorldPartitionOps` class with streaming-aware APIs
- [x] `FStreamingActorReference` with streaming state, bounds, data layers
- [x] Query actors in unloaded streaming cells via `ForEachActorDescInstance`
- [x] Landscape streaming proxy support
- [x] Data layer queries
- [x] Console commands for WP debugging (5 new commands)
- [x] gRPC integration (7 new RPCs)
- [x] MCP tools (7 new tools including `execute_console_command`)

**Key APIs:**
| Function | Description |
|----------|-------------|
| `QueryAllActors()` | Query actors including unloaded |
| `GetActorStreamingState()` | Check Loaded/Unloaded/Invalid |
| `QueryLandscapeProxies()` | List all landscape proxies |
| `LoadActor()` / `LoadRegion()` | Force-load actors (editor) |
| `DeleteActorWP()` | Delete with WP cleanup |
| `GetDataLayers()` | List data layers |

**Console Commands:**
- `AgentBridge.IsPartitioned` - Check if world uses WP
- `AgentBridge.QueryAllActors [Pattern] [Limit]` - Query including unloaded
- `AgentBridge.StreamingState <GUID>` - Get streaming state
- `AgentBridge.QueryLandscape` - List landscape proxies
- `AgentBridge.DataLayers` - List data layers

**MCP Tools (new):**
- `is_world_partitioned` - Check if world uses WP
- `query_all_actors` - Query including unloaded actors
- `get_streaming_state` - Get actor streaming state by GUID
- `query_landscape` - List landscape proxies
- `get_data_layers` - List data layers
- `get_actors_in_data_layer` - Get actors in a data layer
- `execute_console_command` - Run arbitrary console commands

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
| WorldContextManager | Done | Multi-world support, PIE handling, GetCapabilities() |
| ActorOperations | Done | Query, spawn, delete, modify actors |
| AgentPropertyPath | Done | Nested property resolution ("Mesh.Materials[0].Color") |
| DebugCommands | Done | Console commands for testing (Capabilities added) |

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
| agentbridge package | Done | Full HTTP API coverage |
| test_client.py | Done | HTTP tests (port 8080) |
| test_grpc.py | Done | gRPC tests via Tempo (port 10001) |

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
│   │   ├── server.py            # MCP server - auto-discovers services
│   │   ├── client.py            # Legacy gRPC client wrapper
│   │   ├── tools.py             # Legacy tool definitions
│   │   └── services/            # Modular service modules
│   │       ├── __init__.py      # Service registry + auto-registration
│   │       ├── base.py          # Shared utilities (create_channel, safe_call)
│   │       ├── agentbridge.py   # AgentBridge gRPC service (11 tools)
│   │       ├── tempo_time.py    # TimeService (6 tools)
│   │       ├── tempo_actor_control.py  # ActorControlService (17 tools)
│   │       ├── tempo_core.py    # TempoCoreService (6 tools)
│   │       ├── tempo_core_editor.py    # TempoCoreEditorService (6 tools)
│   │       ├── tempo_geographic.py     # GeographicService (5 tools)
│   │       ├── tempo_movement.py       # MovementControlService (5 tools)
│   │       ├── tempo_world_state.py    # WorldStateService (2 tools)
│   │       ├── tempo_labels.py         # LabelService (1 tool)
│   │       ├── tempo_sensors.py        # SensorService (1 tool)
│   │       ├── tempo_map_query.py      # MapQueryService (3 tools)
│   │       └── tempo_agents_editor.py  # TempoAgentsEditorService (1 tool)
│   ├── scripts/
│   │   └── generate_mcp_service.py  # Proto-to-MCP generator
│   ├── mcp_config.json          # Claude Code config example
│   ├── requirements.txt
│   ├── test_client.py           # HTTP test script
│   └── test_grpc.py             # gRPC test script (tests via Tempo)
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
      "command": "D:/tempo/TempoSample/TempoEnv/Scripts/python.exe",
      "args": ["-m", "mcp", "--host", "localhost", "--port", "10001"],
      "cwd": "D:/tempo/TempoSample/Plugins/AgentBridge/Python",
      "env": {
        "PYTHONPATH": "D:/tempo/TempoSample/Plugins/Tempo/TempoCore/Content/Python/API/tempo"
      }
    }
  }
}
```

**IMPORTANT:** You must use the TempoEnv Python (`TempoEnv/Scripts/python.exe`), not the system Python. TempoEnv contains the required grpcio and protobuf packages built for the correct Python version.

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

# Material operations
materials = client.list_materials(filter_pattern="Wood", limit=20)
mat_info = client.get_material_info("/Game/Materials/M_Wood")
instance = client.create_material_instance("/Game/Materials/M_Wood", "MyWoodInstance")
client.set_material_parameter("StaticMeshActor", "BaseColor", (1.0, 0.5, 0.2, 1.0), "Vector")
client.apply_material_to_actor("StaticMeshActor", "/Game/Materials/M_Wood")

# PCG operations
pcg_actors = client.list_pcg_actors(pattern="Forest")
result = client.regenerate_pcg("PCG_ForestGenerator")
client.set_pcg_parameter("PCG_ForestGenerator", "Density", "0.5")
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
# Run HTTP test suite (port 8080)
cd D:/tempo/TempoSample/Plugins/AgentBridge/Python
python test_client.py

# Run gRPC test suite (port 10001)
python test_grpc.py [--host HOST] [--port PORT]
```

### Test Scripts

| Script | Protocol | Port | Description |
|--------|----------|------|-------------|
| `test_client.py` | HTTP/JSON | 8080 | Tests HTTP server endpoints |
| `test_grpc.py` | gRPC/Protobuf | 10001 | Tests gRPC service via Tempo |

The gRPC test script (`test_grpc.py`) provides comprehensive coverage:
- World operations (ListWorlds)
- Actor discovery (QueryActors, GetActor)
- Actor manipulation (SpawnActor, SetActorTransform, DeleteActor)
- Property operations (GetPropertyPath, SetPropertyPath)
- Function invocation (CallFunction)
- Type discovery (ListClasses)

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

## Adding New MCP Service Modules

### Using the Generator Script

Generate a stub from a proto file:

```bash
cd D:/tempo/TempoSample/Plugins/AgentBridge/Python
python scripts/generate_mcp_service.py \
    "D:/tempo/TempoSample/Plugins/Tempo/TempoFoo/Source/TempoFoo/Public/Foo.proto" \
    --output mcp/services/ \
    --prefix tempo
```

This generates a stub that requires manual editing:
1. Add parameter schemas to TOOLS based on proto message fields
2. Implement request building in client methods
3. Add result parsing in execute handlers

### Manual Service Module Pattern

Each service module must:
1. Define `TOOLS` list with MCP tool schemas
2. Create a client class wrapping the gRPC stub
3. Implement `connect(host, port)` factory function
4. Implement `execute(client, tool_name, args)` dispatcher
5. Call `register_service(ServiceModule(...))` at module load

Example minimal module:

```python
from . import register_service, ServiceModule
from .base import create_channel, safe_call

TOOLS = [
    {
        "name": "my_tool",
        "description": "Does something",
        "inputSchema": {"type": "object", "properties": {}, "required": []},
    },
]

class MyClient:
    def __init__(self, host, port):
        self.channel = create_channel(host, port)
        self.stub = MyServiceStub(self.channel)

def connect(host, port): return MyClient(host, port)

def execute(client, tool_name, args):
    return json.dumps({"success": True})

register_service(ServiceModule(
    name="my_service",
    description="My service",
    tools=TOOLS,
    execute=execute,
    connect=connect,
))
```

Then add to `services/__init__.py`:
```python
from . import my_service  # in _auto_register()
```

---

*Document Version: 12.0*
*Last Updated: December 31, 2025*
*All 5 Phases Complete - 22 RPCs, 74 MCP Tools, Self-Documenting Help System*

---

## Known Issues & Gotchas

### Python Environment (TempoEnv)

The MCP server **must** use TempoEnv's Python, not the system Python:
- **TempoEnv location:** `D:/tempo/TempoSample/TempoEnv/`
- **Python executable:** `TempoEnv/Scripts/python.exe` (Python 3.11)
- **Required packages:** grpcio 1.62.2, protobuf 4.25.3 (pre-installed)

**Why this matters:**
1. System Python may not have grpcio/protobuf installed
2. Even if installed, version mismatches cause `cygrpc` import errors
3. Tempo's generated Python stubs expect specific protobuf versions

**Quick test:**
```bash
D:/tempo/TempoSample/TempoEnv/Scripts/python.exe -c "import grpc; print(grpc.__version__)"
# Should output: 1.62.2
```

---

## Known Claude Code Issues

### "File has been unexpectedly modified" Error (Windows)

The Edit tool may fail with this error even immediately after reading a file. This is a known bug on Windows where NTFS Last Access Time updates cause false positives in modification detection.

**Workarounds:**

```bash
# Use sed for simple replacements
sed -i 's/old_text/new_text/g' file.txt

# Use cat with heredoc for appending
cat >> file.txt << 'HEREDOC'
new content here
HEREDOC

# Use Python for complex edits
python -c "
content = open('file.txt').read()
content = content.replace('old', 'new')
open('file.txt', 'w').write(content)
"
```

**Permanent fix (requires admin + reboot):**
```powershell
fsutil behavior set DisableLastAccess 1
```

See: [GitHub Issue #7443](https://github.com/anthropics/claude-code/issues/7443)
