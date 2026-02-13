# AgentBridge Plugin

> UE 5.6 plugin exposing editor/runtime state to external AI agents via gRPC + MCP.
> Primary use case: "Build me a level" - agents need full read/write/discover capabilities.

---

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

---

## Quick Reference

| What | Where |
|------|-------|
| gRPC Port | 10001 (via Tempo) |
| HTTP Port | 8080 (fallback) |
| Python Env | `<PROJECT_ROOT>/TempoEnv/Scripts/python.exe` |
| Build Script | `<PROJECT_ROOT>/Scripts/Build.sh` (if using Tempo build) |
| Run Editor (GUI) | `cd <PROJECT_ROOT> && ./Plugins/Tempo/Scripts/Run.sh` |
| Kill Editor | `cmd //c "taskkill /F /IM UnrealEditor.exe"` (Windows) |
| User Docs | `README.md` |

Replace `<PROJECT_ROOT>` with your Unreal project directory (e.g., `/home/user/MyGame` or `D:/MyGame`).

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
| Blueprint nodes | **WORKING** | 6 MCP tools for BP graph manipulation |
| PCG graphs | **WORKING** | 6 MCP tools for PCG graph manipulation |

**Tool Count:** ~100 MCP tools across modular services (+ 14 bp_toolkit when present)

---

## Architecture

```
External Agents (Claude, LLMs)
         |
         v
MCP Server (Python) --- Modular tool loading
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
| mcp (Python) | MCP server, gRPC client, tests | `mcp/CLAUDE.md` (submodule) |
| bp_toolkit | UAsset parsing, Blueprint modification (optional) | `bp_toolkit/CLAUDE.md` (submodule) |

**User-Facing Documentation:** `README.md` - comprehensive guide with tool reference.

---

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
- **Verify line endings after editing source files** - the Edit tool on WSL converts LF to CRLF on
  Windows mounts (`/mnt/d/`). After editing, run `file <path>` to check. If it shows "CRLF line
  terminators", fix with `sed -i 's/\r$//' <path>`. **Proto files are especially sensitive** -
  protoc silently fails on CRLF and GenProtos.sh suppresses the error with `|| true`.
- **Use ASCII only in source files** - avoid em dashes (`—`), smart quotes, etc. in comments.
  Use regular dashes (`-`) instead. Check with `file <path>` — should say "ASCII text" not "UTF-8".

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

---

## Adding New gRPC RPCs

When adding new gRPC RPCs to AgentBridge, follow this checklist:

1. Add proto message + RPC to `AgentBridge.proto`
2. Regenerate proto files (`GenProtos.sh`)
3. Add handler method to `AgentBridgeServiceSubsystem.h`
4. Implement handler in `AgentBridgeServiceSubsystem.cpp`
5. ⚠️ **Register in `RegisterScriptingServices()`** ← EASY TO FORGET!
6. Add Python client method in `agentbridge.py`
7. Add MCP tool wrapper
8. Add to `MODULES` dict in `__init__.py`

**Tempo Proto Gotcha:** Tempo's `TempoScripting::Rotation` proto uses SHORT field names:
- `.r` = roll, `.p` = pitch, `.y` = yaw (NOT `.roll`, `.pitch`, `.yaw`)

---

## Build Commands

**IMPORTANT: Kill the editor before building!** DLLs will be locked and build will fail.

```bash
# STEP 1: Kill editor first (REQUIRED for full builds)
cmd //c "taskkill /F /IM UnrealEditor.exe"  # Windows
# pkill -f UnrealEditor                       # Linux/Mac

# STEP 2: Full build (~1 min)
cd <PROJECT_ROOT>/Scripts && ./Build.sh

# Or direct UBT (adjust paths for your engine location)
"<ENGINE_ROOT>/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" \
  YourProjectEditor Win64 Development \
  -Project="<PROJECT_ROOT>/YourProject.uproject" -WaitMutex

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
cd <PROJECT_ROOT> && ./Plugins/Tempo/Scripts/Run.sh

# Wait ~30 seconds for gRPC server to be ready on port 10001

# Force-quit GUI editor (IMPORTANT: use cmd wrapper in Git Bash on Windows)
cmd //c "taskkill /F /IM UnrealEditor.exe"

# Headless mode for automation
"<ENGINE_ROOT>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "<PROJECT_ROOT>/YourProject.uproject" \
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
cd <PROJECT_ROOT>/Plugins/AgentBridge

# gRPC tests (port 10001) - auto-detects Tempo API path
python -m mcp.tests.test_grpc

# HTTP tests (port 8080)
python -m mcp.tests.test_client
```

## Key Paths

| Purpose | Path |
|---------|------|
| Project Root | `<PROJECT_ROOT>` (your Unreal project directory) |
| Plugin Root | `<PROJECT_ROOT>/Plugins/AgentBridge` |
| Engine | `<ENGINE_ROOT>` (your UE installation) |
| Project Logs | `<PROJECT_ROOT>/Saved/Logs/<ProjectName>.log` |
| Tempo Plugin | `<PROJECT_ROOT>/Plugins/Tempo` |
| bp_toolkit | `Plugins/AgentBridge/bp_toolkit` (submodule) |
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
- `assets` - Asset creation, saving, file operations
- `components` - Component transforms, attachment
- `console` - Console commands
- `workflows` - Common multi-step operations (includes PCG biome workflow)
- `pcg_volume` - PCG volume types and sizing
- `volume_sizing` - BoxComponent sizing details
- `bp_toolkit` - Offline asset manipulation (when submodule present)

---

## Submodules

### mcp - MCP Server (Python)

Python MCP server providing ~100 tools for AI agent integration.

**Location:** `mcp/` (git submodule)

**Setup:**
```bash
cd <PROJECT_ROOT>/Plugins/AgentBridge
git submodule update --init --recursive
```

**Running:**
```bash
# From AgentBridge directory (parent of mcp/)
cd <PROJECT_ROOT>/Plugins/AgentBridge

# Auto-detection finds Tempo API path automatically
python -m mcp --host localhost --port 10001

# Or explicitly set the path
TEMPO_API_PATH="<PROJECT_ROOT>/Plugins/Tempo/TempoCore/Content/Python/API/tempo" \
  python -m mcp --host localhost --port 10001
```

**Documentation:** See `mcp/CLAUDE.md` for Python development details.

#### Claude Code MCP Integration (WSL)

When connecting the AgentBridge MCP server to Claude Code running in WSL, there are several gotchas:

**1. Config file location:** Claude Code MCP servers go in `~/.claude.json` under the top-level `mcpServers` key — NOT in `~/.claude/mcp.json` (that path is ignored). Use `claude mcp add` CLI or edit `~/.claude.json` directly.

**2. CWD is required:** The `python.exe -m mcp` command must run from the AgentBridge directory so Python can find the `mcp` package. The `claude mcp add` CLI doesn't support a `cwd` field, so either edit `~/.claude.json` to add `"cwd"` manually, or use a wrapper script (recommended).

**3. Use a shell wrapper script:** The most reliable approach on WSL is a bash wrapper that handles CWD and exec. Claude Code spawns this natively, and `exec` replaces the shell with the Windows Python process for clean stdio piping:

```bash
# ~/.claude/agentbridge-mcp.sh
#!/bin/bash
cd /mnt/d/tempo/TempoSample/Plugins/AgentBridge
exec /mnt/d/tempo/TempoSample/TempoEnv/Scripts/python.exe -m mcp --host localhost --port 10001 --profile full "$@"
```

Then in `~/.claude.json`:
```json
{
  "mcpServers": {
    "agentbridge": {
      "type": "stdio",
      "command": "/home/inc/.claude/agentbridge-mcp.sh",
      "args": [],
      "env": {}
    }
  }
}
```

**4. localhost works from Windows Python:** Even though WSL's `localhost` is different from Windows' `localhost`, this isn't an issue because the MCP server runs as a **Windows Python process** (`.exe`), which connects to gRPC via the Windows network stack where Unreal is listening.

**5. Editor must be running first:** The MCP server connects to gRPC on port 10001 during `initialize`. Start the Unreal Editor and wait ~30 seconds for gRPC before starting/restarting Claude Code.

**6. Restart Claude Code after config changes:** MCP server configs are read at Claude Code startup. After editing `~/.claude.json` or the wrapper script, restart Claude Code and verify with `/mcp`.

---

### Known Tool Issues (TODO)

Issues discovered during AGENTS.md testing (2026-02-12). These require C++ fixes in the
AgentBridge gRPC backend. Documented with workarounds in AGENTS.md for now.

| # | Issue | Workaround in AGENTS.md | Fix Location |
|---|-------|------------------------|--------------|
| 1 | `list_classes(name_pattern=...)` is case-insensitive **exact match** only, not substring like `query_actors` | Changed examples to use `base_class_name` for discovery | gRPC handler for ListClasses |
| 2 | `query_actors` has no `folder_path` filter parameter (silently ignored if passed) | Changed examples to use `label_pattern` instead | gRPC handler for QueryActors — add folder_path filter |
| 3 | `query_actors(include_unloaded=true)` ignores `class_name` filter — returns all actor types | Added warning note; suggest client-side filtering | gRPC handler for QueryActors (unloaded path) |

**Full test findings:** See `AGENTS_MD_TEST_FINDINGS.md` for complete details including
reproduction steps and screenshots of each issue.

---

### bp_toolkit - Blueprint/Asset Toolkit (Optional)

A Python toolkit for parsing, modifying, and creating Unreal Engine assets via JSON manipulation.
Works offline without Unreal running.

**Location:** `bp_toolkit/` (git submodule)

**MCP Integration:** When bp_toolkit is present, the MCP server exposes 26 additional tools:
- 6 live Blueprint graph editing tools (bp_create_node, bp_connect_pins, etc.)
- 6 live PCG graph editing tools (pcg_add_node, pcg_connect, etc.)
- 14 offline asset manipulation tools (bp_export_asset, bp_import_asset, etc.)

Use `help(topic="bp_toolkit")` for tool reference.

#### Setup

```bash
# Initialize submodules (includes UAssetGUI with UAssetAPI)
cd <PROJECT_ROOT>/Plugins/AgentBridge
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

#### MCP Tools (26 tools total)

**Live Blueprint Editing (6):**
`bp_create_node`, `bp_connect_pins`, `bp_disconnect_pins`, `bp_delete_node`, `bp_list_nodes`, `bp_list_pins`

**Live PCG Editing (6):**
`pcg_add_node`, `pcg_connect`, `pcg_disconnect`, `pcg_delete_node`, `pcg_list_nodes`, `pcg_get_input_output_nodes`

**Offline Asset Manipulation (14):**
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

<!-- PART III: DEVELOPMENT GUIDE -->

---

## Development SOPs

| SOP | Description |
|-----|-------------|
| **Plan First** | **Non-trivial features require a plan in `docs/plans/`. See Feature Planning SOP below.** |
| Fail Fast | Errors surface immediately. No swallowed exceptions. Return meaningful error messages. |
| Live Test | **Automated tests are not sufficient. Always live test with Unreal Editor + MCP before committing.** |
| Document | Each phase updates this file and module CLAUDE.md files. |
| **Commit Often** | **Commit after each phase/logical unit. Don't wait for "everything done."** |
| **Branch per Plan** | **One feature branch per plan. Merge only after checklist complete + user sign-off.** |
| **Don't Revert Unrelated Changes** | **Only stage files YOU modified. NEVER use `git checkout` or `git restore` on files you didn't change.** |
| No Dead Code | Delete unused code. Clean up after yourself. |
| **Zero Build Failures** | **All builds must succeed. If a failure is introduced, document it in Known Issues below.** |

### Live Testing Requirement (MANDATORY)

**Automated tests alone are NOT sufficient to commit changes.** Before any commit that affects MCP tools, gRPC handlers, property access, or actor operations:

1. Start the Unreal Editor and wait for gRPC (port 10001)
2. Connect MCP server (Claude Code or manual)
3. Test the changed functionality with real actors/assets
4. Verify results visually in the editor viewport when applicable
5. Check for silent failures — `set_property` can report success but not actually work

This catches issues that unit tests miss, such as:
- `set_property` storing values but not triggering visual updates
- Type mismatches silently failing on object references
- Plugin content assets behaving differently from `/Game/` assets
- Class loading differences between fresh and warm editor sessions

### Version Control

#### Commit Frequency

Commit after each completed phase or logical unit. Don't wait for "everything done."

- **After each checklist phase** (P1, P2, etc.) — phases are designed as atomic units
- **Before switching modules** — if you've been in Runtime and need to touch Server, commit first
- **When builds pass** — green build = safe checkpoint
- **Before risky changes** — about to refactor? commit the working state first
- **Rule of thumb**: if you'd be upset losing the work, commit it

Commit messages should reference the plan and checklist item when applicable:
```
feat(runtime): add PostEditChangeProperty after BoxExtent set (AGENTBRIDGE-BUGS P1.3)
```

#### Branching

- **One branch per plan** — `feature/<plan-name>` (e.g., `feature/call-function-fix`, `feature/volume-sizing`)
- **Branch before starting implementation** — not during exploration/planning
- **Keep branches focused** — don't mix unrelated changes

#### Pushing

- **Feature branches:** Push after each commit — provides backup, enables CI, no downside
- **Main/master:** Only via PR merge or with user sign-off

#### Merging

- **Merge when:** Plan checklist complete + builds pass + live testing done + user sign-off
- **Don't merge:** Partial implementations, broken builds, untested changes
- **Merge strategy:** Squash for small plans, regular merge for large plans

### Feature Planning SOP

All non-trivial features should follow this planning process. Plans live in `docs/plans/` as markdown files. Completed plans are moved to `.archive/`.

#### Planning Process

| Phase | Description |
|-------|-------------|
| 1. Intent | Discuss what the feature should accomplish, user-facing behavior |
| 2. Explore | Research relevant code, understand existing patterns |
| 3. Feasibility | Discuss technical approach, identify blockers or concerns |
| 4. Scope | Define what's included, deferred, and explicitly excluded |
| 5. General Plan | High-level architecture, design decisions with rationale |
| 6. Validate | Validate plan against actual codebase patterns |
| 7. Detailed Plan | Concrete implementation with exact file paths, code snippets |
| 8. Checklist | Implementation checklist with task IDs for parallel execution |
| 9. Documentation | Document what changed for users and future developers |

**CRITICAL: Update plan file after EVERY phase.** Planning sessions can be interrupted. Write findings to `docs/plans/<PLAN-NAME>.md` incrementally. A partial plan with 4 phases completed is far better than losing everything.

#### Plan Document Structure

```markdown
# Plan: Feature Name

## Overview
Brief description, current state, goal.

## Scope
### Included in v1
### Deferred to Future
### Explicitly Excluded

## Design Decisions
| Question | Decision | Rationale |
Record WHY choices were made, not just what.

## Architecture
Affected modules, file paths, integration points.

## Implementation Details
Concrete code, exact file paths.

## Testing Strategy
Unit tests, live testing plan, visual verification steps.

## Implementation Checklist
Phased checklist with task IDs (P1.1, P1.2, etc.)
Must include a Documentation phase.
```

#### Implementation Checklist Format

Checklists are designed for parallelization:

```markdown
### Phase 1: Foundation (Required First)
- [ ] **P1.1** Create base types
- [ ] **P1.2** Implement core logic (requires P1.1)

### Phase 2: Features (After Phase 1)
- [ ] **P2.1** Feature A (can parallel with P2.2)
- [ ] **P2.2** Feature B (can parallel with P2.1)
- [ ] **P2.3** Integration (requires P2.1, P2.2)

### Phase 3: Documentation (After Implementation)
- [ ] **P3.1** Update CLAUDE.md with changes
- [ ] **P3.2** Update AGENTS.md workflow sections
- [ ] **P3.3** Update MCP tool descriptions/help text
```

**Parallelization rules:**
- Items in the same phase can run in parallel unless noted
- Different phases are sequential (Phase 2 waits for Phase 1)
- Note dependencies explicitly: `(requires P1.3)` or `(can parallel with P2.1)`
- Avoid multiple agents editing the same file simultaneously

#### Documentation Phase (Required)

Every implementation checklist MUST include a documentation phase:

| Type | What to Update |
|------|----------------|
| **MCP tools** | Tool descriptions, help text in `agentbridge.py`, AGENTS.md |
| **gRPC changes** | Proto comments, module CLAUDE.md, README.md |
| **Workflows** | AGENTS.md workflow sections, troubleshooting tips |
| **Bug fixes** | Remove/update warnings in AGENTS.md, update Known Issues |

---

## Project Organization

```
docs/
├── plans/              # Active plans and actionable items
│   ├── AGENTS_MD_CHANGES.md    # Pending AGENTS.md documentation updates
│   └── AGENTBRIDGE_BUGS.md     # Pending code fixes with priorities
.archive/               # Completed plans, test logs, historical reference
│   ├── PCG_BIOME_WORKFLOW_TEST.md  # Full test log from 2026-02-12
│   └── AGENTS_MD_TEST_FINDINGS.md  # Earlier AGENTS.md test findings
.old.claude/            # Legacy development notes (pre-2026-02)
```

---

## Known Issues

Issues discovered during testing. See `docs/plans/AGENTBRIDGE_BUGS.md` for full details and fix plans.

| # | Severity | Issue | Workaround |
|---|----------|-------|------------|
| 1 | **CRITICAL** | `call_function` broken — `'AgentBridgeClient' object has no attribute 'host'` | None — tool is non-functional |
| 2 | **CRITICAL** | `save_asset` crashes on assets duplicated from plugin content | Pre-copy templates to `/Game/` first |
| 3 | **HIGH** | `set_property` on BoxExtent doesn't trigger visual update | Use `set_transform` on component scale instead |
| 4 | **HIGH** | `set_property` silently fails on type-mismatched object refs | Always verify with `get_property` after setting |
| 5 | **MEDIUM** | `list_classes` can't find plugin Blueprint classes | Use full asset paths for `spawn_actor` |
| 6 | **MEDIUM** | Struct array `set_property` fails at array level | Use element-level access (`Array[0].Field`) |

## Known Limitations

| Limitation | Status | Workaround |
|------------|--------|------------|
| `TSoftObjectPtr` assignment | Won't fix | Use `TObjectPtr` properties |
| gRPC header conflicts | By design | Business logic in Scripting module |
| FunctionInvoker struct returns | Auto-fixed | Redirected to property access |
| WSL Edit tool corrupts line endings | **Known** | Run `sed -i 's/\r$//'` after editing; TODO: investigate Claude Code PostToolUse hook to automate |

---

## Archived Documentation

Historical development notes are preserved in:
- `.archive/` — Completed plans and test logs from 2026-02 onward
- `.old.claude/` — Legacy development notes (pre-2026-02, gitignored)

**Key findings from early development:**

- **UAssetAPI round-trip validated**: Export→JSON→modify→reimport works for Blueprints, PCG Graphs, Behavior Trees
- **MetaDataMap workaround**: UE 5.7 Blueprints need MetaDataMap nulled before reimport (FName key issue)
- **All major bugs fixed**: TArray SET, GET returns empty, struct schema, element_type, asset path normalization
- **MCP size reduction**: Modular loading architecture, tool consolidation (transform/attach/detach unified)

---

*Self-Documenting Help System • Modular Tool Loading • Full Actor/Property/Asset Control*
