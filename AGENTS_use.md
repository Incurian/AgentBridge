# AGENTS_use.md - AgentBridge Usage Guide

> For AI agents using AgentBridge tools to control Unreal Editor.
> This file is for operating AgentBridge, not developing it. For implementation work, use `AGENTS_dev.md`.

---

## 1) Quick Start

Prerequisites:
- Unreal Editor is running
- Tempo gRPC service is up on port `10001`
- MCP server is connected

Editor launch (from project root):

```bash
cd <PROJECT_ROOT> && ./Plugins/Tempo/Scripts/Run.sh
```

If force-kill is needed on Windows from Git Bash:

```bash
cmd //c "taskkill /F /IM UnrealEditor.exe"
```

Important:
- gRPC is not ready immediately; wait ~30 seconds after launch.
- `tempo_quit` can hang on save dialogs; `taskkill /F` is the reliable fallback.

---

## 2) First Tool Calls

Start with help:

1. `help()`
2. `help(topic="actors")`
3. `help(topic="properties")`
4. `help(topic="workflows")`

If a tool is missing, load modules:

```python
load_modules(modules=["bp_toolkit"])
load_modules(modules=["world_partition", "tempo_sim"])
```

Profiles:
- `standard` (default): level editing
- `editor`: world partition + landscape queries
- `scripting`: blueprint/PCG graph tools
- `simulation`: tempo simulation tools
- `full`: all modules

---

## 3) Critical Rules (Read Before Acting)

### Rule 1: Struct writes use Unreal string format

Use parenthesized Unreal format, not JSON objects.

```python
set_property(..., path="RootComponent.RelativeLocation", value="(X=100,Y=200,Z=300)")
set_property(..., path="LightComponent0.LightColor", value="(R=1,G=0,B=0,A=1)")
```

### Rule 2: Size biome volumes with `set_transform`, not BoxExtent writes

`set_property` on `BoxExtent` may persist data but not update visual/render bounds.
Use component world scale:

```python
bounds = get_landscape_bounds()
sx = bounds["extent"][0] / 100
sy = bounds["extent"][1] / 100
sz = (bounds["extent"][2] + 10000) / 100
set_transform(target="BiomeCore->Volume", scale=[sx, sy, sz], world_space=True)
```

### Rule 3: `duplicate_asset` destination must be under `/Game/`

```python
duplicate_asset(source_path="...", dest_package_path="/Game/MyFolder")
```

### Rule 4: Asset references require `Package.Asset` form

```python
set_property(..., path="Definition", value="/Game/Biomes/RedBiome.RedBiome")
```

### Rule 5: Prefer full Blueprint class paths for plugin assets

```python
spawn_actor(class_name="/PCGBiomeCore/Blueprints/BP_PCGBiomeCore.BP_PCGBiomeCore_C", ...)
```

Short names can fail on fresh sessions if not loaded.

### Rule 6: Verify object references after setting

```python
set_property(actor_id="MyActor", path="Definition", value="/Game/Biomes/RedBiome.RedBiome")
get_property(actor_id="MyActor", path="Definition")
```

---

## 4) Property Access Patterns

### Read/write asymmetry is expected

- Write structs as Unreal strings.
- Readback returns JSON-like lowercase fields.

### Use component instance names, not class names

Find names first:

```python
get_actor(actor_id="MyLight", include_components=True)
```

Then use:

```python
get_property(actor_id="MyLight", path="LightComponent0.Intensity")
```

### Arrays

Object reference arrays need JSON array string:

```python
set_property(actor_id="MyActor", path="Assets",
             value='["/Game/A.AssetA", "/Game/B.AssetB"]')
```

For nested struct arrays, element-level writes are most reliable:

```python
set_property(actor_id="/Game/X.X", path="BiomeAssets[0].Mesh",
             value="/Game/Meshes/SM_Tree.SM_Tree")
```

### DataAssets

Use full asset path as `actor_id`:

```python
get_property(actor_id="/Game/MyData/MyAsset.MyAsset", path="SomeStruct.SomeField")
```

---

## 5) Actor and Transform Patterns

Spawn/query:

```python
spawn_actor(class_name="PointLight", location=[0, 0, 200], label="KeyLight")
query_actors(label_pattern="KeyLight")
```

Use `label_pattern` over `name_pattern` in most cases.

Unified transform (actors and components):

```python
set_transform(target="MyActor", location=[100, 0, 0])
set_transform(target="MyActor->SomeComponent", rotation=[0, 90, 0])
get_transform(target="MyActor->SomeComponent", world_space=False)
```

Attachment:

```python
attach(child="Lamp", parent="Table")
detach(target="Lamp")
```

---

## 6) Asset Operations

Preferred creation path: duplicate templates, then edit, then save.

```python
duplicate_asset(source_path="/Game/Templates/DefaultBiome",
                dest_package_path="/Game/PCGBiomes/Definitions",
                dest_asset_name="ForestBiome")

set_property(actor_id="/Game/PCGBiomes/Definitions/ForestBiome.ForestBiome",
             path="BiomeDefinition.BiomeName", value="Forest")

save_asset(asset_path="/Game/PCGBiomes/Definitions/ForestBiome")
```

Constraints:
- Save assets after modification (`save_asset`) or changes remain in-memory.
- Keep duplication destinations in `/Game/`.

---

## 7) PCG Biome Essentials

Minimal reliable sequence:

1. Duplicate templates into `/Game/` first.
2. Create one Definition + one Asset DataAsset per biome.
3. Configure definition fields (name, color, priority).
4. Configure mesh entries in biome assets.
5. Save assets.
6. Spawn one BiomeCore and biome texture actors.
7. Size volumes with `set_transform` on components (`Volume`, `BiomeTextureVolume`).
8. Set `Definition`, `Assets`, `BiomeTexture` references.
9. Verify references with `get_property`.
10. Trigger regeneration.

Volume component names:
- `BP_PCGBiomeCore` -> `Volume`
- `BP_PCGBiomeTexture` -> `BiomeTextureVolume`

---

## 8) Troubleshooting Quick Map

- Property "set succeeded" but value unchanged:
  - Check type/path; for structs use Unreal string format.
- Volume wireframe unchanged:
  - Use `set_transform` scale, not `BoxExtent` property writes.
- `spawn_actor` class not found:
  - Use full class path with `_C`, especially for plugin Blueprints.
- Asset reference reads empty:
  - Ensure `Package.Asset` format and correct expected type.
- Tool missing:
  - Load needed module/profile.
- Tempo sim tools failing:
  - Enter PIE first when required.
- `bp_toolkit` export path errors:
  - Use Windows paths (e.g., `D:/...`) for offline toolkit tools.

---

## 9) Efficiency Tips

- Prefer targeted `get_property` over `get_actor(..., include_properties=True)`.
- Query by `label_pattern` for predictable filtering.
- Reuse known class/property/component names from prior calls.
- Use `help(topic=...)` before discovery-heavy probing.
- Batch logically related operations, then verify key outputs.

---

## 10) Safety Checklist Before Finishing

1. Verify critical writes via `get_property` / `get_transform`.
2. Save modified assets and (if relevant) level state.
3. Confirm spawned actors/labels with `query_actors`.
4. Report limitations clearly if a known engine/tooling issue is hit.

---

## 11) Further Reading (Module READMEs)

If you need more detail beyond this usage guide, these module READMEs exist:

- Core concepts and reflection behavior:
  - `AgentBridgeCore/README.md`
- World contexts, actor ops, and world partition:
  - `AgentBridgeRuntime/README.md`
- Command execution and JSON conversion path:
  - `AgentBridgeScripting/README.md`
- gRPC/HTTP server behavior and proto flow:
  - `AgentBridgeServer/README.md`
- MCP setup, profiles, and tool modules:
  - `mcp/README.md`
- Optional offline Blueprint/DataAsset workflows:
  - `bp_toolkit/README.md`

Also useful:
- Top-level overview and onboarding: `README.md`
- Built-in help text reference: `docs/reference/HELP_REFERENCE.md`
