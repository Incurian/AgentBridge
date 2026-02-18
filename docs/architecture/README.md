# AgentBridge Architecture

Complete class diagrams and architectural documentation for the AgentBridge plugin system.

## Documents

| File | Contents |
|------|----------|
| [OVERVIEW.md](OVERVIEW.md) | Unified class diagram, data flow, and layer summary |
| [CORE.md](CORE.md) | AgentBridgeCore - reflection primitives |
| [RUNTIME.md](RUNTIME.md) | AgentBridgeRuntime - world operations |
| [SCRIPTING.md](SCRIPTING.md) | AgentBridgeScripting - command dispatch |
| [SERVER.md](SERVER.md) | AgentBridgeServer - gRPC/HTTP network layer |
| [MCP.md](MCP.md) | Python MCP server - external agent interface |
| [BP_TOOLKIT.md](BP_TOOLKIT.md) | bp_toolkit + UAssetGUI/UAssetAPI - offline asset manipulation |

## Layer Dependency Map

```
External AI Agent (Claude, etc.)
       |
  [ MCP.md ] ............... Python MCP Server (JSON-RPC over stdio)
       |                          |
       |  gRPC/HTTP          Offline (Python-only)
       |                          |
  [ SERVER.md ] ............ [ BP_TOOLKIT.md ]
       |                          |
  [ SCRIPTING.md ] ......... AssetModifier / AssetParser
       |                          |
  [ RUNTIME.md ] ........... UAssetGUI.exe (.NET 8)
       |                          |
  [ CORE.md ] .............. UAssetAPI (C# library)
       |                          |
  UE Reflection System       .uasset files on disk
```

## Quick Reference

- **~100 MCP tools** total across all services
- **4 C++ plugins**: Core, Runtime, Scripting, Server
- **3 execution paths**: gRPC (port 10001), HTTP (port 8080), offline (Python-only)
- **51 gRPC RPCs** registered in AgentBridgeServiceSubsystem
- **116+ command types** in EAgentCommandType enum
- **60+ property types** handled by both FPropertyAccessor (live) and UAssetAPI (offline)

---

*Generated 2026-02-18 from codebase analysis.*
