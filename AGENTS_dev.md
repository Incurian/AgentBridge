# AGENTS_dev.md - AgentBridge Development Guide

> For AI agents and developers implementing or modifying AgentBridge itself.
> This file is for programming work. If you are using tools in an Unreal project, use `AGENTS_use.md`.

---

## 1) Scope and Intent

Use this guide when you are:
- Adding or changing C++ plugin behavior
- Adding or changing gRPC RPCs and protobuf contracts
- Adding or changing MCP tools in Python
- Debugging architecture, build, registration, or conversion issues

Core principle:
- User-facing tools should "just work." Keep complexity in lower layers (Core/Runtime/Scripting), not in the API surface.

---

## 2) Architecture You Must Respect

AgentBridge is split into 4 standalone UE plugins plus Python MCP:

1. `AgentBridgeCore` - reflection primitives (properties, paths, type discovery, function invoke)
2. `AgentBridgeRuntime` - world context, actor ops, target resolution, world partition
3. `AgentBridgeScripting` - command structs + `CommandExecutor` (business logic)
4. `AgentBridgeServer` - gRPC/HTTP transport (thin handlers only)
5. `mcp/` - Python MCP server and clients

Layering rule:
- **Server is transport only.**
- **Scripting owns behavior.**
- **Runtime/Core own reusable engine-level operations.**

Critical server restriction:
- `AgentBridgeServer` must not include headers that conflict with gRPC/Windows SDK (Editor/AssetRegistry/ImageWrapper/etc.).
- If a feature needs those headers, implement logic in `AgentBridgeScripting/Private/CommandExecutor.cpp` and keep server handlers thin.

---

## 3) Key File Map

- Proto contract:
  - `AgentBridgeServer/Source/AgentBridgeServer/Public/AgentBridge.proto`
- gRPC handlers:
  - `AgentBridgeServer/Source/AgentBridgeServer/Public/AgentBridgeServiceSubsystem.h`
  - `AgentBridgeServer/Source/AgentBridgeServer/Private/AgentBridgeServiceSubsystem.cpp`
- Command types and implementation:
  - `AgentBridgeScripting/Source/AgentBridgeScripting/Public/AgentCommands.h`
  - `AgentBridgeScripting/Source/AgentBridgeScripting/Public/CommandExecutor.h`
  - `AgentBridgeScripting/Source/AgentBridgeScripting/Private/CommandExecutor.cpp`
- Runtime support:
  - `AgentBridgeRuntime/Source/AgentBridgeRuntime/Private/*.cpp`
- Reflection/property internals:
  - `AgentBridgeCore/Source/AgentBridgeCore/Private/*.cpp`
- MCP Python:
  - `mcp/services/agentbridge.py`
  - `mcp/services/__init__.py`
  - `mcp/server.py`

---

## 4) Development Workflow

### Build/Run

```bash
# Kill editor first for full builds (Windows from Git Bash)
cmd //c "taskkill /F /IM UnrealEditor.exe"

# Build (from project root)
cd <PROJECT_ROOT>/Scripts && ./Build.sh

# Run editor
cd <PROJECT_ROOT> && ./Plugins/Tempo/Scripts/Run.sh
```

Notes:
- Wait ~30 seconds for gRPC readiness on port `10001`.
- `tempo_quit` can hang on save dialogs; use `taskkill /F` when needed.
- Live Coding (`Ctrl+Alt+F11`) is acceptable for small local changes.

### Testing

Use both:
1. Automated tests (Python side)
2. Live editor validation (required for behavior changes)

```bash
cd <PROJECT_ROOT>/Plugins/AgentBridge
python -m mcp.tests.test_grpc
python -m mcp.tests.test_client
```

Live testing is mandatory for changes touching:
- tool behavior
- property set/get
- actor operations
- gRPC handlers
- asset operations

---

## 5) Adding a New gRPC + MCP Tool (Canonical Checklist)

Complete all steps or tools may compile but fail/hang at runtime.

1. Add RPC + messages to `AgentBridge.proto`.
2. Regenerate protos (build usually triggers this).
3. Add handler declaration in `AgentBridgeServiceSubsystem.h`.
4. Implement thin handler in `AgentBridgeServiceSubsystem.cpp`.
5. **Register handler in `RegisterScriptingServices()`**.
6. Add command/response structs in `AgentCommands.h`.
7. Add `Execute(...)` declaration in `CommandExecutor.h`.
8. Implement behavior in `CommandExecutor.cpp`.
9. Add Python client method and tool schema in `mcp/services/agentbridge.py`.
10. Register module/tool exposure in `mcp/services/__init__.py` if needed.
11. Rebuild, restart editor, and live-test the exact tool path.

Common miss:
- Step 5. Missing registration returns runtime `unimplemented`.

---

## 6) Critical Implementation Rules

### 6.1 Property path/write semantics

- For nested path writes in Core, use direct-value-pointer semantics correctly:
  - `WriteProperty(...)` when you have container + property.
  - `WritePropertyDirect(...)` when path resolution already gave final `ValuePtr`.
- Do not double-apply `ContainerPtrToValuePtr`.

### 6.2 Array/map traversal

- Arrays: element pointer becomes container for inner field traversal.
- Maps: iterate with `GetMaxIndex()` and guard with `IsValidIndex()` (sparse storage).

### 6.3 World context correctness

- `GIsEditor` is true during PIE; use `WorldType` (`Editor`, `PIE`, `Game`) for behavior.

### 6.4 Tempo rotation proto gotcha

- Tempo proto uses `r/p/y` for roll/pitch/yaw fields, not long names.

### 6.5 Line endings and encoding

- Keep source/proto files ASCII and LF.
- CRLF in `.proto` can silently break regeneration in current pipeline.
- After edits on Windows mounts, verify and normalize if needed.

---

## 7) Documentation Contract for Code Changes

When behavior changes, update all relevant docs in same work unit:

1. code comments (if needed)
2. module `CLAUDE.md`
3. MCP help text (`help()` topics in `agentbridge.py`)
4. tool descriptions/schemas
5. user docs (`README.md`, `AGENTS_use.md`)

If a limitation is fixed, remove stale warnings from user docs.
If a workaround is still required, document it clearly and early.

---

## 8) Known Limitations You Must Design Around

- `set_property` on BoxExtent does not reliably update visual bounds; users should size PCG volumes via `set_transform` scale.
- `duplicate_asset` is safe under `/Game/`; plugin/engine content duplication has edge cases.
- `call_function` argument support is limited in current gRPC path.
- `spawn_actor` with `relative_to` has known backend issues.
- `TSoftObjectPtr` assignment is not reliable through current reflection write path.

Do not hide these issues. Surface clear errors or guidance.

---

## 9) Collaboration and Git Hygiene

- Do not revert unrelated user changes.
- Stage only files touched for the active change.
- Use small, phase-based commits.
- Keep one focused branch per feature/plan when working outside direct user sessions.
- Never use destructive git commands unless explicitly requested.

---

## 10) When to Use the Other Guide

Use `AGENTS_use.md` when you are:
- operating existing AgentBridge tools,
- building levels/assets through MCP,
- troubleshooting agent usage, not code internals.

---

## 11) Module READMEs (Further Reading)

Each major module has its own developer-facing README. Use these first when working in that area:

- Core reflection internals:
  - `AgentBridgeCore/README.md`
- World/actor/runtime behavior:
  - `AgentBridgeRuntime/README.md`
- Command layer and business logic:
  - `AgentBridgeScripting/README.md`
- Network/gRPC/HTTP transport:
  - `AgentBridgeServer/README.md`
- Python MCP server and tool registration:
  - `mcp/README.md`
- Optional offline Blueprint/asset toolkit:
  - `bp_toolkit/README.md`

Recommended doc order for deep implementation work:
1. module `README.md`
2. module `CLAUDE.md`
3. `docs/architecture/*.md`
