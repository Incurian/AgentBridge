# Biome-Inspired Fixes Plan

> Improvements identified while testing the PCG Biome workflow.
> Goal: Make MCP tools intuitive enough that users don't need to know Unreal implementation details.

---

## Executive Summary

Testing the PCG Biome workflow via MCP tools revealed two categories of friction:

1. **Missing Workflow Documentation** - Users don't know what actors to spawn, what properties to set, or how to calculate correct values.

2. **Leaky Abstractions** - Users must know Unreal internals (Blueprint `_C` suffix, `TObjectPtr` vs `TSoftObjectPtr`, array index syntax, etc.) to use tools correctly.

This document outlines fixes for both categories.

---

## Part 1: Workflow Documentation

### Problem

A user who says "set up PCG Biome on my landscape" currently needs to:
1. Discover which actors to spawn (trial and error)
2. Find correct Blueprint paths with `_C` suffix
3. Calculate landscape bounds manually (complex algorithm)
4. Know which properties to set and in what format
5. Understand the relationship between actors

### Solution: Workflow Help Topic

Add a new `help(topic="workflows/pcg-biome")` that provides step-by-step guidance.

**Proposed Content:**

```
## PCG Biome Setup Workflow

### Overview
The PCG Biome system populates landscapes with biome-specific meshes (trees, rocks, etc.)
based on a texture map that defines biome regions.

### Required Actors

| Actor | Purpose |
|-------|---------|
| BP_PCGBiomeCore | Main PCG generation volume |
| BP_PCGBiomeTexture | Alternative that uses texture-based biome mapping |

### Step 1: Spawn the Biome Actor

```python
spawn_actor(
    class_name="BP_PCGBiomeTexture",  # or BP_PCGBiomeCore
    location=[0, 0, 0],
    label="MyBiome"
)
```

### Step 2: Size Volume to Cover Landscape

```python
# Get landscape bounds first
bounds = get_landscape_bounds()

# Set transform to cover entire landscape
set_actor_transform(
    actor_id="MyBiome",
    location=bounds.center,
    scale=[bounds.extent_x / 100, bounds.extent_y / 100, bounds.extent_z / 100]
)
```

### Step 3: Assign Biome Configuration

```python
# Assign definition (what biomes exist)
set_asset_property(
    actor="MyBiome",
    property="Definition",
    value="/PCGBiomeSample/BiomeDefinitions/BroadleafForest"
)

# Assign assets (what meshes to spawn)
set_asset_property(
    actor="MyBiome",
    property="Assets[0]",
    value="/PCGBiomeSample/BiomeAssets/BroadleafForest"
)

# Assign texture (where each biome goes)
set_asset_property(
    actor="MyBiome",
    property="BiomeTexture",
    value="/Game/Textures/BiomeMap"
)
```

### Step 4: Generate

```python
call_function(actor="MyBiome", component="PCG", function="Generate")
```

### Available Sample Content

| Type | Path |
|------|------|
| BiomeDefinitions | `/PCGBiomeSample/BiomeDefinitions/*` |
| BiomeAssets | `/PCGBiomeSample/BiomeAssets/*` |
| Generators | `/PCGBiomeSample/BiomeGenerators/*` |
```

### Implementation Tasks

| Task | Priority | Effort |
|------|----------|--------|
| Add `workflows/pcg-biome` help topic | P1 | 1 hour |
| Add `get_landscape_bounds` command | P1 | 4 hours |
| Document other workflows (lighting, materials, etc.) | P2 | 2 hours each |

---

## Part 2: Tool Improvements

### 2.1 Blueprint Class Name Normalization

**Problem:** Users must know to add `_C` suffix to Blueprint class names.

```python
# Current - fails
spawn_actor(class_name="BP_PCGBiomeCore")

# Required - works
spawn_actor(class_name="/PCGBiomeCore/Blueprints/BP_PCGBiomeCore.BP_PCGBiomeCore_C")
```

**Solution:** Automatically normalize Blueprint class references.

**Algorithm:**
```python
def normalize_class_name(name: str) -> str:
    # Already a full path with _C suffix - use as-is
    if name.endswith("_C") and "/" in name:
        return name

    # Starts with BP_ but no path - search for it
    if name.startswith("BP_") and "/" not in name:
        # Try to find the Blueprint class
        found = find_class_by_name(name)
        if found:
            return found
        # If not found, try with _C suffix
        found = find_class_by_name(name + "_C")
        if found:
            return found

    # Native class (PointLight, StaticMeshActor) - use as-is
    return name
```

**Implementation Location:**
- `AgentBridgeScripting/CommandExecutor.cpp` - `NormalizeBlueprintClassName()` helper
- Called from `SpawnActor`, `ListClasses`, `GetClassSchema`

| Task | Priority | Effort |
|------|----------|--------|
| Add `NormalizeBlueprintClassName()` helper | P0 | 2 hours |
| Apply to spawn_actor | P0 | 30 min |
| Apply to list_classes | P1 | 30 min |
| Apply to get_class_schema | P1 | 30 min |

---

### 2.2 Unified Property Setter

**Problem:** Users must choose between multiple property setters based on property type:
- `set_property` - Generic string-based
- `tempo_set_float_property` - For floats
- `tempo_set_color_property` - For colors
- `tempo_set_asset_property` - For object references
- None work for `TSoftObjectPtr`

**Solution:** Single `set_property` that auto-detects type and routes correctly.

**Algorithm:**
```python
def set_property(actor, path, value):
    prop_info = get_property_info(actor, path)

    if prop_info.type == "TObjectPtr" or prop_info.type == "TSoftObjectPtr":
        return set_asset_property(actor, path, value)
    elif prop_info.type == "float" or prop_info.type == "double":
        return set_float_property(actor, path, value)
    elif prop_info.type == "FColor" or prop_info.type == "FLinearColor":
        return set_color_property(actor, path, parse_color(value))
    elif prop_info.type == "FVector":
        return set_vector_property(actor, path, parse_vector(value))
    else:
        return set_string_property(actor, path, str(value))
```

**Implementation Location:**
- `AgentBridgeScripting/CommandExecutor.cpp` - Enhanced `SetPropertyPath()`
- MCP layer: Remove typed setters, enhance `set_property`

| Task | Priority | Effort |
|------|----------|--------|
| Add TSoftObjectPtr support to property setter | P0 | 3 hours |
| Add property type detection to set_property | P1 | 2 hours |
| Smart value parsing (color, vector, etc.) | P1 | 2 hours |
| Deprecate typed setters (keep for compatibility) | P2 | 1 hour |

---

### 2.3 Actor-Agnostic Property Access

**Problem:** Property tools only work on Actors, not UObjects (DataAssets, Materials, etc.)

```python
# Works - actor
set_property(actor="MyLight", path="Intensity", value="5000")

# Fails - DataAsset
set_property(actor="/Game/MyAsset.MyAsset", path="SomeProperty", value="X")
# Error: "Actor not found"
```

**Solution:** Extend property operations to any UObject.

**Algorithm:**
```python
def resolve_object(identifier: str) -> UObject*:
    # Try as actor in current world
    actor = find_actor_by_name_or_label(identifier)
    if actor:
        return actor

    # Try as asset path
    if identifier.startswith("/"):
        asset = load_object(identifier)
        if asset:
            return asset

    return None
```

**Implementation Location:**
- `AgentBridgeRuntime/ActorOperations.cpp` - Add `ResolveObject()` that tries actor then asset
- Update all property operations to use `ResolveObject()` instead of `FindActor()`

| Task | Priority | Effort |
|------|----------|--------|
| Add `ResolveObject()` helper | P1 | 2 hours |
| Update GetProperty to use it | P1 | 1 hour |
| Update SetProperty to use it | P1 | 1 hour |
| Test with DataAssets | P1 | 1 hour |

---

### 2.4 Function Parameter Support

**Problem:** `call_function` only works with `void()` signatures.

```python
# Fails
call_function(actor="X", function="Generate", parameters={"bForce": True})
# Error: "Only functions with no arguments and void return type are currently supported"
```

**Solution:** Extend `FunctionInvoker` to handle parameters and return values.

**Implementation Notes:**
- This is already partially implemented in C++ (`FunctionInvoker.cpp`)
- The limitation is in Tempo's gRPC layer, not AgentBridge
- Need to coordinate with Tempo team or implement our own RPC

| Task | Priority | Effort |
|------|----------|--------|
| Investigate Tempo `call_function` limitations | P1 | 2 hours |
| Implement parameter serialization in AgentBridge | P1 | 4 hours |
| Add AgentBridge-native CallFunction RPC | P2 | 6 hours |
| Return value support | P2 | 4 hours |

---

### 2.5 Smart Actor Search

**Problem:** `query_actors(name_pattern="MyLight")` searches internal names, not labels.

```python
# User expects this to find actor labeled "MyLight"
query_actors(name_pattern="MyLight")
# Actually searches internal names like "PointLight_UAID_..."
```

**Solution:** Add `label_pattern` parameter and search both by default.

**Algorithm:**
```python
def query_actors(name_pattern=None, label_pattern=None, class_name=None):
    results = []
    for actor in world.actors:
        # Match by internal name
        if name_pattern and fnmatch(actor.GetName(), name_pattern):
            results.append(actor)
            continue

        # Match by label
        if label_pattern and fnmatch(actor.GetActorLabel(), label_pattern):
            results.append(actor)
            continue

        # If only class specified, include all of that class
        if class_name and not name_pattern and not label_pattern:
            if actor.IsA(class_name):
                results.append(actor)

    return results
```

| Task | Priority | Effort |
|------|----------|--------|
| Add `label_pattern` parameter to QueryActors | P1 | 2 hours |
| Update MCP tool and help text | P1 | 30 min |
| Consider making label_pattern default behavior | P2 | 1 hour |

---

### 2.6 Landscape Bounds Command

**Problem:** Getting landscape bounds requires 10+ tool calls and complex math.

**Solution:** Single `get_landscape_bounds` command.

**Algorithm:**
```cpp
FLandscapeBounds GetLandscapeBounds() {
    // Find landscape actor
    ALandscape* Landscape = FindLandscape();

    // Get all landscape streaming proxies
    TArray<ALandscapeStreamingProxy*> Proxies;
    QueryLandscapeProxies(Proxies);

    // Calculate XY bounds from proxy positions + extents
    FVector MinXY = FVector::ZeroVector;
    FVector MaxXY = FVector::ZeroVector;

    // Sample Z bounds from collision components
    float MinZ = MAX_FLT;
    float MaxZ = -MAX_FLT;

    for (Proxy : Proxies) {
        FBox LocalBox = Proxy->GetCollisionComponent()->CachedLocalBox;
        FVector WorldMin = Proxy->GetActorLocation() + LocalBox.Min * Landscape->GetActorScale3D();
        FVector WorldMax = Proxy->GetActorLocation() + LocalBox.Max * Landscape->GetActorScale3D();

        MinXY = MinXY.ComponentMin(WorldMin);
        MaxXY = MaxXY.ComponentMax(WorldMax);
        MinZ = FMath::Min(MinZ, WorldMin.Z);
        MaxZ = FMath::Max(MaxZ, WorldMax.Z);
    }

    return FLandscapeBounds{
        .Min = FVector(MinXY.X, MinXY.Y, MinZ),
        .Max = FVector(MaxXY.X, MaxXY.Y, MaxZ),
        .Center = (Min + Max) / 2,
        .Extent = (Max - Min) / 2
    };
}
```

| Task | Priority | Effort |
|------|----------|--------|
| Add FLandscapeBounds struct | P1 | 30 min |
| Implement GetLandscapeBounds in WorldPartitionOps | P1 | 3 hours |
| Add to CommandExecutor | P1 | 1 hour |
| Add gRPC RPC | P1 | 1 hour |
| Add MCP tool | P1 | 30 min |

---

### 2.7 Value Format Auto-Detection

**Problem:** Users must know exact formats for complex types.

```python
# User might try any of these:
set_property(actor="X", path="Color", value="green")
set_property(actor="X", path="Color", value="(0, 255, 0)")
set_property(actor="X", path="Color", value="rgb(0, 255, 0)")
set_property(actor="X", path="Color", value="#00FF00")

# Only this works:
set_property(actor="X", path="Color", value="(R=0.0,G=1.0,B=0.0,A=1.0)")
```

**Solution:** Accept multiple formats and auto-convert.

**Algorithm:**
```python
def parse_color(value: str) -> FLinearColor:
    # UE format
    if match := re.match(r'\(R=([\d.]+),G=([\d.]+),B=([\d.]+),A=([\d.]+)\)', value):
        return FLinearColor(*map(float, match.groups()))

    # Hex format
    if value.startswith('#'):
        return hex_to_color(value)

    # RGB tuple
    if match := re.match(r'\((\d+),\s*(\d+),\s*(\d+)\)', value):
        r, g, b = map(int, match.groups())
        return FLinearColor(r/255, g/255, b/255, 1.0)

    # Named colors
    named_colors = {"red": (1,0,0,1), "green": (0,1,0,1), ...}
    if value.lower() in named_colors:
        return FLinearColor(*named_colors[value.lower()])

    raise ValueError(f"Unknown color format: {value}")
```

| Task | Priority | Effort |
|------|----------|--------|
| Add flexible color parsing | P2 | 2 hours |
| Add flexible vector parsing | P2 | 1 hour |
| Add flexible rotation parsing | P2 | 1 hour |
| Document accepted formats in help | P2 | 30 min |

---

## Part 3: Error Message Improvements

### Problem

Error messages expose implementation details:
```
"Failed to set path 'Definition'" - Why?
"Property did not have correct type" - What type is expected?
"Actor not found" - Did you mean the asset? Here's how to search...
```

### Solution

Add contextual suggestions to error messages:

```python
# Before
"Property did not have correct type"

# After
"Property 'BiomeTexture' is a TSoftObjectPtr<UTexture2D>.
Use set_asset_property() with a texture asset path like '/Game/Textures/MyTexture'.
Known issue: TSoftObjectPtr properties are not yet fully supported."
```

**Implementation:**
- Add property type to error messages in `SetPropertyPath()`
- Add suggestions for common errors
- Include "Did you mean?" for typos using Levenshtein distance

| Task | Priority | Effort |
|------|----------|--------|
| Enhanced error messages with property type | P1 | 2 hours |
| "Did you mean?" suggestions | P2 | 3 hours |
| Known issues in error messages | P2 | 1 hour |

---

## Implementation Roadmap

### Phase 1: Critical Fixes (Week 1)
| Task | Effort | Impact |
|------|--------|--------|
| TSoftObjectPtr support | 3 hours | Unblocks BiomeTexture |
| get_landscape_bounds command | 5 hours | Simplifies volume sizing |
| Blueprint class normalization | 3 hours | Eliminates _C suffix requirement |

### Phase 2: Usability Improvements (Week 2)
| Task | Effort | Impact |
|------|--------|--------|
| Unified property setter | 4 hours | One tool instead of 5 |
| Actor-agnostic property access | 4 hours | DataAsset editing |
| Smart actor search | 2.5 hours | Intuitive name matching |
| PCG Biome workflow help topic | 1 hour | Self-documenting |

### Phase 3: Polish (Week 3)
| Task | Effort | Impact |
|------|--------|--------|
| Value format auto-detection | 4 hours | Flexible input formats |
| Enhanced error messages | 4 hours | Better debugging |
| Function parameter support | 10 hours | Full function access |

---

## Success Criteria

After implementing these fixes, the following workflow should "just work":

```python
# User says: "Set up PCG Biome on my landscape"

# 1. Get help
help(topic="workflows/pcg-biome")

# 2. Spawn actor (no _C suffix needed)
spawn_actor(class_name="BP_PCGBiomeTexture", label="MyBiome")

# 3. Size to landscape (single command)
bounds = get_landscape_bounds()
set_actor_transform("MyBiome", location=bounds["center"],
                    scale=[b/100 for b in bounds["extent"]])

# 4. Configure (unified setter handles all types)
set_property("MyBiome", "Definition", "/PCGBiomeSample/BiomeDefinitions/BroadleafForest")
set_property("MyBiome", "Assets[0]", "/PCGBiomeSample/BiomeAssets/BroadleafForest")
set_property("MyBiome", "BiomeTexture", "/Game/Textures/BiomeMap")

# 5. Generate (with parameters)
call_function("MyBiome", "PCG", "Generate", {"bForce": True})
```

**Principle:** The user describes WHAT they want, the tools figure out HOW.

---

*Document created: January 1, 2026*
*Based on PCG Biome workflow testing (PCG_BIOME_WORKFLOW.md)*
