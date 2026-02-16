# AgentBridge

> **Let AI agents and scripts control Unreal Engine in real-time.**

**Version:** 0.3.0 | **Engine:** Unreal Engine 5.6 | **License:** MIT | **Status:** Beta

---

## Table of Contents

- [What Is AgentBridge?](#what-is-agentbridge)
- [Why Would You Want This?](#why-would-you-want-this)
- [How It Works (The Big Picture)](#how-it-works-the-big-picture)
- [Key Features](#key-features)
- [Quick Start](#quick-start)
- [A Walkthrough: Your First Interaction](#a-walkthrough-your-first-interaction)
- [Usage Examples](#usage-examples)
- [Tool Reference](#tool-reference)
- [Modular Loading](#modular-loading)
- [Plugin Architecture (For Developers)](#plugin-architecture-for-developers)
- [Installation](#installation)
- [Configuration](#configuration)
- [Integrations](#integrations)
- [Development Guide](#development-guide)
- [Known Limitations](#known-limitations)
- [Contributing](#contributing)
- [Support](#support)

---

## What Is AgentBridge?

AgentBridge is a plugin for **Unreal Engine** (Epic Games' 3D game engine) that lets programs
running *outside* of Unreal Engine read and modify everything inside it - actors (objects in a
scene), their properties (position, color, brightness), assets (meshes, materials, Blueprints),
and more.

Think of it like a **remote control for Unreal Engine**. Just as a TV remote lets you change
channels without touching the TV, AgentBridge lets an AI assistant or a Python script change a
light's color, spawn a new building, or rearrange an entire level - without anyone clicking
through the Unreal Editor's menus by hand.

### What do we mean by "AI agent"?

An **AI agent** is a program powered by a large language model (like Claude or GPT) that can
use **tools** to take actions in the real world - or in this case, inside a game engine. Instead
of just chatting, an agent can:

- Call a tool named `spawn_actor` to place a new light in your scene
- Call `set_property` to change that light's color to warm orange
- Call `query_actors` to find all the trees in a forest and move them

AgentBridge provides about **100 of these tools**, covering nearly everything you can do in the
Unreal Editor. The AI agent does not need to understand C++ or Unreal's internal APIs - it just
calls simple, named tools with straightforward parameters.

### What do we mean by "scripts"?

AgentBridge is not limited to AI. Any program that can make network calls can use it:

- **Python scripts** for batch-processing levels (e.g., "replace all red lights with blue ones")
- **Automation pipelines** for CI/CD (e.g., "open level, run checks, save, export")
- **Custom dashboards** that display live scene data
- **LangChain agents** and other AI orchestration frameworks

---

## Why Would You Want This?

### For Level Designers and Artists

Imagine saying to an AI assistant: *"Build me a forest clearing with a campfire, surrounded by
pine trees, with warm lighting."* Instead of placing each actor by hand, the AI agent calls
AgentBridge tools to spawn the meshes, position the lights, set material properties, and
configure the PCG (Procedural Content Generation) volumes - all in seconds.

### For Technical Artists and TAs

Automate repetitive tasks: *"Go through every PointLight in the level and set its attenuation
radius to 3x its current value"* or *"Find all actors tagged 'placeholder' and replace them
with the production mesh."* Write a Python script that loops through `query_actors` results and
calls `set_property` for each one.

### For Programmers and Tool Developers

Build custom tools on top of AgentBridge's API. Create a web dashboard that shows live actor
counts, a Slack bot that screenshots the viewport on demand, or a test harness that validates
level setups automatically.

### For Researchers and Educators

Use AI agents to explore procedural generation techniques, test game mechanics programmatically,
or teach Unreal Engine concepts through conversational interaction rather than memorizing menu
locations.

---

## How It Works (The Big Picture)

Here is the full picture of how an AI agent's request reaches Unreal Engine:

```
 You (or an AI agent like Claude)
   |
   |  "Spawn a PointLight at position 0, 0, 500"
   v
 MCP Server  (a Python process that translates AI tool calls into network requests)
   |
   |  Sends a gRPC message: SpawnActor { class: "PointLight", location: [0, 0, 500] }
   v
 AgentBridge  (a set of C++ plugins running inside the Unreal Editor)
   |
   |  Receives the message, creates a PointLight actor at (0, 0, 500)
   v
 Unreal Engine  (the light appears in the viewport)
```

### Key Terms Explained

If you are new to this ecosystem, here are the terms you will see throughout this documentation:

| Term | What It Means |
|------|--------------|
| **Unreal Engine (UE)** | A 3D game engine by Epic Games. The thing AgentBridge controls. |
| **Actor** | Any object in an Unreal level - a light, a mesh, a camera, a trigger volume. Everything you see (and some things you don't) is an Actor. |
| **Property** | A data value on an actor or component - position, color, brightness, name, etc. |
| **Component** | A modular piece of functionality attached to an actor. A PointLight actor has a `LightComponent` (the actual light source) and a `RootComponent` (its position in space). |
| **Blueprint (BP)** | Unreal's visual scripting system. Also used to create reusable actor templates. A Blueprint class like `BP_Tree` can be spawned just like a C++ class. |
| **MCP** | **Model Context Protocol** - a standard created by Anthropic that defines how AI agents discover and call tools. AgentBridge implements MCP so that Claude Code (and other MCP-compatible agents) can use its tools automatically. |
| **gRPC** | **Google Remote Procedure Call** - a high-performance framework for programs to call functions in other programs over a network. AgentBridge uses gRPC on **port 10001** for fast communication. Think of it as a "function call over the network." |
| **Protobuf** | **Protocol Buffers** - the compact binary message format used by gRPC. Faster than JSON. Defined in `.proto` files that get compiled into C++ and Python code. |
| **HTTP** | The same protocol your web browser uses. AgentBridge also has an HTTP API on **port 8080** as a simpler fallback for testing. |
| **Tempo** | A simulation framework plugin for Unreal Engine. AgentBridge uses Tempo's gRPC infrastructure rather than building its own networking from scratch. |
| **PIE** | **Play In Editor** - when you click "Play" in the Unreal Editor to test your game. PIE creates a temporary copy of the level where gameplay runs. |
| **World Partition** | UE5's system for managing very large open worlds by streaming parts of the level in and out of memory as needed. |
| **Reflection** | Unreal Engine's system for inspecting C++ types at runtime - what properties an object has, what functions it supports, what its class hierarchy looks like. This is how AgentBridge can read/write *any* property without hardcoding each one. See [AgentBridgeCore's README](AgentBridgeCore/README.md) for a deeper explanation. |

---

## Key Features

| Category | What You Can Do |
|----------|----------------|
| **Actor Operations** | Find, spawn, delete, move, rotate, scale, attach/detach any actor |
| **Property Access** | Read/write any property via dot-notation paths (`LightComponent0.Intensity`, `RootComponent.RelativeLocation.X`) |
| **Type Discovery** | List all available classes, inspect their properties and functions, find Blueprints |
| **Asset Operations** | Create, save, duplicate DataAssets and MaterialInstances; convert actors to Blueprints |
| **Blueprint/PCG Graphs** | Create nodes, connect pins, edit Blueprint and PCG graphs programmatically |
| **World Partition** | Query actors in streaming worlds (even unloaded ones), get landscape bounds, manage data layers |
| **Console Commands** | Execute and search 5,000+ Unreal console commands and variables |
| **Simulation Control** | Start/pause/step simulations, control time of day, command vehicles and AI pawns (via Tempo) |
| **File Operations** | Read, write, copy, list, and delete files in the project directory |
| **Capture** | Screenshot the viewport or render from arbitrary camera positions |

**~100 MCP tools** organized into 7 loadable modules, plus **14 offline tools** for asset manipulation without the editor running (via the optional bp_toolkit submodule).

---

## Quick Start

### What You Need

| Prerequisite | Why | Where to Get It |
|-------------|-----|----------------|
| Unreal Engine 5.6 | The engine AgentBridge controls | [epicgames.com](https://www.unrealengine.com/) |
| Tempo Plugin | Provides the gRPC server that AgentBridge plugs into | [github.com/tempo-sim/Tempo](https://github.com/tempo-sim/Tempo) |
| Python 3.11+ | Runs the MCP server that bridges AI agents to gRPC | Included in Tempo's `TempoEnv` |
| Claude Code (or another MCP client) | The AI agent that uses the tools | [claude.ai/claude-code](https://claude.ai/claude-code) |

### Step 1: Install AgentBridge

Clone AgentBridge into your Unreal project's `Plugins/` directory:

```bash
cd YourProject/Plugins
git clone https://github.com/YOUR_ORG/AgentBridge.git

# Initialize submodules (needed for the MCP server and optional bp_toolkit)
cd AgentBridge
git submodule update --init --recursive
```

AgentBridge consists of **4 separate Unreal Engine plugins** inside the `AgentBridge/` directory.
You do not need to enable them individually - Unreal Build Tool (UBT) discovers them
automatically by scanning subdirectories. They are all enabled by default.

### Step 2: Build Your Project

Close the Unreal Editor if it is open (the build needs to replace DLL files that the editor
locks), then build:

```bash
# If using Tempo's build script:
cd YourProject/Scripts
./Build.sh

# Or use Unreal Build Tool directly (adjust paths for your setup):
"<EngineDir>/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" \
  YourProjectEditor Win64 Development \
  -Project="YourProject/YourProject.uproject" -WaitMutex
```

### Step 3: Start the Unreal Editor

```bash
cd YourProject
./Plugins/Tempo/Scripts/Run.sh
```

Wait about 30 seconds for the gRPC server to start on port 10001. You will see a log message
in the Output Log when it is ready.

### Step 4: Configure Your AI Agent

The MCP server needs to be configured as a tool provider for your AI agent. For **Claude Code**,
create a wrapper script and add it to your settings.

**Create a wrapper script** (e.g., `~/.claude/agentbridge-mcp.sh`):

```bash
#!/bin/bash
cd /path/to/YourProject/Plugins/AgentBridge
exec /path/to/YourProject/TempoEnv/Scripts/python.exe -m mcp \
  --host localhost --port 10001 --profile full "$@"
```

Make it executable: `chmod +x ~/.claude/agentbridge-mcp.sh`

**Add to Claude Code settings** (`~/.claude.json`):

```json
{
  "mcpServers": {
    "agentbridge": {
      "type": "stdio",
      "command": "/home/youruser/.claude/agentbridge-mcp.sh",
      "args": [],
      "env": {}
    }
  }
}
```

**Important notes:**
- The `cwd` must be the `AgentBridge/` directory (parent of `mcp/`), not `AgentBridge/mcp/`
- The Unreal Editor must be running before starting Claude Code (MCP connects to gRPC on startup)
- After changing settings, restart Claude Code and verify with the `/mcp` command

<details>
<summary><strong>Alternative: Direct configuration without wrapper script</strong></summary>

You can also configure it directly in `~/.claude.json`:

```json
{
  "mcpServers": {
    "agentbridge": {
      "command": "/path/to/YourProject/TempoEnv/Scripts/python.exe",
      "args": ["-m", "mcp", "--host", "localhost", "--port", "10001", "--profile", "full"],
      "cwd": "/path/to/YourProject/Plugins/AgentBridge",
      "env": {}
    }
  }
}
```

Note: Not all MCP clients support the `cwd` field. The wrapper script approach is more portable.
</details>

### Step 5: Use It

In Claude Code, just ask naturally:

```
> What actors are in my level?
> Spawn a PointLight at position 0, 0, 500
> Set its intensity to 8000 and make it warm white
> Find all StaticMeshActors and list their mesh names
```

The AI agent will call AgentBridge tools (`query_actors`, `spawn_actor`, `set_property`, etc.)
to carry out your requests.

---

## A Walkthrough: Your First Interaction

Here is what happens behind the scenes when you ask Claude Code to spawn a light:

**You say:** *"Spawn a PointLight at position 100, 200, 300 and name it KitchenLight"*

**Step 1 - Tool Call:** Claude decides to use the `spawn_actor` tool and calls it with:
```json
{ "class_name": "PointLight", "location": [100, 200, 300], "label": "KitchenLight" }
```

**Step 2 - MCP to gRPC:** The Python MCP server converts this into a gRPC `SpawnActor` request
and sends it to Unreal Engine on port 10001.

**Step 3 - Inside Unreal:** The `AgentBridgeServer` plugin receives the gRPC message. Its
handler (`UAgentBridgeServiceSubsystem::SpawnActor` in
[AgentBridgeServer/](AgentBridgeServer/README.md)) creates a command struct and passes it to
`FCommandExecutor::Execute()` in [AgentBridgeScripting/](AgentBridgeScripting/README.md).

**Step 4 - Command Execution:** The CommandExecutor resolves `"PointLight"` to Unreal's
`APointLight` class (handling `_C` suffixes for Blueprints, partial name matching, etc.),
creates the actor at the specified location, and labels it `"KitchenLight"`.

**Step 5 - Response:** A success response flows back through gRPC to the MCP server, which
tells Claude the actor was created. Claude responds: *"I've spawned a PointLight named
KitchenLight at (100, 200, 300)."*

**You say:** *"Make it brighter - set intensity to 10000"*

Claude calls `set_property(actor_id="KitchenLight", path="LightComponent0.Intensity", value=10000)`.
The property path `LightComponent0.Intensity` means: "find the component named
`LightComponent0` on the actor, then access its `Intensity` property." AgentBridge resolves this
automatically using the property path system in [AgentBridgeCore/](AgentBridgeCore/README.md).

---

## Usage Examples

### Property Paths

AgentBridge supports nested property paths with automatic component resolution. You can reach
any property on any actor or asset using dot-notation:

```python
# Direct property on the actor
get_property(actor_id="MyActor", path="Health")

# Component property (use INSTANCE name like LightComponent0, not class name)
get_property(actor_id="MyLight", path="LightComponent0.Intensity")

# Nested struct field
get_property(actor_id="MyActor", path="RootComponent.RelativeLocation.X")

# Array element
get_property(actor_id="MyActor", path="Inventory[0].ItemName")

# DataAsset properties (use the asset path as actor_id)
get_property(actor_id="/Game/Data/MyAsset.MyAsset", path="SomeProperty")
```

For a deep dive into how property paths work, see
[AgentBridgeCore's README](AgentBridgeCore/README.md#fagentpropertypath---dot-notation-property-paths)
(the `FAgentPropertyPath` class).

### Value Formats

Values are automatically parsed based on the target property type. You can use whichever format
is most convenient:

```python
# Colors - all of these work
set_property(actor_id="Light", path="LightColor", value="(R=255,G=128,B=0,A=255)")
set_property(actor_id="Light", path="LightColor", value="[255, 128, 0, 255]")

# Vectors - both formats work
set_property(actor_id="Actor", path="Location", value="(X=100,Y=200,Z=300)")
set_property(actor_id="Actor", path="Location", value="[100, 200, 300]")

# Simple types
set_property(actor_id="Light", path="bVisible", value="true")
set_property(actor_id="Light", path="Intensity", value="5000")

# String arrays
set_property(actor_id="Actor", path="Tags", value='["Tag1", "Tag2", "Tag3"]')
```

### Python (Direct gRPC)

You can also use AgentBridge from Python scripts without an AI agent:

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

### PCG Biome Workflow

For complex procedural generation workflows (spawning volumes, creating DataAssets, linking them
together), see the `help(topic="workflows")` and `help(topic="pcg_volume")` built-in help topics.

<details>
<summary><strong>Expand: Full PCG biome workflow example</strong></summary>

```python
# 1. Get landscape bounds for sizing
bounds = get_landscape_bounds()

# 2. Spawn biome actors at landscape center
spawn_actor(class_name="BP_PCGBiomeCore", location=bounds["center"], label="MyBiomeCore")
spawn_actor(class_name="BP_PCGBiomeVolume", location=bounds["center"], label="MyBiomeVolume")

# 3. Size the volume to cover landscape
# BoxExtent is HALF-SIZE in Unreal units - add Z margin for spawn variation
set_property(actor_id="MyBiomeVolume", path="BiomeVolume.BoxExtent",
    value=f"(X={bounds['half_extents'][0]},Y={bounds['half_extents'][1]},Z={bounds['half_extents'][2] + 5000})")

# 4. Create and configure a BiomeDefinition DataAsset
create_asset(asset_class="BiomeDefinitionTemplate", package_path="/Game/Biomes", asset_name="ForestBiome")
set_property(actor_id="/Game/Biomes/ForestBiome.ForestBiome",
             path="BiomeDefinition.BiomeName", value="Forest")
save_asset(asset_path="/Game/Biomes/ForestBiome")

# 5. Link DataAsset to the volume
set_property(actor_id="MyBiomeVolume", path="Definition",
             value="/Game/Biomes/ForestBiome.ForestBiome")
```

**Key points:**
- Use `BP_PCGBiomeVolume` (BoxComponent) not native `PCGVolume` (BrushComponent)
- Component names are INSTANCE names (e.g., `BiomeVolume`), not class names
- `BoxExtent` is HALF the actual volume size
- DataAsset properties use asset path format: `/Game/Path/AssetName.AssetName`
</details>

---

## Tool Reference

AgentBridge provides a built-in help system. Call `help()` for an overview, or specify a topic:

| Topic | What It Covers |
|-------|---------------|
| `help()` | Overview of all capabilities |
| `help(topic="actors")` | Finding, spawning, transforming actors |
| `help(topic="properties")` | Property paths, reading/writing values |
| `help(topic="classes")` | Type discovery, class schemas |
| `help(topic="assets")` | Asset creation, saving, file operations |
| `help(topic="components")` | Component transforms, attachment |
| `help(topic="console")` | Console commands and CVars |
| `help(topic="workflows")` | Common multi-step operations (includes PCG biome workflow) |
| `help(topic="pcg_volume")` | PCG volume types and sizing |
| `help(topic="volume_sizing")` | BoxComponent sizing details |
| `help(topic="bp_toolkit")` | Offline asset manipulation (when submodule present) |

### Core AgentBridge Tools (~57 tools)

#### World Operations
| Tool | Description |
|------|-------------|
| `list_worlds` | List all world contexts (Editor, PIE, Game) |
| `set_target_world` | Switch which world subsequent commands operate on |

#### Actor Operations
| Tool | Description |
|------|-------------|
| `query_actors` | Find actors by class, name, label, or tag |
| `get_actor` | Get actor details, properties, and components |
| `spawn_actor` | Create a new actor with transform and properties |
| `delete_actor` | Remove an actor from the world |
| `set_actor_transform` | Move, rotate, or scale an actor |

#### Property Operations
| Tool | Description |
|------|-------------|
| `get_property` | Read any property via dot-notation path (supports nested paths) |
| `set_property` | Write any property via dot-notation path |

#### Type Discovery
| Tool | Description |
|------|-------------|
| `list_classes` | Find actor, component, or object classes |
| `get_class_schema` | Get all properties and functions of a class |
| `call_static_function` | Call Blueprint library functions |

#### World Partition
| Tool | Description |
|------|-------------|
| `is_world_partitioned` | Check if the world uses streaming |
| `query_all_actors` | Query including unloaded streaming actors |
| `get_streaming_state` | Get an actor's load state (loaded/unloaded) |
| `query_landscape` | List landscape proxies (terrain chunks) |
| `get_landscape_bounds` | Get full landscape extents |
| `get_data_layers` | List data layers (streaming groups) |
| `get_actors_in_data_layer` | Query actors by data layer |

#### Asset Operations
| Tool | Description |
|------|-------------|
| `create_asset` | Create a DataAsset, MaterialInstance, etc. |
| `save_asset` | Save an asset to disk |
| `duplicate_asset` | Copy an asset with a new name |
| `save_actor_as_blueprint` | Convert an actor into a reusable Blueprint |

#### File Operations
| Tool | Description |
|------|-------------|
| `read_project_file` | Read a file from the project directory |
| `write_project_file` | Write a file to the project directory |
| `list_project_directory` | List files with glob patterns |
| `copy_project_file` | Copy a file within the project |

#### Transform and Attachment (Unified)

These tools work on **both actors and components** using `Actor->Component` syntax:

| Tool | Description |
|------|-------------|
| `set_transform` | Set location/rotation/scale |
| `get_transform` | Get current transform |
| `attach` | Attach to a parent (supports sockets) |
| `detach` | Detach from parent (maintains world transform) |

**Example:** `set_transform(target="MyLight->LightComponent0", location=[0,0,500])`

#### Console Commands
| Tool | Description |
|------|-------------|
| `execute_console_command` | Run any Unreal console command |
| `search_console_commands` | Search 5,000+ commands and CVars by keyword |

#### Blueprint Graph Operations (6 tools)
| Tool | Description |
|------|-------------|
| `bp_create_node` | Create a node (CallFunction, Event, Variable, Branch, Sequence, Comment) |
| `bp_connect_pins` | Connect two pins between nodes |
| `bp_disconnect_pins` | Disconnect pins |
| `bp_delete_node` | Delete a node from a graph |
| `bp_list_nodes` | List all nodes with GUIDs, types, and positions |
| `bp_list_pins` | List all pins on a node with directions and types |

**Note:** Blueprints must have an initialized EventGraph (use editor-created or `duplicate_asset()`, not `create_asset()`).

#### PCG Graph Operations (6 tools)
| Tool | Description |
|------|-------------|
| `pcg_add_node` | Add a node (SurfaceSampler, StaticMeshSpawner, etc.) |
| `pcg_connect` | Connect two PCG nodes |
| `pcg_disconnect` | Disconnect nodes |
| `pcg_delete_node` | Delete a node |
| `pcg_list_nodes` | List all nodes with their pins |
| `pcg_get_input_output_nodes` | Get the InputNode and OutputNode (entry/exit points) |

**Note:** Pin labels can be unintuitive - InputNode's output pin is named `In`, OutputNode's input pin is named `Out`. Use `pcg_list_nodes` to discover actual pin names.

### Tempo Simulation Tools (~30 tools)

These tools are provided by the [Tempo](https://github.com/tempo-sim/Tempo) plugin and require
a PIE (Play In Editor) session to be active. They control simulation playback, time, vehicles,
and AI navigation.

<details>
<summary><strong>Expand: Full Tempo tool list</strong></summary>

#### Simulation Control
| Tool | Description |
|------|-------------|
| `tempo_play` | Start/resume simulation |
| `tempo_pause` | Pause simulation |
| `tempo_step` | Advance one frame |
| `tempo_advance_steps` | Advance N frames |
| `tempo_set_time_mode` | WALL_CLOCK (real-time) or FIXED_STEP |
| `tempo_set_sim_rate` | Set steps per second in FIXED_STEP mode |
| `tempo_set_control_mode` | NONE, USER, OPEN_LOOP, or CLOSED_LOOP |
| `tempo_load_level` | Load a level with deferred/start_paused options |
| `tempo_finish_loading_level` | Complete a deferred level load |
| `tempo_set_viewport_render` | Enable/disable viewport rendering (for headless mode) |

#### Level Control (Editor)
| Tool | Description |
|------|-------------|
| `play_in_editor` | Start a PIE session |
| `simulate` | Start Simulate mode (physics without player control) |
| `stop` | Stop PIE or Simulate |
| `save_level` | Save the current level |
| `open_level` | Open a level in the editor |
| `new_level` | Create an empty level |
| `get_current_level` | Get the name of the loaded level |

#### Geographic and Time
| Tool | Description |
|------|-------------|
| `tempo_set_date` | Set simulation date |
| `tempo_set_time_of_day` | Set time of day |
| `tempo_set_day_cycle_rate` | Set day/night cycle speed (1.0 = real-time, 60.0 = 1 hour/minute) |
| `tempo_get_datetime` | Get current simulation datetime |
| `tempo_set_geographic_reference` | Set latitude/longitude/altitude reference point |

#### Movement and AI
| Tool | Description |
|------|-------------|
| `tempo_get_commandable_vehicles` | List controllable vehicles |
| `tempo_command_vehicle` | Send steering and acceleration commands |
| `tempo_get_commandable_pawns` | List controllable AI pawns |
| `tempo_pawn_move_to` | Navigate a pawn to a location |
| `tempo_rebuild_navigation` | Rebuild the navigation mesh |
| `tempo_run_zone_graph_builder` | Build zone graph for AI navigation |

#### World State and Sensors
| Tool | Description |
|------|-------------|
| `tempo_get_actor_state` | Get an actor's transform, velocity, and bounds |
| `tempo_get_actors_near` | Get all actors within a radius |
| `tempo_get_available_sensors` | List cameras and their capabilities |
| `tempo_get_label_map` | Instance-to-semantic-ID mapping (for segmentation) |

#### Map Queries
| Tool | Description |
|------|-------------|
| `tempo_get_lanes` | Query lane center points, width, and connections |
| `tempo_get_lane_accessibility` | Check traffic light/sign status between lanes |
| `tempo_get_zones` | Query zone boundaries and connections |
</details>

### bp_toolkit Offline Tools (14 tools, optional)

These tools work **without the Unreal Editor running**. They export `.uasset` files to JSON,
let you modify them, and import them back. Useful for batch asset modification, CI/CD pipelines,
and working on assets when you do not have the editor available.

**Requires:** The `bp_toolkit` submodule and .NET 8+ (for UAssetGUI).

| Tool | Description |
|------|-------------|
| `bp_export_asset` | Export a .uasset file to JSON |
| `bp_import_asset` | Import modified JSON back to .uasset |
| `bp_detect_type` | Detect asset type (Blueprint, PCG, DataAsset, etc.) |
| `bp_get_info` | Get a summary of an asset |
| `bp_list_properties` | List all properties with types and values |
| `bp_get_property` | Get a property by path |
| `bp_set_property` | Set a property by path |
| `bp_clone_asset` | Clone an asset with a new name |
| `bp_list_graphs` | List graphs in a Blueprint or PCG asset |
| `bp_add_comment` | Add a comment node to a graph |
| `bp_clone_node` | Clone an existing node |
| `bp_find` | Search asset names and exports |
| `bp_query` | Run type-specific queries |
| `bp_parse` | Full Blueprint parsing with call graphs |

---

## Modular Loading

Tools are organized into 7 modules and loaded via predefined profiles. This lets you control
how many tools your AI agent sees - fewer tools means faster responses and less confusion for
smaller models.

| Profile | Modules Loaded | Tool Count | Best For |
|---------|---------------|------------|----------|
| `core` | core | ~6 | Minimal: just actor queries and properties |
| `standard` | core, classes, editor, files | ~35 | Level editing (this is the **default**) |
| `editor` | + world_partition | ~42 | Full editor work including streaming worlds |
| `scripting` | + bp_toolkit | ~61 | Blueprint and PCG graph editing |
| `simulation` | core, classes, tempo_sim | ~34 | Runtime/PIE testing and simulation |
| `full` | all modules | ~100 | Everything (used in the Quick Start above) |

Set the profile when starting the MCP server:
```bash
python -m mcp --host localhost --port 10001 --profile standard
```

Or load additional modules at runtime:
```python
load_modules(modules=["bp_toolkit", "tempo_sim"])
```

---

## Plugin Architecture (For Developers)

AgentBridge consists of **4 independent Unreal Engine plugins**, each with its own `.uplugin`
file. There is no wrapper plugin - UBT discovers all four by recursively scanning the
`Plugins/AgentBridge/` directory. This follows the same pattern used by the Tempo plugin.

The plugins form a linear dependency chain:

```
AgentBridgeServer       (51 gRPC handlers, HTTP fallback, proto definitions)
  |                     Thin handlers only - no business logic here
  | depends on
  v
AgentBridgeScripting    (60+ command structs, CommandExecutor, JSON conversion)
  |                     ALL business logic lives here
  | depends on
  v
AgentBridgeRuntime      (world context, actor CRUD, target resolution, World Partition)
  |                     Knows about actors, worlds, and components
  | depends on
  v
AgentBridgeCore         (reflection primitives, property paths, function invocation, type discovery)
                        Knows about C++ types but nothing about actors or worlds
```

**Why this split?** The Server plugin includes gRPC headers, which conflict with certain Unreal
Engine headers on Windows. By keeping the Server plugin thin (just proto-to-struct conversion),
all the actual logic lives in Scripting where every UE header is available. See
[AgentBridgeServer's README](AgentBridgeServer/README.md#why-thin-handlers) for details.

### Plugin Details

| Plugin | Key Classes | README |
|--------|-------------|--------|
| **AgentBridgeCore** | `FPropertyAccessor`, `FAgentPropertyPath`, `FFunctionInvoker`, `FTypeDiscovery` | [AgentBridgeCore/README.md](AgentBridgeCore/README.md) |
| **AgentBridgeRuntime** | `FWorldContextManager`, `FActorOperations`, `FWorldPartitionOps`, `TargetResolution` | [AgentBridgeRuntime/README.md](AgentBridgeRuntime/README.md) |
| **AgentBridgeScripting** | `FCommandExecutor`, 60+ command/response structs in `AgentCommands.h` | [AgentBridgeScripting/README.md](AgentBridgeScripting/README.md) |
| **AgentBridgeServer** | `UAgentBridgeServiceSubsystem`, `FAgentHttpServer`, `AgentBridge.proto` (51 RPCs, 105 messages) | [AgentBridgeServer/README.md](AgentBridgeServer/README.md) |

Each plugin also has a `CLAUDE.md` with deeper implementation details intended for AI assistants
and experienced developers.

### External Components

| Component | Location | Purpose |
|-----------|----------|---------|
| **MCP Server** | `mcp/` (git submodule) | Python process that translates MCP tool calls into gRPC requests |
| **bp_toolkit** | `bp_toolkit/` (git submodule, optional) | Offline asset manipulation via JSON (export, modify, import `.uasset` files) |

---

## Installation

### From Source (Recommended)

```bash
# Clone into your project's Plugins directory
cd YourProject/Plugins
git clone https://github.com/YOUR_ORG/AgentBridge.git

# Initialize submodules (MCP server + optional bp_toolkit)
cd AgentBridge
git submodule update --init --recursive
```

If you want to use the **bp_toolkit** offline tools, you also need to build UAssetGUI:

```bash
# Requires .NET 8 SDK (https://dotnet.microsoft.com/download)
cd bp_toolkit/vendor/UAssetGUI
dotnet build -c Release
```

### Dependencies

| Dependency | Version | Purpose | Included? |
|------------|---------|---------|-----------|
| Tempo Plugin | Latest | gRPC infrastructure for Unreal | Must be in your project's Plugins/ |
| Python | 3.11+ | MCP server | Included in Tempo's `TempoEnv/` |
| grpcio | 1.62.2+ | Python gRPC client library | Included in `TempoEnv/` |
| protobuf | 4.25.3+ | Protocol Buffer serialization | Included in `TempoEnv/` |
| .NET 8 SDK | (optional) | bp_toolkit / UAssetGUI | [Download](https://dotnet.microsoft.com/download) |

### Package Naming Note

The MCP server Python package is named `agentbridge-mcp` (for pip/PyPI), but the Python import
is `mcp`:

```python
# pip install agentbridge-mcp  (package name)
from mcp import serve           # import name
from mcp.services import agentbridge
```

This follows common Python convention (similar to `pip install Pillow` -> `import PIL`).

---

## Configuration

### Network Ports

| Port | Protocol | Purpose |
|------|----------|---------|
| 10001 | gRPC | Primary communication channel (managed by Tempo) |
| 8080 | HTTP | JSON fallback for testing and simple scripts |

Both ports bind to localhost only. The gRPC port is available whenever the Unreal Editor is
running with Tempo enabled. The HTTP server starts automatically when the AgentBridgeServer
module loads.

### World Context

AgentBridge can operate on different "worlds" within Unreal Engine:

| Context | When Active | What It Is | Capabilities |
|---------|-------------|------------|-------------|
| **Editor** | Always (editor open) | The level you are editing | Full access including undo/redo |
| **PIE** | While playing in editor | Temporary gameplay copy | Runtime behavior, limited editor ops |
| **Game** | Packaged/standalone game | The shipping game | Runtime only |

AgentBridge automatically targets the PIE world when it exists, otherwise the Editor world. You
can switch explicitly:

```python
set_target_world("editor")  # Force editor world
set_target_world("pie")     # Force PIE world
```

See [AgentBridgeRuntime's README](AgentBridgeRuntime/README.md#what-is-a-world-context) for
a detailed explanation of world contexts.

---

## Integrations

### LangChain

AgentBridge works with [LangChain](https://www.langchain.com/) via the
`langchain-mcp-adapters` package. This enables using AgentBridge tools with LangChain agents
and LangGraph workflows - useful if you want to build custom AI pipelines that control Unreal.

```bash
pip install langchain-mcp-adapters langgraph langchain-anthropic
```

```python
import asyncio
from langchain_mcp_adapters.client import MultiServerMCPClient
from langgraph.prebuilt import create_react_agent
from langchain_anthropic import ChatAnthropic

async def main():
    async with MultiServerMCPClient({
        "agentbridge": {
            "command": "<YourProject>/TempoEnv/Scripts/python.exe",
            "args": ["-m", "mcp", "--host", "localhost", "--port", "10001", "--profile", "full"],
            "cwd": "<YourProject>/Plugins/AgentBridge",
            "transport": "stdio",
        }
    }) as client:
        tools = await client.get_tools()
        llm = ChatAnthropic(model="claude-sonnet-4-20250514")
        agent = create_react_agent(llm, tools)

        result = await agent.ainvoke({
            "messages": [("user", "Spawn a PointLight at 0,0,500 and set intensity to 8000")]
        })
        print(result["messages"][-1].content)

asyncio.run(main())
```

See `mcp/examples/langchain_integration.py` for interactive sessions, batch operations, and
direct tool call examples.

---

## Development Guide

### Building

**Important:** Close the Unreal Editor before building. The build process needs to replace DLL
files that the editor locks.

```bash
# Kill the editor (Windows, from Git Bash)
cmd //c "taskkill /F /IM UnrealEditor.exe"

# Build (using Tempo's build script)
cd YourProject/Scripts && ./Build.sh

# Alternative: Live Coding (editor stays running, for small C++ changes only)
# Press Ctrl+Alt+F11 in the editor
```

### Testing

AgentBridge includes both automated and manual testing capabilities.

```bash
cd YourProject/Plugins/AgentBridge

# gRPC integration tests (editor must be running)
YourProject/TempoEnv/Scripts/python.exe -m mcp.tests.test_grpc

# HTTP integration tests (editor must be running)
YourProject/TempoEnv/Scripts/python.exe -m mcp.tests.test_client
```

**Note:** Run tests from the `AgentBridge/` directory (parent of `mcp/`), not from inside `mcp/`.

### Console Commands

Debug commands available in the Unreal Editor console (press `~` or use the Output Log). All
output goes to the `LogAgentBridge` log category.

| Command | Description |
|---------|-------------|
| `AgentBridge.ListWorlds` | List all world contexts |
| `AgentBridge.Capabilities` | Show current context capabilities |
| `AgentBridge.DumpActor <name> [depth]` | Dump all properties of an actor |
| `AgentBridge.DumpClass <name>` | Dump a class's schema (properties + functions) |
| `AgentBridge.QueryActors [pattern] [limit]` | Find actors by name pattern |
| `AgentBridge.GetPath <actor> <path>` | Read a nested property |
| `AgentBridge.SetPath <actor> <path> <value>` | Write a nested property |
| `AgentBridge.IsPartitioned` | Check if the world uses World Partition |

See [AgentBridgeRuntime's README](AgentBridgeRuntime/README.md#console-commands) for the full
list of 22+ console commands.

### Adding New Tools

If you want to extend AgentBridge with new functionality, changes are needed across multiple
plugins. Here is the simplified flow:

1. **Define command struct** in `AgentBridgeScripting/Source/AgentBridgeScripting/Public/AgentCommands.h`
2. **Implement logic** in `AgentBridgeScripting/Source/AgentBridgeScripting/Private/CommandExecutor.cpp`
3. **Add proto message + RPC** in `AgentBridgeServer/Source/AgentBridgeServer/Public/AgentBridge.proto`
4. **Add thin gRPC handler** in `AgentBridgeServer/Source/AgentBridgeServer/Private/AgentBridgeServiceSubsystem.cpp`
5. **Register the handler** in `RegisterScriptingServices()` (easy to forget!)
6. **Add Python MCP tool** in `mcp/services/agentbridge.py`
7. **Update help text** in `_get_help_text()`

See [AgentBridgeScripting's README](AgentBridgeScripting/README.md#how-to-add-a-new-command)
for a detailed walkthrough with code examples, or
[AgentBridgeServer's README](AgentBridgeServer/README.md#how-to-add-a-new-rpc) for the gRPC
side.

For project-wide development conventions, build instructions, and testing protocols, see
[CLAUDE.md](CLAUDE.md).

---

## Known Limitations

| Limitation | Impact | Workaround |
|------------|--------|------------|
| `call_function` only supports zero-arg void functions | Cannot pass parameters to function calls via gRPC | Use `set_property`/`get_property` for parameterized operations. The C++ layer (`FFunctionInvoker`) supports full args - only the gRPC transport is limited. |
| `duplicate_asset` crashes on engine/plugin content | Cannot duplicate built-in engine assets | Copy plugin templates to `/Game/` first, then duplicate from there |
| `set_property` on BoxExtent does not update visuals | Wireframe does not refresh after setting BoxExtent | Use `set_transform` on the component's scale instead |
| `spawn_actor` with `relative_to` creates ghost actors | The `relative_to` parameter routes through a different backend | Spawn normally, then use `set_transform` to position relative to another actor |
| `TSoftObjectPtr` assignment does not work | Soft object references cannot be set via reflection | Use `TObjectPtr` properties where possible |
| gRPC header conflicts on Windows | Server plugin cannot include certain UE headers | All business logic lives in AgentBridgeScripting (by design) |

---

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Follow existing patterns in the module `CLAUDE.md` files
4. Update documentation:
   - Code comments for developers
   - Module `CLAUDE.md` for AI assistants
   - Help text in `agentbridge.py` for agents using the tools
   - Tool descriptions in MCP tool definitions
   - This README for users
5. Test with both gRPC and the editor running (automated tests are not sufficient alone)
6. Submit a pull request

---

## Support

- **Issues:** [GitHub Issues](https://github.com/YOUR_ORG/AgentBridge/issues)
- **Built-in help:** Use the `help()` tool for runtime documentation
- **Module docs:** Each plugin has its own [README.md](AgentBridgeCore/README.md) and
  [CLAUDE.md](AgentBridgeCore/CLAUDE.md)

---

## Design Philosophy

> **Users and agents should not need to know implementation details. Tools should just work.**

When a tool has multiple ways to accomplish something, AgentBridge figures out the right approach
automatically. The complexity lives in the lower modules; the API surface stays simple.

| What You Type | What AgentBridge Does Behind the Scenes |
|--------------|---------------------------------------|
| `spawn_actor("BP_MyActor")` | Auto-adds `_C` suffix, searches loaded classes, falls back to path loading |
| `set_property(path="Color", value=[1,0,0])` | Detects array format, converts to `(R=1,G=0,B=0,A=1)`, handles color/vector/rotator |
| `get_property(actor="MyLight", path="Intensity")` | Resolves label to actor name, finds the right component, traverses property path |
| `query_actors(class="PointLight")` | Matches `APointLight`, `PointLight`, `PointLightActor` - any common variation |

This philosophy is implemented through the layered architecture: [AgentBridgeCore](AgentBridgeCore/README.md)
handles type-level edge cases, [AgentBridgeRuntime](AgentBridgeRuntime/README.md) handles
world-level resolution, [AgentBridgeScripting](AgentBridgeScripting/README.md) handles
command dispatch, and [AgentBridgeServer](AgentBridgeServer/README.md) handles network
transport.

---

## Credits

- **[Tempo](https://github.com/tempo-sim/Tempo)** - gRPC infrastructure and simulation control
- **[UAssetAPI](https://github.com/atenfyr/UAssetAPI)** - Asset parsing for bp_toolkit
- **[Claude Code](https://claude.ai/claude-code)** - Development assistance

---

*Version 0.3.0 - ~100 MCP tools, 51 gRPC RPCs, modular loading, 4 independent UE plugins*
