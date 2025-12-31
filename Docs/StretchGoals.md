# AgentBridge Stretch Goals & Research Notes

> Brainstorming document for future features, research tasks, and use case exploration.
> Primary goal: "Complete set of agent tools for Unreal that might plausibly be helpful in some future scenario"

---

## Table of Contents

1. [Primary Use Cases](#primary-use-cases)
2. [Feature Roadmap](#feature-roadmap)
3. [DataAssets Support](#dataassets-support)
4. [Visual Capture](#visual-capture)
5. [Audio Capture](#audio-capture)
6. [Editor Module Integration](#editor-module-integration)
7. [Competitor Analysis](#competitor-analysis)
8. [Research Notes](#research-notes)

---

## Primary Use Cases

### 1. "Build Me a Level" (Core)
Agent needs to:
- Query existing actors and understand scene composition
- Spawn actors of various types (lights, meshes, volumes)
- Position and transform actors in 3D space
- Modify properties (light intensity, mesh materials, etc.)
- Organize actors in folders
- Save work (transactions for undo)

**Status:** Mostly complete. Need to add more granular material/mesh operations.

### 2. "Populate My Level with Data"
Agent needs to:
- Read DataTables and DataAssets
- Create/modify DataAsset entries
- Link actors to data (via soft references)
- Batch operations for large datasets

**Status:** Pending - DataAssets support needed.

### 3. "Help Me Debug This"
Agent needs to:
- Inspect actor state at runtime
- Read property values during PIE
- Call debug functions on actors
- Capture screenshots of issues
- Record gameplay sequences

**Status:** Partial - PIE support done, visual capture needed.

### 4. "Create a Procedural System"
Agent needs to:
- Create/modify PCG graphs
- Set up PCG spawners
- Configure PCG nodes and parameters
- Preview PCG results

**Status:** Not started - PCG module hooks needed.

### 5. "Set Up Materials and Visuals"
Agent needs to:
- Create material instances
- Modify material parameters
- Apply materials to meshes
- Create/modify texture assets

**Status:** Not started - Material editor hooks needed.

### 6. "Record Audio/Capture Media"
Agent needs to:
- Take viewport screenshots
- Capture scene through SceneCapture components
- Record audio from world (for feedback)
- Record player mic (for voice commands)

**Status:** Not started - Visual and audio capture needed.

### 7. "Control Simulation/Testing"
Agent needs to:
- Start/stop PIE
- Control simulation time
- Spawn test actors
- Verify expected states

**Status:** Good via Tempo integration (tempo_core_editor, tempo_time services).

---

## Feature Roadmap

### High Priority
1. **DataAssets Support** - Critical for data-driven workflows
2. **Visual Capture** - Screenshots and scene capture
3. **AgentBridge.Capabilities Console Command** - Debug context info

### Medium Priority
4. **Audio Capture** - World audio and player mic
5. **Material Editor Integration** - Material instance manipulation
6. **PCG Integration** - Procedural content generation

### Lower Priority (Research First)
7. **Sequencer Integration** - Cinematics control
8. **Animation Blueprint Access** - Animation state machines
9. **Niagara Integration** - Particle system control
10. **Level Streaming** - Multi-level operations

---

## DataAssets Support

### Overview
DataAssets in UE are UObject-derived classes that wrap data for asset management. They're commonly used for:
- Game configuration (DataTables)
- Item/weapon definitions
- Character stats
- Dialogue/quest data

### Technical Approach

#### 1. UDataAsset Base Class
```cpp
// All DataAssets derive from UDataAsset
UDataAsset* Asset = LoadObject<UDataAsset>(nullptr, TEXT("/Game/Data/MyData.MyData"));

// Access properties via reflection (already supported!)
for (TFieldIterator<FProperty> PropIt(Asset->GetClass()); PropIt; ++PropIt)
{
    // Same FPropertyAccessor code works
}
```

#### 2. UDataTable Special Case
```cpp
// DataTables have special row iteration
UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/Items.Items"));
if (Table)
{
    // Get row struct type
    UScriptStruct* RowStruct = Table->GetRowStruct();

    // Iterate rows
    Table->ForeachRow<FMyRowStruct>(TEXT("AgentBridge"),
        [](const FName& Key, const FMyRowStruct& Row)
        {
            // Process row
        });

    // Or by name
    FMyRowStruct* Row = Table->FindRow<FMyRowStruct>(FName("RowName"), TEXT(""));
}
```

#### 3. UPrimaryDataAsset
```cpp
// PrimaryDataAssets have asset manager integration
FPrimaryAssetId AssetId = Asset->GetPrimaryAssetId();
```

### Commands to Add

```cpp
// List DataAssets
struct FListDataAssetsCommand : FAgentCommandBase
{
    FString BaseClassName;  // UDataAsset, UDataTable, specific type
    FString PathFilter;     // /Game/Data/*
    int32 Limit;
};

// Get DataAsset
struct FGetDataAssetCommand : FAgentCommandBase
{
    FString AssetPath;
    bool bIncludeRows;  // For DataTables
    int32 MaxDepth;
};

// Get DataTable Row
struct FGetDataTableRowCommand : FAgentCommandBase
{
    FString TablePath;
    FString RowName;
};

// Set DataTable Row (Editor only)
struct FSetDataTableRowCommand : FAgentCommandBase
{
    FString TablePath;
    FString RowName;
    TMap<FString, FString> Values;
};
```

### Nested Struct/Array/UObject Verification
Need to verify that nested structures work correctly:
- `TArray<UObject*>` inside struct
- `TMap<FName, TArray<FSomeStruct>>`
- `TSoftObjectPtr<UTexture>` arrays
- Recursive struct references

---

## Visual Capture

### Viewport Screenshot

#### Method 1: FScreenshotRequest (Simplest)
```cpp
void CaptureViewport(const FString& Filename)
{
    FScreenshotRequest::RequestScreenshot(Filename, true, false);
}
```

#### Method 2: ReadPixels (More Control)
```cpp
void CaptureViewportToTexture(UWorld* World)
{
    FViewport* Viewport = GEngine->GameViewport->Viewport;
    TArray<FColor> Pixels;
    Viewport->ReadPixels(Pixels);

    // Convert to texture or save to file
    FIntPoint Size = Viewport->GetSizeXY();
    // ... create texture from Pixels
}
```

### SceneCapture Component

#### Creating SceneCapture
```cpp
ASceneCapture2D* Capture = World->SpawnActor<ASceneCapture2D>();
Capture->GetCaptureComponent2D()->TextureTarget = RenderTarget;
Capture->GetCaptureComponent2D()->CaptureScene();

// Get pixels from RenderTarget
TArray<FColor> Pixels;
RenderTarget->ReadPixels(Pixels);
```

### Commands to Add

```cpp
// Capture viewport
struct FCaptureViewportCommand : FAgentCommandBase
{
    FString OutputPath;  // File path or empty for memory
    int32 Width;         // 0 = current viewport size
    int32 Height;
    bool bShowUI;        // Include UI?
};

struct FCaptureViewportResponse : FAgentResponseBase
{
    FString FilePath;    // If saved to file
    TArray<uint8> ImageData;  // PNG/JPG bytes if in memory
    int32 Width;
    int32 Height;
};

// Capture from SceneCapture component
struct FCaptureSceneCommand : FAgentCommandBase
{
    FString ActorId;     // Actor with SceneCaptureComponent
    FString ComponentName;  // Optional if multiple captures
    FVector Location;    // Optional override
    FRotator Rotation;   // Optional override
    int32 Width;
    int32 Height;
};
```

### Context Considerations
- **Editor:** Full viewport access
- **PIE:** Viewport exists but may be different
- **Packaged:** No editor viewport, use SceneCapture

---

## Audio Capture

### World Audio (Scene Audio)
Goal: Let agent "hear" what's happening in the world

#### Method 1: Audio Analysis Component
```cpp
// UAudioAnalysisComponent for real-time analysis
class UAudioAnalysisComponent : public UActorComponent
{
    // Frequency bands, beat detection, etc.
};
```

#### Method 2: Audio Capture Component
```cpp
// Available in newer UE versions
UAudioCaptureComponent* Capture = NewObject<UAudioCaptureComponent>(Actor);
Capture->Start();
// ... capture audio data
```

### Player Mic
Goal: Record player voice for commands or feedback

#### Method: Voice Capture
```cpp
#include "Voice/VoiceCapture.h"

TSharedPtr<IVoiceCapture> VoiceCapture = FVoiceModule::Get().CreateVoiceCapture();
VoiceCapture->Start();
// ... get audio data
```

### Commands to Add

```cpp
// Start audio capture
struct FStartAudioCaptureCommand : FAgentCommandBase
{
    enum EAudioSource { WorldMix, PlayerMic, SpecificActor };
    EAudioSource Source;
    FString ActorId;     // If SpecificActor
    float Duration;      // Max seconds (0 = until stopped)
    int32 SampleRate;    // Default: 44100
};

// Stop and get audio
struct FStopAudioCaptureCommand : FAgentCommandBase
{
    FString CaptureId;
};

struct FAudioCaptureResponse : FAgentResponseBase
{
    TArray<uint8> AudioData;  // WAV bytes
    float Duration;
    int32 SampleRate;
};

// Get audio analysis (non-blocking)
struct FGetAudioAnalysisCommand : FAgentCommandBase
{
    FString ActorId;  // Actor with audio component
};

struct FAudioAnalysisResponse : FAgentResponseBase
{
    TArray<float> FrequencyBands;  // e.g., bass, mid, treble
    float AverageVolume;
    bool bBeatDetected;
};
```

---

## Editor Module Integration

### Material Editor

#### Creating Material Instances
```cpp
UMaterialInstanceDynamic* MatInst = UMaterialInstanceDynamic::Create(BaseMaterial, Owner);
MatInst->SetScalarParameterValue(FName("Roughness"), 0.5f);
MatInst->SetVectorParameterValue(FName("BaseColor"), FLinearColor::Red);
```

#### Getting Material Parameters
```cpp
TArray<FMaterialParameterInfo> Params;
TArray<FGuid> Guids;
Material->GetAllScalarParameterInfo(Params, Guids);
```

### PCG (Procedural Content Generation)

#### Accessing PCG Graphs
```cpp
// PCG graphs are stored in UPCGGraphSettings
APCGActor* PCGActor = /* find actor */;
UPCGComponent* PCGComp = PCGActor->GetComponent();
UPCGGraph* Graph = PCGComp->GetGraph();
```

#### Modifying PCG Nodes
```cpp
// PCG nodes have typed settings
for (UPCGNode* Node : Graph->Nodes)
{
    if (UPCGPointFilterSettings* FilterSettings = Cast<UPCGPointFilterSettings>(Node->GetSettings()))
    {
        // Modify filter parameters
    }
}
```

### Sequencer

#### Controlling Sequences
```cpp
ALevelSequenceActor* SeqActor = /* find actor */;
ULevelSequencePlayer* Player = SeqActor->GetSequencePlayer();
Player->Play();
Player->Pause();
Player->SetPlaybackPosition(FFrameTime(FrameNumber));
```

---

## Competitor Analysis

### MCP Unreal Engine Projects (Researched December 2024)

#### 1. [unreal-mcp (chongdashu)](https://github.com/chongdashu/unreal-mcp)
**Focus:** AI-assisted level editing via natural language

**Features:**
- Create/delete actors (cubes, spheres, lights, cameras)
- Set actor transforms (position, rotation, scale)
- Query actor properties and find by name
- **Blueprint creation** with custom components
- **Component configuration** (mesh, camera, light, physics)
- **Blueprint node graph** manipulation (events, functions, variables)
- Viewport focus and camera control
- **Input mapping** creation

**Ideas to Adopt:**
- [ ] Blueprint class creation
- [ ] Blueprint graph node manipulation
- [ ] Component configuration helpers
- [ ] Viewport camera control

#### 2. [Unreal_mcp (ChiR24)](https://github.com/ChiR24/Unreal_mcp)
**Focus:** Comprehensive automation bridge with TypeScript/C++/Rust

**Features:**
| Tool | Capabilities |
|------|--------------|
| `manage_asset` | Assets, Materials, Render Targets, Behavior Trees |
| `control_actor` | Spawn, delete, transform, physics, tags |
| `control_editor` | PIE, Camera, viewport, **screenshots** |
| `manage_level` | Load/Save, **World Partition**, streaming |
| `manage_lighting` | Spawn lights, global illumination, shadows, build lighting |
| `manage_performance` | Profiling, optimization, scalability settings |
| `animation_physics` | Animation blueprints, vehicle control, ragdoll physics |
| `manage_effect` | **Niagara**, Particles, Debug Shapes |
| `manage_blueprint` | Create, SCS, Graph Editing |
| `build_environment` | **Landscape**, Foliage, Procedural generation |
| `system_control` | UBT, Tests, Logs, Project Settings, CVars |
| `manage_sequence` | **Sequencer** and cinematics control |
| `inspect` | Object introspection |
| `manage_audio` | Audio assets & components |
| `manage_behavior_tree` | **Behavior tree** graph editing |
| `manage_input` | Enhanced input actions and contexts |

**Ideas to Adopt:**
- [ ] Viewport screenshots (HIGH PRIORITY)
- [ ] World Partition/streaming support
- [ ] Niagara particle system control
- [ ] Sequencer cinematics control
- [ ] Behavior tree editing
- [ ] Landscape/foliage generation
- [ ] Performance profiling access
- [ ] CVar manipulation

#### 3. [UE5-MCP Research](https://github.com/VedantRGosavi/UE5-MCP)
**Focus:** Research and best practices

**Key Findings:**
- Human-in-the-loop workflows superior to fully autonomous
- Version control checkpoints recommended before AI modifications
- Command scope limitations prevent asset overwrites
- Comprehensive API documentation improves AI context
- Log all executed commands for debugging

**Recommendations to Implement:**
- [ ] Command logging system
- [ ] Undo/checkpoint before batch operations
- [ ] Scope limitations for destructive operations
- [ ] Rich error messages with suggestions

### Feature Gap Analysis

| Feature | AgentBridge | chongdashu | ChiR24 | Priority |
|---------|-------------|------------|--------|----------|
| Actor manipulation | ✓ | ✓ | ✓ | - |
| Property access | ✓ | ✓ | ✓ | - |
| Type discovery | ✓ | ✓ | ✓ | - |
| DataAssets | ◯ | ✗ | ◯ | HIGH |
| Screenshots | ◯ | ✗ | ✓ | HIGH |
| Audio capture | ◯ | ✗ | ◯ | MEDIUM |
| Blueprint graphs | ✗ | ✓ | ✓ | MEDIUM |
| Niagara/VFX | ✗ | ✗ | ✓ | LOW |
| Sequencer | ✗ | ✗ | ✓ | LOW |
| Landscape | ✗ | ✗ | ✓ | LOW |
| Behavior trees | ✗ | ✗ | ✓ | LOW |

✓ = Implemented, ◯ = Planned, ✗ = Not implemented

### Our Unique Advantages

1. **Tempo Integration** - Native gRPC via TempoScripting (not HTTP/TCP)
2. **Context-aware** - PIE/Editor/Runtime capability detection
3. **Deep reflection** - Full FProperty/UFunction access
4. **Nested paths** - "Component.Mesh.Materials[0].Color" syntax
5. **12 services** - Unified access to Tempo's simulation features

---

## Research Notes

### Session Log

#### December 2024 - Initial Setup
- Phase 1-3 complete (Core, Tempo, MCP)
- 12 services, 64+ tools
- HTTP and gRPC dual transport

#### December 2024 - PIE/Runtime Support (Phase 4)
- Added FWorldContextCapabilities
- GetCapabilities command for agents
- AgentBridgeServer changed to Runtime module type
- Python client ContextCapabilities type

### Open Questions

1. **DataAsset Creation**: Can we create new DataAssets at runtime or only in editor?
2. **Audio Streaming**: What's the best approach for streaming audio to external processes?
3. **Large Level Support**: How do we handle levels with thousands of actors efficiently?
4. **Networked PIE**: How to handle multiple PIE instances in multiplayer testing?
5. **Asset Hot-Reload**: Can we reload assets without restarting?

### Technical Debt

- [ ] FunctionInvoker return values don't work for structs
- [ ] HTTP server body parsing null-termination issue (workaround in place)
- [ ] Need more robust error messages in MCP tools

---

## Future Use Case Ideas

### Content Creation
- Procedural level generation with constraints
- Automatic lighting setup
- Material variation generation
- LOD creation assistance

### Testing & QA
- Automated test recording
- Visual regression testing
- Performance profiling
- Collision checking

### AI/ML Integration
- Training data capture (images, states)
- Reinforcement learning environments
- Behavior tree authoring
- Dialogue system testing

### Collaboration
- Multi-agent level building
- Real-time streaming to external apps
- Voice-controlled editing
- Screen sharing with annotations

---

*Document Version: 1.0*
*Created: December 2024*
*Purpose: Research and planning for future features*
