# AgentBridge Wishlist Implementation Plan

> Comprehensive feature expansion for AI agent capabilities.
> Created: December 31, 2025

---

## Overview

This document outlines the implementation plan for expanding AgentBridge capabilities based on the user's wishlist. The goal is to enable AI agents to perform comprehensive level-building tasks including asset creation, PCG biome workflows, landscape manipulation, and more.

---

## Priority Levels

| Priority | Description |
|----------|-------------|
| P0 | Foundation - Required for other features |
| P1 | High Value - Core level-building capabilities |
| P2 | Medium Value - Enhanced workflows |
| P3 | Research/Experimental - May require significant effort |

---

## Feature Categories

### Category 1: Tempo Duplicate Audit (P0)
**Goal:** Identify and document overlapping tools between AgentBridge and Tempo.

| AgentBridge Tool | Tempo Equivalent | Keep/Remove | Rationale |
|------------------|------------------|-------------|-----------|
| `spawn_actor` | `tempo_spawn_actor` | **Keep Both** | AB has label/folder, Tempo has relative_to |
| `delete_actor` | `tempo_destroy_actor` | **Keep Both** | Identical, but AB naming is clearer |
| `set_actor_transform` | `tempo_set_actor_transform` | **Keep Both** | AB has sweep/teleport, Tempo has relative_to |
| `query_actors` | `tempo_get_all_actors` | **Keep Both** | AB has filtering/patterns, Tempo is simpler |
| `get_property` | `tempo_get_actor_properties` | **Keep Both** | AB uses paths, Tempo returns all |
| `set_property` | `tempo_set_*_property` | **Keep Both** | AB is generic, Tempo is typed (easier) |

**Recommendation:** Keep both systems. AgentBridge provides discovery/flexibility, Tempo provides typed convenience.

---

### Category 2: UAsset Creation & Saving (P0)

**Why P0:** Required for persisting agent-created content.

#### Implementation Plan

1. **Add new RPC to AgentBridge.proto:**
```protobuf
message CreateAssetRequest {
  string asset_class = 1;      // "DataAsset", "MaterialInstance", etc.
  string package_path = 2;     // "/Game/AgentCreated/MyAsset"
  string asset_name = 3;       // "MyAsset"
  repeated PropertyKeyValue initial_properties = 4;
}

message CreateAssetResponse {
  bool success = 1;
  string asset_path = 2;       // Full path to created asset
  string error = 3;
}

message SaveAssetRequest {
  string asset_path = 1;       // Existing asset path
}

message SaveActorToAssetRequest {
  string actor_id = 1;         // Actor to save as Blueprint
  string package_path = 2;
  string asset_name = 3;
}

message DuplicateAssetRequest {
  string source_path = 1;
  string dest_package = 2;
  string dest_name = 3;
}
```

2. **C++ Implementation (CommandExecutor.cpp):**
   - Use `IAssetTools::CreateAsset()` for new assets
   - Use `UPackage::SavePackage()` for saving
   - Constrain paths to `/Game/` (project content only)
   - Use `FKismetEditorUtilities::CreateBlueprintFromActor()` for actor→BP

3. **MCP Tools:**
   - `create_asset(class, path, name, properties)`
   - `save_asset(path)`
   - `save_actor_as_blueprint(actor_id, path, name)`
   - `duplicate_asset(source, dest_path, dest_name)`

**Testing:**
- [ ] Create a DataAsset
- [ ] Set properties on it
- [ ] Save it
- [ ] Verify file exists
- [ ] Create Blueprint from spawned actor

---

### Category 3: Component System (P1)

#### 3a. Scene Component Transforms

**Current State:** Tempo has `SetComponentTransform` RPC, but MCP tool not exposed.

**Implementation:**
1. Add MCP wrapper for `tempo_set_component_transform`
2. Add `get_component_transform` (read side)

#### 3b. Component Attachment

**Implementation:**
```protobuf
message AttachComponentRequest {
  string actor_id = 1;
  string component = 2;
  string parent_component = 3;  // Empty = attach to root
  string socket = 4;            // Optional socket name
  int32 attach_rules = 5;       // EAttachmentRule flags
}

message AttachActorRequest {
  string child_actor = 1;
  string parent_actor = 2;
  string parent_component = 3;  // Optional
  string socket = 4;            // Optional
}
```

**MCP Tools:**
- `attach_component(actor, component, parent_component, socket)`
- `attach_actor(child, parent, parent_component, socket)`
- `detach_component(actor, component)`
- `detach_actor(actor)`

---

### Category 4: INI/Config Automation (P2)

**Research Notes:**
- Unreal uses `GConfig->GetXXX()` and `GConfig->SetXXX()` for INI access
- `UPROPERTY(Config)` enables automatic INI serialization
- `DefaultEngine.ini`, `DefaultGame.ini` are primary config files
- `FConfigCacheIni::LoadGlobalIniFile()` loads config files

**Implementation:**
```protobuf
message GetConfigValueRequest {
  string ini_file = 1;         // "Engine", "Game", "Editor", or path
  string section = 2;          // e.g., "/Script/Engine.PhysicsSettings"
  string key = 3;
}

message SetConfigValueRequest {
  string ini_file = 1;
  string section = 2;
  string key = 3;
  string value = 4;
  bool save = 5;               // Write to disk
}

message ListConfigSectionsRequest {
  string ini_file = 1;
}
```

**MCP Tools:**
- `get_config_value(ini, section, key)`
- `set_config_value(ini, section, key, value, save)`
- `list_config_sections(ini)`

---

### Category 5: Constrained File Manipulation (P1)

**Constraints:**
- Limit to project directory only
- Use UE's file system APIs (FFileHelper, IPlatformFile)
- Whitelist operations: read, write, copy, delete, list
- Blacklist patterns: *.exe, *.dll, *.pdb, Saved/Logs/*, etc.

**Implementation:**
```protobuf
message ReadFileRequest {
  string path = 1;             // Relative to project root
  bool as_base64 = 2;          // For binary files
}

message WriteFileRequest {
  string path = 1;
  string content = 2;          // Text or base64
  bool is_base64 = 3;
  bool create_directories = 4;
}

message ListDirectoryRequest {
  string path = 1;
  string pattern = 2;          // Glob pattern
  bool recursive = 3;
}

message CopyFileRequest {
  string source = 1;
  string dest = 2;
}
```

**Validation:**
```cpp
bool IsPathAllowed(const FString& RelativePath)
{
    // Must be within project
    // No .. traversal
    // Not in Binaries/, Saved/Crashes/, etc.
    // Not *.exe, *.dll, *.pdb
    return true;
}
```

---

### Category 6: PCG Biome Workflow (P1)

**Reference:** `D:/EL_UE/UE_5.6/Engine/Plugins/Experimental/PCGBiomeCore/`

**Workflow Steps:**
1. Get landscape bounds (trace or query)
2. Spawn `BP_PCGBiomeCore` or `BP_PCGBiomeCore_Runtime`
3. Set collision box extents to cover landscape
4. Spawn `BP_PCGBiomeTexture`
5. Set its box extents
6. Create/assign BiomeDefinition DataAsset
7. Create/assign BiomeAsset DataAsset
8. Trigger regeneration

**Implementation:**

```protobuf
message GetLandscapeBoundsRequest {
  string landscape_actor = 1;  // Optional - auto-detect if empty
}

message GetLandscapeBoundsResponse {
  BoundingBox bounds = 1;
  string landscape_name = 2;
}

message SetupPCGBiomeRequest {
  BoundingBox bounds = 1;
  string biome_definition_path = 2;  // Existing asset
  string biome_asset_path = 3;       // Existing asset
  bool use_runtime = 4;              // Use runtime version
}
```

**High-Level MCP Tool:**
```python
def setup_pcg_biome(landscape=None, biome_definition=None, biome_asset=None):
    """
    One-shot PCG biome setup.

    1. Get landscape bounds (or use provided)
    2. Spawn BP_PCGBiomeCore
    3. Configure collision
    4. Spawn BP_PCGBiomeTexture
    5. Assign biome assets
    6. Regenerate
    """
```

---

### Category 7: Landscape & Heightmap (P2)

**Reference:** `LandscapeImportHelper.cpp`, `LandscapeFileFormatPng.cpp`

**Implementation:**
```protobuf
message CreateLandscapeRequest {
  int32 size = 1;              // 127, 255, 511, 1023, etc.
  int32 sections_per_component = 2;
  int32 components_x = 3;
  int32 components_y = 4;
  string material_path = 5;
  TempoScripting.Vector location = 6;
}

message ImportHeightmapRequest {
  string landscape_actor = 1;
  string heightmap_path = 2;   // PNG or RAW file in project
  int32 format = 3;            // PNG16, RAW16, RAW8
}

message ExportHeightmapRequest {
  string landscape_actor = 1;
  string output_path = 2;
  int32 format = 3;
}

message SculptLandscapeRequest {
  string landscape_actor = 1;
  repeated LandscapeBrushStroke strokes = 2;
}

message LandscapeBrushStroke {
  TempoScripting.Vector location = 1;
  float radius = 2;
  float strength = 3;
  int32 brush_type = 4;        // Raise, Lower, Smooth, Flatten
}
```

---

### Category 8: Image Capture & Ingestion (P1)

**Current State:** Tempo has camera/sensor services.

**New Capabilities:**
```protobuf
message CaptureViewportRequest {
  int32 width = 1;
  int32 height = 2;
  bool include_ui = 3;
  string output_path = 4;      // Save to file
}

message CaptureViewportResponse {
  bytes image_data = 1;        // PNG or raw bytes
  string saved_path = 2;
}

message CaptureSceneCaptureRequest {
  string scene_capture_actor = 1;
  string output_path = 2;
}

message IngestImageRequest {
  string image_path = 1;       // Path to image file
  string purpose = 2;          // "thumbnail", "reference", "texture"
}

message IngestImageResponse {
  string description = 1;      // AI description of image
  // For texture: could import as texture asset
}
```

---

### Category 9: Sound Capture (P3)

**Research Required:**
- `FAudioDevice` for in-game audio
- Audio capture requires platform-specific APIs
- May need Submix capture for spatial audio

**Tentative:**
```protobuf
message StartAudioRecordRequest {
  float duration = 1;          // Max seconds
  string submix = 2;           // Optional submix to capture
}

message StopAudioRecordRequest {}

message GetAudioRecordingResponse {
  bytes wav_data = 1;
  float duration = 2;
}
```

---

### Category 10: Asset Thumbnails (P2)

**Reference:** ThumbnailRendering/

**Implementation:**
```protobuf
message GetAssetThumbnailRequest {
  string asset_path = 1;
  int32 width = 2;
  int32 height = 3;
}

message GetAssetThumbnailResponse {
  bytes thumbnail_data = 1;    // PNG bytes
  string asset_type = 2;
}
```

Uses `UThumbnailManager` and various `UThumbnailRenderer` subclasses.

---

### Category 11: Graph Editing (P3 - Research)

**Complexity:** Very high - requires understanding of:
- `UEdGraph` base class
- `UK2Node` for Blueprint graphs
- `UMaterialExpression` for material graphs
- `UPCGGraph` for PCG graphs
- `UAnimGraphNode` for animation graphs

**Possible Approach:**
1. Start with simple read-only introspection
2. Add node creation/deletion
3. Add connection management

**NOT recommended for initial implementation.** Consider as stretch goal.

---

### Category 12: Widget Interaction (P3 - Research)

**Complexity:** UMG widgets are highly dynamic.

**Possible Limited Scope:**
- List UI widgets in viewport
- Get/set widget properties
- Simulate button clicks

**NOT recommended for initial implementation.**

---

## Implementation Order

### Phase A: Foundation (Do First)
1. ✅ Tempo Duplicate Audit (document only)
2. UAsset Creation & Saving
3. Component Attachment
4. Scene Component Transforms

### Phase B: Core Features
5. Constrained File Manipulation
6. PCG Biome Workflow (high-level tool)
7. Image Capture & Ingestion

### Phase C: Enhancement
8. Landscape & Heightmap
9. INI/Config Automation
10. Asset Thumbnails

### Phase D: Research
11. Graph Editing (read-only first)
12. Widget Interaction
13. Sound Capture

---

## Testing Checklist

### To Test When Returning:
- [ ] `get_actor(include_properties=True)` returns data
- [ ] `get_class_schema("SceneCaptureComponent2D")` returns schema
- [ ] `call_static_function("KismetSystemLibrary", "PrintString", {...})`

### New Features Testing:
- [ ] Create DataAsset, set properties, save
- [ ] Attach component to another component
- [ ] Create landscape from heightmap
- [ ] PCG biome full workflow
- [ ] Capture viewport to file
- [ ] Read/write files in project directory

---

## Debugging Strategy Documentation

When stuck, record:
1. **Symptom** - What went wrong
2. **Investigation** - What was checked
3. **Root Cause** - Why it happened
4. **Solution** - How it was fixed
5. **Tool Opportunity** - Could this become an agent tool?

Example:
```
Symptom: get_actor returns empty properties
Investigation: Traced through gRPC → C++ → found Python layer ignoring data
Root Cause: Python only extracted actor_info, not properties/components
Solution: Added _property_value_to_dict helper, fixed extraction
Tool Opportunity: No - this was a bug fix
```

---

## Notes for Plugin Release

When releasing AgentBridge as standalone:
1. Remove TempoScripting dependency (or make optional)
2. Provide standalone gRPC server (not via Tempo)
3. Bundle Python MCP server
4. Document minimal UE version requirements
5. Test in fresh project without Tempo

---

*Document Version: 1.0*
*Author: Claude (overnight session)*
