# Phase 2: Modular Loading Architecture

> **Goal:** Load only needed tool modules, with sensible defaults and easy expansion

---

## Design Principles

1. **Zero-Config Usability** - Default profile should work for 90% of use cases
2. **Agent-Friendly** - Agents can dynamically request additional modules
3. **Minimal Friction** - No complex configuration required
4. **Predictable** - Same modules always provide same tools

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                         MCP SERVER STARTUP                          │
└───────────────────────────────┬─────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│  Load modules based on:                                              │
│  1. --modules CLI arg          (explicit list)                       │
│  2. --profile CLI arg          (predefined set)                      │
│  3. AGENTBRIDGE_MODULES env    (explicit list)                       │
│  4. AGENTBRIDGE_PROFILE env    (predefined set)                      │
│  5. Default: "standard" profile                                      │
└───────────────────────────────┬─────────────────────────────────────┘
                                │
           ┌────────────────────┼────────────────────┐
           ▼                    ▼                    ▼
    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
    │   CORE       │    │  STANDARD    │    │    FULL      │
    │  (always)    │    │  (default)   │    │   (all)      │
    │              │    │              │    │              │
    │  12 tools    │    │  42 tools    │    │ 104 tools    │
    └──────────────┘    └──────────────┘    └──────────────┘
```

---

## Module Groups

### Core (Always Loaded)

Essential tools that every workflow needs. Cannot be disabled.

| Module | Tools | Purpose |
|--------|-------|---------|
| `help` | 1 | Self-documentation |
| `worlds` | 2 | list_worlds, set_target_world |
| `actors` | 4 | query_actors, get_actor, spawn_actor, delete_actor |
| `properties` | 2 | get_property, set_property |
| `types` | 2 | list_classes, get_class_schema |

**Total: 11 tools**

### Standard Modules (Default Profile)

Commonly needed for editor workflows.

| Module | Tools | Added By |
|--------|-------|----------|
| `actor_transform` | 2 | set_actor_transform, duplicate_actor |
| `assets` | 4 | create_asset, save_asset, duplicate_asset, save_actor_as_blueprint |
| `console` | 2 | execute_console_command, search_console_commands |
| `functions` | 2 | call_static_function, call_asset_function |
| `files` | 4 | read/write/list/copy_project_file |

**Total: 14 tools** (Core 11 + Standard 14 = **25 tools**)

### Extended Modules (Opt-In)

Specialized functionality.

| Module | Tools | When Needed |
|--------|-------|-------------|
| `components` | 6 | Component transforms, attach/detach |
| `world_partition` | 7 | Large world streaming queries |
| `blueprints` | 6 | Blueprint node creation/connection (bp_create_node, etc.) |
| `pcg` | 6 | PCG graph manipulation (pcg_add_node, etc.) |
| `simulation` | 6 | tempo_play/pause/step/time |
| `tempo_actors` | 17 | Tempo's actor control (typed setters) |
| `tempo_editor` | 6 | PIE, simulate, save/open/new level |
| `tempo_geographic` | 5 | Date/time, coordinates |
| `tempo_movement` | 5 | Vehicle/pawn control |
| `tempo_state` | 2 | Actor state queries |
| `tempo_misc` | 6 | Labels, sensors, map query, zone graph |
| `bp_toolkit` | 14 | Offline asset manipulation |

**Total: 86 additional tools**

---

## Profiles

Predefined module sets for common scenarios.

### `core` Profile

Absolute minimum. For testing or highly constrained contexts.

```
core
```

**11 tools**

### `standard` Profile (DEFAULT)

Editor-focused work without simulation. Good for level building.

```
core + actor_transform + assets + console + functions + files
```

**25 tools**

### `editor` Profile

Full editor capabilities including components and world partition.

```
standard + components + world_partition
```

**38 tools**

### `simulation` Profile

For runtime/PIE testing with Tempo.

```
core + simulation + tempo_actors + tempo_editor + tempo_state
```

**42 tools**

### `scripting` Profile

For Blueprint and PCG graph manipulation.

```
standard + blueprints + pcg
```

**37 tools**

### `full` Profile

Everything, including Tempo and bp_toolkit.

```
all modules
```

**116 tools** (or 102 without bp_toolkit)

---

## Configuration Methods

### Method 1: CLI Arguments (Recommended for Config Files)

```json
{
  "mcpServers": {
    "agentbridge": {
      "command": "python",
      "args": ["-m", "mcp", "--profile", "standard"],
      "cwd": "..."
    }
  }
}
```

Or explicit modules:

```json
{
  "args": ["-m", "mcp", "--modules", "core,assets,simulation"]
}
```

### Method 2: Environment Variable

```bash
# Profile
AGENTBRIDGE_PROFILE=editor python -m mcp

# Explicit modules
AGENTBRIDGE_MODULES=core,assets,simulation python -m mcp
```

### Method 3: Dynamic Loading (Agent-Friendly)

A special `load_modules` tool available in all profiles:

```python
{
    "name": "load_modules",
    "description": "Load additional tool modules. Available: components, world_partition, simulation, tempo_actors, tempo_editor, tempo_geographic, tempo_movement, tempo_state, tempo_misc, bp_toolkit",
    "inputSchema": {
        "type": "object",
        "properties": {
            "modules": {
                "type": "array",
                "items": {"type": "string"},
                "description": "Module names to load"
            }
        },
        "required": ["modules"]
    }
}
```

**Usage by agent:**

```
Agent: I need to work with simulation controls.
       load_modules(modules=["simulation", "tempo_actors"])

MCP:   Loaded 23 additional tools from: simulation, tempo_actors
       New tools available: tempo_play, tempo_pause, ...
```

This allows agents to expand their toolset without requiring user reconfiguration.

---

## Implementation Plan

### 1. Update service registry (`services/__init__.py`)

```python
# Module definitions with dependencies
MODULES = {
    # Core (always loaded)
    "core": {
        "services": ["agentbridge_core"],
        "depends_on": [],
        "tools": ["help", "list_worlds", "set_target_world", "query_actors",
                  "get_actor", "spawn_actor", "delete_actor", "get_property",
                  "set_property", "list_classes", "get_class_schema"],
    },

    # Standard (default profile)
    "actor_transform": {
        "services": ["agentbridge_transforms"],
        "depends_on": ["core"],
        "tools": ["set_actor_transform", "duplicate_actor"],
    },
    "assets": {
        "services": ["agentbridge_assets"],
        "depends_on": ["core"],
        "tools": ["create_asset", "save_asset", "duplicate_asset", "save_actor_as_blueprint"],
    },
    # ... etc

    # Extended
    "simulation": {
        "services": ["tempo_time"],
        "depends_on": ["core"],
        "tools": ["tempo_play", "tempo_pause", "tempo_step", ...],
    },
    # ... etc
}

PROFILES = {
    "core": ["core"],
    "standard": ["core", "actor_transform", "assets", "console", "functions", "files"],
    "editor": ["core", "actor_transform", "assets", "console", "functions", "files",
               "components", "world_partition"],
    "simulation": ["core", "simulation", "tempo_actors", "tempo_editor", "tempo_state"],
    "full": list(MODULES.keys()),
}

DEFAULT_PROFILE = "standard"
```

### 2. Split agentbridge.py into focused files

Current monolithic structure:
```
agentbridge.py (37 tools, ~2500 lines)
```

Proposed split:
```
services/
├── agentbridge_core.py      # 11 core tools
├── agentbridge_transforms.py # 2 transform tools
├── agentbridge_assets.py     # 4 asset tools
├── agentbridge_console.py    # 2 console tools
├── agentbridge_functions.py  # 2 function tools
├── agentbridge_files.py      # 4 file tools
├── agentbridge_components.py # 6 component tools
├── agentbridge_partition.py  # 7 world partition tools
└── agentbridge_shared.py     # Shared client code
```

Each file follows same pattern:
```python
from . import register_service, ServiceModule
from .agentbridge_shared import AgentBridgeClient, safe_execute

TOOLS = [...]

def execute(client, tool_name, args):
    return safe_execute(client, tool_name, args)

# Registration happens only when module is loaded
def _register():
    register_service(ServiceModule(
        name="agentbridge_assets",
        description="Asset operations",
        tools=TOOLS,
        execute=execute,
        connect=lambda h, p: AgentBridgeClient(h, p),
    ))
```

### 3. Update server.py for module loading

```python
class MCPServer:
    def __init__(self, host, port, modules=None, profile=None):
        # Determine which modules to load
        if modules:
            self.enabled_modules = self._parse_modules(modules)
        elif profile:
            self.enabled_modules = PROFILES.get(profile, PROFILES[DEFAULT_PROFILE])
        else:
            self.enabled_modules = PROFILES[DEFAULT_PROFILE]

        # Always include core
        if "core" not in self.enabled_modules:
            self.enabled_modules.insert(0, "core")

        # Load only selected modules
        self._load_modules(self.enabled_modules)

        # Add load_modules tool for dynamic expansion
        self._add_dynamic_loader()

    def _add_dynamic_loader(self):
        """Add the load_modules tool for runtime expansion."""
        # This tool is always available and lets agents load more modules
        pass
```

### 4. Update CLI (`server.py` main)

```python
parser.add_argument(
    "--profile",
    choices=["core", "standard", "editor", "simulation", "full"],
    default=None,
    help="Load predefined module set (default: standard)",
)
parser.add_argument(
    "--modules",
    type=str,
    default=None,
    help="Comma-separated list of modules to load",
)
```

---

## User Experience

### For Users: Simple Config

Most users just specify a profile:

```json
{
  "mcpServers": {
    "agentbridge": {
      "args": ["-m", "mcp", "--profile", "editor"]
    }
  }
}
```

Or accept the default (`standard`) by specifying nothing extra.

### For Agents: Transparent Expansion

Agent workflow:

1. Agent starts with `standard` profile (25 tools)
2. User asks: "Set up the scene with proper lighting and run the simulation"
3. Agent sees it needs simulation tools
4. Agent calls: `load_modules(["simulation", "tempo_actors"])`
5. Server confirms: "Loaded 23 tools"
6. Agent proceeds with full simulation capabilities

No user intervention required.

### For Power Users: Fine-Grained Control

```bash
# Only what's needed for a specific task
AGENTBRIDGE_MODULES=core,simulation,components python -m mcp
```

---

## Tool Count by Profile

| Profile | Core | Added | Total | Typical Use Case |
|---------|------|-------|-------|------------------|
| `core` | 11 | 0 | 11 | Minimal testing |
| `standard` | 11 | 14 | 25 | Level editing |
| `editor` | 11 | 27 | 38 | Full editor work |
| `scripting` | 11 | 26 | 37 | Blueprint/PCG editing |
| `simulation` | 11 | 31 | 42 | Runtime testing |
| `full` | 11 | 105 | 116 | Everything |

---

## Token Impact

With Phase 1 compression (~30 tokens/tool average):

| Profile | Tools | Estimated Tokens |
|---------|-------|------------------|
| `core` | 11 | ~330 |
| `standard` | 25 | ~750 |
| `editor` | 38 | ~1,140 |
| `scripting` | 37 | ~1,110 |
| `simulation` | 42 | ~1,260 |
| `full` | 116 | ~3,480 |

With Claude Code wrapper overhead (~100 tokens/tool):

| Profile | Tools | Estimated Context |
|---------|-------|-------------------|
| `core` | 11 | ~1,430 |
| `standard` | 25 | ~3,250 |
| `editor` | 38 | ~4,940 |
| `scripting` | 37 | ~4,810 |
| `simulation` | 42 | ~5,460 |
| `full` | 116 | ~15,080 |

**Reduction from current (~69,600):**
- `standard`: 95% reduction
- `editor`: 93% reduction
- `scripting`: 93% reduction
- `full` with compression: 78% reduction

---

## Migration Path

### Step 1: No Breaking Changes

Initial release:
- Default profile = `full` (current behavior)
- All existing setups continue working
- New `--profile` and `--modules` args available

### Step 2: Communicate New Defaults

Update documentation to recommend `standard` profile.

### Step 3: Change Default (Optional)

After user adoption, change default from `full` to `standard`.
Users needing all tools can specify `--profile full`.

---

## Files to Modify

| File | Changes |
|------|---------|
| `services/__init__.py` | Add MODULES, PROFILES, module loading logic |
| `services/agentbridge.py` | Split into focused modules |
| `server.py` | Add --profile/--modules args, dynamic loading |
| `README.md` | Document profiles and configuration |
| `CLAUDE.md` | Update architecture section |

---

## Dynamic Loading Implementation Detail

The `load_modules` tool needs special handling:

```python
# In server.py

def _handle_load_modules(self, args):
    """Dynamically load additional modules."""
    requested = args.get("modules", [])
    already_loaded = set(self.enabled_modules)

    newly_loaded = []
    for module_name in requested:
        if module_name in already_loaded:
            continue
        if module_name not in MODULES:
            continue

        # Load the module
        self._load_module(module_name)
        newly_loaded.append(module_name)
        self.enabled_modules.append(module_name)

    # Return list of newly available tools
    new_tools = []
    for module_name in newly_loaded:
        new_tools.extend(MODULES[module_name]["tools"])

    return {
        "loaded_modules": newly_loaded,
        "new_tools": new_tools,
        "total_tools": len(self.tool_to_service),
    }
```

**Important:** After loading new modules, Claude Code will see the new tools in subsequent `tools/list` responses. The agent may need to call `tools/list` or start a new tool selection to see them.

---

## Testing Checklist

- [ ] `python -m mcp` loads `standard` profile by default
- [ ] `--profile core` loads only 11 tools
- [ ] `--profile full` loads all 104 tools
- [ ] `--modules core,simulation` loads exactly those modules
- [ ] `load_modules` tool successfully expands available tools
- [ ] bp_toolkit only loads when submodule present (even in `full`)
- [ ] All tools in each module work correctly
- [ ] help() reflects currently loaded modules

---

*Phase 2 complete. Proceed to Phase 3 (Consolidation) for deep analysis.*
