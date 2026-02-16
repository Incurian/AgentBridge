# Plan: Split AgentBridge into Separate UE Plugins

## Overview

**Current state:** AgentBridge is a single UE plugin (`AgentBridge.uplugin`) containing 4 C++
modules with a clean linear dependency chain: Core -> Runtime -> Scripting -> Server. No circular
dependencies exist.

**Goal:** Split each module into its own UE plugin within the same git repo, following the
Tempo multi-plugin pattern (`Plugins/Tempo/TempoCore/`, `Plugins/Tempo/TempoWorld/`, etc.).
This provides UE-level modularity and preserves the option to later extract any plugin into
a separate git submodule.

**Branch:** `feature/plugin-split` (created from `master`)

---

## Validation Findings (2026-02-13)

Three explorer agents validated the plan. Two key findings:

1. **GenProtos + path references: SAFE.** Proto generation uses recursive scanning (gen_protos.py),
   TempoModuleRules resolves include paths relative to module directory. Zero hardcoded paths in
   C++, Build.cs, or Python. Only docs need path updates.

2. **Plugin auto-discovery: PLAN FLAW DETECTED.** UE 5.6 does NOT auto-load nested plugins just
   because they have `EnabledByDefault: true`. Tempo sub-plugins load via **dependency chains** -
   TempoROS (in .uproject) depends on TempoCore (triggers load). Without a dependency chain,
   sub-plugins at `Plugins/AgentBridge/AgentBridgeCore/` would NOT load.

   **Fix:** Keep a lightweight wrapper `AgentBridge.uplugin` at the root (no modules, just
   plugin dependencies). Wrapper has `EnabledByDefault: true` (proven working today), depends
   on 4 sub-plugins, triggering them to load. No .uproject changes needed.

---

## Scope

### Included in v1

- Split 4 modules into 4 separate UE plugins with individual `.uplugin` descriptors
- Keep wrapper `AgentBridge.uplugin` at root (no modules, just plugin dependencies)
- Move source directories to per-plugin layout (`AgentBridgeX/Source/AgentBridgeX/`)
- Create proper plugin dependency declarations (matching the `.Build.cs` module deps)
- Delete legacy files (`Protos/`, `Scripts/run_cmd.bat`)
- Update documentation path references (`CLAUDE.md`, `README.md`)
- Update `.gitignore` for per-plugin build artifacts
- Verify build + live test with MCP

### Deferred to Future

- Extracting any plugin to a separate git submodule (structure enables this with `git subtree split`)
- Per-plugin versioning (all share v0.2.0 for now)
- Per-plugin Content directories (currently `CanContainContent: false` for all)
- Per-plugin PreBuildSteps for proto generation (currently handled by Tempo's build pipeline)

### Explicitly Excluded

- Changing any `.Build.cs` files (module names and dependencies are name-based, no path changes)
- Changing any `.h` or `.cpp` files (`#include` paths use module names, not file paths)
- Changing the `.uproject` file (plugins use `EnabledByDefault: true`, auto-discovered)
- Moving `mcp/` or `bp_toolkit/` submodules (they're repo-level tooling, stay at root)
- Splitting the proto file (single `AgentBridge.proto` stays in Server module)

---

## Design Decisions

| Question | Decision | Rationale |
|----------|----------|-----------|
| How many plugins? | 4 (one per module) | Matches existing module boundaries. Each is a logical unit with distinct responsibilities. Maximizes future submodule flexibility. |
| Follow Tempo pattern or invent new? | Follow Tempo exactly | Tempo's multi-plugin-in-one-repo pattern is proven in this project. Same UE version, same build pipeline. |
| Wrapper plugin at root? | YES (validation fix) | UE 5.6 does NOT auto-load nested plugins. Tempo sub-plugins load via dependency chains, not auto-discovery. Keep `AgentBridge.uplugin` as a module-less wrapper that depends on all 4 sub-plugins. Wrapper has `EnabledByDefault: true` (proven working today). |
| Where do mcp/ and bp_toolkit/ live? | Stay at repo root | They're Python tooling, not UE plugins. Cross-cutting concern. Moving them into a C++ plugin directory would be confusing. |
| What happens to legacy `Protos/`? | Delete | Contains a 602-line legacy proto that's diverged from the active 888-line proto in `Source/AgentBridgeServer/Public/`. The active proto moves with the Server module. |
| What happens to `Scripts/run_cmd.bat`? | Delete | Hardcoded paths to a different project (`VR_Project`). Not functional. |
| How to handle `.uproject`? | No changes | Wrapper `AgentBridge.uplugin` keeps `EnabledByDefault: true` (same as today). Sub-plugins load via wrapper's dependency chain, not auto-discovery. |
| `git mv` or plain `mv`? | `git mv` | Preserves file history across the move. `git log --follow` will track files through the rename. |
| Build artifact locations? | Per-plugin `Binaries/` and `Intermediate/` | UBT generates these relative to the `.uplugin` location. Need `.gitignore` patterns to catch them. |

---

## Architecture

### Current Structure (Single Plugin)

```
Plugins/AgentBridge/
+-- AgentBridge.uplugin          <- 1 plugin, 4 modules
+-- Source/
|   +-- AgentBridgeCore/         <- Foundation (reflection, types)
|   +-- AgentBridgeRuntime/      <- World ops, actor ops, property paths
|   +-- AgentBridgeScripting/    <- Command dispatch, JSON, business logic
|   +-- AgentBridgeServer/       <- gRPC/HTTP (Tempo integration)
+-- mcp/                         <- Python MCP server (submodule)
+-- bp_toolkit/                  <- Python BP toolkit (submodule)
+-- Protos/                      <- Legacy proto (unused)
+-- Scripts/                     <- Legacy script (unused)
```

### Target Structure (Wrapper + 4 Sub-Plugins)

```
Plugins/AgentBridge/                   <- git repo root (container)
+-- AgentBridge.uplugin                <- WRAPPER: no modules, deps on 4 sub-plugins
+-- AgentBridgeCore/                   <- UE Plugin 1
|   +-- AgentBridgeCore.uplugin        <- No plugin deps
|   +-- Source/AgentBridgeCore/
+-- AgentBridgeRuntime/                <- UE Plugin 2
|   +-- AgentBridgeRuntime.uplugin     <- Deps: [AgentBridgeCore]
|   +-- Source/AgentBridgeRuntime/
+-- AgentBridgeScripting/              <- UE Plugin 3
|   +-- AgentBridgeScripting.uplugin   <- Deps: [Core, Runtime]
|   +-- Source/AgentBridgeScripting/
+-- AgentBridgeServer/                 <- UE Plugin 4
|   +-- AgentBridgeServer.uplugin      <- Deps: [Core, Runtime, Scripting, TempoCore]
|   +-- Source/AgentBridgeServer/
+-- mcp/                               <- Unchanged
+-- bp_toolkit/                        <- Unchanged
+-- CLAUDE.md, README.md, AGENTS.md    <- Updated paths
```

### Dependency Graph (Unchanged)

```
AgentBridgeCore          (no AgentBridge deps)
       ^
       |
AgentBridgeRuntime       (depends on Core)
       ^
       |
AgentBridgeScripting     (depends on Core + Runtime)
       ^
       |
AgentBridgeServer        (depends on Core + Runtime + Scripting + TempoCore)
```

Module-to-module dependencies are name-based strings in `.Build.cs`. UBT resolves them
across plugin boundaries when the plugin dependency is declared in `.uplugin`. This is
exactly how Tempo plugins (TempoWorld -> TempoCore) already work.

### Future Submodule Extraction Path

Each plugin directory is self-contained. To extract as a git submodule:

```bash
# 1. Split the subdirectory into its own branch
git subtree split --prefix=AgentBridgeCore/ -b core-split

# 2. Push to new repo
git push git@github.com:Incurian/AgentBridgeCore.git core-split:main

# 3. Remove from this repo and add as submodule
git rm -r AgentBridgeCore/
git submodule add git@github.com:Incurian/AgentBridgeCore.git AgentBridgeCore

# 4. Nothing else changes -- UE resolves by name, not path
```

---

## Implementation Details

### .uplugin Files (1 rewritten wrapper + 4 new sub-plugin files)

**AgentBridge.uplugin (WRAPPER - rewrite existing file):**
```json
{
    "FileVersion": 3,
    "Version": 1,
    "VersionName": "0.2.0",
    "FriendlyName": "AgentBridge",
    "Description": "AI agent integration for Unreal Engine (wrapper - loads sub-plugins)",
    "Category": "Editor",
    "CreatedBy": "AgentBridge",
    "CanContainContent": false,
    "IsBetaVersion": true,
    "IsExperimentalVersion": true,
    "Installed": false,
    "EnabledByDefault": true,
    "Plugins": [
        { "Name": "AgentBridgeCore", "Enabled": true },
        { "Name": "AgentBridgeRuntime", "Enabled": true },
        { "Name": "AgentBridgeScripting", "Enabled": true },
        { "Name": "AgentBridgeServer", "Enabled": true }
    ]
}
```
Note: NO `Modules` array. This is a dependency-only wrapper that triggers loading of all 4
sub-plugins. It retains `EnabledByDefault: true` (same as today), so no .uproject changes needed.

All sub-plugins share common metadata. Only `Plugins` and `Modules` arrays differ.

**AgentBridgeCore/AgentBridgeCore.uplugin:**
```json
{
    "FileVersion": 3,
    "Version": 1,
    "VersionName": "0.2.0",
    "FriendlyName": "AgentBridge Core",
    "Description": "Reflection primitives for AgentBridge (FProperty, UFunction, type discovery)",
    "Category": "Editor",
    "CreatedBy": "AgentBridge",
    "CanContainContent": false,
    "IsBetaVersion": true,
    "IsExperimentalVersion": true,
    "Installed": false,
    "EnabledByDefault": true,
    "Modules": [
        {
            "Name": "AgentBridgeCore",
            "Type": "Runtime",
            "LoadingPhase": "Default"
        }
    ]
}
```

**AgentBridgeRuntime/AgentBridgeRuntime.uplugin:**
```json
{
    "FileVersion": 3,
    "Version": 1,
    "VersionName": "0.2.0",
    "FriendlyName": "AgentBridge Runtime",
    "Description": "World context, actor operations, and property paths for AgentBridge",
    "Category": "Editor",
    "CreatedBy": "AgentBridge",
    "CanContainContent": false,
    "IsBetaVersion": true,
    "IsExperimentalVersion": true,
    "Installed": false,
    "EnabledByDefault": true,
    "Plugins": [
        {
            "Name": "AgentBridgeCore",
            "Enabled": true
        }
    ],
    "Modules": [
        {
            "Name": "AgentBridgeRuntime",
            "Type": "Runtime",
            "LoadingPhase": "Default"
        }
    ]
}
```

**AgentBridgeScripting/AgentBridgeScripting.uplugin:**
```json
{
    "FileVersion": 3,
    "Version": 1,
    "VersionName": "0.2.0",
    "FriendlyName": "AgentBridge Scripting",
    "Description": "Command dispatch, JSON serialization, and business logic for AgentBridge",
    "Category": "Editor",
    "CreatedBy": "AgentBridge",
    "CanContainContent": false,
    "IsBetaVersion": true,
    "IsExperimentalVersion": true,
    "Installed": false,
    "EnabledByDefault": true,
    "Plugins": [
        {
            "Name": "AgentBridgeCore",
            "Enabled": true
        },
        {
            "Name": "AgentBridgeRuntime",
            "Enabled": true
        }
    ],
    "Modules": [
        {
            "Name": "AgentBridgeScripting",
            "Type": "Runtime",
            "LoadingPhase": "Default"
        }
    ]
}
```

**AgentBridgeServer/AgentBridgeServer.uplugin:**
```json
{
    "FileVersion": 3,
    "Version": 1,
    "VersionName": "0.2.0",
    "FriendlyName": "AgentBridge Server",
    "Description": "gRPC and HTTP server exposing AgentBridge to external AI agents",
    "Category": "Editor",
    "CreatedBy": "AgentBridge",
    "CanContainContent": false,
    "IsBetaVersion": true,
    "IsExperimentalVersion": true,
    "Installed": false,
    "EnabledByDefault": true,
    "Plugins": [
        {
            "Name": "AgentBridgeCore",
            "Enabled": true
        },
        {
            "Name": "AgentBridgeRuntime",
            "Enabled": true
        },
        {
            "Name": "AgentBridgeScripting",
            "Enabled": true
        },
        {
            "Name": "TempoCore",
            "Enabled": true
        }
    ],
    "Modules": [
        {
            "Name": "AgentBridgeServer",
            "Type": "Runtime",
            "LoadingPhase": "Default"
        }
    ]
}
```

### File Moves (git mv)

```bash
# Create plugin directories with Source/ subdirs
mkdir -p AgentBridgeCore/Source
mkdir -p AgentBridgeRuntime/Source
mkdir -p AgentBridgeScripting/Source
mkdir -p AgentBridgeServer/Source

# Move source directories (git mv preserves history)
git mv Source/AgentBridgeCore     AgentBridgeCore/Source/AgentBridgeCore
git mv Source/AgentBridgeRuntime  AgentBridgeRuntime/Source/AgentBridgeRuntime
git mv Source/AgentBridgeScripting AgentBridgeScripting/Source/AgentBridgeScripting
git mv Source/AgentBridgeServer   AgentBridgeServer/Source/AgentBridgeServer
```

### File Deletions

```bash
# Delete legacy files
git rm -r Protos/              # Legacy proto (active proto is in Server/Source/.../Public/)
git rm -r Scripts/             # Only run_cmd.bat with hardcoded paths to VR_Project
# AgentBridge.uplugin is REWRITTEN as wrapper (not deleted)
rmdir Source/                  # Empty after moves (not tracked by git)
```

### .gitignore Additions

```
# Per-plugin build artifacts (UBT generates these relative to .uplugin)
*/Binaries/
*/Intermediate/
```

### Documentation Path Updates

In `CLAUDE.md`, update the module documentation table:
```
| Module | Doc |
| AgentBridgeCore | `AgentBridgeCore/Source/AgentBridgeCore/CLAUDE.md` |
| AgentBridgeRuntime | `AgentBridgeRuntime/Source/AgentBridgeRuntime/CLAUDE.md` |
| AgentBridgeScripting | `AgentBridgeScripting/Source/AgentBridgeScripting/CLAUDE.md` |
| AgentBridgeServer | `AgentBridgeServer/Source/AgentBridgeServer/CLAUDE.md` |
```

Update architecture section, Key Paths table, and any `Source/AgentBridgeX/` references.

### Files NOT Changed

| File | Why |
|------|-----|
| `.Build.cs` (all 4) | Module names and dependency strings unchanged |
| `.h` / `.cpp` (all) | `#include` uses module names, not file paths |
| `.uproject` | `EnabledByDefault: true` means auto-discovery |
| `.gitmodules` | mcp/ and bp_toolkit/ paths unchanged |
| `mcp/`, `bp_toolkit/` | No path dependencies on C++ Source/ layout |

---

## Testing Strategy

### Build Verification

1. Kill editor
2. Full rebuild via `Build.sh`
3. Expected: clean build, no errors
4. If proto generation fails: check that `TempoModuleRules` finds the proto at its
   new path (`AgentBridgeServer/Source/AgentBridgeServer/Public/AgentBridge.proto`)

### Plugin Discovery Verification

1. Launch editor
2. Check logs for all 4 plugin names loading
3. Alternatively: Edit -> Plugins, search "AgentBridge" - should show 4 entries

### MCP End-to-End Verification

1. Connect MCP server (Claude Code or manual)
2. `help()` - verify tools load
3. `query_actors()` - verify gRPC round-trip works
4. `spawn_actor(class_name="PointLight", location=[0,0,500])` - verify full pipeline
5. `get_property(actor_id="PointLight", path="LightComponent0.Intensity")` - verify
   Core -> Runtime -> Scripting -> Server chain works

### Git History Verification

```bash
git log --follow AgentBridgeCore/Source/AgentBridgeCore/Public/AgentBridgeTypes.h
# Should show commits from before the move
```

---

## Implementation Checklist

### Phase 1: Create Plugin Directories and Move Sources

- [ ] **P1.1** Create 4 plugin directories with `Source/` subdirs
- [ ] **P1.2** `git mv Source/AgentBridgeCore AgentBridgeCore/Source/AgentBridgeCore`
- [ ] **P1.3** `git mv Source/AgentBridgeRuntime AgentBridgeRuntime/Source/AgentBridgeRuntime`
- [ ] **P1.4** `git mv Source/AgentBridgeScripting AgentBridgeScripting/Source/AgentBridgeScripting`
- [ ] **P1.5** `git mv Source/AgentBridgeServer AgentBridgeServer/Source/AgentBridgeServer`
- [ ] **P1.6** `git rm -r Protos/` (legacy proto; active proto in Server module)
- [ ] **P1.7** `git rm -r Scripts/` (legacy run_cmd.bat)
- [ ] **P1.8** Delete empty `Source/` directory

### Phase 2: Create .uplugin Files (can parallel all 4)

- [ ] **P2.1** Create `AgentBridgeCore/AgentBridgeCore.uplugin` (no plugin deps)
- [ ] **P2.2** Create `AgentBridgeRuntime/AgentBridgeRuntime.uplugin` (deps: Core)
- [ ] **P2.3** Create `AgentBridgeScripting/AgentBridgeScripting.uplugin` (deps: Core, Runtime)
- [ ] **P2.4** Create `AgentBridgeServer/AgentBridgeServer.uplugin` (deps: Core, Runtime, Scripting, TempoCore)
- [ ] **P2.5** Rewrite `AgentBridge.uplugin` as module-less wrapper (deps on all 4 sub-plugins)

### Phase 3: Update Configuration

- [ ] **P3.1** Update `.gitignore` with per-plugin build artifact patterns

### Phase 4: Build and Verify (requires P1-P3)

- [ ] **P4.1** Kill editor, full rebuild
- [ ] **P4.2** Launch editor, verify 4 plugins discovered
- [ ] **P4.3** Connect MCP, verify tools work end-to-end
- [ ] **P4.4** Verify line endings on new files

### Phase 5: Documentation

- [ ] **P5.1** Update `CLAUDE.md` - path references, architecture section, module doc table
- [ ] **P5.2** Update `README.md` - any source path references
- [ ] **P5.3** Update per-module `CLAUDE.md` files if they reference sibling module paths

### Phase 6: Commit and Push

- [ ] **P6.1** Commit: `refactor: split AgentBridge into 4 separate UE plugins`
- [ ] **P6.2** Push to `feature/plugin-split`, merge to master after verification

---

## Risk Assessment

| Risk | Likelihood | Mitigation |
|------|-----------|------------|
| UE doesn't discover nested plugins | Low | Tempo already does `Plugins/Tempo/TempoCore/TempoCore.uplugin` successfully |
| GenProtos.sh can't find proto at new path | Low | TempoModuleRules scans `Source/*/Public/*.proto` relative to `.uplugin` location |
| Build artifacts in wrong location | Low | UBT generates `Binaries/`/`Intermediate/` relative to `.uplugin`, which is now per-plugin |
| Missing `.gitignore` patterns for per-plugin artifacts | Medium | Add `*/Binaries/`, `*/Intermediate/` patterns in Phase 3 |
| Plugin load order issues | Low | All plugins use `LoadingPhase: Default` and UE resolves dependency order from `.uplugin` Plugins array |
