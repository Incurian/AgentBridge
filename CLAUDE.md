# AgentBridge Plugin

> UE 5.6 plugin exposing editor/runtime state to external AI agents via gRPC + MCP.
> Primary use case: "Build me a level" - agents need full read/write/discover capabilities.

## Core Design Philosophy

**🎯 THE MOST IMPORTANT PRINCIPLE:**

> **Users and agents should not need to know implementation details. Tools should just work.**

When a tool has multiple ways to accomplish something, it should figure out the right approach
under the hood. The complexity lives in the lower modules; the API surface stays simple.

**📝 THE SECOND MOST IMPORTANT PRINCIPLE:**

> **When you discover something "interesting" (unexpected behavior, workarounds, edge cases),
> document it immediately and explain how to handle it.**

Every friction point discovered is an opportunity to either:
1. Fix it so it "just works" (preferred)
2. Document the workaround clearly if a fix isn't feasible

### Examples of This Philosophy

| User Intent | Tool Behavior |
|-------------|---------------|
| `spawn_actor("BP_MyActor")` | Auto-adds `_C` suffix, searches loaded classes, falls back to path loading |
| `set_property(path="Color", value=[1,0,0])` | Detects array format, converts to `(R=1,G=0,B=0,A=1)`, handles color/vector/rotator |
| `get_property(actor="MyLight", path="Intensity")` | Resolves label to actor name, finds component, traverses path |
| `query_actors(class="PointLight")` | Matches `APointLight`, `PointLight`, `PointLightActor` variants |

### Implementation Layering

```
┌─────────────────────────────────────┐
│  MCP Tools (Python)                 │  ← Simple API, smart defaults
│  - Auto-detect value types          │
│  - Normalize class names            │
│  - Provide helpful error messages   │
├─────────────────────────────────────┤
│  CommandExecutor (Scripting)        │  ← Route to correct handler
│  - Dispatch based on input format   │
│  - Validate and transform inputs    │
├─────────────────────────────────────┤
│  Core/Runtime                       │  ← Handle edge cases
│  - Multiple resolution strategies   │
│  - Fallback paths for failures      │
│  - Type coercion and conversion     │
└─────────────────────────────────────┘
```

**When adding features:** Put the intelligence in lower modules. The user-facing API should
be minimal and obvious. If something "should just work," make it work automatically.

## Quick Start

| What | Where |
|------|-------|
| gRPC Port | 10001 (via Tempo) |
| HTTP Port | 8080 (fallback) |
| Python Env | `D:/tempo/TempoSample/TempoEnv/Scripts/python.exe` |
| Build Script | `D:/tempo/TempoSample/Scripts/Build.sh` |
| Run Editor | `cd D:/tempo/TempoSample && ./Plugins/Tempo/Scripts/Run.sh` |
| Kill Editor | `cmd //c "taskkill /F /IM UnrealEditor-Cmd.exe"` |

## Architecture

```
External Agents (Claude, LLMs)
         │
         ▼
MCP Server (Python) ─── 90 tools across 12 services
         │
         ▼
gRPC (port 10001) / HTTP (port 8080)
         │
         ▼
┌─────────────────────────────────────────────────┐
│ AgentBridgeServer   │ gRPC handlers, HTTP API  │
│ AgentBridgeScripting│ Commands, JSON dispatch  │
│ AgentBridgeRuntime  │ World ops, property paths│
│ AgentBridgeCore     │ Reflection primitives    │
└─────────────────────────────────────────────────┘
         │
         ▼
    Unreal Engine 5.6
```

## Module Documentation

Each module has its own CLAUDE.md with detailed context:

| Module | Focus | Doc |
|--------|-------|-----|
| AgentBridgeCore | Reflection (FProperty, UFunction, TypeDiscovery) | `Source/AgentBridgeCore/CLAUDE.md` |
| AgentBridgeRuntime | World context, actor ops, property paths | `Source/AgentBridgeRuntime/CLAUDE.md` |
| AgentBridgeScripting | Command layer, JSON serialization | `Source/AgentBridgeScripting/CLAUDE.md` |
| AgentBridgeServer | gRPC/HTTP server, proto definitions | `Source/AgentBridgeServer/CLAUDE.md` |
| Python | MCP server, gRPC client, tests | `Python/CLAUDE.md` |
| bp_toolkit | UAsset parsing, Blueprint analysis (optional submodule) | `bp_toolkit/README.md` |

## Critical Rules

### Never Do

- Modify Tempo plugin or UE Engine source - work around limitations
- Put UE editor headers in AgentBridgeServer (gRPC header conflicts)
- Use `git push --force` or destructive git commands
- Commit files without `git status` and `git diff` first

### Always Do

- Close editor before builds OR use Live Coding (Ctrl+Alt+F11)
- Use `WITH_EDITOR` for editor-only code, `GIsEditor` for runtime branching
- Use `TWeakObjectPtr` for stored UObject references
- Bounce UObject operations to game thread from async contexts

### Header Conflict Warning

**AgentBridgeServer cannot include certain UE headers** due to Windows SDK conflicts with gRPC:
- `AssetRegistry/AssetRegistryModule.h`
- `Editor.h`, `LevelEditor.h`
- `IImageWrapper.h`

Put such functionality in AgentBridgeScripting/CommandExecutor.cpp instead.

## Build Commands

```bash
# Full build (~1 min)
cd D:/tempo/TempoSample/Scripts && ./Build.sh

# Or direct UBT
"D:/EL_UE/UE_5.6/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" \
  TempoSampleEditor Win64 Development \
  -Project="D:/tempo/TempoSample/TempoSample.uproject" -WaitMutex

# Live Coding (editor running)
Ctrl+Alt+F11
```

## Running the Editor

```bash
# Start editor (headless, runs in background)
cd D:/tempo/TempoSample && ./Plugins/Tempo/Scripts/Run.sh

# Wait ~30 seconds for gRPC server to be ready on port 10001

# Force-quit editor (IMPORTANT: use cmd wrapper in Git Bash)
cmd //c "taskkill /F /IM UnrealEditor-Cmd.exe"
```

**Important Gotchas:**

| Issue | Solution |
|-------|----------|
| Git Bash interprets `/F` as path | Use `cmd //c "taskkill /F ..."` wrapper |
| `tempo_quit` may hang on save dialog | Use `taskkill /F` for guaranteed termination |
| gRPC not ready immediately | Wait ~30 seconds after Run.sh starts |
| Editor already running | Kill first, then rebuild, then run |

## Testing

```bash
# Use TempoEnv Python (required for grpcio/protobuf)
cd D:/tempo/TempoSample/Plugins/AgentBridge/Python

# gRPC tests (port 10001)
D:/tempo/TempoSample/TempoEnv/Scripts/python.exe test_grpc.py

# HTTP tests (port 8080)
D:/tempo/TempoSample/TempoEnv/Scripts/python.exe test_client.py
```

## Key Paths

| Purpose | Path |
|---------|------|
| Project Root | `D:/tempo/TempoSample` |
| Plugin Root | `D:/tempo/TempoSample/Plugins/AgentBridge` |
| Engine | `D:/EL_UE/UE_5.6` |
| Project Logs | `D:/tempo/TempoSample/Saved/Logs/TempoSample.log` |
| Tempo Plugin | `D:/tempo/TempoSample/Plugins/Tempo` |
| bp_toolkit | `D:/tempo/TempoSample/Plugins/AgentBridge/bp_toolkit` (submodule) |
| bp_toolkit repo | `D:/repos/bp_toolkit.git` (local bare repo) |
| UAssetGUI.exe | `bp_toolkit/vendor/UAssetGUI/UAssetGUI/bin/Release/net8.0-windows/UAssetGUI.exe` |

## Session Continuity

See `.claude/HANDOVER.md` for current session context and next steps. Update FREQUENTLY!

## Documentation Process

When fixing bugs or adding features, documentation is a multi-step process:

| Step | What to Update | Why |
|------|---------------|-----|
| 1. Code docs | Comments in C++/Python code | For developers |
| 2. Module CLAUDE.md | Per-module context files | For AI continuity |
| 3. Help text | `_get_help_text()` in `agentbridge.py` | For agents using the tools |
| 4. Tool descriptions | MCP tool `description` fields | Agents see these first |
| 5. Handover | `.claude/HANDOVER.md` | For session continuity |

**Help text is critical** - it's what agents see when they call `help()`. If a limitation is
fixed, remove any warnings. If new capabilities are added, document them with examples.

Help topics in `agentbridge.py`:
- `actors` - Finding, creating, modifying actors
- `properties` - Reading/writing properties with paths
- `classes` - Type discovery
- `console` - Console commands
- `workflows` - Common multi-step operations

---

## Optional Submodules

### bp_toolkit - Blueprint/Asset Parsing Toolkit

A Python toolkit for parsing Unreal Engine assets exported to JSON. Useful for understanding
Blueprint logic, analyzing asset dependencies, and documenting complex Blueprints.

**Location:** `bp_toolkit/` (submodule from `D:\repos\bp_toolkit.git`)

**MCP Integration:** When bp_toolkit is present, the MCP server automatically exposes 14 additional
tools (`bp_export_asset`, `bp_import_asset`, `bp_detect_type`, `bp_get_property`, etc.) for offline
asset manipulation. No Unreal connectivity required - these tools work directly on JSON exports.

#### Setup

```bash
# Initialize submodules (includes UAssetGUI with UAssetAPI)
cd D:/tempo/TempoSample/Plugins/AgentBridge
git submodule update --init --recursive

# Build UAssetGUI (.NET 8+ required)
cd bp_toolkit/vendor/UAssetGUI && dotnet build -c Release
```

#### Capabilities

| Tool | Purpose |
|------|---------|
| `asset_parser.py` | **Main tool** - Multi-asset parser with query modes |
| `bp_parser.py` | Blueprint-specific deep parsing with call graphs |
| `bp_export.py` | UAsset ↔ JSON conversion wrapper for UAssetGUI |
| `bp_batch.py` | Batch processing multiple assets |

#### Supported Asset Types

- **Blueprint** - Full K2Node extraction, call graphs, Mermaid diagrams
- **Animation Blueprint** - State machines, anim nodes, blend spaces
- **Behavior Tree** - Tree hierarchy, ASCII visualization
- **PCG Graph** - Node connections, data flow diagrams
- **Material** - Expression flow, texture/parameter extraction
- **MetaSound** - Audio routing, wave asset references
- **Niagara** - Emitter hierarchy, module stages

#### Quick Examples

```bash
cd D:/tempo/TempoSample/Plugins/AgentBridge/bp_toolkit

# Export a uasset to JSON
python bp_export.py "D:/tempo/TempoSample/Content/SomeBlueprint.uasset"

# Detect asset type
python asset_parser.py SomeBlueprint.json --detect

# List Blueprint events
python asset_parser.py BP_Character.json --list-events

# List Behavior Tree tasks
python asset_parser.py BT_EnemyAI.json --list-tasks

# Search for patterns
python asset_parser.py AnyAsset.json --find "velocity"

# Full parse with output directory
python asset_parser.py BP_Pawn.json parsed_output/

# Comment-node visualization (6 formats)
python asset_parser.py BP_Pawn.json --flow-tagged
python asset_parser.py BP_Pawn.json --flow-boxes
python asset_parser.py BP_Pawn.json --flow-all
```

#### Query Modes (No File Output)

Fast lookups without generating parsed directories:

| Flag | Asset Types | Description |
|------|-------------|-------------|
| `--find <pattern>` | All | Search namemap and exports |
| `--list-events` | Blueprint | Event nodes (BeginPlay, Tick, etc.) |
| `--list-functions` | Blueprint | User-defined functions |
| `--variables` | Blueprint | Variable Get/Set with names |
| `--comments` | Blueprint | Extract comment node text |
| `--flow-tagged` | Blueprint | Tree view with comment tags |
| `--list-tasks` | Behavior Tree | Task node types |
| `--blackboard` | Behavior Tree | Blackboard key references |
| `--list-nodes` | PCG Graph | Node types with counts |
| `--connections` | PCG Graph | Node-to-node connections |
| `--textures` | Material | Texture sample references |
| `--emitters` | Niagara | Emitter list (for Systems) |

#### Submodule Structure

```
bp_toolkit/
├── asset_parser.py      # v3.3.0 - Main multi-asset parser
├── bp_parser.py         # v2.0.0 - Blueprint-specific deep parser
├── bp_export.py         # v1.2.0 - UAssetGUI wrapper
├── bp_batch.py          # v1.0.0 - Batch processor
├── README.md            # Full documentation
├── CLAUDE_SKILL.md      # Claude Code skill definitions
├── MCP_SERVER.md        # MCP server implementation guide
└── vendor/
    └── UAssetGUI/       # Submodule (GitHub: atenfyr/UAssetGUI)
        ├── UAssetAPI/   # Nested submodule
        └── UAssetGUI/bin/Release/net8.0-windows/
            └── UAssetGUI.exe  # Built binary
```

#### Notes

- **Build artifacts are gitignored** - Binary exists locally but not in repo
- **UAssetGUI requires .NET 8+** - Build once after submodule init
- **JSON files can be large** - 40-100MB for complex Blueprints, gitignored by default
- **Local bare repo** at `D:\repos\bp_toolkit.git` - Push changes there

---

*38 RPCs, 90 MCP Tools, Self-Documenting Help System, bp_toolkit Asset Parsing*
