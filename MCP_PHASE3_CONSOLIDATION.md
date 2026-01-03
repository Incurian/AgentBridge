# Phase 3: Tool Consolidation Deep Dive

> **Goal:** Reduce tool count while preserving ALL functionality
>
> **Critical:** Before consolidating, analyze what each tool does under the hood.
> Similar names ≠ redundant functionality.

---

## Analysis Approach

For each proposed consolidation:

1. **Examine current implementation** - What gRPC calls does each tool make?
2. **Identify unique functionality** - What does each tool provide that others don't?
3. **Design consolidated interface** - How to expose all functionality with one tool?
4. **Identify required changes** - What modifications needed in lower modules?

---

## Consolidation Analysis

### 1. Tempo Typed Property Setters (9 → 1 tool)

#### Current Tools

| Tool | gRPC Method | Value Type |
|------|-------------|------------|
| `tempo_set_float_property` | `SetFloatProperty` | `float` |
| `tempo_set_int_property` | `SetIntProperty` | `int` |
| `tempo_set_bool_property` | `SetBoolProperty` | `bool` |
| `tempo_set_string_property` | `SetStringProperty` | `string` |
| `tempo_set_vector_property` | `SetVectorProperty` | `x, y, z` (separate params) |
| `tempo_set_rotator_property` | `SetRotatorProperty` | `roll, pitch, yaw` (separate) |
| `tempo_set_color_property` | `SetColorProperty` | `r, g, b` (0-255) |
| `tempo_set_asset_property` | `SetAssetProperty` | `string` (path) |

#### Implementation Details

Each tool in `tempo_actor_control.py` calls a **separate gRPC RPC** on Tempo's ActorControlService:

```python
# tempo_set_float_property -> calls:
self.stub.SetFloatProperty(pb.SetFloatPropertyRequest(
    actor=actor, component=component, property=property, value=value
))

# tempo_set_vector_property -> calls:
self.stub.SetVectorProperty(pb.SetVectorPropertyRequest(
    actor=actor, component=component, property=property, x=x, y=y, z=z
))
```

**Key Insight:** These are Tempo's gRPC methods, not AgentBridge's. We cannot modify Tempo's proto.

#### Consolidation Strategy

**Python-level unification.** The unified tool detects value type and dispatches to the appropriate gRPC call.

```python
{
    "name": "tempo_set_property",
    "description": "Set property on actor/component. Type auto-detected from value.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "actor": {"type": "string"},
            "property": {"type": "string"},
            "value": {},  # Any type - detected automatically
            "component": {"type": "string"}
        },
        "required": ["actor", "property", "value"]
    }
}
```

#### Type Detection Logic

```python
def _detect_and_set_property(client, actor, property, value, component=""):
    """Detect value type and call appropriate Tempo method."""

    # Boolean
    if isinstance(value, bool):
        return client.set_bool_property(actor, property, value, component)

    # Integer
    if isinstance(value, int) and not isinstance(value, bool):
        return client.set_int_property(actor, property, value, component)

    # Float
    if isinstance(value, float):
        return client.set_float_property(actor, property, value, component)

    # String (could be asset path or plain string)
    if isinstance(value, str):
        if value.startswith("/Game/") or value.startswith("/Script/"):
            return client.set_asset_property(actor, property, value, component)
        return client.set_string_property(actor, property, value, component)

    # Array - could be vector, rotator, or color
    if isinstance(value, (list, tuple)):
        if len(value) == 3:
            # Ambiguous: could be vector, rotator, or RGB color
            # Check property name for hints
            prop_lower = property.lower()
            if 'color' in prop_lower:
                return client.set_color_property(actor, property,
                    int(value[0]), int(value[1]), int(value[2]), component)
            elif 'rotation' in prop_lower or 'rotator' in prop_lower:
                return client.set_rotator_property(actor, property,
                    value[0], value[1], value[2], component)
            else:
                # Default to vector
                return client.set_vector_property(actor, property,
                    value[0], value[1], value[2], component)
        elif len(value) == 4:
            # RGBA color
            return client.set_color_property(actor, property,
                int(value[0]), int(value[1]), int(value[2]), component)

    # Dict - check keys
    if isinstance(value, dict):
        if 'x' in value and 'y' in value and 'z' in value:
            return client.set_vector_property(actor, property,
                value['x'], value['y'], value['z'], component)
        elif 'r' in value and 'g' in value and 'b' in value:
            return client.set_color_property(actor, property,
                int(value['r']), int(value['g']), int(value['b']), component)
        elif 'pitch' in value or 'yaw' in value or 'roll' in value:
            return client.set_rotator_property(actor, property,
                value.get('roll', 0), value.get('pitch', 0), value.get('yaw', 0), component)

    raise ValueError(f"Cannot determine property type for value: {value}")
```

#### Edge Cases & Fallback

Add optional `type_hint` parameter for ambiguous cases:

```python
{
    "properties": {
        # ... existing properties ...
        "type_hint": {
            "type": "string",
            "enum": ["float", "int", "bool", "string", "vector", "rotator", "color", "asset"],
            "description": "Force type interpretation (usually auto-detected)"
        }
    }
}
```

#### Required Changes

| Layer | Change |
|-------|--------|
| `tempo_actor_control.py` | Add `_detect_and_set_property()`, new `tempo_set_property` handler |
| TOOLS list | Replace 8 tool definitions with 1 |
| gRPC layer | **None** - still calls same Tempo RPCs |
| C++ | **None** - Tempo handles it |

#### Functionality Preserved

| Original Tool | Consolidated Equivalent |
|---------------|-------------------------|
| `tempo_set_float_property(actor, prop, 5.0)` | `tempo_set_property(actor, prop, 5.0)` |
| `tempo_set_vector_property(actor, prop, x=1, y=2, z=3)` | `tempo_set_property(actor, prop, [1, 2, 3])` |
| `tempo_set_color_property(actor, prop, r=255, g=0, b=0)` | `tempo_set_property(actor, prop, [255, 0, 0])` |
| `tempo_set_color_property(actor, "LightColor", ...)` | `tempo_set_property(actor, "LightColor", [255, 0, 0])` *(auto-detected)* |

**Savings:** 8 tools eliminated, ~2,400 tokens saved (with compression).

---

### 2. Query Tools (3 → 1 tool)

#### Current Tools

| Tool | gRPC RPC | Unique Features |
|------|----------|-----------------|
| `query_actors` | `QueryActors` | Fast query of loaded actors only |
| `query_all_actors` | `QueryAllActors` | Includes unloaded WP actors, streaming state |
| `get_actors_in_data_layer` | `GetActorsInDataLayer` | Filters specifically by data layer |

#### Implementation Comparison

```python
# query_actors - Fast, loaded actors only
def query_actors(class_name, name_pattern, label_pattern, tag, limit, include_hidden):
    return self.stub.QueryActors(QueryActorsRequest(...))
    # Returns: ActorDescriptor[] with basic info

# query_all_actors - Comprehensive, includes unloaded
def query_all_actors(class_name, name_pattern, include_loaded, include_unloaded, data_layer, limit):
    return self.stub.QueryAllActors(QueryAllActorsRequest(...))
    # Returns: StreamingActorInfo[] with streaming_state, bounds, data_layers

# get_actors_in_data_layer - Data layer specific
def get_actors_in_data_layer(data_layer, include_unloaded, limit):
    return self.stub.GetActorsInDataLayer(GetActorsInDataLayerRequest(...))
    # Returns: StreamingActorInfo[]
```

#### Key Differences

| Feature | query_actors | query_all_actors | get_actors_in_data_layer |
|---------|--------------|------------------|--------------------------|
| Loaded actors | ✓ | ✓ (optional) | ✓ (optional) |
| Unloaded actors | ✗ | ✓ (optional) | ✓ (optional) |
| Streaming state | ✗ | ✓ | ✓ |
| Bounds info | ✗ | ✓ | ✓ |
| Data layer info | ✗ | ✓ | ✓ (required filter) |
| Response type | `ActorDescriptor` | `StreamingActorInfo` | `StreamingActorInfo` |
| Performance | Fastest | Slower | Moderate |

#### Consolidation Strategy

Unify into one tool with mode selection. Default to fast mode, opt-in to streaming queries.

```python
{
    "name": "query_actors",
    "description": "Find actors. Add include_unloaded=true for streaming actors.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "class_name": {"type": "string"},
            "name_pattern": {"type": "string"},
            "label_pattern": {"type": "string"},
            "tag": {"type": "string"},
            "data_layer": {"type": "string"},
            "include_unloaded": {"type": "boolean", "default": false},
            "include_hidden": {"type": "boolean", "default": false},
            "limit": {"type": "integer", "default": 100}
        }
    }
}
```

#### Routing Logic

```python
def execute_query_actors(client, args):
    data_layer = args.get("data_layer")
    include_unloaded = args.get("include_unloaded", False)

    # Route 1: Data layer query (most specific)
    if data_layer:
        response = client.get_actors_in_data_layer(
            data_layer=data_layer,
            include_unloaded=include_unloaded,
            limit=args.get("limit", 100)
        )
        return format_streaming_actors(response)

    # Route 2: All actors query (includes unloaded)
    if include_unloaded:
        response = client.query_all_actors(
            class_name=args.get("class_name", ""),
            name_pattern=args.get("name_pattern", ""),
            include_loaded=True,
            include_unloaded=True,
            limit=args.get("limit", 100)
        )
        return format_streaming_actors(response)

    # Route 3: Fast loaded-only query (default)
    response = client.query_actors(
        class_name=args.get("class_name", ""),
        name_pattern=args.get("name_pattern", ""),
        label_pattern=args.get("label_pattern", ""),
        tag=args.get("tag", ""),
        limit=args.get("limit", 100),
        include_hidden=args.get("include_hidden", False)
    )
    return format_basic_actors(response)
```

#### Required Changes

| Layer | Change |
|-------|--------|
| `agentbridge.py` | Modify `query_actors` handler to route based on params |
| TOOLS list | Remove `query_all_actors`, `get_actors_in_data_layer` definitions |
| gRPC layer | **None** - same RPCs, just routed differently |
| C++ | **None** |

#### Functionality Preserved

| Original | Consolidated Equivalent |
|----------|------------------------|
| `query_actors(class_name="PointLight")` | `query_actors(class_name="PointLight")` |
| `query_all_actors(include_unloaded=True)` | `query_actors(include_unloaded=True)` |
| `get_actors_in_data_layer(data_layer="Foliage")` | `query_actors(data_layer="Foliage")` |
| `query_all_actors(data_layer="Foliage", include_unloaded=True)` | `query_actors(data_layer="Foliage", include_unloaded=True)` |

**Savings:** 2 tools eliminated, ~800 tokens saved.

---

### 3. File Operations (4 → 1 tool)

#### Current Tools

| Tool | gRPC RPC | Function |
|------|----------|----------|
| `read_project_file` | `ReadProjectFile` | Read file content |
| `write_project_file` | `WriteProjectFile` | Write file content |
| `list_project_directory` | `ListProjectDirectory` | List files with glob |
| `copy_project_file` | `CopyProjectFile` | Copy file |

#### Consolidation Strategy

```python
{
    "name": "project_file",
    "description": "File operations: read, write, list, copy.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "action": {"type": "string", "enum": ["read", "write", "list", "copy"]},
            "path": {"type": "string"},
            "content": {"type": "string"},     # for write
            "dest": {"type": "string"},        # for copy
            "pattern": {"type": "string"},     # for list
            "recursive": {"type": "boolean"},  # for list
            "append": {"type": "boolean"},     # for write
            "as_base64": {"type": "boolean"}   # for read/write
        },
        "required": ["action", "path"]
    }
}
```

#### Routing Logic

```python
def execute_project_file(client, args):
    action = args["action"]
    path = args["path"]

    if action == "read":
        return client.read_project_file(path, args.get("as_base64", False))

    elif action == "write":
        return client.write_project_file(
            path,
            args.get("content", ""),
            args.get("is_base64", False),
            args.get("create_directories", True),
            args.get("append", False)
        )

    elif action == "list":
        return client.list_project_directory(
            path,
            args.get("pattern", "*"),
            args.get("recursive", False),
            args.get("limit", 100)
        )

    elif action == "copy":
        return client.copy_project_file(
            path,  # source
            args.get("dest", ""),
            args.get("overwrite", False)
        )
```

#### Required Changes

| Layer | Change |
|-------|--------|
| `agentbridge.py` | Modify file handlers, consolidate into one |
| TOOLS list | Replace 4 definitions with 1 |
| gRPC layer | **None** |
| C++ | **None** |

**Savings:** 3 tools eliminated, ~1,200 tokens saved.

---

### 4. Asset Operations (4 → 1 tool)

#### Current Tools

| Tool | gRPC RPC | Function |
|------|----------|----------|
| `create_asset` | `CreateAsset` | Create DataAsset, MaterialInstance, etc. |
| `save_asset` | `SaveAsset` | Save modified asset to disk |
| `duplicate_asset` | `DuplicateAsset` | Copy asset with new name |
| `save_actor_as_blueprint` | `SaveActorAsBlueprint` | Convert actor to Blueprint |

#### Unique Aspects

Each serves a distinct purpose and has different required parameters:
- `create_asset`: Needs `asset_class`, `package_path`, `asset_name`
- `save_asset`: Just needs `asset_path`
- `duplicate_asset`: Needs `source_path`, `dest_package_path`, `dest_asset_name`
- `save_actor_as_blueprint`: Needs `actor_id`, `package_path`, `blueprint_name`

#### Consolidation Strategy

```python
{
    "name": "asset",
    "description": "Asset operations: create, save, duplicate, actor_to_bp.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "action": {"type": "string", "enum": ["create", "save", "duplicate", "actor_to_bp"]},
            "path": {"type": "string"},           # Asset path (for save/duplicate source)
            "class": {"type": "string"},          # For create
            "package_path": {"type": "string"},   # Destination folder
            "name": {"type": "string"},           # Asset/blueprint name
            "actor_id": {"type": "string"},       # For actor_to_bp
            "parent": {"type": "string"},         # For create (parent asset)
            "properties": {"type": "object"}      # For create (initial properties)
        },
        "required": ["action"]
    }
}
```

#### Required Changes

| Layer | Change |
|-------|--------|
| `agentbridge.py` | Consolidate asset handlers |
| TOOLS list | Replace 4 definitions with 1 |
| gRPC layer | **None** |
| C++ | **None** |

**Savings:** 3 tools eliminated, ~1,200 tokens saved.

---

### 5. Component Operations (6 → 2 tools)

#### Current Tools

| Tool | gRPC RPC | Function |
|------|----------|----------|
| `get_component_transform` | `GetComponentTransform` | Read transform |
| `set_component_transform` | `SetComponentTransform` | Write transform |
| `attach_actor` | `AttachActor` | Attach actor to actor |
| `detach_actor` | `DetachActor` | Detach actor |
| `attach_component` | `AttachComponent` | Attach component to component |
| `detach_component` | `DetachComponent` | Detach component |

#### Consolidation: Transform Operations

Get and set can be combined - if transform values provided, set; otherwise, get.

```python
{
    "name": "component_transform",
    "description": "Get/set component transform. Omit transform to get current.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "actor_id": {"type": "string"},
            "component": {"type": "string"},
            "location": {"type": "array"},      # Set if provided
            "rotation": {"type": "array"},
            "scale": {"type": "array"},
            "world_space": {"type": "boolean", "default": true}
        },
        "required": ["actor_id", "component"]
    }
}
```

#### Consolidation: Attachment Operations

```python
{
    "name": "attach",
    "description": "Attach/detach actors or components.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "action": {"type": "string", "enum": ["attach", "detach"]},
            "actor_id": {"type": "string"},
            "parent_actor_id": {"type": "string"},   # For actor attach
            "component": {"type": "string"},         # For component ops
            "parent_component": {"type": "string"},  # For component attach
            "socket": {"type": "string"},
            "keep_world_transform": {"type": "boolean", "default": true}
        },
        "required": ["action", "actor_id"]
    }
}
```

#### Routing Logic

```python
def execute_attach(client, args):
    action = args["action"]
    actor_id = args["actor_id"]
    component = args.get("component")

    if action == "attach":
        parent_actor = args.get("parent_actor_id")
        parent_component = args.get("parent_component")

        # Component-to-component attachment
        if component and parent_component:
            return client.attach_component(actor_id, component, parent_component,
                socket_name=args.get("socket", ""),
                location_rule="keep_world" if args.get("keep_world_transform", True) else "keep_relative"
            )

        # Actor-to-actor attachment
        if parent_actor:
            return client.attach_actor(actor_id, parent_actor,
                parent_component_name=parent_component or "",
                socket_name=args.get("socket", ""),
                location_rule="keep_world" if args.get("keep_world_transform", True) else "keep_relative"
            )

    elif action == "detach":
        # Component detachment
        if component:
            return client.detach_component(actor_id, component,
                maintain_world_transform=args.get("keep_world_transform", True)
            )

        # Actor detachment
        return client.detach_actor(actor_id,
            maintain_world_position=args.get("keep_world_transform", True)
        )
```

#### Required Changes

| Layer | Change |
|-------|--------|
| `agentbridge.py` | Consolidate handlers |
| TOOLS list | Replace 6 definitions with 2 |
| gRPC layer | **None** |
| C++ | **None** |

**Savings:** 4 tools eliminated, ~1,600 tokens saved.

---

### 6. Simulation Control (6 → 1 tool)

#### Current Tools (tempo_time.py)

| Tool | gRPC RPC | Function |
|------|----------|----------|
| `tempo_play` | `Play` | Start/resume simulation |
| `tempo_pause` | `Pause` | Pause simulation |
| `tempo_step` | `Step` | Advance one frame |
| `tempo_advance_steps` | `AdvanceSteps` | Advance N frames |
| `tempo_set_time_mode` | `SetTimeMode` | WALL_CLOCK or FIXED_STEP |
| `tempo_set_sim_rate` | `SetSimRate` | Steps per second |

#### Consolidation Strategy

```python
{
    "name": "simulation",
    "description": "Control simulation: play, pause, step, set timing.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "action": {"type": "string", "enum": ["play", "pause", "step", "status"]},
            "steps": {"type": "integer"},                  # For step action
            "time_mode": {"type": "string", "enum": ["wall_clock", "fixed_step"]},
            "rate": {"type": "number"}                     # Steps per second
        }
    }
}
```

Calling with just `action` performs that action. Calling with `time_mode` or `rate` sets those values.

**Savings:** 5 tools eliminated, ~1,500 tokens saved.

---

### 7. Editor Control (6 → 1 tool)

#### Current Tools (tempo_core_editor.py)

| Tool | Function |
|------|----------|
| `tempo_play_in_editor` | Start PIE |
| `tempo_simulate` | Start simulate mode |
| `tempo_stop` | Stop PIE/simulate |
| `tempo_save_level` | Save level |
| `tempo_open_level` | Open level |
| `tempo_new_level` | Create empty level |

#### Consolidation Strategy

```python
{
    "name": "editor",
    "description": "Editor session: pie, simulate, stop, save/open/new level.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "action": {"type": "string", "enum": ["pie", "simulate", "stop", "save", "open", "new"]},
            "path": {"type": "string"}    # For save/open
        },
        "required": ["action"]
    }
}
```

**Savings:** 5 tools eliminated, ~1,500 tokens saved.

---

### 8. bp_toolkit Consolidation (14 → 5 tools)

#### Proposed Groupings

| New Tool | Merges | Purpose |
|----------|--------|---------|
| `bp_io` | bp_export_asset, bp_import_asset | Import/export uasset ↔ JSON |
| `bp_info` | bp_detect_type, bp_get_info, bp_list_properties | Asset analysis |
| `bp_property` | bp_get_property, bp_set_property | Property access |
| `bp_asset` | bp_clone_asset, bp_list_graphs | Asset manipulation |
| `bp_edit` | bp_add_comment, bp_clone_node, bp_find, bp_query, bp_parse | Graph editing/query |

**Savings:** 9 tools eliminated, ~2,000 tokens saved.

---

### 9. Blueprint Node Tools (6 tools) - Analysis

#### Current Tools

| Tool | Function | Implementation |
|------|----------|----------------|
| `bp_create_node` | Create node (CallFunction, Event, Variable, etc.) | gRPC `CreateBlueprintNode` |
| `bp_connect_pins` | Connect two node pins | gRPC `ConnectBlueprintPins` |
| `bp_disconnect_pins` | Disconnect two node pins | gRPC `DisconnectBlueprintPins` |
| `bp_delete_node` | Delete a node | gRPC `DeleteBlueprintNode` |
| `bp_list_nodes` | List all nodes in graph | gRPC `ListBlueprintNodes` |
| `bp_list_pins` | List pins on a node | gRPC `ListBlueprintPins` |

#### Consolidation Assessment

**Recommendation: Keep separate.** These tools represent distinct operations with clear semantics:

- **CRUD pattern**: Create, connect, disconnect, delete are fundamental operations
- **Query tools**: list_nodes and list_pins are discovery operations
- **Parameters differ significantly**: create_node has many type-specific params, connect has pin params

**Potential minor consolidation:**
- `bp_connect_pins` and `bp_disconnect_pins` could merge into `bp_pin_connection(action="connect"|"disconnect")`
- Saves: 1 tool (~300 tokens)

**Savings if consolidated:** 1 tool, ~300 tokens (not recommended - minimal gain, reduced clarity)

---

### 10. PCG Graph Tools (6 tools) - Analysis

#### Current Tools

| Tool | Function | Implementation |
|------|----------|----------------|
| `pcg_add_node` | Add node to PCG graph | `call_asset_function(AddNodeOfType)` |
| `pcg_connect` | Connect PCG nodes | `call_asset_function(AddEdge)` |
| `pcg_disconnect` | Disconnect PCG nodes | `call_asset_function(RemoveEdge)` |
| `pcg_delete_node` | Delete PCG node | `call_asset_function(RemoveNode)` |
| `pcg_list_nodes` | List nodes with pins | Multiple `call_asset_function` calls |
| `pcg_get_input_output_nodes` | Get InputNode/OutputNode | `call_asset_function(GetInputNode/GetOutputNode)` |

#### Consolidation Assessment

**Recommendation: Keep separate.** Same reasoning as Blueprint tools:

- **CRUD pattern**: Add, connect, disconnect, delete
- **Special tools**: list_nodes and get_input_output_nodes serve discovery
- **PCG-specific semantics**: Each maps to PCG graph API methods

**Potential minor consolidation:**
- `pcg_connect` and `pcg_disconnect` could merge into `pcg_edge(action="add"|"remove")`
- Saves: 1 tool (~300 tokens)

**Savings if consolidated:** 1 tool, ~300 tokens (not recommended)

---

### Assessment: New Tools Don't Need Consolidation

The Blueprint and PCG tools were designed with consolidation principles in mind:

1. **Each tool has a clear single purpose** - No redundant operations
2. **Parameter sets are unique** - Different inputs required for each
3. **Clear naming** - Action is obvious from the name
4. **Consistency** - BP and PCG tools mirror each other's patterns

These 12 tools should remain as-is. Consolidating them would reduce clarity without meaningful token savings.

---

## Summary of Consolidations

| Category | Before | After | Eliminated | Token Savings |
|----------|--------|-------|------------|---------------|
| Tempo typed setters | 9 | 1 | 8 | ~2,400 |
| Query tools | 3 | 1 | 2 | ~800 |
| File operations | 4 | 1 | 3 | ~1,200 |
| Asset operations | 4 | 1 | 3 | ~1,200 |
| Component operations | 6 | 2 | 4 | ~1,600 |
| Simulation control | 6 | 1 | 5 | ~1,500 |
| Editor control | 6 | 1 | 5 | ~1,500 |
| bp_toolkit | 14 | 5 | 9 | ~2,000 |
| Blueprint nodes | 6 | 6 | 0 | 0 |
| PCG graph | 6 | 6 | 0 | 0 |
| **Total** | **64** | **25** | **39** | **~12,200** |

*Note: Blueprint and PCG tools (12 total) are already well-designed and don't benefit from consolidation.*

---

## Implementation Priority

### Phase 3a: Python-Only Consolidations (No C++ Changes)

These only require changes to Python code:

1. **Tempo typed setters** - Highest impact, straightforward
2. **Query tools** - High impact, moderate complexity
3. **File operations** - Simple routing
4. **Asset operations** - Simple routing
5. **Component operations** - Moderate complexity
6. **Simulation/Editor control** - Simple routing
7. **bp_toolkit** - Independent, can be done anytime

### Phase 3b: Potential Proto Changes (Optional, Requires C++ Changes)

Some future optimizations could benefit from proto changes:

| Optimization | Proto Change | Benefit |
|--------------|--------------|---------|
| Unified property setter | Add `SetPropertyGeneric` RPC | Single call, server-side type detection |
| Batch operations | Add `BatchCommand` RPC | Multiple operations in one call |
| Streaming queries | Add server-side filtering | Less data transfer |

**Recommendation:** Phase 3a provides 95% of the benefit with no C++ changes. Phase 3b is optional future work.

---

## Required File Changes

### Python Changes Only

| File | Changes |
|------|---------|
| `tempo_actor_control.py` | Add type detection, replace 8 tools with 1 |
| `agentbridge.py` TOOLS | Consolidate definitions |
| `agentbridge.py` execute | Add routing logic for consolidated tools |
| `tempo_time.py` | Consolidate simulation tools |
| `tempo_core_editor.py` | Consolidate editor tools |
| `bp_toolkit.py` | Consolidate bp_toolkit tools |

### No Changes Required

| Layer | Why |
|-------|-----|
| `AgentBridge.proto` | Same RPCs, just called differently |
| `AgentBridgeServiceSubsystem.cpp` | No new handlers needed |
| `CommandExecutor.cpp` | No new commands needed |
| `AgentBridgeRuntime` | No changes |
| `AgentBridgeCore` | No changes |

---

## Testing Checklist

For each consolidation:

- [ ] All original functionality accessible via new interface
- [ ] Type detection works correctly for edge cases
- [ ] Error messages still helpful
- [ ] help() topics updated to reflect new tool
- [ ] Old tool names documented as deprecated (optional)

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| Type detection fails | Add `type_hint` fallback parameter |
| Agent confusion from fewer tools | Update help() with clear examples |
| Breaking existing scripts | Keep old tool names as aliases (optional) |
| Missing edge cases | Thorough testing with real workflows |

---

*Phase 3 complete. Implementation can proceed with Python-only changes.*
