# AgentBridge Plugin

> UE 5.6 plugin exposing editor/runtime state to external AI agents via gRPC + MCP.
> Primary use case: "Build me a level" - agents need full read/write/discover capabilities.

## Core Design Philosophy

**THE MOST IMPORTANT PRINCIPLE:**

> **Users and agents should not need to know implementation details. Tools should just work.**

When a tool has multiple ways to accomplish something, it should figure out the right approach
under the hood. The complexity lives in the lower modules; the API surface stays simple.

**THE SECOND MOST IMPORTANT PRINCIPLE:**

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
+-------------------------------------+
|  MCP Tools (Python)                 |  <- Simple API, smart defaults
|  - Auto-detect value types          |
|  - Normalize class names            |
|  - Provide helpful error messages   |
+-------------------------------------+
|  CommandExecutor (Scripting)        |  <- Route to correct handler
|  - Dispatch based on input format   |
|  - Validate and transform inputs    |
+-------------------------------------+
|  Core/Runtime                       |  <- Handle edge cases
|  - Multiple resolution strategies   |
|  - Fallback paths for failures      |
|  - Type coercion and conversion     |
+-------------------------------------+
```

**When adding features:** Put the intelligence in lower modules. The user-facing API should
be minimal and obvious. If something "should just work," make it work automatically.

## Quick Reference

| What | Where |
|------|-------|
| GitHub (private) | https://github.com/Incurian/AgentBridge |
| gRPC Port | 10001 (via Tempo) |
| HTTP Port | 8080 (fallback) |
| Python Env | `D:/tempo/TempoSample/TempoEnv/Scripts/python.exe` |
| Build Script | `D:/tempo/TempoSample/Scripts/Build.sh` |
| Run Editor (GUI) | `cd D:/tempo/TempoSample && ./Plugins/Tempo/Scripts/Run.sh` |
| Kill Editor | `cmd //c "taskkill /F /IM UnrealEditor.exe"` |
| User Docs | `README.md` |
| **Testing** | |
| Test Level | `Content/freshtest/FreshMap_1` |
| Generated Content | `Content/freshtest/CreatedThings/` |
| Trash/Experiments | `Content/freshtest/Trash/` |
| Test Results Log | `Content/freshtest/TEST_RESULTS.md` |
| bp_toolkit Assets | `D:/tempo/uassets/` |

## Current Status

| Capability | Status | Notes |
|------------|--------|-------|
| Actor operations | **WORKING** | Query, spawn, delete, transform, attach |
| Property paths | **WORKING** | GET/SET with component paths (`LightComponent0.Intensity`) |
| Nested structs | **WORKING** | GET/SET with nested paths (`RootComponent.RelativeLocation`) |
| Type discovery | **WORKING** | List classes, get schemas, BP normalization |
| Asset operations | **WORKING** | Create, save, duplicate assets |
| World Partition | **WORKING** | Query streaming actors, landscape bounds |
| Console commands | **WORKING** | Execute and search 5000+ commands |
| bp_toolkit (offline) | **WORKING** | 14 tools for JSON asset manipulation |

**Tool Count:** 104 MCP tools across 13 services (90 core + 14 bp_toolkit when present)

## Architecture

```
External Agents (Claude, LLMs)
         |
         v
MCP Server (Python) --- 104 tools across 13 services
         |
         v
gRPC (port 10001) / HTTP (port 8080)
         |
         v
+-------------------------------------------------+
| AgentBridgeServer   | gRPC handlers, HTTP API  |
| AgentBridgeScripting| Commands, JSON dispatch  |
| AgentBridgeRuntime  | World ops, property paths|
| AgentBridgeCore     | Reflection primitives    |
+-------------------------------------------------+
         |
         v
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
| bp_toolkit | UAsset parsing, Blueprint modification (optional) | `bp_toolkit/CLAUDE.md` |

**User-Facing Documentation:** `README.md` - comprehensive guide with tool reference.

## Critical Rules

### Never Do

- **Delete files or folders without asking first** - always ask for confirmation
- Modify Tempo plugin or UE Engine source - work around limitations
- Put UE editor headers in AgentBridgeServer (gRPC header conflicts)
- Use `git push --force` or destructive git commands
- Commit files without `git status` and `git diff` first

### Always Do

- Close editor before builds OR use Live Coding (Ctrl+Alt+F11)
- Use `WITH_EDITOR` for editor-only code, `GIsEditor` for runtime branching
- Use `TWeakObjectPtr` for stored UObject references
- Bounce UObject operations to game thread from async contexts

### Centaur Testing Protocol

When doing human-AI collaborative testing with visual verification:
- **Wait for human verification before cleanup** - don't delete test actors until user confirms they saw the expected result
- Record unexpected behaviors in `TEST_RESULTS.md` immediately, even if test eventually passes
- Explain what the human should look for before running each test
- **Save the level before killing the editor** - otherwise recovery dialog blocks next launch
  - Either save via Ctrl+S or delete test actors before exit
  - Or use `taskkill /F` which skips save prompts (data loss is OK for test actors)

### Header Conflict Warning

**AgentBridgeServer cannot include certain UE headers** due to Windows SDK conflicts with gRPC:
- `AssetRegistry/AssetRegistryModule.h`
- `Editor.h`, `LevelEditor.h`
- `IImageWrapper.h`

Put such functionality in AgentBridgeScripting/CommandExecutor.cpp instead.

## Build Commands

**IMPORTANT: Kill the editor before building!** DLLs will be locked and build will fail.

```bash
# STEP 1: Kill editor first (REQUIRED for full builds)
cmd //c "taskkill /F /IM UnrealEditor.exe"

# STEP 2: Full build (~1 min)
cd D:/tempo/TempoSample/Scripts && ./Build.sh

# Or direct UBT
"D:/EL_UE/UE_5.6/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" \
  TempoSampleEditor Win64 Development \
  -Project="D:/tempo/TempoSample/TempoSample.uproject" -WaitMutex

# ALTERNATIVE: Live Coding (editor stays running, for small changes only)
Ctrl+Alt+F11
```

## Running the Editor

**Two editor modes:**

| Executable | Mode | When to Use |
|------------|------|-------------|
| `UnrealEditor.exe` | **Full GUI** | Interactive work, visual testing, Run.sh uses this |
| `UnrealEditor-Cmd.exe` | **Headless** | Automation, CI, ExecCmds scripts |

```bash
# Start editor with full GUI (Run.sh uses UnrealEditor.exe)
cd D:/tempo/TempoSample && ./Plugins/Tempo/Scripts/Run.sh

# Wait ~30 seconds for gRPC server to be ready on port 10001

# Force-quit GUI editor (IMPORTANT: use cmd wrapper in Git Bash)
cmd //c "taskkill /F /IM UnrealEditor.exe"

# Headless mode for automation (separate executable, not Run.sh)
"D:/EL_UE/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "D:/tempo/TempoSample/TempoSample.uproject" \
  -ExecCmds="AgentBridge.ListWorlds,Quit" -unattended -NullRHI -nosplash
```

**Important Gotchas:**

| Issue | Solution |
|-------|----------|
| Git Bash interprets `/F` as path | Use `cmd //c "taskkill /F ..."` wrapper |
| `tempo_quit` may hang on save dialog | Use `taskkill /F` for guaranteed termination |
| gRPC not ready immediately | Wait ~30 seconds after Run.sh starts |
| Editor already running | Kill first, then rebuild, then run |
| Wrong taskkill target | GUI = `UnrealEditor.exe`, Headless = `UnrealEditor-Cmd.exe` |

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
| bp_toolkit GitHub | https://github.com/Incurian/BP_Toolkit |
| bp_toolkit local | `D:/repos/bp_toolkit.git` (backup bare repo) |
| UAssetGUI.exe | `bp_toolkit/vendor/UAssetGUI/UAssetGUI/bin/Release/net8.0-windows/UAssetGUI.exe` |

## Documentation Process

When fixing bugs or adding features, documentation is a multi-step process:

| Step | What to Update | Why |
|------|---------------|-----|
| 1. Code docs | Comments in C++/Python code | For developers |
| 2. Module CLAUDE.md | Per-module context files | For AI continuity |
| 3. Help text | `_get_help_text()` in `agentbridge.py` | For agents using the tools |
| 4. Tool descriptions | MCP tool `description` fields | Agents see these first |
| 5. README.md | User-facing documentation | For humans reading the repo |

**Help text is critical** - it's what agents see when they call `help()`. If a limitation is
fixed, remove any warnings. If new capabilities are added, document them with examples.

Help topics in `agentbridge.py`:
- `actors` - Finding, creating, modifying actors
- `properties` - Reading/writing properties with paths
- `classes` - Type discovery
- `console` - Console commands
- `workflows` - Common multi-step operations
- `bp_toolkit` - Offline asset manipulation (when submodule present)

---

## Optional Submodules

### bp_toolkit - Blueprint/Asset Toolkit

A Python toolkit for parsing, modifying, and creating Unreal Engine assets via JSON manipulation.
Works offline without Unreal running.

**Location:** `bp_toolkit/` (submodule from https://github.com/Incurian/BP_Toolkit)

**MCP Integration:** When bp_toolkit is present, the MCP server automatically exposes 14 additional
tools for offline asset manipulation. Use `help(topic="bp_toolkit")` for tool reference.

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
| `bp_builder.py` | **Asset modification** - Add comments, clone nodes, property paths |
| `asset_parser.py` | **Multi-asset parser** - Query modes for all asset types |
| `bp_parser.py` | Blueprint-specific deep parsing with call graphs |
| `bp_export.py` | UAsset to JSON conversion wrapper for UAssetGUI |
| `bp_batch.py` | Batch processing multiple assets |

#### Asset Type Support

| Asset Type | Parse | Modify | Round-Trip |
|------------|-------|--------|------------|
| Blueprint | YES | Comments, cloning | YES |
| PCG Graph | YES | Full modification | YES |
| DataAsset | YES | Property paths | YES |
| Animation BP | YES | - | - |
| Behavior Tree | YES | - | - |
| Material | YES | - | - |
| MetaSound | YES | - | - |
| Niagara | YES | - | - |

#### MCP Tools (14 tools)

| Tool | Description |
|------|-------------|
| `bp_export_asset` | Export .uasset to JSON |
| `bp_import_asset` | Import JSON to .uasset |
| `bp_detect_type` | Detect asset type |
| `bp_get_info` | Get asset summary |
| `bp_list_properties` | List properties with types |
| `bp_get_property` | Get property by path |
| `bp_set_property` | Set property by path |
| `bp_clone_asset` | Clone asset with new name |
| `bp_list_graphs` | List graphs in asset |
| `bp_add_comment` | Add comment node |
| `bp_clone_node` | Clone existing node |
| `bp_find` | Search namemap/exports |
| `bp_query` | Type-specific queries |
| `bp_parse` | Full Blueprint parsing |

#### Notes

- **Build artifacts are gitignored** - Binary exists locally but not in repo
- **UAssetGUI requires .NET 8+** - Build once after submodule init
- **JSON files can be large** - 40-100MB for complex Blueprints, gitignored by default
- **GitHub is primary** - `git push` goes to GitHub, `local` remote is backup

---

## Archived Documentation

Historical development notes are preserved in `.old.claude/` for reference:

| File | Contents |
|------|----------|
| `IMPROVEMENT_PLAN.md` | Detailed fix documentation for Sessions 22-25 (TArray, GET/SET, struct schema, element_type) |
| `RESEARCH.md` | UAssetAPI round-trip research, Blueprint/PCG creation approaches validated |
| `BP_JSON_WORKFLOW.md` | Step-by-step guide for exporting/parsing Blueprint JSON with UAssetAPI |
| `UASSET_DISSECTION_GUIDE.md` | Comprehensive AI agent guide for analyzing Blueprint assets |
| `HANDOVER.md` | Session-by-session development log with testing plans |

**Key findings preserved here:**

- **UAssetAPI round-trip validated**: Export→JSON→modify→reimport works for Blueprints, PCG Graphs, Behavior Trees
- **MetaDataMap workaround**: UE 5.7 Blueprints need MetaDataMap nulled before reimport (FName key issue)
- **All major bugs fixed**: TArray SET, GET returns empty, struct schema, element_type, asset path normalization

---

## Known Limitations

| Limitation | Status | Workaround |
|------------|--------|------------|
| `TSoftObjectPtr` assignment | Won't fix | Use `TObjectPtr` properties |
| gRPC header conflicts | By design | Business logic in Scripting module |
| FunctionInvoker struct returns | Auto-fixed | Redirected to property access |

---

*39 RPCs, 105 MCP Tools (90 + 14 bp_toolkit), Self-Documenting Help System*

---

## Recent Addition: call_asset_function (2026-01-02)

New tool for calling instance methods on UObject assets:

```python
# Works - parameterless functions
call_asset_function(
    asset_path='/Game/MyPCG.MyPCG',
    function_name='GetInputNode'
)

# Crashes - TSubclassOf<> parameters need FFunctionInvoker fix
call_asset_function(
    asset_path='/Game/MyPCG.MyPCG',
    function_name='AddNodeOfType',
    parameters={'InSettingsClass': '/Script/PCG.PCGSurfaceSamplerSettings'}
)
```

**Status:** Parameterless works, complex parameters need FFunctionInvoker enhancement for `FClassProperty` handling.
