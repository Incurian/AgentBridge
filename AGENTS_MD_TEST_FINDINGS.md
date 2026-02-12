# AGENTS.md Test Findings

Testing each section of AGENTS.md against live editor (TestingMap, World Partition enabled).
Recording gotchas, errors, and recommended fixes.

---

## Finding 1: `label_pattern` and `name_pattern` are substring matches, NOT glob/wildcard

**Section:** Actor Operations, Help text (`help(topic="actors")`)

**What happened:**
- `query_actors(label_pattern="TestLight")` → found 2 actors (TestLight, TestLight2) — substring match
- `query_actors(label_pattern="TestLight*")` → found 0 — `*` treated as literal character
- `query_actors(name_pattern="PointLight")` → found 7 — substring match
- `query_actors(name_pattern="PointLight*")` → found 0

**Where it's wrong in AGENTS.md:**
- The help text in `agentbridge.py` shows `query_actors(name_pattern="*Door*")` — wildcards silently fail
- Line 1175: `list_classes(name_pattern="*Biome*")` — wildcards don't work here either

**Recommended fix:**
1. Remove all `*` wildcards from `label_pattern` and `name_pattern` examples in AGENTS.md and help text
2. Add a note: "Pattern matching is substring-based, not glob. Use `Door` not `*Door*`"
3. OR: fix the C++ side to support glob/wildcard patterns (better UX)

---

## Finding 2: `list_classes(name_pattern=...)` is case-insensitive exact match only

**Section:** Type Discovery (line 1175)

**What happened:**
- `list_classes(name_pattern="Light")` → 0 results (not a substring match)
- `list_classes(name_pattern="PointLight")` → 1 result (exact match)
- `list_classes(name_pattern="pointlight")` → 1 result (case-insensitive)
- `list_classes(name_pattern="PointL")` → 0 results (not prefix match)
- `list_classes(name_pattern="Point")` → 0 results (not prefix match)
- `list_classes(base_class_name="Light")` → 5 results (hierarchy query, works great)

**Inconsistency:** `query_actors` name_pattern = substring match. `list_classes` name_pattern = case-insensitive exact match. Same parameter name, completely different behavior.

**Where it's wrong in AGENTS.md:**
- Line 1175: `list_classes(name_pattern="*Biome*")` — would return 0 results
- Line 1176: `list_classes(base_class_name="DataAsset", name_pattern="*Template*")` — name_pattern would return 0

**Recommended fix:**
1. Change examples to use `base_class_name` instead of `name_pattern` for discovery
2. Document that `name_pattern` on `list_classes` only does exact (case-insensitive) matching
3. OR: fix `list_classes` to do substring matching like `query_actors` (preferred)

---

## Finding 3: `query_actors(folder_path=...)` parameter doesn't exist

**Section:** Phase 9 Verify (line 804), Performance Tips (line 1323), Common Workflows (line 1240)

**What happened:**
- `query_actors` tool parameters: `class_name`, `name_pattern`, `label_pattern`, `tag`, `data_layer`, `include_unloaded`, `include_hidden`, `limit`
- NO `folder_path` parameter exists
- Passing `folder_path` is silently ignored (MCP ignores unknown params)

**Where it's wrong in AGENTS.md:**
- Line 804: `query_actors(folder_path="PCGBiomes")` — doesn't work
- Line 1240: `query_actors(folder_path="LightGrid")` — doesn't work
- Line 1323: "Use `query_actors(folder_path=...)` to verify" — doesn't work

**Recommended fix:**
1. Replace `folder_path` examples with `label_pattern` (e.g., search for actor labels you know)
2. OR: add `folder_path` as a filter parameter to `query_actors` (useful feature)
3. Update Performance Tips section accordingly

---

## Finding 4: `query_all_actors` doesn't exist as a separate tool

**Section:** World Partition (lines 895-898, 916-918)

**What happened:**
- AGENTS.md references `query_all_actors(class_name="StaticMeshActor", limit=200)` — no such tool
- The actual tool is `query_actors(include_unloaded=true)`
- Line 916-918 contrasts `query_actors` with `query_all_actors` as if they're different tools

**Recommended fix:**
1. Replace all `query_all_actors(...)` with `query_actors(..., include_unloaded=true)`
2. Update the distinction text to explain the `include_unloaded` parameter instead

---

## Finding 5: `query_actors(include_unloaded=true)` ignores `class_name` filter

**Section:** World Partition

**What happened:**
- `query_actors(class_name="PointLight", include_unloaded=false, limit=3)` → 3 PointLight actors (correct)
- `query_actors(class_name="PointLight", include_unloaded=true, limit=3)` → 3 WorldPartitionHLOD actors (WRONG — class filter ignored!)

**Also:** Response format changes when `include_unloaded=true` — adds `streaming_state`, `is_spatially_loaded`, `data_layers` fields, drops `rotation`/`scale`.

**Recommended fix:**
1. Document that `include_unloaded=true` may not respect class_name filtering
2. OR: fix the C++ backend to apply class_name filter to unloaded queries too (preferred)
3. Document the different response format for unloaded queries

---

## Finding 6: `get_current_level()` fails — "Service is not active"

**Section:** Editor & Levels (line 834)

**What happened:**
- `get_current_level()` → `{"error": "gRPC error: UNAVAILABLE - Service is not active"}`
- Requires TempoCoreEditor service which may not be active in all projects

**Other editor tools status:**
- `list_worlds()` → WORKS (returns Editor world)
- `list_project_directory()` → WORKS
- `search_console_commands()` → WORKS
- `execute_console_command()` → WORKS

**Recommended fix:**
1. Add a note in the Editor & Levels section: "Level management tools (`get_current_level`, `save_level`, `open_level`, `new_level`) require the TempoCoreEditor service. If you get 'Service is not active', these tools are not available in your project configuration."
2. Suggest `execute_console_command("SaveCurrentLevel")` as an alternative for saving

---

## Finding 7: Hawaii-specific paths in generic AGENTS.md

**Section:** Schema Reference (lines 555, 584), Rule 5 example (line 204)

**What happened:**
- Line 555: `BP_PCGBiomeCore` class path uses `/CoreAIHawaiiTest/MM_Testing/...`
- Line 584: `BP_PCGBiomeTexture` class path uses `/CoreAIHawaiiTest/MM_Testing/...`
- Line 204: Rule 5 example uses `/CoreAIHawaiiTest/...`
- These are project-specific paths that won't exist in other projects

**OK paths (plugin-level, will exist in all projects with PCGBiomeCore):**
- Line 571: `BP_PCGBiomeBaseActor` at `/PCGBiomeCore/Blueprints/...`
- Line 607: `BiomeDefinitionTemplate` at `/PCGBiomeCore/BiomeDefinitions/...`
- Line 628: `BiomeAssetTemplate` at `/PCGBiomeCore/BiomeAssets/...`

**Recommended fix:**
1. Use placeholder `<PROJECT_CONTENT_PATH>` for project-specific paths
2. OR: note that these are TempoSample-specific paths and other projects will differ
3. The Rule 5 example could use a generic example instead

---

## Finding 8: `save_asset` fails for empty PrimaryDataAsset

**Section:** Asset Operations

**What happened:**
- `create_asset(asset_class="PrimaryDataAsset", package_path="/Game/TestBiomes", asset_name="TestAsset")` → SUCCESS
- `save_asset(asset_path="/Game/TestBiomes/TestAsset")` → FAIL: "Failed to save package"
- `save_actor_as_blueprint(...)` then `save_asset(...)` → WORKS fine

**Recommended fix:**
1. Add note: "Saving may fail for empty or abstract DataAssets. Use concrete subclasses (e.g., `BiomeDefinitionTemplate`) or populate properties before saving."
2. OR: improve the error message to explain why save failed

---

## Sections Tested Successfully

| Section | Status | Notes |
|---------|--------|-------|
| Getting Started / help() | PASS | All topics work, all topic names valid |
| help(topic="actors") | PASS | Content returned (wildcard issue in examples, see Finding 1) |
| help(topic="properties") | PASS | Content returned, comprehensive |
| help(topic="workflows") | PASS | Content returned, comprehensive |
| query_actors (no filter) | PASS | Returns actors correctly |
| query_actors (class_name) | PASS | Filters by class |
| query_actors (label_pattern) | PASS | Substring matching works |
| spawn_actor (PointLight) | PASS | Label, location, folder_path work |
| get_actor (components+properties) | PASS | Component instance names correct (LightComponent0) |
| get_property (component path) | PASS | `LightComponent0.Intensity` returns 5000.0 |
| set_property (simple value) | PASS | Intensity changed to 10000, verified |
| set_property (Unreal struct) | PASS | `(R=1,G=0,B=0,A=1)` color works, verified |
| get_transform | PASS | Returns location/rotation/scale |
| set_transform | PASS | Location and rotation applied correctly |
| set_transform (offset mode) | PASS | offset=true adds to current (100+50=150) |
| duplicate_actor | PASS | Copies properties, accepts new label/location |
| delete_actor (by label) | PASS | Works |
| attach / detach | PASS | Both work by label |
| list_classes (base_class_name) | PASS | Returns subclasses correctly |
| get_class_schema | PASS | Returns properties, respects include_inherited |
| get_landscape_bounds | PASS | Returns center, extent, min, max, proxy_count |
| is_world_partitioned | PASS | Returns true with world name |
| get_data_layers | PASS | Returns empty list (none configured) |
| query_landscape | PASS | Returns 65 proxies with streaming state |
| list_worlds | PASS | Returns Editor world info |
| search_console_commands | PASS | Returns 5 shadow commands out of 418 matches |
| execute_console_command | PASS | `stat fps` executed |
| call_function (static) | PASS | `KismetSystemLibrary::PrintString` works |
| list_project_directory | PASS | Lists Content/ correctly |
| create_asset | PASS | Creates PrimaryDataAsset |
| save_actor_as_blueprint | PASS | Converts actor to BP asset |
| save_asset (Blueprint) | PASS | Saves BP to disk |
| load_modules | PASS | Reports already loaded (full profile) |

---

## Not Tested (require PIE or project-specific setup)

| Section | Reason |
|---------|--------|
| Tempo Simulation (time, vehicles, pawns) | Requires `play_in_editor()` |
| bp_toolkit offline tools | Requires Windows paths to .uasset files |
| PCG Biome Workflow end-to-end | Requires project-specific template assets |
| Level management (save/open/new) | TempoCoreEditor service not active |

---

## Session Context (for resuming work)

**MCP Setup:** AgentBridge MCP connected via Claude Code user-level config. Wrapper script at `~/.claude/agentbridge-mcp.sh` handles CWD + exec. Config in `~/.claude.json` under `mcpServers.agentbridge`. Editor must be running on port 10001 before starting Claude Code.

**Test actors created and cleaned up:** TestLight, TestLight2, AttachParent all deleted. BP_TestLight and TestAsset created under `/Game/TestBiomes/` — still exist in editor.

**Doc fixes applied to AGENTS.md:**
- Finding 1 (wildcards): Removed all `*` wildcards from pattern examples, added note about substring matching
- Finding 2 (list_classes): Changed examples to use `base_class_name` instead of `name_pattern`
- Finding 3 (folder_path on query_actors): Replaced all with `label_pattern`
- Finding 4 (query_all_actors): Replaced all with `query_actors(..., include_unloaded=true)`
- Finding 5 (include_unloaded class filter): Added doc note about client-side filtering
- Finding 6 (get_current_level): No change (already documented limitation)
- Finding 7 (Hawaii paths): Replaced all `/CoreAIHawaiiTest/...` with correct `/PCGBiomeCore/...` paths
- Finding 8 (save_asset): No change (already documented limitation)
- Added default instance paths for BiomeDefinitionTemplate and BiomeAssetTemplate
- Added duplicate_asset examples using default instances
- Added note about projects providing custom template paths
- Fixed `get_actors_in_data_layer` (nonexistent) → `query_actors(data_layer=..., include_unloaded=true)`
- Added concrete class path for BP_PCGBiomeTexture in Phase 6 spawn example

**Remaining issues requiring C++ fixes (not doc-fixable):**
- Finding 2: `list_classes(name_pattern=...)` is exact-match only (should be substring like query_actors)
- Finding 3: `query_actors` has no `folder_path` parameter (useful feature to add)
- Finding 5: `query_actors(include_unloaded=true)` ignores `class_name` filter
