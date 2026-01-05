# AgentBridge

> **Expose Unreal Engine to AI Agents via gRPC and MCP**

**Version:** 0.3.0
**Engine:** Unreal Engine 5.6
**License:** MIT
**Status:** Beta / Experimental

AgentBridge enables AI agents (Claude Code, LLMs, automation scripts) to interact with Unreal Engine in real-time. Query actors, modify properties, spawn objects, discover types, and build levels programmatically.

---

## Key Features

| Category | Capabilities |
|----------|-------------|
| **Actor Operations** | Query, spawn, delete, transform, attach/detach actors |
| **Property Access** | Read/write any property via paths (`Component.SubObject.Value`) |
| **Type Discovery** | List classes, get schemas, find Blueprints |
| **Asset Operations** | Create, save, duplicate assets; save actors as Blueprints |
| **Blueprint/PCG Graphs** | Create nodes, connect pins, edit graphs programmatically |
| **World Partition** | Query streaming actors, landscape bounds, data layers |
| **Console Commands** | Execute and search 5000+ console commands/CVars |
| **Tempo Integration** | Simulation control, time manipulation, vehicle/pawn commands |

**~100 MCP tools** across 7 modules (organized into profiles), including **26 bp_toolkit tools** for Blueprint/PCG editing and offline asset manipulation.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        External AI Agents                           │
│              (Claude Code, LLMs, Python scripts)                    │
└────────────────────────────────┬────────────────────────────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │    MCP Server (Python)   │
                    │   ~100 tools / 7 modules │
                    └────────────┬────────────┘
                                 │
        ┌────────────────────────┼────────────────────────┐
        │                        │                        │
        ▼                        ▼                        ▼
   gRPC (10001)           HTTP (8080)            bp_toolkit
   High performance       Fallback/testing       Offline JSON
                                                 manipulation
        │                        │
        └────────────┬───────────┘
                     │
    ┌────────────────▼────────────────┐
    │      AgentBridge Plugin         │
    ├─────────────────────────────────┤
    │ Server    │ gRPC/HTTP handlers  │
    │ Scripting │ Commands, dispatch  │
    │ Runtime   │ World, actors       │
    │ Core      │ Reflection, types   │
    └─────────────────────────────────┘
                     │
                     ▼
            Unreal Engine 5.6
```

### Modular Loading

Tools are organized into 7 modules with predefined profiles:

| Profile | Modules | Tools | Use Case |
|---------|---------|-------|----------|
| `core` | core | 6 | Minimal operations |
| `standard` | core, classes, editor, files | ~35 | Level editing (DEFAULT) |
| `editor` | + world_partition | ~42 | Full editor work |
| `scripting` | + bp_toolkit | ~61 | Blueprint/PCG editing |
| `simulation` | core, classes, tempo_sim | ~34 | Runtime/PIE testing |
| `full` | all modules | ~100 | Everything |

Use `load_modules(modules=["bp_toolkit"])` at runtime to load additional modules.

---

## Quick Start

### Prerequisites

- Unreal Engine 5.6
- [Tempo Plugin](https://github.com/tempo-sim/Tempo) (provides gRPC infrastructure)
- Python 3.11 with grpcio (included in TempoEnv)

### 1. Enable the Plugin

Add to your `.uproject`:
```json
{
  "Plugins": [
    { "Name": "AgentBridge", "Enabled": true },
    { "Name": "TempoCore", "Enabled": true }
  ]
}
```

### 2. Configure MCP Server

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

### 3. Start the Editor

```bash
# Using Tempo's run script
cd D:/tempo/TempoSample
./Plugins/Tempo/Scripts/Run.sh

# Wait ~30 seconds for gRPC server on port 10001
```

### 4. Use the Tools

In Claude Code:
```
> Query all lights in the scene
> Spawn a PointLight at position 100, 200, 300
> Set the light intensity to 5000
```

---

## Installation

### From Source

```bash
# Clone into your project's Plugins directory
cd YourProject/Plugins
git clone https://github.com/YOUR_ORG/AgentBridge.git

# Initialize optional submodules (bp_toolkit for asset manipulation)
cd AgentBridge
git submodule update --init --recursive

# Build UAssetGUI if using bp_toolkit
cd bp_toolkit/vendor/UAssetGUI && dotnet build -c Release
```

### Dependencies

| Dependency | Version | Purpose |
|------------|---------|---------|
| Tempo Plugin | Latest | gRPC infrastructure |
| Python | 3.11+ | MCP server |
| grpcio | 1.62.2+ | gRPC client |
| protobuf | 4.25.3+ | Message serialization |
| .NET 8 | (optional) | bp_toolkit / UAssetGUI |

---

## Usage Examples

### Python (Direct gRPC)

```python
from mcp.services.agentbridge import connect, execute

client = connect("localhost", 10001)

# Query actors
result = execute(client, "query_actors", {"class_name": "PointLight"})

# Spawn an actor
result = execute(client, "spawn_actor", {
    "class_name": "PointLight",
    "location": [100, 200, 300],
    "label": "MyLight"
})

# Set properties
execute(client, "set_property", {
    "actor_id": "MyLight",
    "path": "LightComponent.Intensity",
    "value": "5000"
})
```

### Property Paths

AgentBridge supports nested property paths with automatic component resolution:

```python
# Direct property
get_property(actor_id="MyActor", path="Health")

# Component property (uses INSTANCE names like LightComponent0)
get_property(actor_id="MyLight", path="LightComponent0.Intensity")

# Nested struct
get_property(actor_id="MyActor", path="RootComponent.RelativeLocation.X")

# Array index
get_property(actor_id="MyActor", path="Inventory[0].ItemName")
```

### Value Formats

Values are automatically parsed based on property type:

```python
# Colors - multiple formats supported
set_property(actor_id="Light", path="LightColor", value="(R=255,G=128,B=0,A=255)")
set_property(actor_id="Light", path="LightColor", value="#FF8000")
set_property(actor_id="Light", path="LightColor", value="[255, 128, 0, 255]")

# Vectors
set_property(actor_id="Actor", path="Location", value="(X=100,Y=200,Z=300)")
set_property(actor_id="Actor", path="Location", value="[100, 200, 300]")

# Booleans, numbers, strings
set_property(actor_id="Light", path="bVisible", value="true")
set_property(actor_id="Light", path="Intensity", value="5000")
```

### PCG Biome Workflow

Setting up procedural content generation with biomes involves spawning volumes, creating DataAssets, and linking them together:

```python
# 1. Get landscape bounds for sizing
bounds = get_landscape_bounds()
# Returns: {"center": [x,y,z], "half_extents": [x,y,z], "min": [...], "max": [...]}

# 2. Spawn biome actors at landscape center
spawn_actor(class_name="BP_PCGBiomeCore", location=bounds["center"], label="MyBiomeCore")
spawn_actor(class_name="BP_PCGBiomeVolume", location=bounds["center"], label="MyBiomeVolume")

# 3. Size the volume to cover landscape (IMPORTANT: verify component name first!)
get_actor(actor_id="MyBiomeVolume", include_components=True)  # Find component names

# BoxExtent is HALF-SIZE in Unreal units - add Z margin for spawn variation
set_property(actor_id="MyBiomeVolume", path="BiomeVolume.BoxExtent",
    value=f"(X={bounds['half_extents'][0]},Y={bounds['half_extents'][1]},Z={bounds['half_extents'][2] + 5000})")
set_property(actor_id="MyBiomeVolume", path="BiomeVolume.RelativeScale3D",
    value="(X=1,Y=1,Z=1)")

# 4. Create BiomeDefinition DataAsset
create_asset(asset_class="BiomeDefinitionTemplate", package_path="/Game/Biomes", asset_name="ForestBiome")

# Set DataAsset properties using asset path as actor_id
set_property(actor_id="/Game/Biomes/ForestBiome.ForestBiome",
             path="BiomeDefinition.BiomeName", value="Forest")
set_property(actor_id="/Game/Biomes/ForestBiome.ForestBiome",
             path="BiomeDefinition.BiomePriority", value=10)
save_asset(asset_path="/Game/Biomes/ForestBiome")

# 5. Create BiomeAsset for spawnable meshes (two-step for object refs)
create_asset(asset_class="BiomeAssetTemplate", package_path="/Game/Biomes", asset_name="TreeAssets")

# First create array elements with simple properties
set_property(actor_id="/Game/Biomes/TreeAssets.TreeAssets", path="BiomeAssets",
             value='[{"Enabled":true, "AssetType":"Mesh", "Weight":1.0}]')
# Then set object references individually
set_property(actor_id="/Game/Biomes/TreeAssets.TreeAssets", path="BiomeAssets[0].Mesh",
             value="/Game/Foliage/SM_Tree.SM_Tree")
save_asset(asset_path="/Game/Biomes/TreeAssets")

# 6. Link DataAssets to the volume
set_property(actor_id="MyBiomeVolume", path="Definition",
             value="/Game/Biomes/ForestBiome.ForestBiome")
set_property(actor_id="MyBiomeVolume", path="Assets",
             value='["/Game/Biomes/TreeAssets.TreeAssets"]')
```

**Key Points:**
- Use `BP_PCGBiomeVolume` (BoxComponent) not native `PCGVolume` (BrushComponent)
- Component names are INSTANCE names (e.g., `BiomeVolume`), not class names
- `BoxExtent` is HALF the actual volume size
- DataAsset properties use asset path format: `/Game/Path/AssetName.AssetName`
- Arrays with object references require two-step creation
- Add Z margin (5000+ units) for spawn height variation

See `help(topic="pcg_volume")` and `help(topic="workflows")` for more details.

---

## Tool Reference

### Help System

Call `help()` for an overview, or specify a topic:

| Topic | Description |
|-------|-------------|
| `help()` | Overview of all capabilities |
| `help(topic="actors")` | Actor queries, spawning, transforms |
| `help(topic="properties")` | Property paths, reading/writing values |
| `help(topic="classes")` | Type discovery, class schemas |
| `help(topic="assets")` | Asset creation, saving, file operations |
| `help(topic="components")` | Component transforms, attachment |
| `help(topic="console")` | Console commands and CVars |
| `help(topic="workflows")` | Common multi-step operations |
| `help(topic="pcg_volume")` | PCG volume types and sizing |
| `help(topic="volume_sizing")` | BoxComponent sizing details |
| `help(topic="bp_toolkit")` | Offline asset manipulation (if available) |

### Core AgentBridge Tools (~57 tools)

#### World Operations
| Tool | Description |
|------|-------------|
| `list_worlds` | List all world contexts (Editor, PIE, Game) |
| `set_target_world` | Switch target world for operations |

#### Actor Operations
| Tool | Description |
|------|-------------|
| `query_actors` | Find actors by class, name, label, or tag |
| `get_actor` | Get actor details, properties, components |
| `spawn_actor` | Create new actor with transform and properties |
| `delete_actor` | Remove actor from world |
| `set_actor_transform` | Move, rotate, or scale actor |

#### Property Operations
| Tool | Description |
|------|-------------|
| `get_property` | Read property via path (supports nested paths) |
| `set_property` | Write property via path |

#### Type Discovery
| Tool | Description |
|------|-------------|
| `list_classes` | Find actor/component/object classes |
| `get_class_schema` | Get all properties and functions of a class |
| `call_static_function` | Call Blueprint library functions |

#### World Partition
| Tool | Description |
|------|-------------|
| `is_world_partitioned` | Check if world uses streaming |
| `query_all_actors` | Query including unloaded streaming actors |
| `get_streaming_state` | Get actor's load state |
| `query_landscape` | List landscape proxies |
| `get_landscape_bounds` | Get full landscape extents |
| `get_data_layers` | List data layers |
| `get_actors_in_data_layer` | Query actors by data layer |

#### Asset Operations
| Tool | Description |
|------|-------------|
| `create_asset` | Create DataAsset, MaterialInstance, etc. |
| `save_asset` | Save asset to disk |
| `duplicate_asset` | Copy asset with new name |
| `save_actor_as_blueprint` | Convert actor to Blueprint |

#### File Operations
| Tool | Description |
|------|-------------|
| `read_project_file` | Read file from project directory |
| `write_project_file` | Write file to project directory |
| `list_project_directory` | List files with glob patterns |
| `copy_project_file` | Copy file within project |

#### Transform & Attachment (Unified - works on actors AND components)
| Tool | Description |
|------|-------------|
| `set_transform` | Set location/rotation/scale on actor or component |
| `get_transform` | Get transform of actor or component |
| `attach` | Attach actor/component to parent (supports sockets) |
| `detach` | Detach from parent (maintains world transform) |

**Syntax:** Use `Actor->Component` for components, e.g., `set_transform(target="MyLight->LightComponent0", location=[0,0,500])`

#### Console Commands
| Tool | Description |
|------|-------------|
| `execute_console_command` | Run any console command |
| `search_console_commands` | Search 5000+ commands/CVars |

#### Blueprint Graph Operations (6 tools)
| Tool | Description |
|------|-------------|
| `bp_create_node` | Create node (CallFunction, Event, Variable, Branch, Sequence, Comment) |
| `bp_connect_pins` | Connect two pins between Blueprint nodes |
| `bp_disconnect_pins` | Disconnect Blueprint pins |
| `bp_delete_node` | Delete node from Blueprint graph |
| `bp_list_nodes` | List all nodes with GUIDs, types, positions |
| `bp_list_pins` | List pins on a node with directions and types |

**Note:** Blueprints must have an initialized EventGraph (use editor-created or `duplicate_asset()`, not `create_asset()`).

#### PCG Graph Operations (6 tools)
| Tool | Description |
|------|-------------|
| `pcg_add_node` | Add node (SurfaceSampler, StaticMeshSpawner, FilterByTag, etc.) |
| `pcg_connect` | Connect two PCG nodes |
| `pcg_disconnect` | Disconnect PCG nodes |
| `pcg_delete_node` | Delete node from PCG graph |
| `pcg_list_nodes` | List all nodes with their pins |
| `pcg_get_input_output_nodes` | Get InputNode and OutputNode (graph entry/exit points) |

**Note:** Pin labels can be unintuitive - InputNode's output is `In`, OutputNode's input is `Out`. Use `pcg_list_nodes` to discover pin names.

### Tempo Services (~30 tools)

#### Simulation Control
| Tool | Description |
|------|-------------|
| `tempo_play` | Start/resume simulation |
| `tempo_pause` | Pause simulation |
| `tempo_step` | Advance one frame |
| `tempo_advance_steps` | Advance N frames |
| `tempo_set_time_mode` | WALL_CLOCK or FIXED_STEP |
| `tempo_set_sim_rate` | Set steps per second |

#### Additional Simulation Control
| Tool | Description |
|------|-------------|
| `tempo_set_control_mode` | Set mode: NONE, USER, OPEN_LOOP, CLOSED_LOOP |
| `tempo_load_level` | Load level with deferred/start_paused options |
| `tempo_finish_loading_level` | Complete deferred level load |
| `tempo_set_viewport_render` | Enable/disable viewport (for headless) |

#### Level Control (Editor - no tempo_ prefix)
| Tool | Description |
|------|-------------|
| `play_in_editor` | Start PIE session |
| `simulate` | Start Simulate mode |
| `stop` | Stop PIE/Simulate |
| `save_level` | Save current level |
| `open_level` | Open level in editor |
| `new_level` | Create empty level |
| `get_current_level` | Get loaded level name |

#### Geographic/Time
| Tool | Description |
|------|-------------|
| `tempo_set_date` | Set simulation date |
| `tempo_set_time_of_day` | Set time of day |
| `tempo_set_day_cycle_rate` | Set day/night cycle speed |
| `tempo_get_datetime` | Get current simulation datetime |
| `tempo_set_geographic_reference` | Set lat/lon/alt reference |

#### Movement & AI
| Tool | Description |
|------|-------------|
| `tempo_get_commandable_vehicles` | List controllable vehicles |
| `tempo_command_vehicle` | Send steering/acceleration |
| `tempo_get_commandable_pawns` | List controllable pawns |
| `tempo_pawn_move_to` | Navigate pawn to location |
| `tempo_rebuild_navigation` | Rebuild navmesh |
| `tempo_run_zone_graph_builder` | Build zone graph for AI navigation |

#### World State & Sensors
| Tool | Description |
|------|-------------|
| `tempo_get_actor_state` | Get transform, velocity, bounds |
| `tempo_get_actors_near` | Get actors within radius |
| `tempo_get_available_sensors` | List cameras and capabilities |
| `tempo_get_label_map` | Instance-to-semantic ID mapping |

#### Map Queries
| Tool | Description |
|------|-------------|
| `tempo_get_lanes` | Query lane data (center, width, connections) |
| `tempo_get_lane_accessibility` | Check traffic lights/signs between lanes |
| `tempo_get_zones` | Query zone boundaries |

### bp_toolkit Offline Tools (14 tools, optional)

**Only available when bp_toolkit submodule is present.** These work offline without Unreal running.

| Tool | Description |
|------|-------------|
| `bp_export_asset` | Export .uasset to JSON via UAssetGUI |
| `bp_import_asset` | Import JSON back to .uasset |
| `bp_detect_type` | Detect asset type (Blueprint, PCG, DataAsset) |
| `bp_get_info` | Get asset summary |
| `bp_list_properties` | List properties with types/values |
| `bp_get_property` | Get property by path |
| `bp_set_property` | Set property by path |
| `bp_clone_asset` | Clone asset with new name |
| `bp_list_graphs` | List graphs in Blueprint/PCG |
| `bp_add_comment` | Add comment node to graph |
| `bp_clone_node` | Clone existing node |
| `bp_find` | Search asset namemap/exports |
| `bp_query` | Type-specific queries |
| `bp_parse` | Full Blueprint parsing |

---

## Module Architecture

AgentBridge uses a layered 4-module architecture:

```
┌─────────────────────────────────────────────────────────────────┐
│                    AgentBridgeServer                            │
│  gRPC handlers (38 RPCs), HTTP fallback, proto definitions      │
│  Thin handlers only - business logic in Scripting               │
├─────────────────────────────────────────────────────────────────┤
│                   AgentBridgeScripting                          │
│  Command structs, CommandExecutor, JSON serialization           │
│  ALL business logic lives here (due to header conflicts)        │
├─────────────────────────────────────────────────────────────────┤
│                    AgentBridgeRuntime                           │
│  World context, actor operations, property paths, World Part.   │
├─────────────────────────────────────────────────────────────────┤
│                     AgentBridgeCore                             │
│  Reflection primitives, PropertyAccessor, FunctionInvoker       │
│  TypeDiscovery, Blueprint name normalization                    │
└─────────────────────────────────────────────────────────────────┘
```

### Module Details

| Module | Purpose | Key Classes |
|--------|---------|-------------|
| **Core** | Low-level reflection | `FPropertyAccessor`, `FFunctionInvoker`, `FTypeDiscovery` |
| **Runtime** | World operations | `FWorldContextManager`, `FActorOperations`, `FAgentPropertyPath` |
| **Scripting** | Command dispatch | `FCommandExecutor`, command/response structs |
| **Server** | Network layer | `UAgentBridgeServiceSubsystem`, `FAgentHttpServer` |

Each module has its own `CLAUDE.md` with detailed documentation in `Source/<ModuleName>/CLAUDE.md`.

---

## Configuration

### Ports

| Port | Protocol | Purpose |
|------|----------|---------|
| 10001 | gRPC | Primary communication (via Tempo) |
| 8080 | HTTP | Fallback/testing |

### World Context

AgentBridge supports multiple world contexts:

| Context | When Active | Capabilities |
|---------|-------------|--------------|
| Editor | Level editing | Full access, undo/redo |
| PIE | Play In Editor | Runtime behavior, limited editor ops |
| Game | Packaged game | Runtime only |

Switch contexts with `set_target_world("editor")` or `set_target_world("pie")`.

---

## Development

### Building

```bash
# Full build
cd D:/tempo/TempoSample/Scripts
./Build.sh

# Or direct UnrealBuildTool
"D:/EL_UE/UE_5.6/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" \
  TempoSampleEditor Win64 Development \
  -Project="D:/tempo/TempoSample/TempoSample.uproject" -WaitMutex

# Live Coding (editor running)
Ctrl+Alt+F11
```

### Testing

```bash
cd D:/tempo/TempoSample/Plugins/AgentBridge/Python

# Use TempoEnv Python (required for grpcio/protobuf)
D:/tempo/TempoSample/TempoEnv/Scripts/python.exe test_grpc.py   # gRPC tests
D:/tempo/TempoSample/TempoEnv/Scripts/python.exe test_client.py # HTTP tests
```

### Console Commands

Debug commands available in the editor:

| Command | Description |
|---------|-------------|
| `AgentBridge.ListWorlds` | List world contexts |
| `AgentBridge.Capabilities` | Show current context capabilities |
| `AgentBridge.DumpActor <name> [depth]` | Dump actor properties |
| `AgentBridge.DumpClass <name>` | Dump class schema |
| `AgentBridge.QueryActors <class> [limit]` | Query actors |
| `AgentBridge.IsPartitioned` | Check World Partition status |

### Adding New Tools

1. **Add command struct** in `AgentBridgeScripting/AgentCommands.h`
2. **Implement handler** in `AgentBridgeScripting/CommandExecutor.cpp`
3. **Add RPC** in `AgentBridge.proto`
4. **Add thin handler** in `AgentBridgeServer/AgentBridgeServiceSubsystem.cpp`
5. **Add MCP tool** in `Python/mcp/services/agentbridge.py`
6. **Update help** in `_get_help_text()`

See `CLAUDE.md` for detailed patterns and examples.

---

## Design Philosophy

> **Users and agents should not need to know implementation details. Tools should just work.**

When a tool has multiple ways to accomplish something, it figures out the right approach automatically:

| User Intent | Tool Behavior |
|-------------|---------------|
| `spawn_actor("BP_MyActor")` | Auto-adds `_C` suffix, searches loaded classes, falls back to path loading |
| `set_property(path="Color", value=[1,0,0])` | Detects array format, converts to proper struct, handles color/vector/rotator |
| `get_property(actor="MyLight", path="Intensity")` | Resolves label to actor, finds component, traverses path |
| `query_actors(class="PointLight")` | Matches `APointLight`, `PointLight`, `PointLightActor` variants |

The complexity lives in lower modules; the API surface stays simple.

---

## Optional: bp_toolkit Submodule

bp_toolkit enables offline asset manipulation via JSON - create and modify Blueprints, PCG Graphs, and DataAssets without Unreal running.

### Setup

```bash
cd D:/tempo/TempoSample/Plugins/AgentBridge
git submodule update --init --recursive
cd bp_toolkit/vendor/UAssetGUI && dotnet build -c Release
```

### Capabilities

| Asset Type | Support Level |
|------------|---------------|
| Blueprint | Comments, cloning, property paths |
| PCG Graph | Full modification, round-trip |
| DataAsset | Property paths, cloning |

### Example Workflow

```bash
# Export uasset to JSON
python bp_export.py SomeAsset.uasset

# Modify via Python
python -c "
from bp_builder import AssetModifier
asset = AssetModifier('SomeAsset.json')
asset.set_property('BiomeDefinition.BiomePriority', 5)
asset.save('Modified.json')
"

# Import back to uasset
python bp_export.py Modified.json --import
```

See `bp_toolkit/README.md` for full documentation.

---

## Known Limitations

| Limitation | Workaround |
|------------|------------|
| gRPC header conflicts | Business logic in Scripting module, not Server |
| `TSoftObjectPtr` assignment | Use `TObjectPtr` properties instead |
| Editor still running blocks rebuild | Use `taskkill /F /IM UnrealEditor-Cmd.exe` |
| FunctionInvoker struct returns | Auto-redirected to property access |

---

## Contributing

1. Fork the repository
2. Create a feature branch
3. Follow existing patterns in module CLAUDE.md files
4. Update documentation (help text, CLAUDE.md, README)
5. Test with both gRPC and HTTP
6. Submit PR

### Documentation Checklist

When adding features, update:

| File | Purpose |
|------|---------|
| Code comments | Developer reference |
| Module `CLAUDE.md` | AI/session continuity |
| Help text in `agentbridge.py` | Agent-facing docs |
| Tool descriptions | MCP tool discoverability |
| This README | User documentation |

---

## License

MIT License - see LICENSE file.

---

## Credits

- **Tempo** - gRPC infrastructure and simulation control
- **UAssetAPI** - Asset parsing (bp_toolkit)
- **Claude Code** - Development assistance

---

## Support

- **Issues:** [GitHub Issues](https://github.com/YOUR_ORG/AgentBridge/issues)
- **Documentation:** Module CLAUDE.md files
- **Help:** Use `help()` tool for runtime documentation

---

*Version 0.3.0 - ~100 MCP tools, 51 gRPC RPCs, Modular Loading, PCG Biome Workflows*
