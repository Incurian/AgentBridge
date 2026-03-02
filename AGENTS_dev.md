# AGENTS_dev.md - AgentBridge Development Guide

> For AI agents and developers implementing or modifying AgentBridge itself.
> This file is for programming work. If you are using tools in an Unreal project, use `AGENTS_use.md`.

---

## 1) Scope and Intent

Use this guide when you are:
- adding or changing C++ plugin behavior
- adding or changing gRPC RPCs/protobuf contracts
- adding or changing MCP tools in Python
- debugging architecture, build, registration, or conversion behavior

Core principles:
- Tools should "just work" for users/agents without implementation knowledge.
- Push complexity downward (Core/Runtime/Scripting), not into API friction.
- Document every meaningful edge case and workaround as soon as discovered.

---

## 2) Architecture and Ownership

AgentBridge is split into 4 UE plugins plus Python MCP:

1. `AgentBridgeCore` - reflection primitives (`FProperty`, `UFunction`, type discovery)
2. `AgentBridgeRuntime` - world context, actor operations, target resolution, world partition
3. `AgentBridgeScripting` - command structs + `CommandExecutor` (business logic)
4. `AgentBridgeServer` - gRPC/HTTP transport (thin handlers only)
5. `mcp/` - Python MCP server, tool registry, clients

Layering rules:
- **Server is transport only.**
- **Scripting owns business logic.**
- **Runtime/Core own reusable engine-level behavior.**

### Critical server header restriction

`AgentBridgeServer` must not include headers that conflict with gRPC/Windows SDK, including:
- `AssetRegistry/AssetRegistryModule.h`
- `Editor.h`, `LevelEditor.h`
- `IImageWrapper.h`
- `ThumbnailRendering/ThumbnailManager.h`
- headers that transitively include `IoBuffer.h`

If a feature needs those headers, implement logic in `AgentBridgeScripting/Private/CommandExecutor.cpp` and keep server handlers thin.

---

## 3) Quick Reference

- gRPC port: `10001` (Tempo-backed)
- HTTP fallback port: `8080`
- preferred Python env: `<PROJECT_ROOT>/TempoEnv/Scripts/python.exe` (Windows) or `<PROJECT_ROOT>/TempoEnv/bin/python` (Linux/Mac)
- build script: `<PROJECT_ROOT>/Scripts/Build.sh`
- editor run: `cd <PROJECT_ROOT> && ./Plugins/Tempo/Scripts/Run.sh`
- force-kill editor on Windows/Git Bash: `cmd //c "taskkill /F /IM UnrealEditor.exe"`

Current broad status (high-level):
- actor/property/type/asset/world-partition/console flows are functional
- blueprint + PCG graph tool paths are functional
- optional bp_toolkit offline workflows are functional when submodule is available

---

## 4) Key File Map

- protobuf contract:
  - `AgentBridgeServer/Source/AgentBridgeServer/Public/AgentBridge.proto`
- gRPC handlers:
  - `AgentBridgeServer/Source/AgentBridgeServer/Public/AgentBridgeServiceSubsystem.h`
  - `AgentBridgeServer/Source/AgentBridgeServer/Private/AgentBridgeServiceSubsystem.cpp`
- commands and business logic:
  - `AgentBridgeScripting/Source/AgentBridgeScripting/Public/AgentCommands.h`
  - `AgentBridgeScripting/Source/AgentBridgeScripting/Public/CommandExecutor.h`
  - `AgentBridgeScripting/Source/AgentBridgeScripting/Private/CommandExecutor.cpp`
- runtime support:
  - `AgentBridgeRuntime/Source/AgentBridgeRuntime/Private/*.cpp`
- reflection/property internals:
  - `AgentBridgeCore/Source/AgentBridgeCore/Private/*.cpp`
- MCP Python:
  - `mcp/services/agentbridge.py`
  - `mcp/services/__init__.py`
  - `mcp/server.py`

---

## 5) Critical Rules

### Never do

- delete files/folders without explicit confirmation
- modify Tempo plugin or UE engine source to solve AgentBridge issues
- put business logic in `AgentBridgeServer`
- use destructive git commands (`reset --hard`, force push, etc.) unless explicitly requested
- commit without checking `git status` and `git diff`

### Always do

- close editor before full builds, or use Live Coding intentionally
- use `WITH_EDITOR` for editor-only compilation and world type checks for runtime behavior
- keep UObject operations on the game thread from async contexts
- use ASCII and LF line endings for edited source/proto files
- verify line endings after edits on Windows mounts (`/mnt/d/...`)

### Centaur testing protocol (human + agent)

- wait for human visual verification before cleanup
- state what human should verify before each visual test
- record unexpected behavior immediately
- save level before hard-kill to avoid recovery-dialog startup loops

---

## 6) Build, Run, and Test Workflow

### Build/Run

```bash
# Kill editor first for full builds (Windows/Git Bash)
cmd //c "taskkill /F /IM UnrealEditor.exe"

# Full build
cd <PROJECT_ROOT>/Scripts && ./Build.sh

# Start editor (GUI)
cd <PROJECT_ROOT> && ./Plugins/Tempo/Scripts/Run.sh
```

Notes:
- wait ~30 seconds for gRPC readiness on port `10001`
- `tempo_quit` may hang on save dialogs; `taskkill /F` is the reliable fallback
- Live Coding (`Ctrl+Alt+F11`) is acceptable for small local edits

### Editor modes

- `UnrealEditor.exe`: full GUI (interactive/live validation)
- `UnrealEditor-Cmd.exe`: headless automation/CI

### Testing

Use both:
1. automated tests
2. live editor validation (mandatory for behavior changes)

```bash
cd <PROJECT_ROOT>/Plugins/AgentBridge
python -m mcp.tests.test_grpc
python -m mcp.tests.test_client
```

Live testing is mandatory for changes touching:
- MCP tools or gRPC handlers
- property get/set semantics
- actor operations/transforms/attachment
- asset operations

---

## 7) Adding New gRPC + MCP Tool (Canonical Checklist)

Complete all steps:

1. add proto message + RPC in `AgentBridge.proto`
2. regenerate protos (build usually triggers this)
3. add handler declaration in `AgentBridgeServiceSubsystem.h`
4. implement thin handler in `AgentBridgeServiceSubsystem.cpp`
5. **register in `RegisterScriptingServices()`**
6. add command/response structs in `AgentCommands.h`
7. add `Execute(...)` declaration in `CommandExecutor.h`
8. implement behavior in `CommandExecutor.cpp`
9. add Python client method + tool schema in `mcp/services/agentbridge.py`
10. update modular registration in `mcp/services/__init__.py` if needed
11. rebuild, restart editor, and live-test the exact tool path end-to-end

Common failure:
- missing step 5 compiles cleanly but RPC is runtime `unimplemented`.

Tempo proto gotcha:
- `TempoScripting::Rotation` fields are `r/p/y` (roll/pitch/yaw), not long names.

---

## 8) Critical Implementation Details

### Property write semantics

- use `WriteProperty(...)` when you have container + property
- use `WritePropertyDirect(...)` when path resolution already returned final `ValuePtr`
- do not double-apply `ContainerPtrToValuePtr`

### Array/map traversal

- arrays: element pointer becomes container for inner traversal
- maps: iterate `GetMaxIndex()` and guard each index with `IsValidIndex()`

### World context correctness

- do not use `GIsEditor` to distinguish Editor vs PIE
- use `World->WorldType` (`Editor`, `PIE`, `Game`)

### Line endings and encoding

- keep edited source/proto files ASCII + LF
- CRLF in proto files can silently break generation flow
- check with `file <path>` and normalize with `sed -i 's/\r$//' <path>` when needed

---

## 9) Documentation Contract (Required)

When behavior changes, update docs in the same work unit:

1. code comments (only where needed)
2. module `CLAUDE.md`
3. MCP help text (`_get_help_text()` in `mcp/services/agentbridge.py`)
4. MCP tool descriptions/schemas
5. user docs (`README.md`, `AGENTS_use.md`, relevant `docs/...`)

Help topics to keep in sync:
- `actors`, `properties`, `classes`, `assets`, `components`, `console`, `workflows`, `pcg_volume`, `volume_sizing`, `bp_toolkit`

If a limitation is fixed, remove stale warnings from user docs.
If a workaround remains, document it clearly and near affected workflows.

---

## 10) Submodules and Integration Notes

### `mcp/` (Python MCP)

Setup:
```bash
cd <PROJECT_ROOT>/Plugins/AgentBridge
git submodule update --init --recursive
```

Run:
```bash
cd <PROJECT_ROOT>/Plugins/AgentBridge
python -m mcp --host localhost --port 10001
```

Optional explicit path:
```bash
TEMPO_API_PATH="<PROJECT_ROOT>/Plugins/Tempo/TempoCore/Content/Python/API/tempo" \
  python -m mcp --host localhost --port 10001
```

#### Claude Code + WSL notes

- MCP config belongs in `~/.claude.json` under `mcpServers`
- `cwd` must be `.../Plugins/AgentBridge` so `-m mcp` resolves correctly
- wrapper scripts are the most reliable way to enforce `cwd` and command shape
- editor must be running before MCP startup (`10001` reachable)
- restart Claude Code after config changes

### `bp_toolkit/` (optional)

- provides offline asset manipulation plus additional MCP tools when present
- initialize submodules recursively
- build UAssetGUI when needed:

```bash
cd bp_toolkit/vendor/UAssetGUI && dotnet build -c Release
```

---

## 11) Development SOPs

- plan first for non-trivial features (`docs/plans/...`)
- fail fast with explicit errors; avoid silent fallthroughs
- live-test behavior changes before commit
- document as part of the same implementation phase
- commit often (phase/logical unit granularity)
- avoid dead code and unrelated churn
- maintain green builds

### Version control SOP

- one branch per feature/plan when working in branch-based flow
- commit at safe checkpoints (after phase completion, before risky refactors)
- push feature branches frequently
- merge only after checklist completion, passing builds, and live validation

### Commit style

Prefer messages tied to plan/checklist units when applicable:

```text
feat(runtime): improve X behavior (PLAN-NAME P2.1)
```

---

## 12) Feature Planning SOP

For non-trivial work, create/update a plan in `docs/plans/<PLAN_NAME>.md`.

Planning phases:
1. intent
2. exploration
3. feasibility
4. scope
5. general plan
6. validation against current code patterns
7. detailed implementation
8. checklist
9. documentation updates

### Plan structure template

```markdown
# Plan: Feature Name

## Overview
## Scope
### Included in v1
### Deferred to Future
### Explicitly Excluded
## Design Decisions
## Architecture
## Implementation Details
## Testing Strategy
## Implementation Checklist
```

### Checklist format

```markdown
### Phase 1: Foundation
- [ ] P1.1 ...
- [ ] P1.2 ... (requires P1.1)

### Phase 2: Features
- [ ] P2.1 ...
- [ ] P2.2 ... (can parallel with P2.1)

### Phase 3: Documentation
- [ ] P3.1 ...
- [ ] P3.2 ...
```

Rules:
- phases are sequential; items within a phase may be parallelized when safe
- record explicit dependencies
- avoid multiple agents editing the same file concurrently
- include a documentation phase in every checklist

---

## 13) Project Organization and Historical Context

Current structure (developer-relevant):
- `docs/plans/` - active plans and research
- `docs/tests/` - active test plans
- `.archive/` - historical completed plans/test logs

Historical references worth checking:
- `.archive/LIVE_TEST_RESULTS.md`
- `.archive/AGENTS_MD_TEST_FINDINGS.md`

Archived insights can explain why certain "obvious" fixes were reverted.

---

## 14) Known Issues and Known Limitations

Known issues snapshot (historical context from prior bug sweeps):
- several previously critical issues were fixed (class discovery, type mismatch validation, selected Python client defects)
- some behavior remains intentionally constrained by engine/tooling limits

Current limitations to design around:
- `set_property` on BoxExtent may not visually update bounds; use transform-based volume sizing
- `duplicate_asset` from engine/plugin content can be unstable; duplicate from `/Game/` templates
- `call_function` arg support is limited in current gRPC path
- `spawn_actor` with `relative_to` has known backend/world-context issues
- `TSoftObjectPtr` assignment is unreliable in current reflection write path
- line-ending corruption risk on Windows mounts for edited files

Do not hide these limits; return explicit guidance/errors.

---

## 15) Module READMEs (Further Reading)

Use module READMEs first for area-specific work:
- `AgentBridgeCore/README.md`
- `AgentBridgeRuntime/README.md`
- `AgentBridgeScripting/README.md`
- `AgentBridgeServer/README.md`
- `mcp/README.md`
- `bp_toolkit/README.md`

Then use:
1. module `CLAUDE.md`
2. `docs/architecture/*.md`

---

## 16) When to Use `AGENTS_use.md`

Use `AGENTS_use.md` when tasks are about operating tools in Unreal (level/asset/sim workflows) rather than implementing AgentBridge internals.
