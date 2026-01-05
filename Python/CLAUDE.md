# AgentBridge Python Package

> MCP server, gRPC client, and HTTP client for AI agent integration.

## Purpose

This package provides the Python-side tools for AI agents to interact with Unreal:
- MCP server with ~100 tools across 7 modules (organized into profiles)
- Modular loading: load only the tools you need, or use a profile
- gRPC client for Tempo integration
- HTTP client as fallback

---

## IMPORTANT: Adding New MCP Tools

When adding new gRPC-based MCP tools, there are multiple places that must be updated.
**Missing any step will cause tools to hang or fail silently!**

### Full Checklist (8 Steps)

| Step | File | What to do |
|------|------|------------|
| 1 | `AgentBridge.proto` | Add proto message + RPC definition |
| 2 | Tempo scripts | Run `GenProtos.sh` to regenerate proto files |
| 3 | `AgentBridgeServiceSubsystem.h` | Add handler method declaration |
| 4 | `AgentBridgeServiceSubsystem.cpp` | Implement handler method |
| 5 | `AgentBridgeServiceSubsystem.cpp` | **Register in `RegisterScriptingServices()`** ⚠️ |
| 6 | `agentbridge.py` | Add client method and MCP tool definition |
| 7 | `services/__init__.py` | Add tool to `MODULES` dict if modular |
| 8 | Rebuild C++ | Kill editor → Build → Restart |

### Common Mistake: Missing Registration

**Bug found 2026-01-03:** Phase 2 unified tools (`set_transform`, `get_transform`, `attach`,
`detach`) had C++ handlers implemented but were never registered in `RegisterScriptingServices()`.
Result: gRPC calls hung forever waiting for a response.

**Always remember:** Just having the handler method isn't enough - Tempo requires explicit
registration via `SimpleRequestHandler()` for each RPC.

---

## CRITICAL: Use TempoEnv Python

**Must use the TempoEnv Python**, not system Python:

```bash
# Correct
D:/tempo/TempoSample/TempoEnv/Scripts/python.exe

# Wrong - will fail with grpcio/protobuf errors
python
```

TempoEnv contains:
- Python 3.11
- grpcio 1.62.2
- protobuf 4.25.3

## Directory Structure

```
Python/
├── mcp/                    # MCP server package
│   ├── __init__.py
│   ├── server.py           # MCP server entry point
│   ├── client.py           # Legacy gRPC client
│   └── services/           # Modular service modules
│       ├── __init__.py     # Service registry, MODULES dict, profiles
│       ├── base.py         # Shared utilities
│       ├── agentbridge.py  # AgentBridge service (~57 tools)
│       ├── tempo_time.py   # TimeService (6 tools)
│       ├── tempo_core.py           # Core simulation (4 tools)
│       ├── tempo_core_editor.py    # Editor PIE/level (7 tools)
│       ├── tempo_geographic.py     # Time/geographic (5 tools)
│       ├── tempo_movement.py       # AI/vehicle control (6 tools)
│       ├── tempo_world_state.py    # Actor state (2 tools)
│       ├── tempo_labels.py         # Segmentation labels (1 tool)
│       ├── tempo_sensors.py        # Cameras (1 tool)
│       ├── tempo_map_query.py      # Lanes/zones (3 tools)
│       ├── tempo_agents_editor.py  # Zone graph (1 tool)
│       └── bp_toolkit.py           # 26 tools (6 BP + 6 PCG + 14 offline)
├── agentbridge/            # HTTP client package
│   ├── __init__.py
│   └── client.py
├── scripts/
│   └── generate_mcp_service.py  # Proto-to-MCP generator
├── test_client.py          # HTTP tests
├── test_grpc.py            # gRPC tests
└── mcp_config.json         # Claude Code config example
```

## MCP Server Configuration

Add to Claude Code settings (`~/.claude/settings.json`):

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

## Help System

The MCP server has a self-documenting help system:

```python
help()                        # Overview
help(topic="actors")          # Actor operations
help(topic="properties")      # Property access
help(topic="classes")         # Type discovery
help(topic="assets")          # Asset/file operations
help(topic="components")      # Component transforms, attachment
help(topic="console")         # Console commands
help(topic="workflows")       # Common workflows (includes PCG biome)
help(topic="pcg_volume")      # PCG volume types and sizing
help(topic="volume_sizing")   # BoxComponent sizing details
help(topic="bp_toolkit")      # Offline asset manipulation (if available)
```

### Keeping Help In Sync

When adding/modifying MCP tools, update `_get_help_text()` in `agentbridge.py`.

## Adding New Service Modules

1. Create `mcp/services/my_service.py`:

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

2. Import in `services/__init__.py`:
```python
from . import my_service  # in _auto_register()
```

## Testing

```bash
cd D:/tempo/TempoSample/Plugins/AgentBridge/Python

# gRPC tests (requires editor running, port 10001)
D:/tempo/TempoSample/TempoEnv/Scripts/python.exe test_grpc.py

# HTTP tests (requires editor running, port 8080)
D:/tempo/TempoSample/TempoEnv/Scripts/python.exe test_client.py

# Quick PYTHONPATH setup
PYTHONPATH="D:/tempo/TempoSample/Plugins/Tempo/TempoCore/Content/Python/API/tempo" \
  D:/tempo/TempoSample/TempoEnv/Scripts/python.exe -c "from mcp.services import agentbridge; print('OK')"
```

## Available Tools (~100 tools across 7 modules)

### Module Organization

| Module | Tools | Description |
|--------|-------|-------------|
| `core` | 6 | help, list_worlds, quit, console commands |
| `classes` | ~20 | Actors, properties, transforms, assets |
| `editor` | 7 | PIE, simulate, level management |
| `world_partition` | 7 | Streaming actors, landscape bounds |
| `files` | 4 | Project file operations |
| `bp_toolkit` | 26 | Blueprint/PCG graph editing, offline tools |
| `tempo_sim` | 28 | Simulation, time, AI, sensors, maps |

### Core AgentBridge Tools
- `help`, `list_worlds`, `set_target_world`, `quit`
- `query_actors`, `get_actor`, `spawn_actor`, `delete_actor`, `duplicate_actor`
- `get_property`, `set_property` (works with actors AND DataAssets)
- `set_transform`, `get_transform`, `attach`, `detach` (unified for actors/components)
- `list_classes`, `get_class_schema`, `call_function`
- `execute_console_command`, `search_console_commands`
- Asset/file/component operations...

### Tempo Services (~30 tools)
- Time control: `tempo_play`, `tempo_pause`, `tempo_step`
- Simulation: `tempo_load_level`, `tempo_set_time_mode`
- Geographic: `tempo_set_date`, `tempo_set_time_of_day`
- Movement: `tempo_command_vehicle`, `tempo_pawn_move_to`
- And more...

### bp_toolkit Module (26 tools)

**Requires bp_toolkit submodule.** Contains both live editing and offline tools.

**Live Blueprint Graph Editing (6):**
`bp_create_node`, `bp_connect_pins`, `bp_disconnect_pins`, `bp_delete_node`, `bp_list_nodes`, `bp_list_pins`

**Live PCG Graph Editing (6):**
`pcg_add_node`, `pcg_connect`, `pcg_disconnect`, `pcg_delete_node`, `pcg_list_nodes`, `pcg_get_input_output_nodes`

**Offline Asset Manipulation (14 - no Unreal needed):**
`bp_export_asset`, `bp_import_asset`, `bp_detect_type`, `bp_get_info`, `bp_list_properties`,
`bp_get_property`, `bp_set_property`, `bp_clone_asset`, `bp_list_graphs`, `bp_add_comment`,
`bp_clone_node`, `bp_find`, `bp_query`, `bp_parse`

**Submodule setup:**
```bash
cd D:/tempo/TempoSample/Plugins/AgentBridge
git submodule update --init --recursive
cd bp_toolkit/vendor/UAssetGUI && dotnet build -c Release
```

## Component Names in Property Paths

Use INSTANCE names (like `LightComponent0`), not CLASS names. Both GET and SET work:

```python
# Wrong - class name won't work
get_property(actor="MyLight", path="PointLightComponent.Intensity")

# Correct - use instance name (use get_actor with include_components=True to find names)
get_property(actor="MyLight", path="LightComponent0.Intensity")
set_property(actor="MyLight", path="LightComponent0.Intensity", value="10000")

# RootComponent paths also work
set_property(actor="MyActor", path="RootComponent.RelativeLocation", value="(X=100,Y=200,Z=300)")
```

**Tip:** Partial name matching works - `LightComponent` will match `LightComponent0`.

## DataAsset Properties

Property access also works with DataAssets using asset paths as `actor_id`:

```python
# Read DataAsset property
get_property(actor_id="/Game/Biomes/ForestBiome.ForestBiome",
             path="BiomeDefinition.BiomeName")

# Write DataAsset property
set_property(actor_id="/Game/Biomes/ForestBiome.ForestBiome",
             path="BiomeDefinition.BiomePriority", value=10)
```

**Note:** Use the full asset path with double name format (`AssetName.AssetName`).

### Arrays with Object References

Arrays of structs containing object refs require a two-step process:

```python
# Step 1: Create array elements with simple properties only
set_property(actor_id="/Game/Biomes/TreeAssets.TreeAssets",
             path="BiomeAssets",
             value='[{"Enabled":true, "Weight":1.0}]')

# Step 2: Set object references individually
set_property(actor_id="/Game/Biomes/TreeAssets.TreeAssets",
             path="BiomeAssets[0].Mesh",
             value="/Game/Foliage/SM_Tree.SM_Tree")
```

### Reading Nested Structs

When reading nested struct properties, use the full property path:

```python
# Returns {} (empty) - parent struct serialization issue
get_property(path="DefaultDefinition")

# Returns "Forest" - individual nested fields work!
get_property(path="DefaultDefinition.BiomeName")
```

## Completed Items

- [x] `get_actor` returns properties/components when flags set
- [x] `get_class_schema` returns actual property/function data
- [x] `list_classes` supports `base_class_name="ActorComponent"` or `"Object"`
- [x] `call_static_function` for Blueprint library calls
- [x] Flexible value formats (hex colors, arrays, dicts)
- [x] `label_pattern` parameter for `query_actors`
- [x] Enhanced error messages with suggestions
- [x] PCG Biome workflow documentation in help
- [x] Component property paths GET/SET (`LightComponent0.Intensity`)
- [x] Nested object paths GET/SET (`RootComponent.RelativeLocation`)
- [x] Self-documenting help system (`help()` and `help(topic="...")`)
- [x] Asset operations (create, save, duplicate)
- [x] Component operations (attach, detach, transforms)
- [x] File operations (read, write, list, copy)
- [x] Blueprint class normalization (auto-add `_C` suffix)
- [x] bp_toolkit MCP integration (14 tools for offline asset manipulation)

## Todos

- [ ] Unified property setter (auto-detect type, route correctly)

## Stretch Goals

| Feature | Effort | Notes |
|---------|--------|-------|
| Sound capture | Medium | TempoAudio integration |
| Graph editing help | Low | Document manual graph editing workflows |
| Sequencer help | Low | Document Tempo sequencer tools |
