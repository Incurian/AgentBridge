# AgentBridge Python Package

> MCP server, gRPC client, and HTTP client for AI agent integration.

## Purpose

This package provides the Python-side tools for AI agents to interact with Unreal:
- MCP server with 104 tools across 13 services (90 core + 14 bp_toolkit when submodule present)
- gRPC client for Tempo integration
- HTTP client as fallback

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
│       ├── __init__.py     # Service registry
│       ├── base.py         # Shared utilities
│       ├── agentbridge.py  # AgentBridge service (37 tools)
│       ├── tempo_time.py   # TimeService (6 tools)
│       ├── tempo_actor_control.py  # 17 tools
│       ├── tempo_core.py           # 6 tools
│       ├── tempo_core_editor.py    # 6 tools
│       ├── tempo_geographic.py     # 5 tools
│       ├── tempo_movement.py       # 5 tools
│       ├── tempo_world_state.py    # 2 tools
│       ├── tempo_labels.py         # 1 tool
│       ├── tempo_sensors.py        # 1 tool
│       ├── tempo_map_query.py      # 3 tools
│       ├── tempo_agents_editor.py  # 1 tool
│       └── bp_toolkit.py           # 14 tools (optional, requires submodule)
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
help()                    # Overview
help(topic="actors")      # Actor operations
help(topic="properties")  # Property access
help(topic="classes")     # Type discovery
help(topic="console")     # Console commands
help(topic="workflows")   # Common workflows
help(topic="components")  # Component operations
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

## Available Tools (90 core + 14 optional)

### AgentBridge Service (37 tools)
- `help`, `list_worlds`, `set_target_world`
- `query_actors`, `get_actor`, `spawn_actor`, `delete_actor`
- `set_actor_transform`, `get_property`, `set_property`
- `list_classes`, `get_class_schema`, `call_static_function`
- `is_world_partitioned`, `query_all_actors`, `get_streaming_state`
- `query_landscape`, `get_landscape_bounds`, `get_data_layers`
- `execute_console_command`, `search_console_commands`
- Asset/file/component operations...

### Tempo Services (53 tools)
- Time control: `tempo_play`, `tempo_pause`, `tempo_step`
- Actor properties: `tempo_set_float_property`, `tempo_set_color_property`, etc.
- Level control: `tempo_load_level`, `tempo_save_level`
- Geographic: `tempo_set_date`, `tempo_set_time_of_day`
- Movement: `tempo_command_vehicle`, `tempo_pawn_move_to`
- And more...

### bp_toolkit Service (14 tools, optional)

**Only available when bp_toolkit submodule is present.** These are LOCAL operations
(no Unreal connectivity required) for offline asset manipulation using UAssetGUI.

| Tool | Description |
|------|-------------|
| `bp_export_asset` | Export .uasset to JSON via UAssetGUI |
| `bp_import_asset` | Import JSON back to .uasset |
| `bp_detect_type` | Detect asset type (Blueprint, PCG, DataAsset, etc.) |
| `bp_get_info` | Get asset summary (exports, imports, graphs) |
| `bp_list_properties` | List all properties with types/values |
| `bp_get_property` | Get property by path (`BiomeDefinition.BiomePriority`) |
| `bp_set_property` | Set property by path |
| `bp_clone_asset` | Clone asset with new name |
| `bp_list_graphs` | List graphs in Blueprint/PCG |
| `bp_add_comment` | Add comment node to Blueprint graph |
| `bp_clone_node` | Clone existing Blueprint node |
| `bp_find` | Search asset namemap and exports |
| `bp_query` | Type-specific queries (list-events, list-tasks, textures) |
| `bp_parse` | Full Blueprint parsing with call graphs |

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

# Correct - use instance name (use tempo_get_components to find actual names)
get_property(actor="MyLight", path="LightComponent0.Intensity")
set_property(actor="MyLight", path="LightComponent0.Intensity", value="10000")

# RootComponent paths also work
set_property(actor="MyActor", path="RootComponent.RelativeLocation", value="(X=100,Y=200,Z=300)")
```

**Tip:** Partial name matching works - `LightComponent` will match `LightComponent0`.

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
