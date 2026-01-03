# Tempo Tool Consolidation Plan

> **Philosophy:** Users shouldn't care which library implements a feature.
> They should have ONE tool for each operation that "just works."

---

## Core Problem

Currently there are **two parallel APIs** for common operations:

| Operation | AgentBridge | Tempo |
|-----------|-------------|-------|
| Set property | `set_property` | `tempo_set_property` |
| Get actor info | `get_actor` | `tempo_get_actor_properties` |
| Spawn actor | `spawn_actor` | `tempo_spawn_actor` |
| Delete actor | `delete_actor` | `tempo_destroy_actor` |
| Transform | `set_actor_transform` | `tempo_set_actor_transform` |
| Query actors | `query_actors` | `tempo_get_all_actors` |
| Get components | `get_actor(include_components=True)` | `tempo_get_components` |

This forces users to:
1. Know which library to use
2. Choose between tools with similar names
3. Understand implementation details they shouldn't need to know

---

## Analysis: AgentBridge vs Tempo Capabilities

### Property Setting

| Capability | AgentBridge `set_property` | Tempo `tempo_set_property` |
|------------|---------------------------|---------------------------|
| Primitives (bool, int, float, string) | ✅ | ✅ |
| Vectors, Rotators, Colors | ✅ | ✅ |
| Nested paths (`Component.Property.Field`) | ✅ | ❌ |
| Structs (FVector, custom structs) | ✅ | ❌ |
| Arrays (TArray) | ✅ | ❌ |
| Maps (TMap) | ✅ | ❌ |
| Sets (TSet) | ✅ | ❌ |
| UObject references | ✅ | ✅ (asset paths only) |
| DataAsset properties | ✅ | ❌ |

**Winner:** AgentBridge - strictly more powerful

### Actor Spawning

| Feature | AgentBridge `spawn_actor` | Tempo `tempo_spawn_actor` |
|---------|--------------------------|--------------------------|
| class_name/type | ✅ | ✅ |
| location | ✅ | ✅ |
| rotation | ✅ | ✅ |
| scale | ✅ | ❌ |
| label | ✅ | ❌ |
| folder_path | ✅ | ❌ |
| **relative_to** | ❌ | ✅ |

**Winner:** AgentBridge has more features, but Tempo's `relative_to` is useful.
**Recommendation:** Add `relative_to` to AgentBridge, then remove Tempo version.

### Actor State

| Feature | AgentBridge `get_actor` | Tempo `tempo_get_actor_state` |
|---------|------------------------|------------------------------|
| Transform | ✅ | ✅ |
| Properties | ✅ | ❌ |
| Components | ✅ | ❌ |
| **Linear velocity** | ❌ | ✅ |
| **Angular velocity** | ❌ | ✅ |
| Bounds | ✅ (streaming actors) | ✅ |
| Timestamp | ❌ | ✅ |

**Winner:** Different purposes!
- AgentBridge: Editor-time property inspection
- Tempo: Runtime physics state during simulation

**Recommendation:** Keep `tempo_get_actor_state` - it's genuinely simulation-specific.

---

## Classification of Tempo Tools

### ❌ REDUNDANT - Remove These (AgentBridge is better)

| Tempo Tool | AgentBridge Equivalent | Why Remove |
|------------|----------------------|------------|
| `tempo_set_property` | `set_property` | AgentBridge handles structs, arrays, nested paths |
| `tempo_get_actor_properties` | `get_actor(include_properties=True)` | Same functionality |
| `tempo_get_component_properties` | `get_property` with paths | AgentBridge is more powerful |
| `tempo_get_all_actors` | `query_actors` | AgentBridge has more filters |
| `tempo_destroy_actor` | `delete_actor` | Same functionality |
| `tempo_set_actor_transform` | `set_actor_transform` | Same functionality |
| `tempo_get_components` | `get_actor(include_components=True)` | Same functionality |

**Impact:** -7 tools

### ⚠️ MERGE - Unique Features Should Be Added to AgentBridge

| Tempo Tool | Unique Feature | Action |
|------------|---------------|--------|
| `tempo_spawn_actor` | `relative_to` parameter | Add to AgentBridge, remove Tempo version |
| `tempo_add_component` | Add component dynamically | Add to AgentBridge |
| `tempo_call_function` | Call function on actor instance | Evaluate vs call_static_function |

**Impact:** -2 to -3 tools (after merging features)

### ✅ KEEP - Genuinely Tempo-Specific (Simulation)

These tools do things AgentBridge can't/shouldn't do:

#### Simulation Control (tempo_time.py)
- `tempo_play` - Start simulation
- `tempo_pause` - Pause simulation
- `tempo_step` - Single frame advance
- `tempo_advance_steps` - Multi-frame advance
- `tempo_set_time_mode` - WALL_CLOCK vs FIXED_STEP
- `tempo_set_sim_rate` - Simulation speed

#### Runtime State (tempo_world_state.py)
- `tempo_get_actor_state` - Velocity, bounds at runtime
- `tempo_get_actors_near` - Spatial queries during simulation

#### Geographic/Environment (tempo_geographic.py)
- `tempo_set_date` - Simulation date
- `tempo_set_time_of_day` - Sun position
- `tempo_set_day_cycle_rate` - Day/night speed
- `tempo_get_datetime` - Current sim time
- `tempo_set_geographic_reference` - Lat/lon/alt

#### Movement Control (tempo_movement.py)
- `tempo_get_commandable_vehicles` - List controllable vehicles
- `tempo_command_vehicle` - Send throttle/steering
- `tempo_get_commandable_pawns` - List AI pawns
- `tempo_pawn_move_to` - AI navigation
- `tempo_rebuild_navigation` - Rebuild navmesh

#### Level/Session Control (tempo_core.py, tempo_core_editor.py)
- `tempo_load_level` - Load level during simulation
- `tempo_finish_loading_level` - Complete deferred load
- `tempo_get_current_level` - Current level name
- `tempo_quit` - Exit application
- `tempo_set_viewport_render` - Toggle rendering
- `tempo_set_control_mode` - NONE/USER/OPEN_LOOP/CLOSED_LOOP
- `tempo_play_in_editor` - Start PIE
- `tempo_simulate` - Start simulate mode
- `tempo_stop` - Stop PIE/simulate
- `tempo_save_level` - Save to file
- `tempo_open_level` - Open in editor
- `tempo_new_level` - Create empty level

#### Sensors/Maps (tempo_labels.py, tempo_sensors.py, tempo_map_query.py)
- `tempo_get_label_map` - Semantic segmentation labels
- `tempo_get_available_sensors` - Camera list
- `tempo_get_lanes` - Road lane data
- `tempo_get_lane_accessibility` - Traffic signals
- `tempo_get_zones` - Zone boundaries
- `tempo_run_zone_graph_builder` - Build zone graph

---

## Proposed New Module Structure

### Module: `core` (Always Loaded)
Everything a user needs for basic editor work:
- `help`, `list_worlds`, `set_target_world`
- `query_actors`, `get_actor`, `spawn_actor`, `delete_actor`
- `get_property`, `set_property` ← **THE one property setter**
- `set_actor_transform`, `duplicate_actor`
- `list_classes`, `get_class_schema`
- `add_component` ← moved from Tempo

### Module: `simulation` (For Tempo Users)
Simulation control:
- `tempo_play`, `tempo_pause`, `tempo_step`, `tempo_advance_steps`
- `tempo_set_time_mode`, `tempo_set_sim_rate`

### Module: `tempo_state` (For Simulation Queries)
Runtime state (velocity, spatial):
- `tempo_get_actor_state`, `tempo_get_actors_near`

### Module: `tempo_environment` (For Geographic/Time)
- `tempo_set_date`, `tempo_set_time_of_day`, `tempo_set_day_cycle_rate`
- `tempo_get_datetime`, `tempo_set_geographic_reference`

### Module: `tempo_movement` (For AI/Vehicle Control)
- `tempo_command_vehicle`, `tempo_pawn_move_to`
- `tempo_get_commandable_vehicles`, `tempo_get_commandable_pawns`
- `tempo_rebuild_navigation`

### Module: `tempo_levels` (For Level Management)
- `tempo_load_level`, `tempo_finish_loading_level`
- `tempo_get_current_level`, `tempo_quit`
- `tempo_set_viewport_render`, `tempo_set_control_mode`

### Module: `tempo_editor` (For PIE Control)
- `tempo_play_in_editor`, `tempo_simulate`, `tempo_stop`
- `tempo_save_level`, `tempo_open_level`, `tempo_new_level`

### Module: `tempo_sensors` (For Perception)
- `tempo_get_label_map`, `tempo_get_available_sensors`
- `tempo_get_lanes`, `tempo_get_lane_accessibility`
- `tempo_get_zones`, `tempo_run_zone_graph_builder`

---

## Implementation Plan

### Phase 1: Remove Redundant Tools (Python Only)
1. Remove from `tempo_actor_control.py`:
   - `tempo_set_property`
   - `tempo_get_actor_properties`
   - `tempo_get_component_properties`
   - `tempo_get_all_actors`
   - `tempo_destroy_actor`
   - `tempo_set_actor_transform`
   - `tempo_get_components`

2. Update `__init__.py` MODULES to reflect changes

3. Keep the client methods (Tempo still uses them internally)

### Phase 2: Merge Unique Features into AgentBridge
1. Add `relative_to` parameter to AgentBridge `spawn_actor`
   - Requires C++ changes in CommandExecutor
   - Then remove `tempo_spawn_actor`

2. Add `add_component` to AgentBridge
   - Requires new gRPC RPC and C++ handler
   - Then remove `tempo_add_component`

### Phase 3: Rename Modules for Clarity
- `tempo_actors` → merged into `core`
- `simulation` → `tempo_simulation`
- Keep "tempo_" prefix only for genuinely Tempo-specific features

---

## Impact Summary

| Metric | Before | After | Change |
|--------|--------|-------|--------|
| Total Tempo actor tools | 10 | 0-2 | -8 to -10 |
| Tempo simulation tools | ~30 | ~30 | 0 |
| User confusion | High | Low | ✅ |
| Duplicate functionality | Yes | No | ✅ |

---

## Questions to Resolve

1. **Should `tempo_spawn_actor`'s `relative_to` be added to AgentBridge?**
   - Pro: Consolidation
   - Con: Requires C++ changes

2. **Should `tempo_add_component` move to AgentBridge?**
   - Pro: Useful for editor workflows
   - Con: Requires new gRPC endpoint

3. **Is `tempo_call_function` different enough from `call_static_function`?**
   - `call_static_function`: Blueprint library static functions
   - `tempo_call_function`: Instance methods on actors
   - May need both, but with clearer names

---

## Post-Consolidation Tasks

After the tool consolidation is complete, we'll need to:

### 1. Redo Module Definitions
- Update `MODULES` dict in `services/__init__.py`
- Remove references to deleted tools
- Reorganize Tempo tools into logical groups
- Consider renaming modules for clarity (e.g., `tempo_actors` → remove entirely)

### 2. Redo Tool Descriptions
- Update remaining tool descriptions for consistency
- Ensure no references to "alternative" tools that were removed
- Update help text to reflect consolidated workflow
- Re-run Phase 1 compression on any new/modified descriptions

### 3. Update Documentation
- Update `README.md` with new tool list
- Update `Python/CLAUDE.md` with module changes
- Update help() topics to remove mentions of deleted tools
- Add migration notes for any existing scripts using old tool names

---

## Status

**APPROVED** - All changes approved by user (2026-01-03)

- [x] Analysis complete
- [ ] Phase 1: Remove 7 redundant tools (Python only)
- [ ] Phase 2: Add `relative_to` to AgentBridge spawn_actor (C++ change)
- [ ] Phase 3: Move `add_component` to AgentBridge (new gRPC endpoint)
- [ ] Post: Redo modules and tool descriptions

---

*Implementation can proceed.*
