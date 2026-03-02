# Plan: Landscape Operations for AgentBridge

## Overview

**Current state:** AgentBridge can query existing landscapes (`query_landscape`,
`get_landscape_bounds`) but cannot create, modify terrain, or paint layers.

**Goal:** AI agents can programmatically create landscapes, import heightmap files to shape
terrain, assign materials, and auto-paint layers by elevation - enabling full "build me a level"
workflows that include terrain.

**User-facing behavior:** 4 new MCP tools (`create_landscape`, `import_heightmap`,
`set_landscape_material`, `paint_landscape_layers`) that work like other AgentBridge tools -
simple parameters, smart defaults, meaningful errors.

## Scope

### Included in v1
- Create a new landscape actor with configurable dimensions and auto-sizing
- Import heightmap from file (PNG/RAW/R16) already in the project directory
- Set landscape material by asset path
- Elevation-based auto-painting of landscape layers (computed from heightmap Z values)
- World Partition grid size configuration (streaming proxy creation)
- Editor-only operations with proper `WITH_EDITOR` guards

### Deferred to Future
- Brush-based sculpting and painting
- Procedural heightmap generation (noise, erosion)
- Landscape splines (roads, paths)
- Foliage painting
- Heightmap export
- Manual weight painting (per-region, not elevation-based)
- Runtime landscape modification

### Explicitly Excluded
- Modifying Unreal Engine or Tempo plugin source
- Non-editor landscape operations (runtime terrain modification)
- Custom landscape importer plugins (UE's built-in PNG/RAW formats are sufficient)
- Landscape LOD or rendering configuration (use `set_property` for those)

## Design Decisions

| Decision | Choice | Why |
|----------|--------|-----|
| Number of RPCs | 4 separate | Agents iterate - re-import heightmap without recreating |
| Heightmap transfer | File path (not inline) | Can be 16MB+. Agent uses `write_project_file` first |
| Layer config | `{name, min_elev, max_elev, falloff}` array | Simple, declarative |
| Material | Dedicated RPC | Set `LandscapeMaterial` property + `PostEditChangeProperty` (R5-IM-2: `EditorSetLandscapeMaterial` not exported) |
| WP grid | Parameter on CreateLandscape | Default 0 = don't configure |
| Auto-sizing | `FLandscapeImportHelper::ChooseBestComponentSizeForImport` | Let UE compute |
| Logic location | `CommandExecutor.cpp` | Header conflict constraints |

## Architecture

### Data Flow

```
Agent calls create_landscape(size_x=1009, heightmap_file="terrain.png")
    |
    v
MCP tool (agentbridge.py) -> gRPC CreateLandscapeRequest
    |
    v
AgentBridgeServiceSubsystem.cpp (thin handler: proto -> FCreateLandscapeCommand)
    |
    v
CommandExecutor.cpp (all logic here, WITH_EDITOR guard)
    |-- ILandscapeEditorModule -> load heightmap file
    |-- FLandscapeImportHelper -> auto-size components
    |-- World->SpawnActor<ALandscape>()
    |-- ALandscape::Import() -> create terrain geometry
    |-- ULandscapeSubsystem::ChangeGridSize() -> WP streaming
    v
Response -> proto -> MCP tool result
```

### Affected Modules

| Module | What Changes | Why |
|--------|-------------|-----|
| **AgentBridgeScripting** | Build.cs deps + 4 command structs + 4 Execute() implementations | All business logic lives here (header conflict isolation) |
| **AgentBridgeServer** | 4 proto RPCs + 4 thin gRPC handlers + service registration | Network layer (proto <-> command struct conversion) |
| **mcp (Python)** | 4 client methods + 4 tool defs + 4 handlers + help topic | Agent-facing API |
| AgentBridgeRuntime | No changes | Already has `FWorldPartitionOps::GetMainLandscape()` we reuse |
| AgentBridgeCore | No changes | No landscape-specific reflection needed |

### Existing Code to Reuse

| Function | File | Purpose |
|----------|------|---------|
| `FWorldPartitionOps::GetMainLandscape()` | `AgentBridgeRuntime/.../WorldPartitionOps.cpp:423` | Find primary landscape actor |
| `FActorOperations::FindActorByName()` | `AgentBridgeRuntime/.../ActorOperations.cpp` | Resolve landscape_id to actor |
| `FWorldContextManager::GetTargetWorld()` | `AgentBridgeRuntime/.../WorldContextManager.cpp` | Get current editor world |
| `StartTiming()` / `EndTiming()` | `CommandExecutor.cpp` | Performance measurement pattern |
| `safe_call()` | `mcp/services/base.py` | gRPC error wrapping in Python |

## Verified UE 5.6 API (from engine source)

### Module Boundaries

| Header | Module | Notes |
|--------|--------|-------|
| `LandscapeEdit.h` | **Runtime** Landscape | FHeightmapAccessor, FAlphamapAccessor |
| `LandscapeSubsystem.h` | **Runtime** Landscape | ULandscapeSubsystem::ChangeGridSize |
| `LandscapeDataAccess.h` | **Runtime** Landscape | Height conversion constants |
| `LandscapeImportHelper.h` | **Editor** LandscapeEditor | ChooseBestComponentSizeForImport |
| `LandscapeEditorModule.h` | **Editor** LandscapeEditor | ILandscapeEditorModule |
| `LandscapeFileFormatInterface.h` | **Editor** LandscapeEditor | Heightmap format handlers |

### Key Signatures (verified against UE 5.6 source)

```cpp
// ALandscapeProxy::Import (non-deprecated UE 5.5+ overload)
// File: Engine/Source/Runtime/Landscape/Classes/LandscapeProxy.h:1398
void Import(const FGuid& InGuid, int32 InMinX, int32 InMinY, int32 InMaxX, int32 InMaxY,
    int32 InNumSubsections, int32 InSubsectionSizeQuads,
    const TMap<FGuid, TArray<uint16>>& InImportHeightData,
    const TCHAR* const InHeightmapFileName,
    const TMap<FGuid, TArray<FLandscapeImportLayerInfo>>& InImportMaterialLayerInfos,
    ELandscapeImportAlphamapType InImportMaterialLayerType,
    const TArrayView<const struct FLandscapeLayer>& InImportLayers);

// FHeightmapAccessor - ONE template param
// File: Engine/Source/Runtime/Landscape/Public/LandscapeEdit.h:396
template<bool bInUseInterp> struct FHeightmapAccessor;
// Usage: FHeightmapAccessor<false> accessor(LandscapeInfo);

// FAlphamapAccessor - TWO template params (NOT one!)
// File: Engine/Source/Runtime/Landscape/Public/LandscapeEdit.h:543
template<bool bInUseInterp, bool bInUseTotalNormalize> struct FAlphamapAccessor;
// Usage: FAlphamapAccessor<false, false> accessor(LandscapeInfo, LayerInfo);
// NOTE: bInUseTotalNormalize=false for standard painting. true forces all weights to sum to 255.
// NOTE: SetData() has NO default for PaintingRestriction - must pass ELandscapeLayerPaintingRestriction::None
// NOTE: FLandscapeEditDataInterface is stack-allocated (not heap) - flush happens on scope exit

// Height conversion constants
// File: Engine/Source/Runtime/Landscape/Public/LandscapeDataAccess.h:14,27
// LANDSCAPE_INV_ZSCALE = 128.0f, MidValue = 32768.0f
// local_z = (uint16_val - 32768.0) / 128.0
// world_z = local_z * landscape_scale_z + landscape_location_z

// FLandscapeImportHelper (Editor-only)
// File: Engine/Source/Editor/LandscapeEditor/Public/LandscapeImportHelper.h:138
static void ChooseBestComponentSizeForImport(int32 Width, int32 Height,
    int32& InOutQuadsPerSection, int32& InOutSectionsPerComponent,
    FIntPoint& OutComponentCount);

// ULandscapeSubsystem (Runtime)
// File: Engine/Source/Runtime/Landscape/Public/LandscapeSubsystem.h:172
void ChangeGridSize(ULandscapeInfo* LandscapeInfo, uint32 NewGridSizeInComponents);

// ILandscapeEditorModule (Editor-only)
// File: Engine/Source/Editor/LandscapeEditor/Public/LandscapeEditorModule.h:39
const ILandscapeHeightmapFileFormat* GetHeightmapFormatByExtension(const TCHAR* Extension) const;

// ILandscapeHeightmapFileFormat::Import (Editor-only)
// File: Engine/Source/Editor/LandscapeEditor/Public/LandscapeFileFormatInterface.h:123
FLandscapeHeightmapImportData Import(const TCHAR* Filename,
    FLandscapeFileResolution ExpectedResolution) const;
// Returns: { ResultCode, ErrorMessage, Data (TArray<uint16>) }

// EditorSetLandscapeMaterial (is BlueprintSetter for LandscapeMaterial property)
// File: Engine/Source/Runtime/Landscape/Classes/LandscapeProxy.h:1005
void EditorSetLandscapeMaterial(UMaterialInterface* NewLandscapeMaterial);
```

## Files to Modify

| File | Changes |
|------|---------|
| `AgentBridgeScripting/.../AgentBridgeScripting.Build.cs` | +`Landscape` (private), +`LandscapeEditor` (editor private) |
| `AgentBridgeServer/.../Public/AgentBridge.proto` | +4 RPCs, +~10 messages |
| `AgentBridgeScripting/.../Public/AgentCommands.h` | +4 commands, +3 responses, +1 shared struct, +enum entries |
| `AgentBridgeScripting/.../Public/CommandExecutor.h` | +4 Execute() declarations |
| `AgentBridgeScripting/.../Private/CommandExecutor.cpp` | Business logic (~300-400 lines) |
| `AgentBridgeServer/.../Public/AgentBridgeServiceSubsystem.h` | Forward decls + 4 handler decls |
| `AgentBridgeServer/.../Private/AgentBridgeServiceSubsystem.cpp` | 4 thin handlers + registration |
| `mcp/services/agentbridge.py` | 4 client methods + 4 tools + 4 handlers + help |
| `mcp/services/__init__.py` | Create new `landscape` module, add to `standard` and `editor` profiles |

## Proto Messages

```protobuf
// Shared type
message LandscapeLayerConfig {
  string layer_name = 1;
  string layer_info_path = 2;       // empty = auto-create transient
  float min_elevation = 3;          // world units
  float max_elevation = 4;
  float falloff = 5;                // blend softness (world units, 0=sharp)
}

// RPC 1: CreateLandscape
message CreateLandscapeRequest {
  int32 size_x = 1;                 // vertices (e.g. 505, 1009, 2017)
  int32 size_y = 2;                 // 0 = same as size_x
  TempoScripting.Vector location = 3;
  Scale scale = 4;                  // default 100,100,100
  string heightmap_file = 5;        // optional, relative to project
  string material_path = 6;         // optional
  int32 quads_per_section = 7;      // 0=auto (valid: 7,15,31,63,127,255)
  int32 sections_per_component = 8; // 0=auto (valid: 1 or 2)
  int32 wp_grid_size = 9;           // 0 = no WP grid
  string label = 10;
}
message CreateLandscapeResponse {
  bool success = 1;
  string error_message = 2;
  string actor_name = 3;
  string actor_label = 4;
  int32 actual_size_x = 5;
  int32 actual_size_y = 6;
  int32 num_components = 7;
}

// RPC 2: ImportHeightmap
message ImportHeightmapRequest {
  string landscape_id = 1;          // empty = find main landscape
  string heightmap_file = 2;        // relative to project
}
message ImportHeightmapResponse {
  bool success = 1;
  string error_message = 2;
  int32 resolution_x = 3;
  int32 resolution_y = 4;
}

// RPC 3: SetLandscapeMaterial
message SetLandscapeMaterialRequest {
  string landscape_id = 1;
  string material_path = 2;
}
// R5-PS-12: Uses TempoScripting.Empty as response (matching pattern for
// FAgentResponseBase RPCs like SetPropertyPath, DeleteActor, etc.)
// No SetLandscapeMaterialResponse message needed.

// RPC 4: PaintLandscapeLayers
message PaintLandscapeLayersRequest {
  string landscape_id = 1;
  repeated LandscapeLayerConfig layers = 2;
}
message PaintLandscapeLayersResponse {
  bool success = 1;
  string error_message = 2;
  int32 layers_painted = 3;
  repeated string layer_names = 4;
}

// R5-PS-13: Explicit RPC declarations (add to service block after World Partition RPCs)
//--- Landscape Operations ---
rpc CreateLandscape(CreateLandscapeRequest) returns (CreateLandscapeResponse);
rpc ImportHeightmap(ImportHeightmapRequest) returns (ImportHeightmapResponse);
rpc SetLandscapeMaterial(SetLandscapeMaterialRequest) returns (TempoScripting.Empty);  // R5-PS-12
rpc PaintLandscapeLayers(PaintLandscapeLayersRequest) returns (PaintLandscapeLayersResponse);
```

## C++ Structs (AgentCommands.h)

### Shared struct (insert before landscape command structs)

```cpp
/** Configuration for a landscape layer to paint by elevation. */
struct AGENTBRIDGESCRIPTING_API FLandscapeLayerConfig
{
    FString LayerName;
    FString LayerInfoPath;       // empty = auto-create transient
    float MinElevation = 0.0f;   // world units
    float MaxElevation = 1000.0f;
    float Falloff = 0.0f;        // blend softness (world units, 0=sharp)
};
```

### Enum entries (insert after PCG Commands, before Asset Commands ~line 69)

```cpp
// Landscape Commands
CreateLandscape,
ImportHeightmap,
SetLandscapeMaterial,
PaintLandscapeLayers,
```

### Command structs (insert after FSetPCGParameterCommand)

```cpp
struct AGENTBRIDGESCRIPTING_API FCreateLandscapeCommand : FAgentCommandBase
{
    FCreateLandscapeCommand() { Type = EAgentCommandType::CreateLandscape; }
    int32 SizeX = 505;
    int32 SizeY = 0;                                    // 0 = same as SizeX
    FVector Location = FVector::ZeroVector;
    FVector Scale = FVector(100.0f, 100.0f, 100.0f);
    FString HeightmapFile;                               // optional, relative to project
    FString MaterialPath;                                // optional
    int32 QuadsPerSection = 0;                           // 0=auto
    int32 SectionsPerComponent = 0;                      // 0=auto
    int32 WPGridSize = 0;                                // 0=no WP grid
    FString Label;
};

struct AGENTBRIDGESCRIPTING_API FImportHeightmapCommand : FAgentCommandBase
{
    FImportHeightmapCommand() { Type = EAgentCommandType::ImportHeightmap; }
    FString LandscapeId;                                 // empty = find main landscape
    FString HeightmapFile;
};

struct AGENTBRIDGESCRIPTING_API FSetLandscapeMaterialCommand : FAgentCommandBase
{
    FSetLandscapeMaterialCommand() { Type = EAgentCommandType::SetLandscapeMaterial; }
    FString LandscapeId;
    FString MaterialPath;
};

struct AGENTBRIDGESCRIPTING_API FPaintLandscapeLayersCommand : FAgentCommandBase
{
    FPaintLandscapeLayersCommand() { Type = EAgentCommandType::PaintLandscapeLayers; }
    FString LandscapeId;
    TArray<FLandscapeLayerConfig> Layers;
};
```

### Response structs (insert after FRegeneratePCGResponse)

NOTE: `bSuccess`, `ErrorMessage`, `ExecutionTimeMs` are inherited from `FAgentResponseBase`.
Do NOT duplicate them. `SetLandscapeMaterial` uses `FAgentResponseBase` directly (no custom C++ response struct).
NOTE: The proto RPC returns `TempoScripting.Empty` (R5-PS-12), matching the pattern for other
FAgentResponseBase operations (DeleteActor, SetPropertyPath, etc.). The gRPC handler follows
the same pattern as those existing handlers.

```cpp
struct AGENTBRIDGESCRIPTING_API FCreateLandscapeResponse : FAgentResponseBase
{
    FString ActorName;
    FString ActorLabel;
    int32 ActualSizeX = 0;
    int32 ActualSizeY = 0;
    int32 NumComponents = 0;
};

struct AGENTBRIDGESCRIPTING_API FImportHeightmapResponse : FAgentResponseBase
{
    int32 ResolutionX = 0;
    int32 ResolutionY = 0;
};

// FSetLandscapeMaterialCommand uses FAgentResponseBase directly - no custom response needed

struct AGENTBRIDGESCRIPTING_API FPaintLandscapeLayersResponse : FAgentResponseBase
{
    int32 LayersPainted = 0;
    TArray<FString> LayerNames;
};
```

### Execute() declarations (CommandExecutor.h, after PCG section ~line 163)

```cpp
// Landscape Commands
static void Execute(const FCreateLandscapeCommand& Command, FCreateLandscapeResponse& Response);
static void Execute(const FImportHeightmapCommand& Command, FImportHeightmapResponse& Response);
static void Execute(const FSetLandscapeMaterialCommand& Command, FAgentResponseBase& Response);
static void Execute(const FPaintLandscapeLayersCommand& Command, FPaintLandscapeLayersResponse& Response);
```

---

## Business Logic Approach (CommandExecutor.cpp)

### Every Execute() Function Must Follow This Pattern

**C4 fix: FAgentResponseBase defaults `bSuccess = false`.** Every early-return path inherits
this default, so we only set `bSuccess = true` at the very end on success. Every error path
must set `Response.ErrorMessage` and `Response.ExecutionTimeMs` before returning.

**Error helper macro** (R4-6 fix: REQUIRED, not optional - used in all 4 Execute() functions):
```cpp
#define LANDSCAPE_ERROR_RETURN(Msg) do { \
    Response.ErrorMessage = Msg; \
    Response.ExecutionTimeMs = EndTiming(StartTime); \
    return; \
} while(0)
```

```cpp
void FCommandExecutor::Execute(const FCreateLandscapeCommand& Cmd, FCreateLandscapeResponse& Response)
{
    // R4-1 fix: StartTime and CommandId OUTSIDE #if so both branches can set ExecutionTimeMs.
    // Existing pattern: FCaptureViewportCommand in CommandExecutor.cpp does the same.
    double StartTime = StartTiming();
    Response.CommandId = Cmd.CommandId;

#if WITH_EDITOR
    // NOTE: Response.bSuccess defaults to false. Only set true at end on success.

    UWorld* World = FWorldContextManager::Get().GetTargetWorld();
    if (!World) { LANDSCAPE_ERROR_RETURN(TEXT("No target world")); }

    // ... implementation (every error path uses LANDSCAPE_ERROR_RETURN) ...

    Response.bSuccess = true;
#else
    Response.ErrorMessage = TEXT("Landscape operations require editor build");
#endif

    Response.ExecutionTimeMs = EndTiming(StartTime);
}
```

### Common: ResolveLandscape helper

```cpp
// Static helper in anonymous namespace at top of landscape section
static ALandscapeProxy* ResolveLandscape(const FString& Id, UWorld* World, FString& Error)
{
    ALandscapeProxy* Landscape = nullptr;
    if (Id.IsEmpty())
    {
        Landscape = FWorldPartitionOps::GetMainLandscape(World);
        if (!Landscape) { Error = TEXT("No landscape found in world"); }
    }
    else
    {
        AActor* Actor = FActorOperations::FindActorByName(Id, World);
        if (!Actor) { Error = FString::Printf(TEXT("Actor '%s' not found"), *Id); }
        else
        {
            Landscape = Cast<ALandscapeProxy>(Actor);
            if (!Landscape) { Error = FString::Printf(TEXT("Actor '%s' is not a landscape"), *Id); }
        }
    }
    return Landscape;
}
```

### Common: GetLandscapeInfo helper

```cpp
// Get ULandscapeInfo from proxy. Checks for edit layers (which block accessor writes).
static ULandscapeInfo* GetLandscapeInfoFromProxy(ALandscapeProxy* Proxy, FString& Error)
{
    ALandscape* MainLandscape = Proxy->GetLandscapeActor();
    if (!MainLandscape) { Error = TEXT("Could not get main landscape actor from proxy"); return nullptr; }

    ULandscapeInfo* Info = MainLandscape->GetLandscapeInfo();
    if (!Info) { Error = TEXT("Landscape has no ULandscapeInfo"); return nullptr; }

    if (MainLandscape->HasLayersContent())
    {
        Error = TEXT("Landscape uses Edit Layers - disable Edit Layers first");
        return nullptr;
    }
    return Info;
}
```

### Common: ResolveHeightmapPath helper (M4 fix: absolute vs relative paths)

```cpp
// Resolve heightmap file path. Supports both relative (to project) and absolute paths.
// Returns empty string on failure (sets Error).
static FString ResolveHeightmapPath(const FString& InputPath, FString& Error)
{
    if (InputPath.IsEmpty()) { Error = TEXT("Heightmap file path is empty"); return {}; }

    FString FullPath;
    if (FPaths::IsRelative(InputPath))
    {
        FullPath = FPaths::Combine(FPaths::ProjectDir(), InputPath);
    }
    else
    {
        FullPath = InputPath;  // Absolute path - use as-is
    }

    // H3 fix: Explicit file existence check before expensive format handler
    if (!IFileManager::Get().FileExists(*FullPath))
    {
        Error = FString::Printf(TEXT("Heightmap file not found: %s"), *FullPath);
        return {};
    }
    return FullPath;
}
```

### Common: LoadHeightmapFile helper

```cpp
// Load heightmap from file via ILandscapeEditorModule format handlers.
// Returns empty array on failure (sets Error).
static TArray<uint16> LoadHeightmapFile(const FString& FilePath, int32 ExpectedWidth, int32 ExpectedHeight, FString& Error)
{
    // R5-IM-1: bIncludeDot=true required. Engine extensions store ".png" not "png".
    // All engine callers use bIncludeDot=true (LandscapeImportHelper.cpp:119, etc.)
    FString Extension = FPaths::GetExtension(FilePath, true);
    if (Extension.IsEmpty()) { Error = TEXT("Heightmap file must have extension (.png, .raw, .r16)"); return {}; }

    ILandscapeEditorModule& LandscapeEditorModule =
        FModuleManager::LoadModuleChecked<ILandscapeEditorModule>("LandscapeEditor");

    const ILandscapeHeightmapFileFormat* Format =
        LandscapeEditorModule.GetHeightmapFormatByExtension(*Extension);
    if (!Format) { Error = FString::Printf(TEXT("Unsupported heightmap format: %s"), *Extension); return {}; }

    FLandscapeFileResolution ExpectedRes = { (uint32)ExpectedWidth, (uint32)ExpectedHeight };
    FLandscapeHeightmapImportData ImportData = Format->Import(*FilePath, ExpectedRes);

    if (ImportData.ResultCode == ELandscapeImportResult::Error)
    {
        Error = ImportData.ErrorMessage.ToString();
        return {};
    }
    return MoveTemp(ImportData.Data);
}
```

---

### CreateLandscape (detailed steps)

**Maximum allowed size: 8129** (M3 fix: prevents OOM from 65535x65535 = 8GB allocation)

**Size validation:** Any size from 2 to 8129 is accepted. `ChooseBestComponentSizeForImport()`
finds the best component configuration for arbitrary dimensions. Common "clean" sizes
(505, 1009, 2017, 4033, 8129) divide evenly with standard component configs, but many
other sizes work fine (128, 256, 512, etc.). (R5-CL-4 fix: removed hardcoded valid list)

```
1. Validate size:
   if (SizeY == 0) SizeY = SizeX;

   // M3 fix: Max size guard against OOM
   const int32 MAX_LANDSCAPE_SIZE = 8129;
   if (SizeX < 2 || SizeY < 2) {
     LANDSCAPE_ERROR_RETURN(TEXT("Landscape size must be at least 2x2"));
   }
   if (SizeX > MAX_LANDSCAPE_SIZE || SizeY > MAX_LANDSCAPE_SIZE) {
     LANDSCAPE_ERROR_RETURN(FString::Printf(TEXT("Size %dx%d exceeds maximum %d"),
       SizeX, SizeY, MAX_LANDSCAPE_SIZE));
   }
   // R5-CL-4: No hardcoded valid-size list. ChooseBestComponentSizeForImport handles
   // finding the best component configuration for any reasonable dimensions.

2. H2 fix: Check for existing landscape (prevent corruption from duplicates):
   ALandscapeProxy* ExistingLandscape = FWorldPartitionOps::GetMainLandscape(World);
   if (ExistingLandscape) {
     LANDSCAPE_ERROR_RETURN(TEXT("A landscape already exists. Delete it first with "
       "delete_actor() or use import_heightmap to modify it."));
   }

3. Load heightmap data (TArray<uint16>):
   NOTE: If heightmap_file is provided, its resolution MUST match size_x x size_y exactly.
   If heightmap_file provided:
     a. FString FullPath = ResolveHeightmapPath(Cmd.HeightmapFile, Error);
        if (FullPath.IsEmpty()) { LANDSCAPE_ERROR_RETURN(Error); }  // R4-11 fix: use macro, not bare return
     b. HeightData = LoadHeightmapFile(FullPath, SizeX, SizeY, Error);
     c. if (HeightData.Num() == 0) { LANDSCAPE_ERROR_RETURN(Error); }  // R4-11 fix
   Else (flat landscape):
     a. TArray<uint16> HeightData;
     b. HeightData.SetNum(SizeX * SizeY);
     c. // C1 fix: Memset fills BYTES, so 0x80 gives 0x8080 per uint16 (32896, NOT 32768!)
        // Must use loop to set each uint16 to exactly 32768 (midpoint = Z=0)
        const uint16 MidHeight = 32768;
        for (uint16& H : HeightData) { H = MidHeight; }

4. Auto-size components if needed:
   int32 QuadsPerSection = (Cmd.QuadsPerSection > 0) ? Cmd.QuadsPerSection : 63;
   int32 SectionsPerComponent = (Cmd.SectionsPerComponent > 0) ? Cmd.SectionsPerComponent : 1;
   FIntPoint ComponentCount;
   FLandscapeImportHelper::ChooseBestComponentSizeForImport(
     SizeX, SizeY, QuadsPerSection, SectionsPerComponent, ComponentCount);
   // NOTE: QuadsPerSection and SectionsPerComponent are modified IN-PLACE by this function.
   // After this call they hold the actual values UE chose, not necessarily what was requested.

5. Spawn landscape actor:
   ALandscape* Landscape = World->SpawnActor<ALandscape>();
   if (!Landscape) { LANDSCAPE_ERROR_RETURN(TEXT("Failed to spawn landscape actor")); }
   Landscape->SetActorLocation(Cmd.Location);
   Landscape->SetActorScale3D(Cmd.Scale);
   if (!Cmd.Label.IsEmpty()) Landscape->SetActorLabel(Cmd.Label);
   // R4-5: If Import() fails, we destroy the actor in step 8's error check.

6. Prepare Import() parameters:
   FGuid LandscapeGuid = FGuid::NewGuid();  // Landscape's own GUID (1st param to Import)
   // R5-CL-2: Map keys must be FGuid() (EMPTY), NOT FGuid::NewGuid().
   // Import() internally does FindChecked(FGuid()) to look up data.
   TMap<FGuid, TArray<uint16>> ImportHeightData;
   ImportHeightData.Add(FGuid(), MoveTemp(HeightData));
   // R5-CL-1: ImportMaterialLayerInfos MUST have same entry count as ImportHeightData.
   // Engine has check() assertion at LandscapeEdit.cpp:3263. Empty map = crash.
   TMap<FGuid, TArray<FLandscapeImportLayerInfo>> ImportMaterialLayerInfos;
   ImportMaterialLayerInfos.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());
   TArray<FLandscapeLayer> ImportLayers;  // empty is fine

7. Pre-Import setup:
   // R5-CL-5: Set material BEFORE Import() so component material instances are
   // initialized correctly during Import. Engine does this at
   // LandscapeEditorDetailCustomization_NewLandscape.cpp:1230.
   if (!Cmd.MaterialPath.IsEmpty()) {
     UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *Cmd.MaterialPath);
     if (Material) {
       // R5-IM-2: EditorSetLandscapeMaterial is not LANDSCAPE_API exported.
       // Set the UPROPERTY directly. Import() reads it per-component.
       Landscape->LandscapeMaterial = Material;
     }
   }

   // R5-CL-8: Set lighting LOD (prevents Lightmass OOM on large landscapes)
   Landscape->StaticLightingLOD = FMath::DivideAndRoundUp(
     FMath::CeilLogTwo((SizeX * SizeY) / (2048 * 2048) + 1), (uint32)2);

8. Call Import():
   Landscape->Import(
     LandscapeGuid,  // 1st param = landscape GUID (NOT the map key)
     0, 0, SizeX - 1, SizeY - 1,  // MinX, MinY, MaxX, MaxY (INCLUSIVE, hence -1)
     // R6-CL-1: Parameter names are confusingly opposite to variable names.
     // Engine signature: Import(..., InNumSubsections, InSubsectionSizeQuads, ...)
     // InNumSubsections = SectionsPerComponent (1 or 2)
     // InSubsectionSizeQuads = QuadsPerSection (7,15,31,63,127,255)
     SectionsPerComponent, QuadsPerSection,  // == InNumSubsections, InSubsectionSizeQuads
     ImportHeightData,
     TEXT(""),  // HeightmapFileName (empty string safer than nullptr)
     ImportMaterialLayerInfos,
     ELandscapeImportAlphamapType::Additive,
     ImportLayers  // TArrayView from TArray
   );

9. Verify success and finalize:
   // R5-CL-3: Import() already calls CreateLandscapeInfo() and
   // ReregisterAllComponents() internally. Do NOT call them again.
   // (R6-CL-4: non-layers path uses ReregisterAllComponents, not RegisterAllComponents)
   // Just verify that it worked by retrieving the info object.
   ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
   // R5-CL-7: In WP worlds, components may be split to streaming proxies after
   // Import, making LandscapeComponents.Num() == 0 on the main actor. Check both.
   if (!LandscapeInfo)
   {
     World->DestroyActor(Landscape);
     LANDSCAPE_ERROR_RETURN(TEXT("Import() failed - no landscape info created"));
   }
   // R6-CL-3: No PostEditChange() needed - Import() handles all finalization internally.

   Response.NumComponents = Landscape->LandscapeComponents.Num();
   Response.ActorName = Landscape->GetName();
   Response.ActorLabel = Landscape->GetActorLabel();
   Response.ActualSizeX = SizeX;
   Response.ActualSizeY = SizeY;

10. Optional WP grid:
   if (Cmd.WPGridSize > 0) {
     ULandscapeSubsystem* Subsystem = World->GetSubsystem<ULandscapeSubsystem>();
     if (LandscapeInfo && Subsystem)
       Subsystem->ChangeGridSize(LandscapeInfo, Cmd.WPGridSize);
   }
```

---

### ImportHeightmap (detailed steps)

```
1. Get world:
   UWorld* World = FWorldContextManager::Get().GetTargetWorld();
   if (!World) { LANDSCAPE_ERROR_RETURN(TEXT("No target world")); }

2. Resolve landscape:
   FString Error;
   ALandscapeProxy* Proxy = ResolveLandscape(Cmd.LandscapeId, World, Error);
   if (!Proxy) { LANDSCAPE_ERROR_RETURN(Error); }

3. Get ULandscapeInfo (checks edit layers):
   ULandscapeInfo* LandscapeInfo = GetLandscapeInfoFromProxy(Proxy, Error);
   if (!LandscapeInfo) { LANDSCAPE_ERROR_RETURN(Error); }

4. Query landscape extent (coordinates are INCLUSIVE):
   int32 MinX, MinY, MaxX, MaxY;
   if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY)) {
     LANDSCAPE_ERROR_RETURN(TEXT("Failed to get landscape extent"));
   }
   int32 Width = (MaxX - MinX + 1);
   int32 Height = (MaxY - MinY + 1);

5. Load heightmap file (M4 fix: resolve path first, supports absolute + relative):
   FString FullPath = ResolveHeightmapPath(Cmd.HeightmapFile, Error);
   if (FullPath.IsEmpty()) { LANDSCAPE_ERROR_RETURN(Error); }
   TArray<uint16> HeightData = LoadHeightmapFile(FullPath, Width, Height, Error);
   if (HeightData.Num() == 0) { LANDSCAPE_ERROR_RETURN(Error); }

6. Validate size matches landscape (C3 fix: no sqrt for non-square):
   if (HeightData.Num() != Width * Height) {
     LANDSCAPE_ERROR_RETURN(FString::Printf(
       TEXT("Heightmap has %d pixels but landscape expects %dx%d = %d pixels"),
       HeightData.Num(), Width, Height, Width * Height));
   }

7. Write via accessor (destructor auto-flushes):
   {
     FHeightmapAccessor<false> Accessor(LandscapeInfo);
     Accessor.SetData(MinX, MinY, MaxX, MaxY, HeightData.GetData());
   } // Destructor: flushes textures, updates bounds, collision, navigation

   Response.ResolutionX = Width;
   Response.ResolutionY = Height;
```

---

### SetLandscapeMaterial (detailed steps)

```
1. H4 fix: Validate material_path is non-empty (prevents silent no-op):
   if (Cmd.MaterialPath.IsEmpty()) {
     LANDSCAPE_ERROR_RETURN(TEXT("material_path is required"));
   }

2. Get world + resolve landscape:
   UWorld* World = FWorldContextManager::Get().GetTargetWorld();
   if (!World) { LANDSCAPE_ERROR_RETURN(TEXT("No target world")); }
   FString Error;
   ALandscapeProxy* Proxy = ResolveLandscape(Cmd.LandscapeId, World, Error);
   if (!Proxy) { LANDSCAPE_ERROR_RETURN(Error); }

3. Load material:
   UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *Cmd.MaterialPath);
   if (!Material) {
     LANDSCAPE_ERROR_RETURN(FString::Printf(TEXT("Material not found: %s"), *Cmd.MaterialPath));
   }

4. Apply via property set + PostEditChangeProperty:
   // R5-IM-2: EditorSetLandscapeMaterial is NOT LANDSCAPE_API exported (MinimalAPI class).
   // Direct call = linker error. Use UPROPERTY access + PostEditChangeProperty instead.
   // This is exactly what EditorSetLandscapeMaterial does internally
   // (LandscapeBlueprintSupport.cpp:98-108).
   Proxy->LandscapeMaterial = Material;
   FPropertyChangedEvent Evt(
     FindFieldChecked<FProperty>(Proxy->GetClass(), FName("LandscapeMaterial")));
   Proxy->PostEditChangeProperty(Evt);
   // R5-IM-3: PostEditChangeProperty already calls UpdateAllComponentMaterialInstances
   // internally (LandscapeEdit.cpp:6502). Do NOT call it again.
```

---

### PaintLandscapeLayers (detailed steps - most complex)

**R5-PL-1 correction: Weight normalization, NOT last-writer-wins.** FAlphamapAccessor uses
bWeightAdjust=true internally, which normalizes weights across all blended layers. When painting
Layer B at 255, Layer A is reduced proportionally at those pixels. Painting 3 layers sequentially
produces normalized weights across all 3 - earlier layers may be reduced. This is BETTER than
last-writer-wins for elevation-based painting. Document this in MCP tool description and help text.

```
1. Get world + resolve landscape + get ULandscapeInfo (R6-IM-1/IM-2 fix: World declaration + null check):
   UWorld* World = FWorldContextManager::Get().GetTargetWorld();
   if (!World) { LANDSCAPE_ERROR_RETURN(TEXT("No target world")); }
   FString Error;
   ALandscapeProxy* Proxy = ResolveLandscape(Cmd.LandscapeId, World, Error);
   if (!Proxy) { LANDSCAPE_ERROR_RETURN(Error); }
   ULandscapeInfo* LandscapeInfo = GetLandscapeInfoFromProxy(Proxy, Error);
   if (!LandscapeInfo) { LANDSCAPE_ERROR_RETURN(Error); }

1b. M1 fix: Validate layers array is non-empty:
   if (Cmd.Layers.Num() == 0) {
     LANDSCAPE_ERROR_RETURN(TEXT("layers array is empty - provide at least one layer"));
   }

2. Get landscape transform for elevation conversion:
   FVector LandscapeLoc = Proxy->GetActorLocation();
   FVector LandscapeScale = Proxy->GetActorScale3D();

3. Query extent and read heightmap:
   int32 MinX, MinY, MaxX, MaxY;
   // R5-PL-4: GetLandscapeExtent returns false if no components are loaded (WP unloaded)
   if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY)) {
     LANDSCAPE_ERROR_RETURN(TEXT("No landscape components loaded - cannot read extent"));
   }
   int32 Width = (MaxX - MinX + 1);
   int32 Height = (MaxY - MinY + 1);
   int32 NumVertices = Width * Height;

   TArray<uint16> HeightData;
   HeightData.SetNum(NumVertices);
   {
     // R4-10 fix: Use <false> (no interpolation) for consistency with ImportHeightmap's <false>.
     // Exact texel values give predictable elevation-to-weight mapping for artists.
     FHeightmapAccessor<false> HeightmapAccessor(LandscapeInfo);
     HeightmapAccessor.GetDataFast(MinX, MinY, MaxX, MaxY, HeightData.GetData());
   }

4. For each layer in Cmd.Layers:

   a. Validate layer name is non-empty:
      if (Cmd.Layers[i].LayerName.IsEmpty()) {
        LANDSCAPE_ERROR_RETURN(FString::Printf(TEXT("Layer %d has empty layer_name"), i));
      }

   b. Create or load ULandscapeLayerInfoObject:
      ULandscapeLayerInfoObject* LayerInfo;
      if (Cmd.Layers[i].LayerInfoPath.IsEmpty()) {
        // R4-9 fix: Use GetTransientPackage() as outer so GC doesn't collect prematurely.
        // Transient layer infos survive the function but are lost on save - documented in Risks.
        LayerInfo = NewObject<ULandscapeLayerInfoObject>(
          GetTransientPackage(), FName(*Cmd.Layers[i].LayerName));
        LayerInfo->LayerName = FName(*Cmd.Layers[i].LayerName);
      } else {
        LayerInfo = LoadObject<ULandscapeLayerInfoObject>(nullptr, *Cmd.Layers[i].LayerInfoPath);
        if (!LayerInfo) {
          LANDSCAPE_ERROR_RETURN(FString::Printf(
            TEXT("Layer info not found: %s"), *Cmd.Layers[i].LayerInfoPath));
        }
      }

      // R6-PL-1 fix: Register layer with landscape BEFORE painting so it appears in
      // the editor's Paint Layers panel. CreateTargetLayerSettingsFor() is the UE 5.5+
      // replacement for deprecated CreateLayerEditorSettingsFor(). It registers the
      // LayerInfo with all landscape proxies' target layer list.
      // FAlphamapAccessor::SetData() writes weight textures but doesn't register layers.
      LandscapeInfo->CreateTargetLayerSettingsFor(LayerInfo);

   c. Compute weight array (uint8, 0-255):
      TArray<uint8> WeightData;
      WeightData.SetNum(NumVertices);

      float MinElev = Cmd.Layers[i].MinElevation;
      float MaxElev = Cmd.Layers[i].MaxElevation;
      float Falloff = FMath::Max(Cmd.Layers[i].Falloff, 0.0f);

      for (int32 j = 0; j < NumVertices; ++j)
      {
        // Convert uint16 height to world Z
        float LocalZ = ((float)HeightData[j] - 32768.0f) / 128.0f;
        float WorldZ = LocalZ * LandscapeScale.Z + LandscapeLoc.Z;

        // Compute weight via smooth step with falloff
        float Weight;
        if (Falloff > 0.0f)
        {
          float FadeIn = FMath::SmoothStep(0.0f, 1.0f,
            FMath::GetRangePct(MinElev - Falloff, MinElev, WorldZ));
          float FadeOut = FMath::SmoothStep(0.0f, 1.0f,
            FMath::GetRangePct(MaxElev, MaxElev + Falloff, WorldZ));
          Weight = FadeIn * (1.0f - FadeOut);
        }
        else
        {
          Weight = (WorldZ >= MinElev && WorldZ <= MaxElev) ? 1.0f : 0.0f;
        }

        WeightData[j] = (uint8)FMath::Clamp(Weight * 255.0f, 0.0f, 255.0f);
      }

   d. Write layer via scoped accessor (destructor flushes per layer):
      {
        FAlphamapAccessor<false, false> AlphaAccessor(LandscapeInfo, LayerInfo);
        AlphaAccessor.SetData(MinX, MinY, MaxX, MaxY,
          WeightData.GetData(), ELandscapeLayerPaintingRestriction::None);
      } // Destructor flushes, updates collision

   e. R4-8 fix: Record painted layer in response:
      Response.LayerNames.Add(Cmd.Layers[i].LayerName);
      Response.LayersPainted++;

5. R4-4 fix: After all layers painted, update landscape layer info map:
   // This ensures the landscape knows about all layer info objects.
   // If UpdateLayerInfoMap is not available, try Proxy->EditorApplySpline(false)
   // or LandscapeInfo->RecreateCollisionComponents() as alternative flush.
   LandscapeInfo->UpdateLayerInfoMap(Proxy);
```

## Testing Strategy

### Live Testing (MANDATORY per SOP)

All landscape operations are visual - they create/modify terrain geometry and materials.
Automated tests cannot verify visual correctness. Each test requires human visual verification
in the editor viewport (centaur testing protocol).

| # | Test | MCP Command | Visual Verification |
|---|------|-------------|---------------------|
| T1 | Flat landscape | `create_landscape(size_x=505)` | Green wireframe grid visible at origin, flat |
| T2 | Heightmap landscape | `create_landscape(size_x=1009, heightmap_file="Heightmaps/test.png")` | Terrain has hills/valleys matching the heightmap |
| T3 | Import onto existing | `import_heightmap(heightmap_file="Heightmaps/test.r16")` | Flat terrain transforms to match heightmap |
| T4 | Material | `set_landscape_material(material_path="/Game/Materials/M_Landscape")` | Landscape changes from default to specified material |
| T5 | Layer painting | `paint_landscape_layers(layers=[...])` | Distinct colors/textures at different elevations |
| T6 | WP grid | `create_landscape(size_x=2017, wp_grid_size=512)` | `query_landscape` returns multiple streaming proxies |
| T7 | Error: bad file | `import_heightmap(heightmap_file="nonexistent.png")` | Returns error, no crash |
| T8 | Error: no landscape | `import_heightmap()` in empty level | Returns "no landscape found" error |

**Heightmap test files:** Need a 16-bit PNG and/or R16 file uploaded via `write_project_file`
before T2/T3. Can use any publicly available heightmap or generate one.

**Test order:** T1 first (simplest), then T4 (quick visual check), then T2/T3 (file-dependent),
then T5 (requires material with layer blend nodes), then T6 (WP-specific), then T7/T8 (error cases).

**Wait for human verification before cleanup** - don't delete test landscapes until user confirms
they saw the expected result (per centaur testing protocol in CLAUDE.md).

## Implementation Checklist

### Phase 1: Build.cs + Proto + Structs (required first)
- [ ] **P1.1** Add `Landscape` to AgentBridgeScripting **private** deps in `AgentBridgeScripting.Build.cs` (R4-2 fix: private not public - landscape types are only used in CommandExecutor.cpp, not exposed in AgentCommands.h public API). Add to PrivateDependencyModuleNames (line ~24, before the `if (Target.bBuildEditor)` block). (can parallel with P1.2)
- [ ] **P1.2** Add `LandscapeEditor` to AgentBridgeScripting editor-only private deps in `AgentBridgeScripting.Build.cs` (line ~28, in `if (Target.bBuildEditor)` block) (can parallel with P1.1)
- [ ] **P1.3** Add proto messages after line ~439 (after closing brace of `GetActorsInDataLayerResponse`; R5-PS-1 fix). Add `//--- Landscape Operations ---` section header. Add 4 RPCs to service block after line ~847 (after World Partition, before Console Commands). NOTE: SetLandscapeMaterial returns `TempoScripting.Empty` not a custom response (R5-PS-12). (can parallel with P1.5)
- [ ] **P1.4** Verify proto LF line endings: `file AgentBridge.proto` should say "ASCII text" not "CRLF". Fix with `sed -i 's/\r$//' AgentBridge.proto` if needed (requires P1.3)
- [ ] **P1.5** Add `FLandscapeLayerConfig` struct + 4 enum entries (after PCG Commands ~line 69) + 4 command structs (after `FSetPCGParameterCommand`) + 3 response structs (after `FRegeneratePCGResponse`) to `AgentCommands.h`. SetLandscapeMaterial uses `FAgentResponseBase` directly. (can parallel with P1.3)
- [ ] **P1.6** Add 4 Execute() declarations to `CommandExecutor.h` (after PCG section ~line 163). Note: SetLandscapeMaterial uses `FAgentResponseBase&` not a custom response (requires P1.5)
- [ ] **P1.7** Kill editor, build, verify compilation (requires all P1.x)

### Phase 2: gRPC Handlers (after Phase 1)
- [ ] **P2.1** Add 7 forward declarations (4 request + 3 response classes) in `AgentBridgeServiceSubsystem.h` namespace block (after line 55, after World Partition types). No SetLandscapeMaterialResponse - uses TempoScripting.Empty (R5-PS-12). (can parallel with P2.2)
- [ ] **P2.2** Add 4 handler method declarations in `AgentBridgeServiceSubsystem.h` (after line 270, after World Partition handlers section). Pattern: `void MethodName(const AgentBridgeServer::Request&, const TResponseDelegate<AgentBridgeServer::Response>&)` (can parallel with P2.1)
- [ ] **P2.3** Implement 4 thin handlers in `AgentBridgeServiceSubsystem.cpp`. Pre-defined helpers in anonymous namespace (lines 242-257): `FromProtoVector()`, `FromProtoScale()`. String: `UTF8_TO_TCHAR(Request.field().c_str())`. Vector: `FromProtoVector(Request.location())`. R5-PS-15: Check `Request.has_scale()` before overriding C++ default of (100,100,100) - proto3 defaults to (0,0,0). Repeated input: `for (const auto& item : Request.layers()) { ... Cmd.Layers.Add(cfg); }`. Repeated output: `Response.add_layer_names(TCHAR_TO_UTF8(*Name))`. Float access: `item.min_elevation()`. SetLandscapeMaterial returns `TempoScripting::Empty` (R5-PS-12), following DeleteActor pattern. R5-GH-11: No game thread dispatch needed (Tempo handles it). `.cpp` uses `using namespace AgentBridgeServer;` so no namespace prefix on proto types. Use `grpc::INTERNAL` for failures, `grpc::NOT_FOUND` when landscape not found. (requires P2.1, P2.2)
- [ ] **P2.4** Register all 4 in `RegisterScriptingServices()` (after line 131, after GetActorsInDataLayer, before ExecuteConsoleCommand). Syntax: `SimpleRequestHandler(&AgentBridgeAsyncService::RequestCreateLandscape, &UAgentBridgeServiceSubsystem::CreateLandscape),` (requires P2.3)
- [ ] **P2.5** Build to verify (requires P2.4)

### Phase 3: Business Logic (after Phase 2)
- [ ] **P3.1** Add landscape includes in `CommandExecutor.cpp` under `#if WITH_EDITOR` guard (existing block at lines 42-63): `LandscapeProxy.h`, `Landscape.h`, `LandscapeInfo.h`, `LandscapeEdit.h`, `LandscapeDataAccess.h`, `LandscapeSubsystem.h`, `LandscapeLayerInfoObject.h` (R4-3 fix: needed for PaintLandscapeLayers NewObject), `LandscapeImportHelper.h`, `LandscapeEditorModule.h`, `LandscapeFileFormatInterface.h`. NOTE: `FPaths` and `IFileManager` are already available via transitive includes - no explicit include needed.
- [ ] **P3.2** Implement `LANDSCAPE_ERROR_RETURN` macro (R4-6: REQUIRED, not optional), then `ResolveHeightmapPath()`, `ResolveLandscape()`, `GetLandscapeInfoFromProxy()` helpers as shown in plan (requires P3.1)
- [ ] **P3.3** Implement CreateLandscape - includes R5-CL-1 (matching layer map entry), R5-CL-2 (empty GUID keys), R5-CL-3 (Import handles registration), R5-CL-4 (relaxed size validation), R5-CL-5 (material before Import), R5-CL-8 (StaticLightingLOD), C1 (uint16 loop not Memset), H2 (duplicate check) (requires P3.2, most complex)
- [ ] **P3.4** Implement ImportHeightmap - includes C3 (no sqrt in error), C4 (explicit early-return), M4 (ResolveHeightmapPath). Key: `FHeightmapAccessor<false> accessor(LandscapeInfo); accessor.SetData(MinX, MinY, MaxX, MaxY, Data.GetData());` Coordinates are INCLUSIVE. Array size = `(MaxX-MinX+1)*(MaxY-MinY+1)` (requires P3.2, can parallel with P3.5)
- [ ] **P3.5** Implement SetLandscapeMaterial - includes H4 (empty path check). R5-IM-2: Can't call EditorSetLandscapeMaterial (not exported). Use `Proxy->LandscapeMaterial = Material` + `PostEditChangeProperty()` instead. R5-IM-3: Do NOT call UpdateAllComponentMaterialInstances (PostEditChangeProperty does it). (requires P3.2, can parallel with P3.4)
- [ ] **P3.6** Implement PaintLandscapeLayers - includes M1 (empty layers check), R5-PL-1 (weight normalization, not last-writer-wins), R6-PL-1 (CreateTargetLayerSettingsFor per layer), C4 (explicit early-return). Key: `FAlphamapAccessor<false, false> accessor(LandscapeInfo, LayerInfoObj); accessor.SetData(MinX, MinY, MaxX, MaxY, WeightData.GetData(), ELandscapeLayerPaintingRestriction::None);` NOTE: PaintingRestriction has NO default, must pass explicitly (requires P3.2, second most complex)
- [ ] **P3.7** Build and verify (requires all P3.x)

### Phase 4: Python MCP (after Phase 1 proto; can overlap with Phase 3)
- [ ] **P4.1** 4 client methods in `agentbridge.py` (after `get_actors_in_data_layer` method ~line 919, with new `# Landscape Operations` section header; R5-PY-1 fix). **All methods go in the existing `agentbridge.py` file** (M6 fix). Stub calls use PascalCase: `self.stub.CreateLandscape(request)`. Vector: `self._make_vector(*location)` with CopyFrom or constructor. R5-PY-18/R6-PY-3: Default scale to `(100,100,100)` in Python method signature (primary default). C++ handler also checks `has_scale()` as safety net (R5-PS-15). Both layers are intentional defense-in-depth against proto3 zero-default. R5-PY-17: paint_landscape_layers iteration pattern: `for layer_dict in layers: layer = request.layers.add(); layer.layer_name = layer_dict["layer_name"]; layer.min_elevation = layer_dict["min_elevation"]; ...` (requires P1.3 proto generation)
- [ ] **P4.2** 4 tool definitions (inputSchema) in TOOLS list (after `get_data_layers` tool, before Console Commands separator; R6-PY-1 fix: `get_actors_in_data_layer` has no tool def, `get_data_layers` is the correct anchor). Add `# Landscape Operations` section header. R5-PY-22: Include `"default"` values for optional params, especially `scale` default `[100,100,100]`. Array params: `{"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3}`. R4-7 fix: `paint_landscape_layers` layers param needs COMPLETE nested schema: `{"type": "array", "items": {"type": "object", "properties": {"layer_name": {"type": "string"}, "layer_info_path": {"type": "string"}, "min_elevation": {"type": "number"}, "max_elevation": {"type": "number"}, "falloff": {"type": "number", "default": 0}}, "required": ["layer_name", "min_elevation", "max_elevation"]}, "minItems": 1}`. Add `"description"` fields to params for agent usability (R5-PY-15). R6-PY-4: Each tool must include a `"required"` array at the top level of inputSchema specifying mandatory params (e.g. `create_landscape`: `["size_x"]`, `import_heightmap`: `["heightmap_file"]`, `set_landscape_material`: `["material_path"]`, `paint_landscape_layers`: `["layers"]`). (can parallel with P4.1)
- [ ] **P4.3** 4 tool handlers in `_execute_impl()` (after `get_data_layers` handler, before `execute_console_command` handler; R6-PY-1 fix: `get_actors_in_data_layer` has no handler). Pattern: extract args explicitly (not `**kwargs`), call `safe_call(client.method, arg1=val1, ...)`, check error, return dict. Repeated string fields: `list(result.layer_names)`. R6-PY-2: `set_landscape_material` returns `TempoScripting.Empty` (no fields) - use delete_actor pattern: `safe_call(client.set_landscape_material, ...)` then `return {"success": True}`. The other 3 handlers access response fields normally. R5-PY-21: inputSchema property names (snake_case) must match dict keys used in client method. (requires P4.1, P4.2)
- [ ] **P4.4** Create NEW `landscape` module in `__init__.py` MODULES dict (after `world_partition` ~line 102): `"landscape": {"tools": ["create_landscape", "import_heightmap", "set_landscape_material", "paint_landscape_layers"], "description": "Landscape creation, heightmap import, material/layer painting"}`. Add `"landscape"` to both `standard` and `editor` profile lists (R6-PY-5: primary use case "build me a level" uses standard profile). NOTE: "module" here means an entry in the MODULES dict for tool grouping, not a separate Python file. (can parallel with P4.3)
- [ ] **P4.5** Add `landscape` help topic in `_get_help_text()` topics dict (before line ~2349, the closing brace of the topics dict; R4-12 fix: was ~2347). Cover: tool reference, parameter details, workflow examples, heightmap file requirements (resolution must match size_x x size_y), layer ordering note (R5-PL-1: weight normalization across blended layers, not last-writer-wins), performance note (large landscapes may take 5-30s due to collision generation), limitations. Update overview help topic list. (can parallel with P4.3)

### Phase 5: Live Testing (after Phase 3 + Phase 4)
- [ ] **P5.1** Start editor, wait for gRPC on port 10001
- [ ] **P5.2** Test T1: flat landscape creation (visual: green wireframe grid)
- [ ] **P5.3** Test T4: material assignment (visual: material appears on landscape)
- [ ] **P5.4** Test T2: create with heightmap file (visual: terrain shape)
- [ ] **P5.5** Test T3: import heightmap on existing (visual: terrain changes)
- [ ] **P5.6** Test T5: elevation-based layer painting (visual: color bands)
- [ ] **P5.7** Test T6: WP grid (verify: `query_landscape` shows streaming proxies)
- [ ] **P5.8** Test T7/T8: error cases (verify: meaningful error messages, no crashes)

### Phase 6: Documentation (after Phase 5)
- [ ] **P6.1** Update root `CLAUDE.md` status table and tool count
- [ ] **P6.2** Update `AgentBridgeScripting/CLAUDE.md` with new commands, deps, and landscape section
- [ ] **P6.3** Update `AgentBridgeServer/CLAUDE.md` with RPC count (55 total) and new Landscape Operations category
- [ ] **P6.4** Update root `README.md` with landscape operations section

## Risks

| Risk | Mitigation |
|------|------------|
| Import() fails silently | Check `GetLandscapeInfo()` after Import; destroy actor if null (R4-5, R5-CL-3) |
| LandscapeEditor not loaded | `FModuleManager::LoadModuleChecked` not `GetModuleChecked` |
| Transient layer infos lost on save | Document: use `create_asset` for persistent ones |
| Transient LayerInfo GC'd (R4-9) | Use `GetTransientPackage()` as outer for `NewObject` |
| Edit layers block accessor writes | `GetLandscapeInfoFromProxy()` checks `HasLayersContent()` and returns clear error |
| Resolution mismatch | Clear error with pixel count and expected dimensions (no sqrt - C3 fix) |
| Heightmap size must match landscape | Heightmap resolution must be exactly size_x x size_y pixels |
| Actor found but not a landscape | `ResolveLandscape()` checks Cast result, returns "not a landscape" error |
| Coordinates are INCLUSIVE | Array size = `(MaxX-MinX+1)*(MaxY-MinY+1)`, NOT `(MaxX-MinX)*(MaxY-MinY)` |
| Flat heightmap init wrong (C1) | Use `for (uint16& H : HeightData) { H = 32768; }` NOT `FMemory::Memset(0x80)` |
| Import() handles registration (R5-CL-3) | Do NOT call CreateLandscapeInfo() or ReregisterAllComponents() - Import() does both internally |
| Import map key must be empty GUID (R5-CL-2) | Use `FGuid()` as map key, `FGuid::NewGuid()` only as Import()'s 1st param |
| Import layer map must match (R5-CL-1) | ImportMaterialLayerInfos must have same entry count as ImportHeightData (check assertion) |
| Helper error path inconsistency (C4) | All callers use `LANDSCAPE_ERROR_RETURN` macro (R4-6: required) |
| Invalid landscape size (H1) | Validate Size >= 2 and Size <= 8129; let ChooseBestComponentSizeForImport handle configs (R5-CL-4) |
| Duplicate landscape corruption (H2) | Check `GetMainLandscape()` and error if landscape already exists |
| Heightmap file not found (H3) | `ResolveHeightmapPath()` checks `IFileManager::FileExists()` before loading |
| Empty material_path (H4) | Validate non-empty before LoadObject |
| Empty layers array (M1) | Reject with "provide at least one layer" error |
| Empty layer name | Reject with "layer N has empty layer_name" error |
| Overlapping layer elevations (M2) | Document weight normalization behavior in help text (R5-PL-1: not last-writer-wins) |
| OOM from huge dimensions (M3) | Cap at MAX_LANDSCAPE_SIZE = 8129 |
| Absolute vs relative path (M4) | `ResolveHeightmapPath()` detects via `FPaths::IsRelative()` |
| Layer not registered in editor panel (R6-PL-1) | Call `CreateTargetLayerSettingsFor(LayerInfo)` per layer before painting; `UpdateLayerInfoMap()` after all layers |
| StartTime scope in #else branch (R4-1) | Declare StartTime OUTSIDE `#if WITH_EDITOR`; EndTiming after `#endif` |
| Spawned actor left on failure (R4-5) | Check component count; destroy actor if Import() failed |
| Large landscape performance | Document 5-30s for big terrain in help text |

## Round 3 Validation Fixes Applied

All issues found in Round 3 validation have been incorporated into the plan above.
Cross-reference tags (C1-C4, H1-H4, M1-M7) appear inline where each fix is applied.

| Fix | Severity | Description | Where Applied |
|-----|----------|-------------|---------------|
| C1 | CRITICAL | FMemory::Memset gives 0x8080 not 0x8000 per uint16 | CreateLandscape step 3c |
| C2 | ~~CRITICAL~~ | ~~Missing CreateLandscapeInfo() before RegisterAllComponents~~ | **SUPERSEDED by R5-CL-3**: Import() handles both internally. Do NOT call either. |
| C3 | CRITICAL | sqrt() wrong for non-square heightmaps in error msg | ImportHeightmap step 6 |
| C4 | CRITICAL | ResolveLandscape callers must explicitly early-return | All 4 Execute() functions, template pattern |
| H1 | HIGH | Invalid landscape sizes silently auto-corrected | CreateLandscape step 1 |
| H2 | HIGH | Duplicate landscape creates corrupted state | CreateLandscape step 2 |
| H3 | HIGH | No file existence check before format handler | ResolveHeightmapPath helper |
| H4 | HIGH | Empty material_path is silent no-op | SetLandscapeMaterial step 1 |
| M1 | MEDIUM | Empty layers array = ambiguous success | PaintLandscapeLayers step 1b |
| M2 | MEDIUM | Overlapping layer elevations undocumented | PaintLandscapeLayers header note, P4.5 |
| M3 | MEDIUM | No max size validation (potential OOM) | CreateLandscape step 1 |
| M4 | MEDIUM | Absolute path handling for heightmap | ResolveHeightmapPath helper, all callers |
| M5 | MEDIUM | Python help topic line ~2050 wrong | P4.5 corrected to ~2349 (R4-12 refined) |
| M6 | MEDIUM | Landscape methods file location unclear | P4.1, P4.4 clarified: in agentbridge.py |
| M7 | MEDIUM | Proto message insertion line off by 4 | P1.3 corrected to ~435 |

## Round 4 Validation Fixes Applied

All issues found in Round 4 validation have been incorporated into the plan above.
Cross-reference tags (R4-1 through R4-12) appear inline where each fix is applied.

| Fix | Severity | Description | Where Applied |
|-----|----------|-------------|---------------|
| R4-1 | CRITICAL | StartTime declared inside #if WITH_EDITOR - #else can't call EndTiming | Execute() template pattern (moved outside #if) |
| R4-2 | CRITICAL | Landscape should be private dep, not public | P1.1, Files to Modify table |
| R4-3 | HIGH | Missing LandscapeLayerInfoObject.h include | P3.1 include list |
| R4-4 | HIGH | Layer registration - accessor may not auto-register with landscape | PaintLandscapeLayers step 4b + new step 5, Risks table |
| R4-5 | HIGH | Spawned actor not cleaned up on Import() failure | CreateLandscape step 9 (check GetLandscapeInfo, destroy if null; R5-CL-6 simplified) |
| R4-6 | HIGH | LANDSCAPE_ERROR_RETURN macro was "optional" but used everywhere | Macro section header + P3.2 checklist item |
| R4-7 | HIGH | Missing nested inputSchema for layers parameter | P4.2 checklist item (complete schema added) |
| R4-8 | MEDIUM | Response tracking code not shown in PaintLandscapeLayers | PaintLandscapeLayers step 4d (LayersPainted++ and LayerNames.Add) |
| R4-9 | MEDIUM | NewObject without outer - GC may collect transient LayerInfo | PaintLandscapeLayers step 4b (GetTransientPackage as outer) |
| R4-10 | MEDIUM | HeightmapAccessor<true> inconsistent with ImportHeightmap's <false> | PaintLandscapeLayers step 3 (changed to <false>) |
| R4-11 | MEDIUM | Step 3 pseudo-code showed bare return without error path | CreateLandscape step 3a+3c (now uses LANDSCAPE_ERROR_RETURN) |
| R4-12 | MEDIUM | Help topic line ~2347 should be ~2349 | P4.5 checklist item |

## Validation History

| Round | Agents | Issues Found | Severity Breakdown |
|-------|--------|-------------|-------------------|
| 1 | 8 | 6 | Convention/pattern issues |
| 2 | 5 | 21 | 5 critical (completeness gaps) |
| 3 | 6 | 15 | 4 CRITICAL, 4 HIGH, 7 MEDIUM |
| 4 | 6 | 12 | 2 CRITICAL, 5 HIGH, 5 MEDIUM |
| 5 | 6 | 25 | 4 CRITICAL, 5 HIGH, 16 MEDIUM |
| 6 | 6 | 25 | 0 CRITICAL, 6 HIGH, 10 MEDIUM, 9 LOW |
| 7 | 6 | 7 | 0 CRITICAL, 0 HIGH, 2 MEDIUM, 5 LOW |
| **Total** | **43** | **111** | All incorporated into plan |

## Round 5 Validation Fixes (INTEGRATED)

Round 5 used 6 targeted agents validating against UE 5.6 engine source code.
Reports archived: `/tmp/r5_v1_create_landscape.txt` through `/tmp/r5_v6_python_mcp.txt`

**All R5 fixes have been integrated into the plan body above.** Cross-reference tags (R5-CL-*,
R5-IM-*, R5-PL-*, R5-PS-*, R5-PY-*, R5-GH-*) appear inline where each fix is applied.

| Severity | Count | Key Fixes |
|----------|-------|-----------|
| CRITICAL | 4 | CL-1 (map entry count), CL-2 (empty GUID key), CL-3 (Import handles registration), IM-1 (GetExtension dot) |
| HIGH | 5 | CL-4 (relaxed sizes), CL-5 (material before Import), IM-2 (linker fix), PS-13 (RPC decls), PY-17 (iteration pattern) |
| MEDIUM | 16 | All integrated - see inline tags |

**Key decisions made during R5 integration:**
- R5-PS-12: SetLandscapeMaterial uses `TempoScripting.Empty` return type (matching existing pattern)
- R5-CL-3 supersedes R3-C2: Do NOT call `CreateLandscapeInfo()` or `ReregisterAllComponents()`
- R5-IM-2: Use `Proxy->LandscapeMaterial = Material` + `PostEditChangeProperty()` instead of `EditorSetLandscapeMaterial()`

## Round 6 Validation Fixes (INTEGRATED)

Round 6 used 6 targeted agents. Reports archived: `docs/plans/validation/r6_v1_*.txt` through `r6_v6_*.txt`

**All R6 fixes have been integrated into the plan body above.** Cross-reference tags (R6-*) appear inline.

| Severity | Count | Key Fixes |
|----------|-------|-----------|
| HIGH | 6 | CL-1 (Import param order comments), IM-1 (World null check), PL-1 (CreateTargetLayerSettingsFor), PY-1 (insertion anchor), PY-2 (Empty response pattern), PY-3 (Scale default docs) |
| MEDIUM | 10 | CL-3 (remove PostEditChange), CL-4 (ReregisterAllComponents), IM-2 (PaintLayers World decl), PL-2 (stale checklist ref), PS-6/PS-7 (section headers), PY-4 (required arrays), PY-5 (standard profile), PY-6 (last-writer-wins text) |
| LOW | 9 | All style/documentation nits - no plan changes needed |

## Round 7 Validation Fixes (INTEGRATED)

Round 7 re-validated only sections changed in R6. 6 focused validators, all passed (0C/0H).
Reports archived: `docs/plans/validation/r7_v1_*.txt` through `r7_v6_*.txt`

**All R7 fixes have been integrated into the plan body above.**

| Severity | Count | Key Fixes |
|----------|-------|-----------|
| MEDIUM | 2 | IP-1 (CreateLandscape step 2 bare return -> LANDSCAPE_ERROR_RETURN), V3-M1 (Files to Modify "+4 responses" -> "+3") |
| LOW | 5 | CH-1 (table profile text), CH-2 (dup of V3-M1), CH-3 (stale RegisterAllComponents in history), CH-4 (step 1b macro), V3-L1 (proto comment) |

**Plan status: VALIDATED.** 7 rounds, 43 agents, 111 total findings, all integrated. 0 CRITICAL + 0 HIGH remaining.
