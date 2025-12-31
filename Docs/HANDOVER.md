# AgentBridge Handover Document

> Session handover for Claude Code continuity.
> Last Updated: December 31, 2025

---

## Project Summary

**AgentBridge** is a UE 5.6 plugin that exposes Unreal Editor/runtime state to external AI agents via gRPC + MCP. It allows Claude (and other LLMs) to manipulate actors, properties, materials, and more through natural language.

**Status: All 5 Phases Complete**
- Phase 1: Core Implementation (reflection, actor ops, console commands)
- Phase 2: Tempo Integration (gRPC via TempoScripting)
- Phase 3: MCP Integration (12 services, 72 tools)
- Phase 4: PIE/Runtime Support (context-aware capabilities)
- Phase 5: World Partition & Landscape Streaming (streaming-aware queries)

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
| agentbridge | 11 | `list_worlds`, `spawn_actor`, `query_actors`, `set_property` |
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

**Total: 12 services, 64 tools**

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
