# AGENTS.md — AgentBridge MCP Tool Guide

> **For AI agents using AgentBridge to interact with Unreal Engine.**
> This document encodes hard-won lessons from real-world testing. Following these rules
> will reduce your tool calls by 60%+ compared to discovery-based approaches.

---

## What Is This?

**Unreal Engine** is a 3D game/simulation engine. It has an **Editor** (a GUI application)
where you build levels by placing actors (objects) in a 3D world, setting their properties,
and configuring assets (reusable data files like meshes, textures, Blueprints, etc.).

**AgentBridge** is a plugin that exposes the Unreal Editor to AI agents via ~100 MCP tools.
Instead of clicking in the GUI, you call tools like `spawn_actor`, `set_property`, and
`save_asset` to build levels programmatically. It connects via gRPC (port 10001).

**Key concepts you'll work with:**

| Concept | What It Is |
|---------|-----------|
| **Actor** | An object placed in a level (light, mesh, volume, Blueprint instance) |
| **Component** | A sub-object on an actor (LightComponent, MeshComponent, BoxComponent) |
| **Property** | A named value on an actor or component (Intensity, Color, Location) |
| **Blueprint (BP)** | A visual scripting class — like a template for actors, created in the editor |
| **Asset** | A saved file in the project (mesh, texture, material, DataAsset, Blueprint) |
| **Asset Path** | How you reference assets: `/Game/Folder/AssetName.AssetName` |
| **DataAsset** | A pure-data asset (no visual representation) — used for configuration |
| **PCG** | Procedural Content Generation — spawns content from rules instead of manual placement |
| **World Partition** | Large-world streaming system — not all actors are loaded at once |
| **PIE** | Play-In-Editor — runs the game inside the editor for testing |

---

## Table of Contents

**Essentials — read these first:**
1. [Getting Started](#getting-started)
2. [Module Profiles](#module-profiles)
3. [Critical Rules](#critical-rules)

**Core tool guidance:**
4. [Property Access](#property-access)
5. [Actor Operations](#actor-operations)
6. [Transforms & Attachment](#transforms--attachment)
7. [Asset Operations](#asset-operations)
8. [File Operations](#file-operations)
9. [Volume & Bounds](#volume--bounds)

**Module-specific guidance:**
10. [Editor & Levels](#editor--levels)
11. [World Partition](#world-partition)
12. [Tempo Simulation](#tempo-simulation)
13. [bp_toolkit — Blueprint & Asset Tools](#bp_toolkit--blueprint--asset-tools)
14. [Function Calls](#function-calls)
15. [Type Discovery](#type-discovery)

**Workflows & Reference:**
16. [PCG Biome Workflow](#pcg-biome-workflow)
17. [Common Workflows](#common-workflows)
18. [Troubleshooting](#troubleshooting)
19. [Performance Tips](#performance-tips)
20. [Value Format Cheat Sheet](#value-format-cheat-sheet)

---

## Getting Started

### Prerequisites
- Unreal Editor is open with Tempo gRPC server running on port 10001
- MCP server is connected

### Starting the Editor

If the editor isn't running yet:

```bash
# Start editor (from project root)
cd <PROJECT_ROOT> && ./Plugins/Tempo/Scripts/Run.sh

# Wait ~30 seconds for gRPC to be ready on port 10001
```

**Killing the editor** (Windows — must use `cmd` wrapper in Git Bash):
```bash
cmd //c "taskkill /F /IM UnrealEditor.exe"
```

**Gotchas:**
- gRPC is NOT ready immediately — wait ~30 seconds after Run.sh starts
- `tempo_quit` may hang on a save dialog — use `taskkill /F` for guaranteed termination
- Git Bash interprets `/F` as a path — always wrap with `cmd //c "..."`
- If the editor is already running, kill it first before rebuilding

### First Steps
1. Call `help()` to see available tool categories
2. Call `help(topic="actors")` for actor operations
3. Call `help(topic="properties")` for property access
4. Call `help(topic="workflows")` for multi-step patterns

### Use the Help System
The built-in `help()` tool is your primary reference. Use it before resorting to schema
discovery. Topics: `actors`, `properties`, `classes`, `assets`, `components`, `console`,
`workflows`, `pcg_volume`, `volume_sizing`, `bp_toolkit`.

---

## Module Profiles

Tools are organized into modules loaded via profiles. Not all tools are available by default.

| Profile | Modules | ~Tools | Use Case |
|---------|---------|--------|----------|
| `core` | core | 6 | Minimal: help, console, worlds, quit |
| `standard` | core, classes, editor, files | ~35 | **DEFAULT** — level editing |
| `editor` | standard + world_partition | ~42 | Large worlds, landscape queries |
| `scripting` | standard + bp_toolkit | ~61 | Blueprint/PCG graph editing |
| `simulation` | core, classes, tempo_sim | ~34 | PIE testing, vehicle/pawn control |
| `full` | all modules | ~100 | Everything |

To load additional modules at runtime:

```python
load_modules(modules=["bp_toolkit"])
load_modules(modules=["world_partition", "tempo_sim"])
```

**Tip:** If a tool isn't found, you likely need to load its module first.

---

## Critical Rules

These rules prevent the most common failures. Violating them causes silent data loss or
confusing errors.

### Rule 1: Use Unreal String Format for Structs

**ALWAYS use Unreal parenthesized format for struct values. JSON objects silently fail.**

```python
# WRONG - reports success but value does NOT persist:
set_property(..., path="Volume.BoxExtent", value={"X": 408000, "Y": 408000, "Z": 60000})

# RIGHT:
set_property(..., path="Volume.BoxExtent", value="(X=408000,Y=408000,Z=60000)")
```

This applies to ALL struct types:
| Type | Format |
|------|--------|
| FVector | `"(X=1,Y=2,Z=3)"` |
| FRotator | `"(Pitch=0,Yaw=90,Roll=0)"` |
| FLinearColor | `"(R=1,G=0,B=0,A=1)"` |
| FColor | `"(R=255,G=0,B=0,A=255)"` |
| FTransform | `"(Rotation=(X=0,Y=0,Z=0,W=1),Translation=(X=0,Y=0,Z=0),Scale3D=(X=1,Y=1,Z=1))"` |

### Rule 2: Reset Scale Before Setting BoxExtent

Volume components may have non-unit default scales (e.g., 512). Large BoxExtent values
multiplied by large scales cause integer overflow, producing tiny or inverted volumes.

```python
# ALWAYS do this first:
set_property(actor_id="MyActor", path="Volume.RelativeScale3D", value="(X=1,Y=1,Z=1)")

# THEN set extent:
set_property(actor_id="MyActor", path="Volume.BoxExtent", value="(X=408000,Y=408000,Z=60000)")
```

### Rule 3: duplicate_asset Destination Must Be Under /Game/

```python
# WRONG - fails (destination must be under /Game/):
duplicate_asset(source_path="...", dest_package_path="/PCGBiomeCore/PCGBiomes/...")

# RIGHT:
duplicate_asset(source_path="...", dest_package_path="/Game/PCGBiomes/Definitions")
```

### Rule 4: Asset References Use PackageName.AssetName Format

When setting a property that references another asset, use the full object path:

```python
# RIGHT:
set_property(..., path="Definition", value="/Game/PCGBiomes/Definitions/RedBiome.RedBiome")

# WRONG (missing asset name after dot):
set_property(..., path="Definition", value="/Game/PCGBiomes/Definitions/RedBiome")
```

### Rule 5: Some Blueprint Classes Require Full Paths

Short names work for some BP classes but not all. If `spawn_actor` fails with a short name,
use the full class path with `_C` suffix:

```python
# Short name works:
spawn_actor(class_name="BP_PCGBiomeCore", ...)

# Short name may NOT work — use full path:
spawn_actor(class_name="/PCGBiomeCore/Blueprints/BP_PCGBiomeTexture.BP_PCGBiomeTexture_C", ...)
```

---

## Property Access

### Reading Properties

```python
# Actor property
get_property(actor_id="MyActor", path="PropertyName")

# Component property (dot-separated path)
get_property(actor_id="MyActor", path="LightComponent0.Intensity")

# Nested struct
get_property(actor_id="MyActor", path="RootComponent.RelativeLocation")

# Data asset property (use asset path as actor_id)
get_property(actor_id="/Game/MyAssets/MyAsset.MyAsset", path="BiomeDefinition.BiomeName")
```

### Writing Properties

```python
# Simple values
set_property(actor_id="MyActor", path="bHidden", value=true)
set_property(actor_id="MyActor", path="LightComponent0.Intensity", value=5000)

# Structs (MUST use Unreal string format)
set_property(actor_id="MyActor", path="RootComponent.RelativeLocation",
    value="(X=100,Y=200,Z=300)")

# Object references (asset paths)
set_property(actor_id="MyActor", path="BiomeTexture",
    value="/Game/Textures/MyTexture.MyTexture")

# Arrays of object references
set_property(actor_id="MyActor", path="Assets",
    value='["/Game/Assets/Asset1.Asset1", "/Game/Assets/Asset2.Asset2"]')

# Arrays of structs (JSON array with struct fields)
set_property(actor_id="/Game/MyAsset.MyAsset", path="BiomeAssets",
    value='[{"Enabled": true, "Weight": 1, "Mesh": "/Game/Meshes/Mesh1.Mesh1"}]')
```

### Property Path Rules

- Use **display names** (not internal GUID-suffixed names) in property paths
- Component paths: `ComponentName.PropertyName`
- Nested structs: `StructProperty.FieldName`
- Array elements: `ArrayProperty[0]`
- Data assets work with asset paths as `actor_id`

### Struct Arrays with Object References

Setting an entire struct array in one call (with object refs inline) generally works:

```python
set_property(actor_id="/Game/MyAsset.MyAsset", path="BiomeAssets",
    value='[{"Enabled": true, "Mesh": "/Game/Meshes/Mesh1.Mesh1"}]')
```

**If object references don't persist** in a single-call array set, use the two-step approach:
1. Set the array with simple properties only (bools, numbers, strings)
2. Set each object reference individually with indexed paths

```python
# Step 1: Create array structure without object refs
set_property(actor_id="/Game/MyAsset.MyAsset", path="MyArray",
    value='[{"Enabled": true, "Weight": 1}, {"Enabled": true, "Weight": 1}]')

# Step 2: Set object refs individually
set_property(actor_id="/Game/MyAsset.MyAsset", path="MyArray[0].Mesh",
    value="/Game/Meshes/Mesh1.Mesh1")
set_property(actor_id="/Game/MyAsset.MyAsset", path="MyArray[1].Mesh",
    value="/Game/Meshes/Mesh2.Mesh2")
```

### Component Naming — Use Instance Names

**Use component instance names, NOT class names.** Find them with `get_actor`:

```python
# Find component instance names
get_actor(actor_id="MyLight", include_components=true)
# Returns: components: [{name: "LightComponent0", class: "PointLightComponent"}, ...]

# RIGHT — instance name:
get_property(actor_id="MyLight", path="LightComponent0.Intensity")

# WRONG — class name:
get_property(actor_id="MyLight", path="PointLightComponent.Intensity")
```

---

## Actor Operations

### Spawning

```python
# Native or well-known BP class (short name)
spawn_actor(class_name="PointLight", location=[0, 0, 100], label="MyLight")

# Blueprint class (full path with _C suffix if short name fails)
spawn_actor(
    class_name="/PluginName/Path/To/BP_MyActor.BP_MyActor_C",
    location=[0, 0, 0],
    label="MyActor",
    folder_path="MyFolder")
```

**Gotchas:**
- The `_C` suffix is auto-added for BP classes, but sometimes you need to provide it explicitly
  in the full path form
- `folder_path` creates the outliner folder if it doesn't exist — use it to organize your work
- `relative_to` spawns at an offset from another actor (Tempo feature)

### Querying

```python
# By class
query_actors(class_name="PointLight")

# By label (substring match — no wildcards needed)
query_actors(label_pattern="Biome")

# By internal name (substring match — no wildcards)
query_actors(name_pattern="Light")

# By tag
query_actors(tag="MyTag")

# Include World Partition unloaded actors
query_actors(class_name="StaticMeshActor", include_unloaded=true)
```

**Prefer `label_pattern` over `name_pattern`** — labels are what you set, names are
auto-generated and ugly (e.g., `BP_PCGBiomeTexture_C_2`).

**Pattern matching is substring-based.** Use `"Light"` not `"*Light*"`. Wildcards like
`*` are treated as literal characters and will match nothing. `list_classes(name_pattern=...)`
is case-insensitive exact match only — use `base_class_name` for discovery instead.

### Getting Actor Details

```python
# Basic info
get_actor(actor_id="MyActor")

# With all properties listed (useful for discovering paths)
get_actor(actor_id="MyActor", include_properties=true)

# With component instance names (CRITICAL for property paths)
get_actor(actor_id="MyActor", include_components=true)
```

### Duplicating Actors

```python
duplicate_actor(actor_id="MyActor", new_label="MyActorCopy",
    location=[100, 0, 0])
```

### Adding Components

```python
add_component(actor_id="MyActor", component_type="SpotLightComponent",
    component_name="MySpotLight")
```

### Actor Identification
- `actor_id` can be: actor label, actor name, path, or GUID
- Labels are the human-readable names shown in the outliner
- If ambiguous, use the exact name from `query_actors()` output

---

## Transforms & Attachment

### Unified Transform Tool

`set_transform` works on both actors AND components:

```python
# Actor transform (world space by default)
set_transform(target="MyActor", location=[100, 200, 300])

# Component transform (use -> syntax)
set_transform(target="MyActor->SpotLightComponent", rotation=[45, 0, 0])

# Relative space instead of world
set_transform(target="MyActor", location=[0, 0, 50], world_space=false)

# Add offset to current transform (instead of replacing)
set_transform(target="MyActor", location=[10, 0, 0], offset=true)

# Read current transform
get_transform(target="MyActor")
get_transform(target="MyActor->LightComponent0", world_space=false)
```

**Note:** `set_actor_transform` is the older form that only works on actors. Prefer
`set_transform` for new work — it handles both actors and components.

### Attachment

```python
# Attach child actor to parent
attach(child="Lamp", parent="Table")

# Attach to a specific socket
attach(child="Hat", parent="Character", socket="head")

# Attachment rules: KeepRelative | KeepWorld | SnapToTarget
attach(child="Lamp", parent="Table",
    location_rule="KeepWorld", rotation_rule="KeepWorld")

# Detach
detach(target="Lamp")                              # keep world position
detach(target="Lamp", maintain_world_transform=false)  # keep relative offset
```

---

## Asset Operations

### Creating Assets

```python
# Duplicate from template (PREFERRED — inherits all setup)
duplicate_asset(
    source_path="/PluginName/Path/To/Template",
    dest_package_path="/Game/MyFolder",
    dest_asset_name="NewAssetName")

# Create empty asset (for DataAssets, MaterialInstances, etc.)
create_asset(asset_class="DataAsset", package_path="/Game/MyData",
    asset_name="MyConfig")

# Create MaterialInstance from parent material
create_asset(asset_class="MaterialInstanceConstant",
    package_path="/Game/Materials", asset_name="MI_Red",
    parent_asset_path="/Game/Materials/M_Base")

# Save actor as a reusable Blueprint
save_actor_as_blueprint(actor_id="MyConfiguredActor",
    package_path="/Game/Blueprints", blueprint_name="BP_MyActor")
```

### Key Constraints
- **Destination must be under `/Game/`** — cannot duplicate to plugin content folders
- **Always `save_asset` after modification** — changes are in-memory only until saved
- **Use `PackageName.AssetName`** format when referencing assets in properties
- **`create_asset` makes empty shells** — for Blueprints prefer `save_actor_as_blueprint`

---

## File Operations

Read/write files in the project directory:

```python
# Read a config file
read_project_file(relative_path="Config/DefaultGame.ini")

# Write a file (creates directories automatically)
write_project_file(relative_path="Config/MyConfig.ini",
    content="[Settings]\nKey=Value")

# Append to existing file
write_project_file(relative_path="Saved/Logs/agent.log",
    content="New log entry\n", append=true)

# List directory contents
list_project_directory(relative_path="Content/Blueprints",
    pattern="*.uasset", recursive=true)

# Copy a file
copy_project_file(source_path="Config/DefaultGame.ini",
    dest_path="Config/DefaultGame.ini.bak")
```

**All paths are relative to the project root.** These do NOT work with absolute paths.

---

## Volume & Bounds

### Getting Landscape Bounds

```python
bounds = get_landscape_bounds()
# Returns: center [x, y, z] and extent [x, y, z]
```

### Setting Volume Size

**Critical two-step process:**

```python
# Step 1: Reset component scale (prevents overflow)
set_property(actor_id="MyActor", path="Volume.RelativeScale3D", value="(X=1,Y=1,Z=1)")

# Step 2: Set box extent
set_property(actor_id="MyActor", path="Volume.BoxExtent",
    value="(X=408000,Y=408000,Z=60000)")
```

### Volume Component Names by Blueprint
| Blueprint | Volume Component Path |
|-----------|----------------------|
| BP_PCGBiomeCore | `Volume` |
| BP_PCGBiomeTexture | `BiomeTextureVolume` |

---

## PCG Biome Workflow

A complete, reusable workflow for setting up texture-based PCG biomes. Each biome maps a
color channel in a texture to a set of meshes spawned via PCG.

### Architecture

```
BiomeCore (1 per level)          BiomeTexture (1 per biome)
├── Volume (box bounds)          ├── BiomeTextureVolume (box bounds)
└── PCG Component                ├── BiomeTexture (texture ref)
                                 ├── Definition → BiomeDefinitionTemplate
                                 │   ├── BiomeName
                                 │   ├── BiomeColor
                                 │   └── BiomePriority
                                 └── Assets[] → BiomeAssetTemplate[]
                                     └── BiomeAssets[]
                                         ├── Enabled
                                         ├── Weight
                                         ├── Generator
                                         └── Mesh
```

**Key relationships:**
- One **BiomeCore** per level — owns the PCG component that drives generation
- One **BiomeTexture** per biome — reads a color from the shared texture and spawns meshes
- Each BiomeTexture references one **Definition** (name, color, priority) and one or more **Asset configs** (mesh lists)
- Definitions and Assets are **Data Assets** (duplicated from templates, stored under `/Game/`)

**Default template paths** (from PCGBiomeCore plugin):
- Blueprints: `/PCGBiomeCore/Blueprints/BP_PCGBiomeCore.BP_PCGBiomeCore` and `BP_PCGBiomeTexture`
- Definition: `/PCGBiomeCore/BiomeDefinitions/DefaultBiome.DefaultBiome`
- Asset: `/PCGBiomeCore/BiomeAssets/DefaultAsset.DefaultAsset`

> **Note:** Projects may provide their own biome templates in a different location (e.g. under
> `/Game/` or a project-specific plugin). If the user specifies custom template paths, use those
> instead of the defaults above.

### Schema Reference

#### BP_PCGBiomeCore_C (Level Actor)

**Class Path:** `/PCGBiomeCore/Blueprints/BP_PCGBiomeCore.BP_PCGBiomeCore_C`

| Property | Type | Description |
|----------|------|-------------|
| `BiomeCore` | UPCGComponent* | The PCG component that drives generation |
| `Volume` | UBoxComponent* | Volume bounds component |
| `ActorLabel` | FString | Display name in outliner |
| `FolderPath` | FName | Outliner folder |

Volume sizing:
```python
set_property(actor_id="BiomeCore", path="Volume.BoxExtent", value="(X=408000,Y=408000,Z=60000)")
```

#### BP_PCGBiomeBaseActor_C (Parent Class)

**Class Path:** `/PCGBiomeCore/Blueprints/BP_PCGBiomeBaseActor.BP_PCGBiomeBaseActor_C`

Properties inherited by BP_PCGBiomeTexture:

| Property | Type | Description |
|----------|------|-------------|
| `Enabled` | bool | Whether biome is active |
| `Definition` | UBiomeDefinitionTemplate_C* | Reference to biome definition asset |
| `Assets` | TArray\<UBiomeAssetBaseTemplate_C*\> | Array of asset configuration references |
| `RuntimeAssets` | TArray\<UBiomeRuntimeAssetBaseTemplate_C*\> | Runtime asset configs |

#### BP_PCGBiomeTexture_C (Level Actor)

**Class Path:** `/PCGBiomeCore/Blueprints/BP_PCGBiomeTexture.BP_PCGBiomeTexture_C`

Properties beyond those inherited from BP_PCGBiomeBaseActor:

| Path | Type | Description | Default |
|------|------|-------------|---------|
| `BiomeTexture` | UTexture2D* | Texture to sample for biome placement | — |
| `BiomeColorTolerance` | float | Color matching tolerance | 0.01 |
| `LocalCacheTexelSize` | int32 | Cache resolution | 800 |
| `LocalCacheZExtents` | float | Z bounds for cache | 1600 |
| `LocalCacheSnapToSurface` | bool | Snap to terrain | true |
| `BiomeTextureVolume.BoxExtent` | FVector | Volume bounds | — |
| `BiomeTextureVolume.RelativeScale3D` | FVector | Volume scale (**reset to 1,1,1!**) | — |

#### Volume Component Names

| Blueprint | Volume Component Path |
|-----------|----------------------|
| BP_PCGBiomeCore | `Volume` |
| BP_PCGBiomeTexture | `BiomeTextureVolume` |

#### BiomeDefinitionTemplate_C (Data Asset)

**Class Path:** `/PCGBiomeCore/BiomeDefinitions/Setup/BiomeDefinitionTemplate.BiomeDefinitionTemplate_C`
**Default Instance:** `/PCGBiomeCore/BiomeDefinitions/DefaultBiome.DefaultBiome`

Use `duplicate_asset` with the default instance as source to create new biome definitions:
```python
duplicate_asset(source_path="/PCGBiomeCore/BiomeDefinitions/DefaultBiome",
                dest_package_path="/Game/PCGBiomes/Definitions", dest_asset_name="ForestBiome")
```

Contains the `BiomeDefinition` struct (type `FBiomeDefinition`). Set via `set_property`
with asset path as `actor_id`:

| Path | Type | Example |
|------|------|---------|
| `BiomeDefinition.BiomeName` | FName | `"Red"` |
| `BiomeDefinition.BiomeColor` | FLinearColor | `"(R=1,G=0,B=0,A=1)"` |
| `BiomeDefinition.BiomePriority` | int32 | `1` |

**Internal GUID-suffixed names** (for reference only — use display names in property paths):

| Display Name | Internal Name |
|-------------|---------------|
| BiomeName | `BiomeName_24_1E8586594C50912BBA59FE938C884942` |
| BiomeColor | `BiomeColor_34_BEECC81544741E47AAA35590EF6B404D` |
| BiomePriority | `BiomePriority_29_308259B0449F5BA935CCC9B3DBDB97F3` |

#### BiomeAssetTemplate_C (Data Asset)

**Class Path:** `/PCGBiomeCore/BiomeAssets/Setup/BiomeAssetTemplate.BiomeAssetTemplate_C`
**Default Instance:** `/PCGBiomeCore/BiomeAssets/DefaultAsset.DefaultAsset`

Use `duplicate_asset` with the default instance as source to create new biome asset configs:
```python
duplicate_asset(source_path="/PCGBiomeCore/BiomeAssets/DefaultAsset",
                dest_package_path="/Game/PCGBiomes/Assets", dest_asset_name="ForestAssets")
```

Contains the `BiomeAssets` array (type `TArray<FBiomeAsset>`).

##### FBiomeAsset — Full Field List

| Field | Type | Description |
|-------|------|-------------|
| `Enabled` | bool | Whether this asset entry is active |
| `AssetType` | FName | Type identifier |
| `Weight` | double | Spawn weight (default: 1.0) |
| `Generator` | UBiomeGeneratorTemplate_C* | Generator reference |
| `GeneratorSubType` | FName | Generator sub-type |
| `TransformGraph` | UPCGGraphInterface* | Optional transform PCG graph |
| `Mesh` | UStaticMesh* | Static mesh to spawn |
| `Assembly` | UPCGDataAsset* | PCG data asset |
| `Actor` | FSoftClassPath | Actor class to spawn |
| `ChildAssets` | UBiomeAssetBaseTemplate_C* | Child asset reference |
| `DebugOptions` | FBiomeAsset_DebugOptions | Debug settings |
| `AssetOptions` | FBiomeAsset_AssetOptions | Asset placement options |
| `MeshOptions` | FBiomeAsset_MeshOptions | Mesh rendering options |
| `AssemblyOptions` | FBiomeAsset_AssemblyOptions | Assembly options |
| `FilterOptions` | FBiomeAsset_FilterOptions | Filtering options |
| `RuntimeOptions` | FBiomeAsset_RunTimeOptions | Runtime behavior options |

##### FBiomeAsset_FilterOptions

| Field | Type | Default |
|-------|------|---------|
| `DensityMin` | float | 0.0 |
| `DensityMax` | float | 1.0 |
| `HeightMin` | float | -10000000.0 |
| `HeightMax` | float | 10000000.0 |

##### FBiomeAsset_RuntimeOptions

| Field | Type | Default |
|-------|------|---------|
| `MeshSamplingRadius` | float | 5.0 |
| `ScaleMultiplier` | float | 1.0 |
| `NormalOffset` | float | 1.0 |
| `ZMin` | float | 0.0 |
| `ZMax` | float | 500.0 |
| `InfluenceRadius` | float | 100.0 |

### Step-by-Step Workflow

Follow these phases in order for each biome setup. Your instruction file provides the
biome-specific data (names, colors, meshes, paths); this workflow provides the procedure.

#### Phase 1: Duplicate Templates

Create one Definition and one Asset data asset per biome. Source from your template paths,
destination under `/Game/`.

```python
# For each biome:
duplicate_asset(
    source_path="<DEFINITION_TEMPLATE_PATH>",
    dest_package_path="/Game/PCGBiomes/Definitions",
    dest_asset_name="<BiomeName>Biome")          # e.g. "RedBiome"

duplicate_asset(
    source_path="<ASSET_TEMPLATE_PATH>",
    dest_package_path="/Game/PCGBiomes/Assets",
    dest_asset_name="<BiomeName>Assets")          # e.g. "RedAssets"
```

#### Phase 2: Configure Definitions

Set name, color, and priority on each definition asset:

```python
set_property(
    actor_id="/Game/PCGBiomes/Definitions/<Name>Biome.<Name>Biome",
    path="BiomeDefinition.BiomeName", value="<Name>")
set_property(
    actor_id="/Game/PCGBiomes/Definitions/<Name>Biome.<Name>Biome",
    path="BiomeDefinition.BiomeColor", value="<COLOR_STRING>")    # e.g. "(R=1,G=0,B=0,A=1)"
set_property(
    actor_id="/Game/PCGBiomes/Definitions/<Name>Biome.<Name>Biome",
    path="BiomeDefinition.BiomePriority", value=<PRIORITY_INT>)
```

#### Phase 3: Configure Assets

Set the BiomeAssets array with mesh entries. Use a JSON array string:

```python
set_property(
    actor_id="/Game/PCGBiomes/Assets/<Name>Assets.<Name>Assets",
    path="BiomeAssets",
    value='[{"Enabled": true, "Weight": 1, "Generator": "<GENERATOR_PATH>", "Mesh": "<MESH_PATH_1>"}, {"Enabled": true, "Weight": 1, "Generator": "<GENERATOR_PATH>", "Mesh": "<MESH_PATH_2>"}]')
```

**Note:** The Generator field references the default generator template (e.g.
`/PCGBiomeCore/BiomeGenerators/DefaultGenerator.DefaultGenerator`).

#### Phase 4: Save All Data Assets

Save every asset you modified. Changes are in-memory only until saved:

```python
save_asset(asset_path="/Game/PCGBiomes/Definitions/<Name>Biome")
save_asset(asset_path="/Game/PCGBiomes/Assets/<Name>Assets")
```

#### Phase 5: Get Landscape Bounds

Query the landscape for center and extent — used for actor placement and volume sizing:

```python
bounds = get_landscape_bounds()
# Returns: center [x, y, z] and extent [x, y, z]
```

#### Phase 6: Spawn Level Actors

Spawn ONE BiomeCore and ONE BiomeTexture per biome, all at landscape center:

```python
# One shared BiomeCore
spawn_actor(class_name="BP_PCGBiomeCore",
    location=[center_x, center_y, center_z],
    label="BiomeCore", folder_path="PCGBiomes")

# One BiomeTexture per biome (may require full class path — see Rule 5)
spawn_actor(
    class_name="/PCGBiomeCore/Blueprints/BP_PCGBiomeTexture.BP_PCGBiomeTexture_C",
    location=[center_x, center_y, center_z],
    label="<Name>BiomeTexture", folder_path="PCGBiomes")
```

#### Phase 7: Fix Volume Bounds (CRITICAL)

**You MUST reset component scale to (1,1,1) BEFORE setting BoxExtent.**
Default scales can be very large (e.g. 512), causing overflow with large extents.

```python
# BiomeCore
set_property(actor_id="BiomeCore", path="Volume.RelativeScale3D",
    value="(X=1,Y=1,Z=1)")
set_property(actor_id="BiomeCore", path="Volume.BoxExtent",
    value=f"(X={extent_x},Y={extent_y},Z={extent_z + 10000})")

# Each BiomeTexture
set_property(actor_id="<Name>BiomeTexture", path="BiomeTextureVolume.RelativeScale3D",
    value="(X=1,Y=1,Z=1)")
set_property(actor_id="<Name>BiomeTexture", path="BiomeTextureVolume.BoxExtent",
    value=f"(X={extent_x},Y={extent_y},Z={extent_z + 10000})")
```

The `+ 10000` on Z gives headroom above the landscape. Adjust as needed.

#### Phase 8: Assign References

Connect each BiomeTexture to its texture, definition, and assets:

```python
# Texture (same for all biomes in a texture-based setup)
set_property(actor_id="<Name>BiomeTexture", path="BiomeTexture",
    value="<TEXTURE_ASSET_PATH>")

# Definition reference
set_property(actor_id="<Name>BiomeTexture", path="Definition",
    value="/Game/PCGBiomes/Definitions/<Name>Biome.<Name>Biome")

# Assets array
set_property(actor_id="<Name>BiomeTexture", path="Assets",
    value='["/Game/PCGBiomes/Assets/<Name>Assets.<Name>Assets"]')
```

#### Phase 9: Verify

```python
# Confirm all actors exist (label_pattern is substring match)
query_actors(label_pattern="Biome")

# Spot-check references on one biome
get_property(actor_id="<Name>BiomeTexture", path="BiomeTexture")
get_property(actor_id="<Name>BiomeTexture", path="Definition")
get_property(actor_id="<Name>BiomeTexture", path="Assets")
```

### Estimated Tool Calls (per biome count)

| Phase | Per Biome | 4 Biomes |
|-------|-----------|----------|
| Duplicate | 2 | 8 |
| Configure defs | 3 | 12 |
| Configure assets | 1 | 4 |
| Save | 2 | 8 |
| Get bounds | — | 1 |
| Spawn actors | 1 (+1 core) | 5 |
| Fix volumes | 2 (+2 core) | 10 |
| Assign refs | 3 | 12 |
| Verify | — | 3 |
| **Total** | | **~63** |

---

## Editor & Levels

### Level Management

```python
get_current_level()                    # What level is loaded?
save_level(path="/Game/Maps/MyLevel")  # Save to specific path
open_level(path="/Game/Maps/OtherLevel")
new_level()                            # Create empty level
```

### Play-In-Editor (PIE)

PIE is required for Tempo simulation tools (vehicles, pawns, time control):

```python
play_in_editor()   # Start PIE session
simulate()         # Start Simulate mode (physics, no player)
stop()             # Stop PIE or Simulate
```

**After starting PIE**, the world context changes. Use `set_target_world` to switch:

```python
set_target_world(world_identifier="editor")   # Back to editor world
set_target_world(world_identifier="pie")      # PIE world
```

### World Contexts

```python
list_worlds()  # See all available worlds (editor, PIE instances, etc.)
```

**Common mistake:** Running actor operations after PIE starts but forgetting you're now
targeting the PIE world. If actors seem to "disappear," check your target world.

### Console Commands

```python
# Execute any console command
execute_console_command(command="stat fps")
execute_console_command(command="r.ScreenPercentage 50")

# Search 5000+ available commands by keyword
search_console_commands(keyword="shadow")
search_console_commands(keyword="pcg", search_help=true)  # Also search help text
```

---

## World Partition

**Module:** `world_partition` — load with `load_modules(modules=["world_partition"])` if
not in your profile.

Large worlds use World Partition for streaming. Standard `query_actors` only finds loaded
actors. Use these tools for full coverage.

### Patterns

```python
# Check if the world uses World Partition
is_world_partitioned()

# Query ALL actors (loaded + unloaded streaming cells)
query_actors(class_name="StaticMeshActor", include_unloaded=true, limit=200)

# Filter by data layer
query_actors(data_layer="Vegetation", include_unloaded=true)

# Check if a specific actor is loaded
get_streaming_state(actor_guid="<GUID from query>")
# Returns: Loaded | Unloaded | Invalid

# Get all landscape chunks (even unloaded ones)
query_landscape(include_unloaded=true)

# Get complete terrain bounds (used for volume sizing)
get_landscape_bounds()
# Returns: center, min, max, half_extents

# Data layers
get_data_layers()                                              # List all layers
query_actors(data_layer="Buildings", include_unloaded=true)     # Actors in a layer
```

**Key gotcha:** `query_actors(include_unloaded=true)` returns additional fields (`streaming_state`,
`is_spatially_loaded`, `data_layers`) but may not respect `class_name` filtering for unloaded
actors. Filter results client-side if needed.

---

## Tempo Simulation

**Module:** `tempo_sim` — requires `play_in_editor()` first! These tools control the running
simulation, not the editor.

### Time Control

```python
tempo_play()           # Start/resume simulation
tempo_pause()          # Pause
tempo_step()           # Advance exactly one frame (while paused)
tempo_advance_steps(steps=10)  # Advance N frames

# Fixed-step mode for deterministic simulation
tempo_set_time_mode(mode="FIXED_STEP")
tempo_set_sim_rate(steps_per_second=30)

# Wall-clock mode for real-time
tempo_set_time_mode(mode="WALL_CLOCK")
```

### Date, Time, and Geography

```python
tempo_set_date(day=21, month=6, year=2025)
tempo_set_time_of_day(hour=14, minute=30)
tempo_set_day_cycle_rate(rate=60.0)   # 1 hour per minute
tempo_get_datetime()                   # Current sim time

# Set world geographic reference (affects sun position)
tempo_set_geographic_reference(latitude=21.3069, longitude=-157.8583)  # Honolulu
```

### World State

```python
# Get full state of a specific actor (transform, velocity, bounds)
tempo_get_actor_state(actor_name="MyVehicle")

# Find actors near another actor
tempo_get_actors_near(near_actor_name="Player", search_radius=5000,
    include_static=false)
```

### Vehicle & Pawn Control

```python
# Discover controllable entities
tempo_get_commandable_vehicles()
tempo_get_commandable_pawns()

# Drive a vehicle (acceleration: -1 to 1, steering: -1 to 1)
tempo_command_vehicle(vehicle_name="Car1", acceleration=0.5, steering=0.2)

# Move a pawn to location (uses navigation)
tempo_pawn_move_to(pawn_name="NPC_01", location=[1000, 2000, 0])
# Returns: SUCCESS | BLOCKED | OFF_PATH | ABORTED | INVALID
```

**Common mistake:** Calling vehicle/pawn tools without PIE running. These only work during
simulation — call `play_in_editor()` first.

### Navigation & Map

```python
# Rebuild nav mesh after modifying the level
tempo_rebuild_navigation()

# Query road lanes near a point
tempo_get_lanes(center=[0, 0], radius=5000)

# Check if two lanes connect
tempo_get_lane_accessibility(from_id=1, to_id=2)
# Returns: GREEN | YELLOW | RED | STOP_SIGN | YIELD_SIGN | NO_TRAFFIC_CONTROL

# Query zones (areas defined by ZoneShapes)
tempo_get_zones(center=[0, 0, 0], radius=10000)

# Build zone graph for AI navigation
tempo_run_zone_graph_builder()
```

### Sensors & Rendering

```python
# List available cameras/sensors
tempo_get_available_sensors()

# Get semantic label mapping
tempo_get_label_map()

# Toggle viewport rendering (save GPU during headless work)
tempo_set_viewport_render(enabled=false)

# Control mode: NONE | USER | OPEN_LOOP | CLOSED_LOOP
tempo_set_control_mode(mode="OPEN_LOOP")
```

---

## bp_toolkit — Blueprint & Asset Tools

**Module:** `bp_toolkit` — load with `load_modules(modules=["bp_toolkit"])`. Requires the
`bp_toolkit` submodule to be initialized.

26 tools split into three categories: live Blueprint editing, live PCG editing, and offline
asset manipulation.

### Live Blueprint Graph Editing (6 tools)

Edit Blueprint event graphs while the editor is running:

```python
# Create a node in a Blueprint graph
bp_create_node(blueprint_path="/Game/BP_Enemy",
    node_type="CallFunction", function_reference="PrintString")

# List existing nodes and their pins
bp_list_nodes(blueprint_path="/Game/BP_Enemy", graph_name="EventGraph")
bp_list_pins(blueprint_path="/Game/BP_Enemy", node_id="<GUID>")

# Connect two pins
bp_connect_pins(blueprint_path="/Game/BP_Enemy",
    source_node="<GUID1>", source_pin="Then",
    target_node="<GUID2>", target_pin="Execute")

# Disconnect and delete
bp_disconnect_pins(...)  # same params as connect
bp_delete_node(blueprint_path="/Game/BP_Enemy", node_id="<GUID>")
```

**Node types:** `CallFunction`, `Event`, `VariableGet`, `VariableSet`, `Branch`,
`Sequence`, `Comment`

**Gotcha:** The Blueprint must have an initialized EventGraph. Freshly `create_asset`'d
Blueprints may not — prefer `save_actor_as_blueprint` or duplicating existing BPs.

### Live PCG Graph Editing (6 tools)

Edit PCG graphs while the editor is running:

```python
# Add a PCG node
pcg_add_node(graph_path="/Game/MyPCG", node_type="SurfaceSampler")

# List nodes and find input/output
pcg_list_nodes(graph_path="/Game/MyPCG")
pcg_get_input_output_nodes(graph_path="/Game/MyPCG")

# Connect nodes
pcg_connect(graph_path="/Game/MyPCG",
    from_node="SurfaceSampler", from_pin="Out",
    to_node="StaticMeshSpawner", to_pin="In")

# Disconnect and delete
pcg_disconnect(...)   # same params as connect
pcg_delete_node(graph_path="/Game/MyPCG", node_path="<path>")
```

**PCG node types:** `SurfaceSampler`, `StaticMeshSpawner`, `FilterByTag`,
`TransformPoints`, `Branch`, `Difference`, `Union`, `Intersection`, and more.

**Pin naming gotcha:** InputNode's output pin is named `In`, OutputNode's input pin is
named `Out`. Yes, it's backwards — that's UE's convention.

### Offline Asset Manipulation (14 tools)

Parse and modify `.uasset` files as JSON without the editor. Works on Blueprints, PCG
Graphs, DataAssets, Behavior Trees, Materials, and more.

**Workflow:** Export to JSON → modify → reimport:

```python
# Export .uasset to JSON (MUST use Windows paths, not WSL paths)
bp_export_asset(uasset_path="D:/Project/Content/BP_Enemy.uasset")

# Detect what kind of asset it is
bp_detect_type(json_path="D:/Project/Content/BP_Enemy.json")

# Get summary info
bp_get_info(json_path="D:/Project/Content/BP_Enemy.json")

# List/get/set properties by path
bp_list_properties(json_path="...", export_index=0)
bp_get_property(json_path="...", property_path="BiomeDefinition.BiomePriority")
bp_set_property(json_path="...", property_path="...", value=42)

# Clone entire asset
bp_clone_asset(json_path="...", new_name="BP_EnemyVariant")

# List graphs, add comments, clone nodes
bp_list_graphs(json_path="...")
bp_add_comment(json_path="...", graph_name="EventGraph", text="TODO: Fix this")
bp_clone_node(json_path="...", node_name="K2Node_CallFunction_0")

# Search inside asset
bp_find(json_path="...", pattern="Damage")

# Type-specific queries
bp_query(json_path="...", query_type="list-events")

# Full Blueprint parse with call graphs
bp_parse(json_path="...", output_dir="D:/Project/docs/")

# Reimport modified JSON back to .uasset
bp_import_asset(json_path="D:/Project/Content/BP_Enemy.json")
```

**CRITICAL gotcha — Windows paths only!** The offline tools shell out to UAssetGUI.exe,
which requires Windows paths:
```python
# WRONG (WSL path):
bp_export_asset(uasset_path="/mnt/d/Project/Content/BP.uasset")

# RIGHT (Windows path):
bp_export_asset(uasset_path="D:/Project/Content/BP.uasset")
```

**JSON files can be huge** (40-100MB for complex Blueprints). They're gitignored by default.

---

## Function Calls

Call Blueprint and C++ functions directly:

```python
# Static function on a Blueprint library
call_function(call="KismetSystemLibrary::PrintString",
    parameters={"InString": "Hello from agent"})

# Instance method on an actor
call_function(call="MyActor.MyFunction")

# Component method
call_function(call="MyActor.LightComponent0.SetIntensity",
    parameters={"NewIntensity": 5000})

# Function on an asset
call_function(call="/Game/MyPCG.MyPCG::GetInputNode")
```

**Limitation:** Some function routing only works with void/no-arg functions. For complex
operations, prefer `set_property` or console commands.

---

## Type Discovery

When documentation isn't available and you need to explore:

```python
# Search for classes by base class hierarchy (preferred — name_pattern is exact match only)
list_classes(base_class_name="PrimaryDataAsset")
list_classes(base_class_name="DataAsset")

# Get full schema for a class (properties + optionally functions)
get_class_schema(class_name="BP_PCGBiomeTexture_C")
get_class_schema(class_name="PointLightComponent", include_functions=true)

# Inspect a specific actor's properties and components
get_actor(actor_id="MyActor", include_properties=true, include_components=true)
```

**Best practice:** Use `get_actor(include_components=true)` first to learn component instance
names, then `get_class_schema` on the component class if you need the full property list.
Avoid calling `get_class_schema` repeatedly — the results don't change.

---

## Common Workflows

### Set Up a New Actor with Properties

```python
# 1. Spawn
spawn_actor(class_name="PointLight", location=[0, 0, 200],
    label="MyLight", folder_path="Lighting")

# 2. Find component names
get_actor(actor_id="MyLight", include_components=true)

# 3. Set properties using component instance names
set_property(actor_id="MyLight", path="LightComponent0.Intensity", value=10000)
set_property(actor_id="MyLight", path="LightComponent0.LightColor",
    value="(R=255,G=200,B=150,A=255)")
```

### Clone and Modify an Asset

```python
# 1. Duplicate
duplicate_asset(source_path="/Game/Templates/BaseConfig",
    dest_package_path="/Game/Configs", dest_asset_name="NewConfig")

# 2. Modify (use asset path as actor_id)
set_property(actor_id="/Game/Configs/NewConfig.NewConfig",
    path="SomeProperty", value="NewValue")

# 3. Save
save_asset(asset_path="/Game/Configs/NewConfig")
```

### Mass-Spawn Actors in a Pattern

```python
# Get reference point
bounds = get_landscape_bounds()
cx, cy, cz = bounds["center"]

# Spawn in grid
for i in range(5):
    for j in range(5):
        spawn_actor(class_name="PointLight",
            location=[cx + i * 500, cy + j * 500, cz + 200],
            label=f"GridLight_{i}_{j}", folder_path="LightGrid")

# Verify all spawned
query_actors(label_pattern="GridLight")
```

### Save the Level

```python
execute_console_command(command="SaveCurrentLevel")
```

### Explore an Unknown Blueprint Class

```python
# 1. Search for it
list_classes(name_pattern="MyBlueprint")

# 2. Get its schema
get_class_schema(class_name="BP_MyBlueprint_C")

# 3. Spawn one and inspect it
spawn_actor(class_name="BP_MyBlueprint", location=[0,0,0], label="TestInstance")
get_actor(actor_id="TestInstance", include_properties=true, include_components=true)

# 4. Clean up when done
delete_actor(actor_id="TestInstance")
```

---

## Troubleshooting

### "Property set successfully" but value didn't change
**Cause:** You used a JSON object for a struct value.
**Fix:** Use Unreal string format: `"(X=1,Y=2,Z=3)"` instead of `{"X": 1, "Y": 2, "Z": 3}`.

### Volume appears tiny or inverted after setting BoxExtent
**Cause:** Component had a large default scale (e.g. 512). Scale * Extent overflowed.
**Fix:** Reset `RelativeScale3D` to `(X=1,Y=1,Z=1)` before setting BoxExtent.

### spawn_actor fails with "class not found"
**Cause:** Blueprint short name not resolvable.
**Fix:** Use full path: `/PluginName/Path/BP_Name.BP_Name_C`

### duplicate_asset fails
**Cause:** Destination path is not under `/Game/`.
**Fix:** Always use `/Game/...` as `dest_package_path`.

### get_property returns empty `{}` for a struct
**Cause:** Parent struct container returns empty; need to access individual fields.
**Fix:** Use full path to the specific field: `BiomeDefinition.BiomeName` not `BiomeDefinition`.

### Asset reference doesn't take effect
**Cause:** Missing `.AssetName` suffix on the path.
**Fix:** Use `PackageName.AssetName` format: `/Game/Folder/MyAsset.MyAsset`

### Tool not found / "no such tool"
**Cause:** The tool's module isn't loaded in your current profile.
**Fix:** Call `load_modules(modules=["module_name"])`. Check [Module Profiles](#module-profiles).

### Tempo sim tools return errors
**Cause:** PIE is not running.
**Fix:** Call `play_in_editor()` first, then `set_target_world("pie")`.

### bp_toolkit export fails
**Cause:** Using WSL/Linux paths instead of Windows paths.
**Fix:** Use `D:/Project/...` not `/mnt/d/Project/...`

### Actors "disappeared" after PIE
**Cause:** Target world changed to PIE world automatically.
**Fix:** Call `set_target_world("editor")` to return to editor context.

### get_actor component names don't match schema
**Cause:** Using class name instead of instance name in property paths.
**Fix:** Use `get_actor(include_components=true)` to find instance names like
`LightComponent0`, not class names like `PointLightComponent`.

---

## Performance Tips

1. **Don't discover schemas if you have documentation.** Schema calls are expensive. Use this
   document and the help system first.
2. **Batch array operations.** Set an entire array in one `set_property` call rather than
   setting elements individually.
3. **Use `query_actors(label_pattern=...)` to verify.** One call confirms all actors exist.
4. **Save assets in batch** after all modifications are complete, not after each change.
5. **Check `help(topic="workflows")` first** for common multi-step patterns.
6. **Use `get_actor(include_components=true)` once**, then reference component names from
   memory. Don't call it repeatedly for the same actor.
7. **`label_pattern` is faster than `class_name`** for finding specific actors you named.
8. **Folder paths are free organization.** Put all your work in a folder so cleanup is one
   query + delete loop.

---

## Value Format Cheat Sheet

| Type | Example | Notes |
|------|---------|-------|
| bool | `true` / `false` | Lowercase |
| int | `42` | No quotes |
| float | `3.14` | No quotes |
| string | `"hello"` | Quoted |
| FName | `"MyName"` | Quoted string |
| FVector | `"(X=1,Y=2,Z=3)"` | String, Unreal format |
| FRotator | `"(Pitch=0,Yaw=90,Roll=0)"` | String, Unreal format |
| FLinearColor | `"(R=1,G=0.5,B=0,A=1)"` | String, 0-1 range |
| FColor | `"(R=255,G=128,B=0,A=255)"` | String, 0-255 range |
| FTransform | `"(Rotation=(X=0,Y=0,Z=0,W=1),Translation=(X=0,Y=0,Z=0),Scale3D=(X=1,Y=1,Z=1))"` | Nested Unreal format |
| Asset ref | `"/Game/Path/Asset.Asset"` | PackageName.AssetName |
| Asset array | `'["/Game/A.A", "/Game/B.B"]'` | JSON array string |
| Struct array | `'[{"Key": "val", ...}]'` | JSON array with fields |
