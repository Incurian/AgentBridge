# AgentBridge Plugin

> UE 5.6 plugin exposing editor/runtime state to external AI agents via gRPC + MCP.
> Primary use case: "Build me a level" - agents need full read/write/discover capabilities.

## Quick Start

| What | Where |
|------|-------|
| gRPC Port | 10001 (via Tempo) |
| HTTP Port | 8080 (fallback) |
| Python Env | `D:/tempo/TempoSample/TempoEnv/Scripts/python.exe` |
| Build Script | `D:/tempo/TempoSample/Scripts/Build.sh` |

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

## Session Continuity

See `.claude/HANDOVER.md` for current session context and next steps. Update FREQUENTLY!

---

*38 RPCs, 90 MCP Tools, Self-Documenting Help System*
