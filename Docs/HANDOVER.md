# AgentBridge Handover Document

> Session handover for Claude Code continuity.
> Last Updated: December 31, 2025 (Session 11 - Wrapping up for MCP restart)

---

## Project Summary

**AgentBridge** is a UE 5.6 plugin that exposes Unreal Editor/runtime state to external AI agents via gRPC + MCP. It allows Claude (and other LLMs) to manipulate actors, properties, materials, and more through natural language.

**Status: All 5 Phases Complete + Wishlist Features**
- Phase 1: Core Implementation (reflection, actor ops, console commands)
- Phase 2: Tempo Integration (gRPC via TempoScripting)
- Phase 3: MCP Integration (12 services, 90 tools)
- Phase 4: PIE/Runtime Support (context-aware capabilities)
- Phase 5: World Partition & Landscape Streaming (streaming-aware queries)
- **Wishlist:** Asset creation, component manipulation, file operations

---

## MCP Prerequisites (CRITICAL)

### 1. Python Environment
**Must use TempoEnv Python, NOT system Python!**

| Requirement | Value |
|-------------|-------|
| Python executable | `D:/tempo/TempoSample/TempoEnv/Scripts/python.exe` |
| Python version | 3.11 |
| grpcio | 1.62.2 (pre-installed) |
| protobuf | 4.25.3 (pre-installed) |

**Why:** System Python doesn't have grpcio, and mixing Python versions causes `cygrpc` import errors.

### 2. Claude Code MCP Config
File: `~/.claude/settings.json`

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

### 3. Unreal Editor
- Must be running with TempoSample project
- gRPC server: **port 10001** (Tempo default, configurable in TempoCoreSettings)
- HTTP server (fallback) on port 8080

### 4. After Config Changes
**Restart Claude Code** for MCP settings to take effect.

---

## Quick Verification

After restart, test MCP is working:
1. `list_worlds` - Should show Editor world
2. `query_actors` with name_pattern="Light" - Should find lights
3. `spawn_actor` with class="PointLight", location=[0,0,500], label="MCP_Test"

### Manual Server Check
```bash
# Verify TempoEnv Python works
D:/tempo/TempoSample/TempoEnv/Scripts/python.exe -c "import grpc; print(grpc.__version__)"
# Should output: 1.62.2

# Test MCP module loads
cd D:/tempo/TempoSample/Plugins/AgentBridge/Python
PYTHONPATH="D:/tempo/TempoSample/Plugins/Tempo/TempoCore/Content/Python/API/tempo" \
  D:/tempo/TempoSample/TempoEnv/Scripts/python.exe -c \
  "from mcp.services import get_all_services; print(f'{len(get_all_services())} services loaded')"
# Should output: 12 services loaded
```

---

## Session Log

### Dec 31 (Session 9) - Wishlist Implementation Phase 1
**Feature:** Implemented 16 new commands for asset creation, component manipulation, and file operations.

**Why it matters:** This enables agents to create persistent content (DataAssets, MaterialInstances), manipulate component hierarchies, and work with project files - core capabilities for "build me a level" workflows.

**New Commands Implemented (C++ layer - CommandExecutor):**

| Category | Commands | Status |
|----------|----------|--------|
| Asset (P0) | `CreateAsset`, `SaveAsset`, `SaveActorAsBlueprint`, `DuplicateAsset`, `GetAssetThumbnail` | ✅ C++ done, stubs for BP/Thumbnail |
| Component (P1) | `GetComponentTransform`, `SetComponentTransform`, `AttachComponent`, `AttachActor`, `DetachComponent`, `DetachActor` | ✅ Fully implemented |
| File (P1) | `ReadProjectFile`, `WriteProjectFile`, `ListProjectDirectory`, `CopyProjectFile`, `DeleteProjectFile` | ✅ Fully implemented with security |

**Key Technical Decisions:**
1. All logic in `CommandExecutor.cpp` (Scripting layer) - avoids Windows SDK header conflicts documented in CLAUDE.md
2. `#if WITH_EDITOR` guards for asset creation/saving (editor-only APIs)
3. File operations constrained to project directory with path validation
4. `EAttachmentRuleType` enum for flexible attachment behavior

**Files Changed:**
- `Source/AgentBridgeScripting/Public/AgentCommands.h` - 16 new command/response structs
- `Source/AgentBridgeScripting/Public/CommandExecutor.h` - Execute() declarations + serializers
- `Source/AgentBridgeScripting/Private/CommandExecutor.cpp` - Full implementations (~1200 lines)

**Build Status:** ✅ Compiles successfully

**Completed in Session 10:**
1. ~~C++ implementation~~ (DONE in Session 9)
2. ~~Add to ExecuteJson dispatcher~~ (DONE - was already in Session 9)
3. ~~Add gRPC layer~~ (DONE - 16 new RPCs, proto messages, ServiceSubsystem handlers)
4. ~~Add MCP tools~~ (DONE - 11 new tools in agentbridge.py)

**Reference:** `Docs/WISHLIST_PLAN.md` has the full feature roadmap.

---

### Dec 31 (Session 10) - Wishlist Implementation Phase 2: gRPC & MCP
**Feature:** Added gRPC layer and MCP tools for the 16 new wishlist commands.

**Why it matters:** With the gRPC layer complete, agents can now use these commands via MCP to create DataAssets, manipulate component hierarchies, and work with project files through natural language.

**New gRPC RPCs (16 total):**
| Category | RPCs |
|----------|------|
| Asset (P0) | `CreateAsset`, `SaveAsset`, `SaveActorAsBlueprint`, `DuplicateAsset`, `GetAssetThumbnail` |
| Component (P1) | `GetComponentTransform`, `SetComponentTransform`, `AttachComponent`, `AttachActor`, `DetachComponent`, `DetachActor` |
| File (P1) | `ReadProjectFile`, `WriteProjectFile`, `ListProjectDirectory`, `CopyProjectFile`, `DeleteProjectFile` |

**New MCP Tools (11 exposed):**
| Tool | Description |
|------|-------------|
| `create_asset` | Create UAssets (DataAssets, MaterialInstances, etc.) |
| `save_asset` | Save modified UAssets to disk |
| `save_actor_as_blueprint` | Convert actor to reusable Blueprint |
| `duplicate_asset` | Copy assets with new names |
| `get_asset_thumbnail` | Get asset preview images (base64 PNG) |
| `get_component_transform` | Get component world/relative transforms |
| `set_component_transform` | Set component world/relative transforms |
| `read_project_file` | Read text/binary files from project dir |
| `write_project_file` | Write files to project dir |
| `list_project_directory` | List directory contents with metadata |
| `copy_project_file` | Copy files within project dir |

**Files Changed:**
- `Source/AgentBridgeServer/Public/AgentBridge.proto` - 16 new message types + RPCs
- `Source/AgentBridgeServer/Public/AgentBridgeServiceSubsystem.h` - Handler declarations
- `Source/AgentBridgeServer/Private/AgentBridgeServiceSubsystem.cpp` - Handler implementations (~400 lines)
- `Python/mcp/services/agentbridge.py` - 11 new tools, client methods, execute handlers

---

### Dec 31 (Session 11) - Autonomous Overnight Session
**Feature:** Completed MCP tool exposure and documentation updates.

**Autonomous mode activated** - Claude worked without user confirmation per CLAUDE.md instructions.

**Work Completed:**
1. **Added 5 missing MCP tools:**
   - `copy_project_file` - was in help text but not TOOLS list
   - `attach_component` - component hierarchy manipulation
   - `attach_actor` - actor parenting
   - `detach_component` - component detachment
   - `detach_actor` - actor detachment

2. **Added "components" help topic** - New help topic covering transforms, attachment, detachment

3. **Code review of CommandExecutor.cpp** - Verified security:
   - File operations properly sandboxed via `IsPathAllowed()` + `ToAbsoluteProjectPath()`
   - Blocks path traversal (`..`), sensitive directories, dangerous extensions
   - All operations have null checks and proper error messages

4. **Created `test_wishlist.py`** - Integration test script for new features

5. **Updated documentation:**
   - Fixed gRPC port from 50051 → 10001 in TestingStrategy.md
   - Added Phase 6 testing section for wishlist features
   - Updated tool counts (now 90 MCP tools, 38 RPCs)

**Tool Count Now:** agentbridge service has 37 tools (was 32)
**Total MCP Tools:** 90 (was 85)

**Files Changed:**
- `Python/mcp/services/agentbridge.py` - 5 new tools + "components" help topic
- `Python/test_wishlist.py` - NEW integration test script
- `CLAUDE.md` - Added autonomous mode section, updated counts
- `Docs/HANDOVER.md` - Session log
- `Docs/TestingStrategy.md` - Fixed ports, added Phase 6

**Build/Proto Generation:**
```bash
cd D:/tempo/TempoSample
./Plugins/Tempo/Scripts/Build.sh  # Regenerates protos + builds
```

**Key Technical Details:**
- `AttachmentRule` proto enum maps to `EAttachmentRuleType` C++ enum
- File operations use security validation (must be within project directory)
- Asset operations wrapped in `#if WITH_EDITOR` (editor-only APIs)
- Proto field names verified against C++ struct fields (e.g., `AssetPath` not `FilePath`)

---

### Session 12 TODO (MCP Access Available)

**Immediate Testing:**
1. Use MCP tools directly to test new features:
   - `list_worlds` - verify connection
   - `spawn_actor` + `attach_actor` - test attachment
   - `write_project_file` / `read_project_file` - test file ops
   - `create_asset` - test asset creation

2. Run test_wishlist.py if preferred:
   ```bash
   PYTHONPATH="D:/tempo/TempoSample/Plugins/Tempo/TempoCore/Content/Python/API/tempo" \
     D:/tempo/TempoSample/TempoEnv/Scripts/python.exe test_wishlist.py
   ```

**New Feature Ideas (after testing):**
- Blueprint graph editing (competitor parity)
- Niagara particle system control
- Sequencer cinematics control
- Improved error messages with suggestions
- Command logging/history system

---

### Dec 31 (Session 8) - Reflection Improvements
**Feature:** Fixed bugs and expanded reflection capabilities based on naive Claude testing.

**Why it matters:** A naive Claude couldn't complete the screenshot workflow because `get_actor` didn't return properties/components, `get_class_schema` returned empty arrays, and there was no way to call static Blueprint library functions.

**Bug Fixes:**
- `get_actor(include_properties=True)` now actually returns properties
- `get_actor(include_components=True)` now actually returns components
- `get_class_schema` now returns actual property/function data (was returning empty)

**New Capabilities:**
- `list_classes(base_class_name="ActorComponent")` - List component types
- `get_class_schema("SceneCaptureComponent2D")` - Works for ANY class
- `call_static_function` - Call Blueprint library functions (KismetRenderingLibrary, etc.)

**New MCP Tool:**
```python
# Call static Blueprint library functions
call_static_function("KismetSystemLibrary", "PrintString", {"InString": "Hello!"})
call_static_function("KismetMathLibrary", "Abs", {"A": -42})  # Returns {"return_value": 42}
```

**Files Changed:**
- `Python/mcp/services/agentbridge.py` - Fixed get_actor, added call_static_function, updated help
- `Source/AgentBridgeScripting/Public/AgentCommands.h` - Added FGetClassSchemaResponse
- `Source/AgentBridgeScripting/Private/CommandExecutor.cpp` - Implemented full schema extraction
- `Source/AgentBridgeServer/Private/AgentBridgeServiceSubsystem.cpp` - Updated gRPC handler

**Plan Document:** `Docs/REFLECTION_IMPROVEMENTS.md` - Full implementation plan with code snippets

---

### Dec 31 (Session 7) - Self-Documenting Help System
**Feature:** Added `help` MCP tool so AI agents can discover how to use AgentBridge without external documentation.

**Why it matters:** A naive Claude with MCP access but no documentation can now call `help()` to get oriented. Topics cover actors, properties, classes, console commands, and common workflows.

**New Tool:**
- `help` - Returns usage overview with topic-specific deep dives

**Usage:**
```python
# Get overview
result = client.help()

# Get specific topic
result = client.help(topic="workflows")  # or: actors, properties, classes, console
```

**Topics Available:**
| Topic | Coverage |
|-------|----------|
| (none) | Quick start, common classes, units, tips |
| `actors` | Finding, creating, modifying, identifying actors |
| `properties` | Reading/setting properties, property paths |
| `classes` | Class discovery, built-in vs Blueprint classes |
| `console` | Command discovery and execution |
| `workflows` | Step-by-step guides for common tasks |

**Files Changed:**
- `agentbridge.py` - Added `help` tool and `_get_help_text()` function

---

### Dec 31 (Session 6) - Console Command Discovery
**Feature:** Added `search_console_commands` tool for AI agents to discover available console commands and CVars.

**Why it matters:** Unreal has ~9000+ console commands/CVars. An agent without web access can now ask "how do I enable vsync?" → search "vsync" → find `r.VSync` with its current value and help text.

**New Components:**
| Component | Description |
|-----------|-------------|
| `AgentBridge.SearchCommands` | Console command for testing |
| `SearchConsoleCommands` RPC | gRPC RPC (22 total now) |
| `search_console_commands` | MCP tool (73 tools total) |

**Usage:**
```python
# Search for console commands by keyword (with pagination)
result = client.search_console_commands("fps", limit=5, offset=0, search_help=True)
print(f"Showing {len(result.commands)} of {result.total_matches} matches")
for cmd in result.commands:
    print(f"{cmd.name} = {cmd.current_value} ({cmd.value_type})")

# Get next page
if result.has_more:
    result = client.search_console_commands("fps", limit=5, offset=result.next_offset)
```

**Pagination:** Use `offset` to get next page, `total_matches` shows how many exist.

**Files Changed:**
- `AgentBridgeDebug.h/.cpp` - Added `Cmd_SearchCommands`
- `AgentBridge.proto` - Added `SearchConsoleCommands` RPC, `ConsoleCommandInfo` message
- `AgentBridgeServiceSubsystem.h/.cpp` - Added handler
- `agentbridge.py` - Added MCP tool

---

### Dec 31 (Session 5) - gRPC/MCP Integration & Console Command Passthrough
**Feature:** Full gRPC/MCP integration for World Partition APIs + console command passthrough with log capture.

**New gRPC RPCs (7 total):**
| RPC | Description |
|-----|-------------|
| `IsWorldPartitioned` | Check if world uses WP |
| `QueryAllActors` | Query actors including unloaded |
| `GetStreamingState` | Get actor streaming state by GUID |
| `QueryLandscape` | List landscape proxies |
| `GetDataLayers` | List data layers |
| `GetActorsInDataLayer` | Get actors in a data layer |
| `ExecuteConsoleCommand` | Run arbitrary console commands with log capture |

**New MCP Tools (7 total):**
- `is_world_partitioned`, `query_all_actors`, `get_streaming_state`
- `query_landscape`, `get_data_layers`, `get_actors_in_data_layer`
- `execute_console_command` - **Key feature**: Captures UE_LOG output!

**Console Command Log Capture:**
The `execute_console_command` tool captures ALL log output during command execution by:
1. Creating a custom `FLogCaptureOutputDevice` that implements `FOutputDevice::Serialize()`
2. Temporarily adding it to `GLog` before command execution
3. Using `GEngine->Exec()` for broadest command support
4. Output still goes to normal log AND is returned to caller

**Files Changed:**
- `AgentBridge.proto` - 7 new RPCs, new message types
- `AgentBridgeServiceSubsystem.h/.cpp` - Handler implementations
- `agentbridge.py` - 7 new MCP tools with execute handlers
- `WorldPartitionOps.h/.cpp` - Added Transform field to FStreamingActorReference

**Build Command (from project root):**
```bash
cd D:/tempo/TempoSample
source TempoEnv/Scripts/activate
./Plugins/Tempo/Scripts/Build.sh
```

---

### Dec 31 (Session 4) - World Partition & Landscape Streaming Support
**Feature:** Added support for querying actors in unloaded World Partition streaming cells and landscape streaming proxies.

**New Files:**
- `Source/AgentBridgeRuntime/Public/WorldPartitionOps.h` - Header with streaming-aware APIs
- `Source/AgentBridgeRuntime/Private/WorldPartitionOps.cpp` - Implementation

**New APIs:**
| Function | Description |
|----------|-------------|
| `IsWorldPartitioned()` | Check if world uses World Partition |
| `QueryAllActors()` | Query actors including unloaded (uses `FWorldPartitionHelpers::ForEachActorDescInstance`) |
| `GetActorStreamingState()` | Check if actor is Loaded, Unloaded, or Invalid |
| `FindActorByGuidEx()` | Get actor metadata even when unloaded |
| `QueryLandscapeProxies()` | List all landscape proxies including streaming |
| `LoadActor()` / `LoadRegion()` | Force-load actors/regions (editor only) |
| `DeleteActorWP()` | Delete actors with proper WP cleanup |
| `GetDataLayers()` / `GetActorsInDataLayer()` | Data layer queries |

**New Console Commands:**
- `AgentBridge.IsPartitioned` - Check if world uses WP
- `AgentBridge.QueryAllActors [Pattern] [Limit]` - Query including unloaded
- `AgentBridge.StreamingState <GUID>` - Get actor streaming state
- `AgentBridge.QueryLandscape` - List landscape proxies
- `AgentBridge.DataLayers` - List data layers

**Key Technical Details:**
- Uses `FWorldPartitionHelpers::ForEachActorDescInstance<T>()` to iterate all actors (loaded + unloaded)
- `FWorldPartitionActorDescInstance::GetActor()` returns nullptr if unloaded
- Added Landscape module dependency for `ALandscapeProxy`, `ALandscapeStreamingProxy`
- `FStreamingActorReference` extends `FActorReference` with streaming state, bounds, data layers

**Gotcha Documented:** Building while editor is open crashes it. Use Live Coding or close editor first.

**Build Command (from project root):**
```bash
cd D:/tempo/TempoSample
source TempoEnv/Scripts/activate   # Activate Python env (required for proto generation)
./Plugins/Tempo/Scripts/Build.sh   # Use Tempo's build script (portable across projects)
```

---

### Dec 31 (Session 3) - gRPC Port Fix
**Problem:** MCP service returned `UNIMPLEMENTED: unknown service AgentBridgeServer.AgentBridgeService`
**Investigation:**
- Port 50051 was in use by `endpointService.exe` (unrelated Windows service)
- Tempo gRPC server logs showed it starts on port 10001, not 50051
- Checked `TempoSample.log`: `Tempo gRPC server listening on 0.0.0.0:10001`

**Root Cause:** Tempo's gRPC port is configured in TempoCoreSettings (default: 10001), not 50051

**Fix:** Updated MCP config to use `--port 10001`

**How to find the correct port:**
```bash
# Check Unreal log for Tempo's gRPC port
grep "Tempo gRPC" D:/tempo/TempoSample/Saved/Logs/TempoSample.log
# Look for: Tempo gRPC server listening on 0.0.0.0:XXXXX
```

**Verification:**
```bash
# Test connection on correct port
PYTHONPATH="D:/tempo/TempoSample/Plugins/Tempo/TempoCore/Content/Python/API/tempo" \
  D:/tempo/TempoSample/TempoEnv/Scripts/python.exe -c \
  "from mcp.services.agentbridge import connect, execute; \
   print(execute(connect('localhost', 10001), 'list_worlds', {}))"
```

Files updated:
- `Python/mcp_config.json` - Changed port from 50051 to 10001
- `Docs/HANDOVER.md` - Added this session log, updated port in config examples

**Note:** User mentioned TempoSample map has heavy Tempo initialization on load. If default map is changed to a simpler one, commandline startup should be faster.

### Dec 31 (Session 2) - MCP Config Fix
**Problem:** MCP server failed to start with `cygrpc` import error
**Cause:** Config used `"command": "python"` (system Python 3.10) instead of TempoEnv Python 3.11
**Fix:** Updated `~/.claude/settings.json` to use `D:/tempo/TempoSample/TempoEnv/Scripts/python.exe`

Files updated:
- `~/.claude/settings.json` - Fixed MCP config
- `Python/mcp_config.json` - Updated example config
- `CLAUDE.md` - Added Python environment requirements section
- `Docs/HANDOVER.md` - Added prerequisites (this file)

### Dec 31 (Session 1) - Testing Strategy
- Created `Docs/TestingStrategy.md` - Comprehensive manual testing guide
- Initial MCP config (had wrong Python path)

---

## Key Files

| File | Purpose |
|------|---------|
| `CLAUDE.md` | Main project documentation |
| `Docs/TestingStrategy.md` | Testing guide |
| `Docs/StretchGoals.md` | Future features, research notes |
| `Python/test_grpc.py` | gRPC test script |
| `Python/test_client.py` | HTTP test script |
| `Python/mcp/services/` | MCP service modules (12 services) |
| `Python/mcp_config.json` | Example Claude Code config |

---

## MCP Tools Available

Once connected, you have access to these service categories:

| Service | Tools | Examples |
|---------|-------|----------|
| agentbridge | 37 | `help`, `list_worlds`, `spawn_actor`, `create_asset`, `attach_actor` |
| tempo_time | 6 | `play`, `pause`, `step_frame` |
| tempo_actor_control | 17 | `set_actor_location`, `set_actor_rotation` |
| tempo_core | 6 | `load_level`, `quit_editor` |
| tempo_core_editor | 6 | `start_pie`, `stop_pie`, `save_level` |
| tempo_geographic | 5 | `set_date_time`, `set_location` |
| tempo_movement | 5 | `set_throttle`, `set_steering` |
| tempo_world_state | 2 | `get_actor_velocity`, `get_actor_bounds` |
| tempo_labels | 1 | `get_label_mapping` |
| tempo_sensors | 1 | `list_sensors` |
| tempo_map_query | 3 | `query_lanes`, `query_zones` |
| tempo_agents_editor | 1 | `build_zone_graph` |

**Total: 12 services, 90 tools**

**Self-Documenting:** Call `help()` for an overview, or `help(topic='workflows')` for detailed guidance.

---

## Known Issues

1. **FunctionInvoker Return Values** - Struct returns are zeroed. Use property paths as workaround.
2. **Windows Edit Tool Bug** - Use sed/python for file edits if "unexpectedly modified" error occurs.
3. **Python Environment** - Must use TempoEnv Python (see prerequisites above).
4. **Building While Editor Open** - Running `Build.sh` while Unreal Editor is open will crash the editor. Close editor before building, or use Live Coding (Ctrl+Alt+F11) for hot reload.

---

## Build & Run Commands

### Building
```bash
cd D:/tempo/TempoSample
source TempoEnv/Scripts/activate   # Required - sets up Python for proto generation
./Plugins/Tempo/Scripts/Build.sh   # Portable across Tempo projects
```

### Running the Editor
```bash
cd D:/tempo/TempoSample
source TempoEnv/Scripts/activate   # Recommended
./Plugins/Tempo/Scripts/Run.sh     # Launches UnrealEditor-Cmd.exe
```

**Startup Time:** ~30 seconds to full load with gRPC server ready.

### Testing Python/MCP
```bash
cd D:/tempo/TempoSample/Plugins/AgentBridge/Python
PYTHONPATH="D:/tempo/TempoSample/Plugins/Tempo/TempoCore/Content/Python/API/tempo" \
  D:/tempo/TempoSample/TempoEnv/Scripts/python.exe -c "
import sys; sys.path.insert(0, '.')
from mcp.services.agentbridge import connect
client = connect('localhost', 10001)
result = client.execute_console_command('AgentBridge.ListWorlds')
print(result.output)
"
```

---

## Test Artifact Naming Convention

When testing, use these prefixes so artifacts are easy to find:
- `Console_Test*` - From console commands
- `HTTP_Test*` - From HTTP client
- `gRPC_Test*` - From gRPC client
- `MCP_Test*` - From MCP/Claude tools
- `PIE_Test*` - From PIE context tests

**Remember:** Artifacts are intentionally NOT auto-deleted. User wants to inspect them.

---

## Cleanup Command (When Ready)

```python
from agentbridge import AgentBridgeClient
client = AgentBridgeClient()
for name in ["Console_TestLight", "HTTP_TestLight", "gRPC_TestLight", "MCP_Test"]:
    try:
        client.delete_actor(name)
        print(f"Deleted: {name}")
    except:
        pass
```

---

*Ready for MCP testing - restart Claude Code first!*
