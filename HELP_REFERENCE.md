# AgentBridge Help Reference

> Complete dump of all help topics available via the `help()` MCP tool.
> These are what AI agents see when they call `help(topic="...")`.

---

## Table of Contents

| Topic | Description |
|-------|-------------|
| [(overview)](#overview) | Quick start, common classes, units, tips |
| [actors](#actors) | Finding, creating, modifying actors |
| [properties](#properties) | Reading/writing properties with paths, flexible value formats |
| [classes](#classes) | Type discovery, Blueprint class normalization |
| [assets](#assets) | Asset creation, saving, file operations |
| [components](#components) | Component transforms, attachment/detachment |
| [console](#console) | Console command discovery and execution |
| [workflows](#workflows) | Common multi-step operations, PCG biome workflow |
| [pcg_volume](#pcg_volume) | PCG volume types and sizing |
| [volume_sizing](#volume_sizing) | BoxComponent sizing details |
| [bp_toolkit](#bp_toolkit) | Offline asset manipulation (when submodule present) |

---

## Overview

**Topic:** `help()` (no topic argument)

```
AgentBridge - Unreal Engine control for AI agents

BEFORE YOU START:
- AgentBridge core tools (query_actors, spawn_actor, set_property) work in Editor mode
- Tempo simulation tools (tempo_*) require Play-In-Editor (PIE) mode - run play_in_editor first
- bp_toolkit offline tools require Windows paths (D:/folder/file.uasset), not WSL (/mnt/d/...)
- File tools (read_project_file, etc.) use paths relative to the Unreal project root

QUICK START:
1. query_actors - Find actors in the scene (e.g., name_pattern="Light*")
2. spawn_actor - Create new actors (e.g., class_name="PointLight", location=[0,0,500])
3. get_actor - Get detailed info about a specific actor
4. set_actor_transform - Move/rotate/scale actors
5. search_console_commands - Find commands by keyword (e.g., "shadow", "fps")

COMMON CLASSES:
- PointLight, SpotLight, DirectionalLight - Lights
- StaticMeshActor - Static geometry
- CameraActor - Cameras
- PlayerStart - Spawn points
- Blueprint: /Game/BP_Name.BP_Name (the _C suffix is auto-added!)

UNITS:
- Location: centimeters (100 = 1 meter)
- Rotation: degrees [Pitch, Yaw, Roll]
- Scale: multiplier [X, Y, Z] where 1.0 = normal

TIPS:
- Use query_actors first to explore what's in the scene
- Use get_class_schema to see what properties a class has
- Use search_console_commands if you need to do something unusual
- execute_console_command is the escape hatch for anything not covered

ADVANCED CAPABILITIES:
- list_classes(base_class_name="ActorComponent") - List component types
- get_class_schema(class_name="SceneCaptureComponent2D") - Works for ANY class
- call_static_function - Call Blueprint library functions (KismetRenderingLibrary, etc.)

ASSET & FILE OPERATIONS:
- create_asset - Create DataAssets, MaterialInstances, etc.
- save_asset - Save modified assets to disk
- save_actor_as_blueprint - Convert actor to reusable Blueprint
- read_project_file / write_project_file - Read/write files in project directory
- list_project_directory - List directory contents

IMPORTANT - COMPONENT NAMES:
- Components use INSTANCE names (LightComponent0), not class names (PointLightComponent)
- Use get_actor(actor_id, include_components=True) to find the correct component name
- set_property handles all types including colors (use [R,G,B] array format)

Use help(topic='actors|properties|classes|console|workflows|pcg_volume|volume_sizing|bp_toolkit')
for detailed help.
```

---

## Actors

**Topic:** `help(topic="actors")`

```
ACTOR OPERATIONS:

Finding actors:
- query_actors(class_name="PointLight") - Filter by type (recommended)
- query_actors(label_pattern="MainLight") - Filter by display label (NEW! Most useful!)
- query_actors(name_pattern="Door") - Filter by internal name
- query_actors(tag="Interactive") - Filter by tag
- get_actor(actor_id="MyLight", include_properties=True) - Full details

LABEL PATTERN vs NAME PATTERN:
- label_pattern: Matches display names (what you see in editor), e.g., "MainLight", "Floor"
- name_pattern: Matches internal names like "PointLight_UAID_123..."

TIP: Use label_pattern for most searches - it matches human-readable names!

Creating actors:
- spawn_actor(class_name="PointLight", location=[0,0,500], label="MyLight")
- spawn_actor(class_name="/Game/BP_Enemy.BP_Enemy", location=[100,0,0])
  (Note: _C suffix is auto-added for Blueprint classes)

Modifying actors:
- set_actor_transform(actor_id="MyLight", location=[100,200,300])
- set_property_path(actor_id="MyLight", path="LightComponent.Intensity", value=5000)
- delete_actor(actor_id="MyLight")

Identifying actors:
- actor_id can be: name, label, path, or GUID
- Labels are editor display names (human-readable)
- Names are internal unique identifiers
```

---

## Properties

**Topic:** `help(topic="properties")`

```
PROPERTY OPERATIONS:

Reading properties:
- get_actor(actor_id, include_properties=True) - All properties
- get_property(actor_id, path="RootComponent.RelativeLocation") - Specific path

Setting properties:
- set_property(actor_id, path="LightComponent0.Intensity", value="5000")
- set_property(actor_id, path="LightComponent0.LightColor", value=[1, 0, 0])

Property paths:
- Simple: "bHidden", "ActorLabel"
- Nested: "RootComponent.RelativeLocation.X"
- Array: "Materials[0]"
- Component: "LightComponent0.Intensity"

FLEXIBLE VALUE FORMATS:
set_property accepts multiple input formats and auto-converts to Unreal format:

Colors (auto-detected from path containing 'color' or RGBA keys):
- [1, 0, 0] -> Red (RGB, alpha=1.0)
- [1, 0, 0, 0.5] -> Red with 50% alpha (RGBA)
- {"r": 1, "g": 0, "b": 0} -> Red (dict format)
- "#FF0000" -> Red (hex format)

Vectors (default for 3-element lists):
- [100, 200, 300] -> location/offset
- {"x": 100, "y": 200, "z": 300} -> dict format

Rotators (auto-detected from path containing 'rotation'):
- [0, 90, 0] -> Pitch=0, Yaw=90, Roll=0
- {"pitch": 0, "yaw": 90, "roll": 0} -> dict format

Simple values:
- "5000" or 5000 for numbers
- true/false for booleans

CRITICAL - COMPONENT NAMING:
Component names are INSTANCE names, not class names!
- WRONG: "PointLightComponent.Intensity" (class name)
- RIGHT: "LightComponent0.Intensity" (instance name)

To find component instance names:
- get_actor(actor_id, include_components=True)
- Or use: get_actor(actor_id="MyLight", include_components=True)

Common instance names:
- PointLight -> LightComponent0
- StaticMeshActor -> StaticMeshComponent0
- CameraActor -> CameraComponent0

WORKING WITH DATAASSETS:
set_property and get_property work with DataAssets, not just actors!

Use the full asset path as actor_id:
  get_property(actor_id="/Game/MyFolder/MyDataAsset.MyDataAsset",
               path="SomeProperty")

  set_property(actor_id="/Game/MyFolder/MyDataAsset.MyDataAsset",
               path="SomeProperty", value="NewValue")

This enables programmatic configuration of:
- BiomeDefinitionTemplate assets
- BiomeAssetTemplate assets
- Any PrimaryDataAsset subclass
- MaterialInstanceConstant (parent references)

Note: The double name format (AssetName.AssetName) is required for UObject resolution.

SETTING ARRAYS WITH OBJECT REFERENCES:
Arrays of structs containing object refs require a two-step process:

Step 1: Create array elements with simple properties only
  set_property(actor_id="MyActor", path="MyArray",
               value='[{"Enabled":true, "Weight":1.0}]')

Step 2: Set object references individually
  set_property(actor_id="MyActor", path="MyArray[0].Mesh",
               value="/Game/Meshes/SM_Tree.SM_Tree")

WRONG (will fail):
  set_property(path="MyArray",
      value='[{"Mesh":"/Game/..."}]')  # Object ref in initial set = FAIL

READING NESTED STRUCT PROPERTIES:
When reading nested struct properties:
  get_property(path="DefaultDefinition")
  # Returns: {} (empty, even when populated!)

  get_property(path="DefaultDefinition.BiomeName")
  # Returns: "Forest" (correct!)

Always use the full property path to read nested struct values.

Use get_class_schema(class_name) to discover available properties!
```

---

## Classes

**Topic:** `help(topic="classes")`

```
CLASS DISCOVERY:

Finding classes:
- list_classes(base_class_name="Light") - Find all light types
- list_classes(name_pattern="*Vehicle*") - Wildcard search
- find_class(class_name="PointLight") - Get class info
- get_class_schema(class_name, include_functions=True) - Full schema

Built-in classes (no path needed):
- PointLight, SpotLight, DirectionalLight, RectLight
- StaticMeshActor, SkeletalMeshActor
- CameraActor, CineCameraActor
- PlayerStart, TargetPoint, Note
- TriggerBox, TriggerSphere, BlockingVolume

Blueprint classes:
- /Game/Blueprints/BP_Enemy.BP_Enemy (path to the asset)
- /Game/Characters/BP_Player.BP_Player
- BP_Enemy (short name with BP_ prefix)

BLUEPRINT CLASS NORMALIZATION:
The _C suffix (for Blueprint Generated Class) is AUTOMATICALLY added!
You can use either format:
- /Game/BP_Enemy.BP_Enemy -> auto-converted to /Game/BP_Enemy.BP_Enemy_C
- BP_Enemy -> auto-converted to BP_Enemy_C
- /Game/BP_Enemy.BP_Enemy_C -> unchanged (already has _C)

This works for: spawn_actor, query_actors, get_class_schema, list_classes
```

---

## Assets

**Topic:** `help(topic="assets")`

```
ASSET & FILE OPERATIONS:

Creating assets WITH PROPERTIES:
- create_asset(asset_class="DataAsset", package_path="/Game/Data", asset_name="MyData",
               properties={"MyProperty": "value", "MyNumber": 42})
- create_asset(asset_class="MaterialInstanceConstant", package_path="/Game/Materials",
               asset_name="MI_Wood", parent_asset_path="/Game/Materials/M_Wood")

The 'properties' parameter sets initial values when creating DataAssets or custom assets.
Property names must match the asset class definition (use get_class_schema to check).

Saving assets:
- save_asset(asset_path="/Game/Data/MyData") - Save to disk (required to persist!)
- save_actor_as_blueprint(actor_id="MyActor", package_path="/Game/Blueprints",
                          blueprint_name="BP_MyActor") - Convert actor to Blueprint

Asset management:
- duplicate_asset(source_path="/Game/Data/MyData", dest_path="/Game/Data",
                  new_name="MyData_Copy")
- get_asset_thumbnail(asset_path="/Game/Meshes/Chair") - Get preview image (base64 PNG)

File operations (constrained to project directory):
- read_project_file(relative_path="Config/DefaultGame.ini") - Read text/binary file
- write_project_file(relative_path="Saved/MyData.json", content="...") - Write file
- list_project_directory(relative_path="Content/Blueprints") - List directory
- copy_project_file(source="A.txt", dest="B.txt") - Copy file

WORKFLOW - Creating a DataAsset:
1. create_asset(asset_class="MyDataAsset", package_path="/Game/Data",
                asset_name="Config1", properties={"Value": 100})
2. save_asset(asset_path="/Game/Data/Config1") - Persist to disk

IMPORTANT:
- All file paths are relative to project root
- File operations are sandboxed - cannot access files outside project
- Binary files are base64 encoded in transport
- Assets created but not saved will be lost when editor closes
```

---

## Components

**Topic:** `help(topic="components")`

```
COMPONENT OPERATIONS:

Getting component transforms:
- get_component_transform(actor_id="MyActor", component_name="MeshComponent0")
- get_component_transform(actor_id, component_name, world_space=True)  # World coords

Setting component transforms:
- set_component_transform(actor_id, component_name, location=[100,0,0])
- set_component_transform(actor_id, component_name, rotation=[0,45,0], world_space=True)

Attaching actors to each other:
- attach_actor(child_actor_id="MovingLight", parent_actor_id="Vehicle")
- attach_actor(child_id, parent_id, parent_component_name="Turret", socket_name="GunMount")

Attachment rules (location_rule, rotation_rule, scale_rule):
- "keep_world" - Maintain world position (default for actors)
- "keep_relative" - Maintain relative offset (default for components)
- "snap_to_target" - Snap to parent's position

Attaching components within an actor:
- attach_component(actor_id, component_name="Light", parent_component_name="Arm")

Detaching:
- detach_actor(actor_id="MovingLight")  # Keep world position
- detach_component(actor_id, component_name, maintain_world_position=True)

Common use cases:
- Attach light to moving vehicle: attach_actor(light, vehicle)
- Build component hierarchies: attach_component in sequence
- Reparent actors: detach + attach to new parent
```

---

## Console

**Topic:** `help(topic="console")`

```
CONSOLE COMMANDS:

Discovery:
- search_console_commands(keyword="shadow") - Find shadow-related
- search_console_commands(keyword="fps", search_help=True) - Search descriptions too
- search_console_commands(keyword="r.", limit=20) - Find rendering CVars

Execution:
- execute_console_command(command="stat fps") - Show FPS overlay
- execute_console_command(command="r.Shadow.MaxResolution 2048") - Set CVar

Useful commands:
- stat fps / stat unit - Performance stats
- show collision - Toggle collision visualization
- viewmode lit/unlit/wireframe - Change view mode
- slomo 0.5 - Slow motion (0.0-1.0)

AgentBridge commands (for debugging):
- AgentBridge.ListWorlds - Show world contexts
- AgentBridge.QueryActors Light 10 - Quick actor search
- AgentBridge.DumpActor MyActor - Dump actor properties
```

---

## Workflows

**Topic:** `help(topic="workflows")`

```
COMMON WORKFLOWS:

Building a simple scene:
1. query_actors() - See what's already there
2. spawn_actor(class_name="PointLight", location=[0,0,500], label="MainLight")
3. spawn_actor(class_name="StaticMeshActor", location=[0,0,0], label="Floor")
4. set_property_path("MainLight", "LightComponent0.Intensity", 10000)

Setting light colors:
1. spawn_actor(class_name="PointLight", location=[0,0,500], label="MyLight")
2. get_actor(actor_id="MyLight", include_components=True)  # Find component names
3. set_property(actor_id="MyLight", path="LightComponent0.LightColor", value="(R=1,G=0,B=0)")
Note: Colors can use UE format "(R=1,G=0,B=0)" with 0-1 range or hex "#FF0000"

Finding and modifying actors:
1. query_actors(name_pattern="*Door*") - Find all doors
2. get_actor("Door_01", include_properties=True) - Inspect one
3. set_property_path("Door_01", "bLocked", True) - Modify it

Exploring available options:
1. list_classes(base_class_name="Light") - What lights exist?
2. get_class_schema("SpotLight", include_functions=True) - What can I set?
3. search_console_commands("shadow") - Any shadow settings?

World Partition (large worlds):
1. is_world_partitioned() - Check if WP is enabled
2. query_all_actors(include_unloaded=True) - Find unloaded actors
3. get_streaming_state(actor_guid) - Check if actor is loaded

Calling static Blueprint library functions:
1. get_class_schema("KismetSystemLibrary", include_functions=True) - See available functions
2. call_static_function("KismetSystemLibrary", "PrintString", {"InString": "Hello!"})
3. call_static_function("KismetMathLibrary", "Abs", {"A": -42})  # Returns {"return_value": 42}

Examples of useful static functions:
- KismetSystemLibrary::PrintString - Debug output
- KismetSystemLibrary::ExecuteConsoleCommand - Run console commands
- KismetRenderingLibrary::CreateRenderTarget2D - Create render targets
- KismetMathLibrary::* - Math operations

Creating and saving assets:
1. spawn_actor(class_name="PointLight", location=[0,0,500], label="MyLight")
2. set_property_path("MyLight", "LightComponent0.Intensity", 10000)
3. save_actor_as_blueprint("MyLight", "/Game/Blueprints", "BP_MyLight")
4. save_asset("/Game/Blueprints/BP_MyLight") - Save Blueprint to disk

Working with project files:
1. list_project_directory("Config") - See config files
2. read_project_file("Config/DefaultGame.ini") - Read config
3. write_project_file("Saved/MyBackup.json", '{"key": "value"}') - Write data

PCG BIOME WORKFLOW (Texture-Based Biomes):
Setting up procedural content generation with biomes. Each biome maps a
color channel in a texture to a set of meshes spawned via PCG.

Architecture:
  BiomeCore (1 per level)          BiomeTexture (1 per biome)
  +-- Volume (box bounds)          +-- BiomeTextureVolume (box bounds)
  +-- PCG Component                +-- Definition -> BiomeDefinitionTemplate
                                   +-- Assets[] -> BiomeAssetTemplate[]
                                   +-- PCG_LocalBiomeCore (regen trigger)

Phase 1 - Get landscape bounds:
   bounds = get_landscape_bounds()
   # Returns: center, extent, min, max, biome_volume_scale
   # biome_volume_scale: pre-computed scale for 100-unit-extent BoxComponent
   #   to cover landscape (Z includes +10000 headroom)

Phase 2 - Duplicate template DataAssets:
   # IMPORTANT: duplicate from /Game/ copies, NOT plugin paths (crash risk)
   duplicate_asset(source_path="/Game/PCGBiomes/Templates/DefaultDefinition",
       dest_package_path="/Game/PCGBiomes/Definitions", dest_asset_name="ForestBiome")
   duplicate_asset(source_path="/Game/PCGBiomes/Templates/DefaultAsset",
       dest_package_path="/Game/PCGBiomes/Assets", dest_asset_name="ForestAssets")

Phase 3 - Configure BiomeDefinition DataAsset:
   set_property(actor_id="/Game/PCGBiomes/Definitions/ForestBiome.ForestBiome",
       path="BiomeDefinition.BiomeName", value="Forest")
   set_property(..., path="BiomeDefinition.BiomePriority", value=1)
   set_property(..., path="BiomeDefinition.BiomeColor",
       value="(R=0,G=1,B=0,A=1)")
   # Note: Colors written as FLinearColor (0-1), read back as FColor (0-255)

Phase 4 - Configure BiomeAsset DataAsset (two-step for object refs):
   set_property(actor_id="/Game/PCGBiomes/Assets/ForestAssets.ForestAssets",
       path="BiomeAssets",
       value='[{"Enabled":true, "AssetType":"Mesh", "Weight":1.0}]')
   # Then set object references individually
   set_property(..., path="BiomeAssets[0].Mesh",
       value="/Game/Foliage/SM_Tree.SM_Tree")

Phase 5 - Save DataAssets:
   save_asset(asset_path="/Game/PCGBiomes/Definitions/ForestBiome")
   save_asset(asset_path="/Game/PCGBiomes/Assets/ForestAssets")

Phase 6 - Spawn actors at landscape center:
   spawn_actor(class_name="/Game/PCGBiomes/Templates/BP_PCGBiomeCore.BP_PCGBiomeCore",
       location=bounds["center"], label="BiomeCore")
   spawn_actor(class_name="/Game/PCGBiomes/Templates/BP_PCGBiomeTexture.BP_PCGBiomeTexture",
       location=bounds["center"], label="ForestBiomeTexture")
   # Use full /Game/ class paths for Blueprint classes

Phase 7 - Size volumes using set_transform + biome_volume_scale:
   scale = bounds["biome_volume_scale"]  # e.g., [2040, 2040, 101]
   set_transform(target="BiomeCore->Volume", scale=scale, world_space=true)
   set_transform(target="ForestBiomeTexture->BiomeTextureVolume",
       scale=scale, world_space=true)
   # Do NOT use set_property on BoxExtent - stores value but no visual update

Phase 8 - Assign references on each BiomeTexture:
   set_property(actor_id="ForestBiomeTexture", path="BiomeTexture",
       value="/Game/Textures/BiomeMap.BiomeMap")
   set_property(actor_id="ForestBiomeTexture", path="Definition",
       value="/Game/PCGBiomes/Definitions/ForestBiome.ForestBiome")
   set_property(actor_id="ForestBiomeTexture", path="Assets",
       value='["/Game/PCGBiomes/Assets/ForestAssets.ForestAssets"]')

Phase 9 - Verify references:
   get_property(actor_id="ForestBiomeTexture", path="Definition")
   get_property(actor_id="ForestBiomeTexture", path="Assets")
   get_transform(target="ForestBiomeTexture->BiomeTextureVolume")

Phase 10 - Trigger PCG regeneration:
   call_function(call="ForestBiomeTexture.PCG_LocalBiomeCore.NotifyPropertiesChangedFromBlueprint")
   # MUST target BiomeTexture's PCG_LocalBiomeCore, NOT the top-level BiomeCore

BIOME ASSET MODIFICATION (after initial setup):
   set_property(..., path="BiomeAssets[0].Weight", value=2.5)
   set_property(..., path="BiomeAssets[0].Mesh", value="/Game/Mesh.Mesh")
   set_property(..., path="BiomeAssets[0].AssetOptions.Scale", value="(X=3,Y=3,Z=3)")
   # Then save_asset + call_function to trigger regen (Phase 10)
   # 3-level nesting works: BiomeAssets[0].AssetOptions.Scale

KEY SUB-STRUCT FIELDS:
   AssetOptions.Scale (FVector) - Mesh size (default 1,1,1)
   AssetOptions.Translation/Rotation - Placement offset
   MeshOptions.CastShadow (bool), .Visible (bool), .Material (ref)
   FilterOptions.DensityMin/DensityMax, .HeightMin/.HeightMax
   RuntimeOptions.ScaleMultiplier, .MeshSamplingRadius
   DebugOptions.Isolate (bool), .ShowBounds (bool)

BIOME COLOR TOLERANCE TROUBLESHOOTING:
If no content spawns after setup, check BiomeColorTolerance on each BiomeTexture:
1. Try BiomeColorTolerance = 0.1 (catches minor imprecision)
2. Try 0.5 (catches major mismatches)
3. Try 0.99 (sanity check - if this fails, not a color issue)
Default 0.01 is very tight - 0.1 is often more practical.

FUNCTION CALLS:
call_function supports zero-arg void functions only (instance methods on actors/components).
Useful for NotifyPropertiesChangedFromBlueprint() for PCG regeneration.
For parameterized operations, use set_property, set_transform, or console commands.

VOLUME COMPONENT NAMES:
| Blueprint           | Component Name        |
| BP_PCGBiomeCore     | Volume                |
| BP_PCGBiomeTexture  | BiomeTextureVolume    |

TIPS:
- Use get_landscape_bounds() with biome_volume_scale for easy volume sizing
- Use label_pattern to find PCG-spawned actors
- Always verify component names with get_actor(include_components=True)
- Use full /Game/ asset paths for Blueprint class spawning
```

### bp_toolkit Addendum (appended when bp_toolkit submodule is present)

```
BP_TOOLKIT WORKFLOWS (Offline Asset Manipulation):
These tools work WITHOUT Unreal running - pure JSON manipulation via UAssetGUI.

Exporting and analyzing a Blueprint:
1. bp_export_asset(uasset_path="/Game/Blueprints/BP_Enemy.uasset")
   # Creates BP_Enemy.json next to the uasset
2. bp_detect_type(json_path="BP_Enemy.json")
   # Returns: {"asset_type": "Blueprint"}
3. bp_get_info(json_path="BP_Enemy.json")
   # Returns: exports count, imports, graphs, namemap size
4. bp_query(json_path="BP_Enemy.json", query_type="list-events")
   # Returns: BeginPlay, Tick, etc.

Modifying a DataAsset:
1. bp_export_asset(uasset_path="D:/Content/BiomeDefinitions/Forest.uasset")
2. bp_get_property(json_path="Forest.json", property_path="BiomeDefinition.BiomePriority")
   # Returns: {"value": 3}
3. bp_set_property(json_path="Forest.json", property_path="BiomeDefinition.BiomePriority", value=10)
4. bp_import_asset(json_path="Forest.json")
   # Converts back to .uasset - reload in editor to see changes

Cloning an asset with modifications:
1. bp_export_asset(uasset_path="D:/Content/Biomes/Forest.uasset")
2. bp_clone_asset(json_path="Forest.json", new_name="Desert")
   # Creates Desert.json with updated name/folder references
3. bp_set_property(json_path="Desert.json", property_path="BiomeDefinition.BiomeColor",
                   value={"R": 0.9, "G": 0.7, "B": 0.4, "A": 1.0})
4. bp_import_asset(json_path="Desert.json")
   # Creates Desert.uasset

Adding documentation comments to a Blueprint:
1. bp_export_asset(uasset_path="BP_Character.uasset")
2. bp_list_graphs(json_path="BP_Character.json")
   # Returns: EventGraph, Walk, Jump, etc.
3. bp_add_comment(json_path="BP_Character.json", graph_name="EventGraph",
                  text="TODO: Add death handling", x=0, y=-500, width=400, height=100)
4. bp_import_asset(json_path="BP_Character.json")

Searching and querying assets:
- bp_find(json_path="BP_Enemy.json", pattern="Health")
  # Searches namemap and exports for "Health"
- bp_query(json_path="BP_Enemy.json", query_type="variables")
  # Lists all variable Get/Set nodes
- bp_query(json_path="BT_AI.json", query_type="list-tasks")
  # Lists Behavior Tree task nodes
- bp_query(json_path="M_Wood.json", query_type="textures")
  # Lists texture references in material

Full Blueprint parsing with call graphs:
1. bp_export_asset(uasset_path="BP_ComplexCharacter.uasset")
2. bp_parse(json_path="BP_ComplexCharacter.json", output_dir="./parsed/")
   # Generates: call_graph.json, Mermaid diagrams, function docs

QUERY TYPES BY ASSET:
| Asset Type | Query Types |
|------------|-------------|
| Blueprint | list-events, list-functions, variables, comments, flow-tagged |
| PCG Graph | list-nodes, connections, input-output |
| Behavior Tree | list-tasks, list-decorators, blackboard |
| Material | textures, shader-inputs |
| Niagara | emitters, modules |
```

---

## PCG Volume

**Topic:** `help(topic="pcg_volume")`

```
PCG VOLUME TYPES:

There are different PCG volume types with different component structures:

BP_PCGBiomeCore (main biome system, 1 per level):
  - Uses BoxComponent named "Volume"
  - Contains BiomeCore PCGComponent
  - spawn_actor(class_name="BP_PCGBiomeCore", ...)

BP_PCGBiomeTexture (1 per biome, RECOMMENDED):
  - Uses BoxComponent named "BiomeTextureVolume"
  - Has Definition, Assets, BiomeTexture properties
  - Contains PCG_LocalBiomeCore (trigger regen here!)
  - spawn_actor(class_name="BP_PCGBiomeTexture", ...)

Native PCGVolume (NOT recommended for biomes):
  - Uses BrushComponent (harder to resize programmatically)
  - spawn_actor(class_name="PCGVolume", ...)
  - Component name: "BrushComponent0"

SIZING VOLUMES - USE set_transform (NOT set_property on BoxExtent):

set_property on BoxExtent stores values but does NOT trigger visual/bounds updates.
The default BoxExtent for both BP types is [100, 100, 100]. Use scale instead.

1. GET LANDSCAPE BOUNDS + biome_volume_scale:
   bounds = get_landscape_bounds()
   scale = bounds["biome_volume_scale"]  # Pre-computed, includes Z headroom

2. APPLY SCALE VIA set_transform:
   set_transform(target="BiomeCore->Volume", scale=scale, world_space=true)
   set_transform(target="ForestBiomeTexture->BiomeTextureVolume",
       scale=scale, world_space=true)

MANUAL CALCULATION (if biome_volume_scale not available):
   sx = bounds["extent"][0] / 100
   sy = bounds["extent"][1] / 100
   sz = (bounds["extent"][2] + 10000) / 100  # Z with headroom
   set_transform(target="...", scale=[sx, sy, sz], world_space=true)

VOLUME COMPONENT NAMES:
| Blueprint           | Component Name        |
| BP_PCGBiomeCore     | Volume                |
| BP_PCGBiomeTexture  | BiomeTextureVolume    |
| Native PCGVolume    | BrushComponent0       |

COMMON MISTAKES:
- Using set_property on BoxExtent (stores value but no visual update)
- Using native PCGVolume (BrushComponent) instead of BP_PCGBiomeTexture
- Confusing component names between BP types (Volume vs BiomeTextureVolume)
- Forgetting to include Z headroom for PCG spawn variation
```

---

## Volume Sizing

**Topic:** `help(topic="volume_sizing")`

```
SIZING BOXCOMPONENT VOLUMES:

PREFERRED: Use set_transform for scale-based sizing.
set_property on BoxExtent stores the value but doesn't trigger visual updates
(no UpdateBounds() / MarkRenderStateDirty() call). Use set_transform instead.

USING set_transform (RECOMMENDED):
The default BoxExtent is typically [100, 100, 100]. Scale = desired_half_extent / 100.

1. For landscape-covering volumes, use biome_volume_scale:
   bounds = get_landscape_bounds()
   scale = bounds["biome_volume_scale"]  # Pre-computed with Z headroom
   set_transform(target="MyActor->Volume", scale=scale, world_space=true)

2. For custom sizes:
   # Want a 2000x2000x1000 volume (half-extents: 1000x1000x500)
   set_transform(target="MyActor->Volume", scale=[10, 10, 5], world_space=true)

USING set_property (NOT recommended - visual won't update):
If you must use set_property, the wireframe won't update in editor but the
bounds ARE stored correctly. Values take effect on next level load or PIE.

KEY PROPERTIES ON BOXCOMPONENT:
- BoxExtent (FVector): HALF-SIZE in each axis (default: 100,100,100)
- RelativeScale3D (FVector): Scale multiplier
- RelativeLocation (FVector): Offset from actor root

RELATIONSHIP: BoxExtent x Scale = Actual half-size
- BoxExtent=[100,100,100] and Scale=[10,10,5] -> actual half-size = 1000x1000x500

COMPONENT INSTANCE NAMES (always verify with get_actor):
| Blueprint           | Component Name        |
| BP_PCGBiomeCore     | Volume                |
| BP_PCGBiomeTexture  | BiomeTextureVolume    |
| TriggerBox          | CollisionComp         |
| Native volumes      | BoxComponent0         |

UNITS: All values in Unreal units (centimeters)
- 100 units = 1 meter
- Typical game level: 10000-50000 units per axis
```

---

## BP Toolkit

**Topic:** `help(topic="bp_toolkit")` (only available when bp_toolkit submodule is present)

```
BP_TOOLKIT - Offline Asset Manipulation Tools

These 14 tools work WITHOUT Unreal running. They manipulate UAssetAPI JSON exports
directly, using UAssetGUI for uasset <-> JSON conversion.

EXPORT/IMPORT:
- bp_export_asset(uasset_path) - Export .uasset to .json (uses UAssetGUI)
- bp_import_asset(json_path) - Import .json back to .uasset

ANALYSIS:
- bp_detect_type(json_path) - Detect asset type (Blueprint, PCG, DataAsset, etc.)
- bp_get_info(json_path) - Get summary (exports, imports, graphs, namemap)
- bp_list_properties(json_path, export_index=0) - List all properties with types
- bp_get_property(json_path, property_path) - Get property by path
- bp_find(json_path, pattern) - Search namemap and exports
- bp_query(json_path, query_type) - Type-specific queries

MODIFICATION:
- bp_set_property(json_path, property_path, value) - Modify property
- bp_clone_asset(json_path, new_name) - Clone with new name
- bp_add_comment(json_path, graph_name, text, x, y) - Add comment node
- bp_clone_node(json_path, node_name) - Clone existing node
- bp_list_graphs(json_path) - List graphs in Blueprint/PCG

PARSING:
- bp_parse(json_path, output_dir) - Full parsing with call graphs

PROPERTY PATH SYNTAX:
- Simple: "BiomePriority"
- Nested struct: "BiomeDefinition.BiomePriority"
- Array access: "BiomeAssets[0].Generator"
- Deep nesting: "BiomeAssets[0].FilterOptions.MinScale"

Note: Property names from Blueprints include GUID suffixes internally
(e.g., "BiomePriority_29_308259B0449F...") but you can use just the base name.

SETUP REQUIRED:
1. git submodule update --init --recursive
2. cd bp_toolkit/vendor/UAssetGUI && dotnet build -c Release
```
