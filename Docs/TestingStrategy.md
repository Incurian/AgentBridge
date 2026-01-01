# AgentBridge Testing Strategy

> Comprehensive manual testing guide for validating AgentBridge features across Editor, PIE, and Runtime contexts.
> **Important:** Test artifacts are NOT auto-cleaned so you can inspect them.

---

## Table of Contents

1. [Testing Overview](#testing-overview)
2. [Environment Setup](#environment-setup)
3. [Automated Build-Run-Test Workflow](#automated-build-run-test-workflow)
4. [Phase 1: Console Command Testing](#phase-1-console-command-testing)
5. [Phase 2: HTTP Client Testing](#phase-2-http-client-testing)
6. [Phase 3: gRPC Client Testing](#phase-3-grpc-client-testing)
7. [Phase 4: MCP Integration Testing](#phase-4-mcp-integration-testing)
8. [Phase 5: Context-Specific Testing](#phase-5-context-specific-testing)
9. [Phase 6: Wishlist Features Testing](#phase-6-wishlist-features-testing)
10. [Test Artifacts Cleanup](#test-artifacts-cleanup)

---

## Testing Overview

### Test Categories

| Category | Protocol | Port | Test Script | Context |
|----------|----------|------|-------------|---------|
| Console Commands | N/A | N/A | Manual in Editor | Editor |
| HTTP API | HTTP/JSON | 8080 | `test_client.py` | Editor |
| gRPC API | gRPC/Protobuf | 10001 | `test_grpc.py` | Editor/PIE |
| MCP Tools | gRPC (via MCP) | 10001 | Manual/Claude | Editor/PIE |
| Wishlist | gRPC | 10001 | `test_wishlist.py` | Editor |

### What Gets Tested

- **Core Reflection:** Property access, function invocation, type discovery
- **Actor Operations:** Spawn, delete, transform, query
- **Property Paths:** Nested paths like `LightComponent.Intensity`
- **Materials:** List, inspect, create instances, set parameters
- **PCG:** List actors, regenerate graphs, set parameters
- **CVars:** Get/set console variables
- **DataAssets:** List, inspect, query data tables
- **Capture:** Viewport screenshots, SceneCapture, audio
- **Context Handling:** Editor vs PIE vs Packaged differences

---

## Environment Setup

### Prerequisites

1. **Unreal Editor** running with TempoSample project
2. **Python 3.8+** with dependencies:
   ```bash
   cd D:/tempo/TempoSample/Plugins/AgentBridge/Python
   pip install -r requirements.txt
   ```
3. **gRPC server active** (Tempo default port 10001)
4. **HTTP server active** (AgentBridge port 8080)

### Verify Servers Are Running

```bash
# Check HTTP server
curl http://localhost:8080/health

# Check gRPC server (Python)
python -c "import grpc; ch = grpc.insecure_channel('localhost:10001'); grpc.channel_ready_future(ch).result(timeout=5); print('OK')"
```

---

## Automated Build-Run-Test Workflow

This section documents the complete automated workflow for building, running, testing, and terminating the editor from the command line.

### Quick Reference

```bash
# 1. Verify TempoEnv Python
D:/tempo/TempoSample/TempoEnv/Scripts/python.exe --version
# Expected: Python 3.11.8

# 2. Build the project (~60 seconds)
cd D:/tempo/TempoSample
./Plugins/Tempo/Scripts/Build.sh

# 3. Run editor in background
./Plugins/Tempo/Scripts/Run.sh &

# 4. Wait for gRPC server (~30 seconds)
sleep 30

# 5. Test gRPC connection
PYTHONPATH="D:/tempo/TempoSample/Plugins/Tempo/TempoCore/Content/Python/API/tempo" \
  D:/tempo/TempoSample/TempoEnv/Scripts/python.exe -c "
import grpc
channel = grpc.insecure_channel('localhost:10001')
grpc.channel_ready_future(channel).result(timeout=5)
print('gRPC server is UP')
"

# 6. Force-quit editor (when done)
# IMPORTANT: Use cmd //c wrapper in Git Bash to avoid /F path interpretation
cmd //c "taskkill /F /IM UnrealEditor-Cmd.exe"
```

### Key Findings

| Step | Tool | Time | Notes |
|------|------|------|-------|
| Build | `Build.sh` | ~60s | Runs UBT, regenerates protos |
| Startup | `Run.sh` | ~30s | Uses `UnrealEditor-Cmd.exe` (headless) |
| gRPC Ready | Port 10001 | ~30s after Run.sh | Tempo gRPC server |
| Quit (graceful) | `tempo_quit` MCP tool | Variable | May block on save dialog |
| Quit (force) | `taskkill /F` | Immediate | Discards unsaved changes |

### Important Gotchas

1. **TempoEnv Required**: Always use `D:/tempo/TempoSample/TempoEnv/Scripts/python.exe`, not system Python
   - grpcio 1.62.2 and protobuf 4.25.3 are pre-installed
   - System Python may have incompatible versions

2. **Git Bash `/F` Flag Issue**: In Git Bash, `/F` is interpreted as a path
   ```bash
   # WRONG - Git Bash interprets /F as path
   taskkill /F /PID 12345

   # CORRECT - Use cmd wrapper
   cmd //c "taskkill /F /PID 12345"
   ```

3. **tempo_quit May Not Force-Terminate**: If there are unsaved changes, `tempo_quit` returns success but the editor may block waiting for user confirmation. Use `taskkill /F` for guaranteed termination.

4. **name_pattern vs label**: When using `query_actors`, the `name_pattern` matches internal UE names (like `PointLight_UAID_...`), NOT the human-readable label. Use `class_name` filter or `get_actor` by label instead.

### Python Test Script Example

```python
#!/usr/bin/env python3
"""Quick MCP connectivity test"""
import sys
sys.path.insert(0, "D:/tempo/TempoSample/Plugins/Tempo/TempoCore/Content/Python/API/tempo")
sys.path.insert(0, "D:/tempo/TempoSample/Plugins/AgentBridge/Python")

from mcp.services.agentbridge import connect, execute

client = connect('localhost', 10001)

# List worlds
result = execute(client, 'list_worlds', {})
print(f"Worlds: {result}")

# Spawn test actor
result = execute(client, 'spawn_actor', {
    'class_name': 'PointLight',
    'location': [0, 0, 500],
    'label': 'AutoTest_Light'
})
print(f"Spawned: {result}")

# Query to verify
result = execute(client, 'query_actors', {'class_name': 'PointLight', 'limit': 5})
print(f"PointLights: {result}")
```

---

## Phase 1: Console Command Testing

### Test in Editor Console (`~` key)

Test these commands directly in the Unreal Editor console. All artifacts will remain in the world for inspection.

#### 1.1 World & Actor Discovery

```
AgentBridge.ListWorlds
AgentBridge.Capabilities
AgentBridge.QueryActors Light 10
AgentBridge.QueryActors * 20
```

**Expected Results:**
- ListWorlds shows Editor world with actor count
- Capabilities shows: `SupportsPropertyIteration: true`, `SupportsTransactions: true`
- QueryActors returns matching actors with names and classes

#### 1.2 Actor Inspection

```
AgentBridge.DumpActor Floor 2
AgentBridge.DumpClass PointLight
AgentBridge.DumpClass StaticMeshActor
```

**Expected Results:**
- DumpActor shows properties recursively to depth 2
- DumpClass shows class schema with properties and functions

#### 1.3 Property Path Operations

```
AgentBridge.GetPath Floor RelativeLocation
AgentBridge.GetPath Floor RelativeLocation.X
AgentBridge.SetPath Floor RelativeScale3D.X 2.0
AgentBridge.GetPath Floor RelativeScale3D
```

**Expected Results:**
- GetPath returns FVector for RelativeLocation
- SetPath modifies scale (visible in viewport)
- Floor actor visually scales in X direction

**Artifact to Inspect:** Floor actor scale change (undo with Ctrl+Z or leave for inspection)

#### 1.4 Spawn Actor (NO AUTO-CLEANUP)

```
AgentBridge.SpawnActor PointLight 0 0 500 TestLight_Console
AgentBridge.QueryActors TestLight_Console 1
AgentBridge.DumpActor TestLight_Console 1
```

**Expected Results:**
- New PointLight spawned at (0, 0, 500)
- Visible in World Outliner as "TestLight_Console"
- QueryActors finds it

**Artifact to Inspect:** `TestLight_Console` actor in World Outliner

#### 1.5 Function Invocation

```
AgentBridge.CallFunc TestLight_Console K2_GetActorLocation
AgentBridge.CallFunc TestLight_Console GetActorRotation
```

**Expected Results:**
- Returns location (may show default values due to known issue)
- Known limitation: struct return values may be zeroed

#### 1.6 Material Operations

```
AgentBridge.ListMaterials /Game 20
AgentBridge.GetMaterial /Engine/BasicShapes/BasicShapeMaterial
```

**Expected Results:**
- Lists materials matching path filter
- Shows material parameters for BasicShapeMaterial

#### 1.7 PCG Operations (if PCG actors exist)

```
AgentBridge.ListPCG *
```

**Expected Results:**
- Lists any PCG actors in the level

#### 1.8 CVar Operations

```
AgentBridge.GetCVar r.ScreenPercentage
AgentBridge.SetCVar r.ScreenPercentage 75
AgentBridge.GetCVar r.ScreenPercentage
AgentBridge.ListCVars Shadow 10
```

**Expected Results:**
- Gets current screen percentage value
- Sets it to 75 (visible quality change)
- ListCVars shows shadow-related CVars

**Artifact to Inspect:** Visual quality change from CVar modification

---

## Phase 2: HTTP Client Testing

### 2.1 Automated Test Suite

```bash
cd D:/tempo/TempoSample/Plugins/AgentBridge/Python
python test_client.py
```

**Note:** The automated test DOES delete its test actor. Run manual tests below to leave artifacts.

### 2.2 Manual HTTP Tests (NO AUTO-CLEANUP)

Open Python REPL:

```bash
cd D:/tempo/TempoSample/Plugins/AgentBridge/Python
python
```

```python
from agentbridge import AgentBridgeClient

client = AgentBridgeClient()

# Health check
print("Health:", client.health_check())

# List worlds
worlds = client.list_worlds()
for w in worlds:
    print(f"  {w.world_type}: {w.world_name} ({w.actor_count} actors)")

# Query existing lights
lights = client.query_actors(name_pattern="Light", limit=5)
for light in lights:
    print(f"  {light.label} at {light.location.to_tuple()}")

# Spawn test actors (WILL NOT BE DELETED)
test_light = client.spawn_actor("PointLight", location=(100, 0, 300), label="HTTP_TestLight")
print(f"Spawned: {test_light.label} GUID: {test_light.guid}")

test_mesh = client.spawn_actor("StaticMeshActor", location=(200, 0, 100), label="HTTP_TestMesh")
print(f"Spawned: {test_mesh.label}")

# Move the light
client.set_actor_transform("HTTP_TestLight", location=(100, 100, 400))
print("Moved HTTP_TestLight to (100, 100, 400)")

# List classes
classes = client.list_classes(base_class_name="Light", limit=10)
for cls in classes:
    print(f"  {cls.class_name}")

# Exit without cleanup
print("\n=== Test artifacts remain in world for inspection ===")
print("Artifacts: HTTP_TestLight, HTTP_TestMesh")
```

**Artifacts to Inspect:**
- `HTTP_TestLight` - PointLight at (100, 100, 400)
- `HTTP_TestMesh` - StaticMeshActor at (200, 0, 100)

---

## Phase 3: gRPC Client Testing

### 3.1 Automated Test Suite

```bash
cd D:/tempo/TempoSample/Plugins/AgentBridge/Python
python test_grpc.py
```

**Note:** The automated test DOES delete its test actor. Run manual tests below to leave artifacts.

### 3.2 Manual gRPC Tests (NO AUTO-CLEANUP)

```bash
cd D:/tempo/TempoSample/Plugins/AgentBridge/Python
python
```

```python
import sys
sys.path.insert(0, "D:/tempo/TempoSample/Plugins/Tempo/TempoCore/Content/Python/API/tempo")

import grpc
from AgentBridgeServer import AgentBridge_pb2 as pb
from AgentBridgeServer import AgentBridge_pb2_grpc as pb_grpc
from TempoScripting import Geometry_pb2

# Connect
channel = grpc.insecure_channel("localhost:10001")
stub = pb_grpc.AgentBridgeServiceStub(channel)

# List worlds
response = stub.ListWorlds(pb.ListWorldsRequest())
for w in response.worlds:
    print(f"World: {w.world_type} - {w.world_name} ({w.actor_count} actors)")

# Query actors
response = stub.QueryActors(pb.QueryActorsRequest(name_pattern="*Light*", limit=5))
print(f"Found {response.total_count} lights")

# Spawn test actor (WILL NOT BE DELETED)
transform = pb.ActorTransform(
    location=Geometry_pb2.Vector(x=300, y=0, z=400),
    rotation=Geometry_pb2.Rotation(p=0, y=0, r=0),
    scale=pb.Scale(x=1, y=1, z=1),
)
response = stub.SpawnActor(pb.SpawnActorRequest(
    class_name="PointLight",
    transform=transform,
    label="gRPC_TestLight",
))
print(f"Spawned: {response.spawned_actor.label}")

# Get property
response = stub.GetPropertyPath(pb.GetPropertyPathRequest(
    actor_id="gRPC_TestLight",
    path="LightComponent.Intensity",
))
print(f"Intensity type: {response.type_name}")

# Set property
value = pb.PropertyValue(type=pb.PROPERTY_TYPE_FLOAT, float_value=10000.0)
stub.SetPropertyPath(pb.SetPropertyPathRequest(
    actor_id="gRPC_TestLight",
    path="LightComponent.Intensity",
    value=value,
))
print("Set intensity to 10000")

# Move actor
new_transform = pb.ActorTransform(
    location=Geometry_pb2.Vector(x=300, y=200, z=500),
)
stub.SetActorTransform(pb.SetActorTransformRequest(
    actor_id="gRPC_TestLight",
    transform=new_transform,
))
print("Moved to (300, 200, 500)")

# List classes
response = stub.ListClasses(pb.ListClassesRequest(
    base_class_name="Light",
    include_blueprint=True,
    limit=10,
))
for cls in response.classes:
    print(f"  {cls.class_name}")

print("\n=== Test artifacts remain in world for inspection ===")
print("Artifact: gRPC_TestLight at (300, 200, 500) with intensity 10000")
```

**Artifacts to Inspect:**
- `gRPC_TestLight` - PointLight at (300, 200, 500) with high intensity

---

## Phase 4: MCP Integration Testing

### 4.1 Setup MCP Server

Ensure Claude Code has the MCP server configured in `~/.claude/settings.json`:

```json
{
  "mcpServers": {
    "agentbridge": {
      "command": "python",
      "args": ["-m", "mcp"],
      "cwd": "D:/tempo/TempoSample/Plugins/AgentBridge/Python",
      "env": {
        "PYTHONPATH": "D:/tempo/TempoSample/Plugins/Tempo/TempoCore/Content/Python/API/tempo"
      }
    }
  }
}
```

### 4.2 Manual MCP Tool Testing

Test these commands via Claude or direct MCP calls. Artifacts remain for inspection.

#### World Operations
- "List all available worlds"
- "What capabilities does the current context have?"

#### Actor Discovery
- "Find all lights in the scene"
- "Query actors with 'Test' in their name"
- "Get detailed info about the Floor actor"

#### Actor Manipulation (NO AUTO-CLEANUP)
- "Spawn a PointLight at position 500, 500, 200 named 'MCP_TestLight'"
- "Move MCP_TestLight to 600, 600, 300"
- "Spawn a StaticMeshActor at 0, 500, 100 named 'MCP_TestMesh'"

#### Property Operations
- "Get the intensity of MCP_TestLight"
- "Set MCP_TestLight intensity to 15000"
- "Get the LightComponent.LightColor of MCP_TestLight"

#### Type Discovery
- "List all Light classes"
- "Get the schema for PointLightComponent"

**Artifacts to Inspect:**
- `MCP_TestLight` - PointLight at (600, 600, 300)
- `MCP_TestMesh` - StaticMeshActor at (0, 500, 100)

---

## Phase 5: Context-Specific Testing

### 5.1 Editor Context Testing

All tests in Phases 1-4 run in Editor context by default.

**Verify Editor Capabilities:**
```
AgentBridge.Capabilities
```

Expected:
- `SupportsPropertyIteration: true`
- `SupportsFunctionInvocation: true`
- `SupportsSpawnDelete: true`
- `SupportsTransactions: true` (Editor only!)
- `SupportsPropertyMetadata: true`

**Test Undo/Redo (Editor only):**
1. Spawn an actor via console
2. Press Ctrl+Z to undo
3. Verify actor is removed
4. Press Ctrl+Y to redo
5. Verify actor is restored

### 5.2 PIE Context Testing

**Setup:**
1. Start Play-In-Editor (Alt+P or Play button)
2. Open console (`~`)

**Test Commands in PIE:**
```
AgentBridge.ListWorlds
AgentBridge.Capabilities
AgentBridge.QueryActors * 10
AgentBridge.SpawnActor PointLight 0 0 200 PIE_TestLight
```

**Verify PIE Capabilities:**
Expected:
- `SupportsTransactions: false` (No undo in PIE!)
- `SupportsPropertyMetadata: true` (GIsEditor is still true)
- World type shows "PIE"

**Test gRPC in PIE:**
```python
# In Python, while PIE is running
stub.ListWorlds(pb.ListWorldsRequest())
# Should show both Editor and PIE worlds
```

**PIE-Specific Tests:**
1. Spawn actor in PIE
2. Verify it appears in PIE world
3. Stop PIE
4. Verify actor is gone (PIE world destroyed)

**Artifacts to Inspect (PIE only - disappear when PIE stops):**
- `PIE_TestLight` - Only exists while PIE is running

### 5.3 Simulate Mode Testing

**Setup:**
1. Click "Simulate" in Editor (Alt+S)
2. Open console

**Test Commands in Simulate:**
```
AgentBridge.ListWorlds
AgentBridge.Capabilities
```

**Verify Simulate Behavior:**
- Should show same capabilities as PIE
- Actors spawned persist in Editor world (unlike PIE)

### 5.4 Target World Switching

**Test switching between worlds (when PIE is running):**

```python
# Start PIE first, then:
stub.ListWorlds(pb.ListWorldsRequest())  # Shows both worlds

# Target Editor world
stub.SetTargetWorld(pb.SetTargetWorldRequest(world_identifier="editor"))
stub.SpawnActor(...)  # Spawns in Editor

# Target PIE world
stub.SetTargetWorld(pb.SetTargetWorldRequest(world_identifier="pie"))
stub.SpawnActor(...)  # Spawns in PIE
```

---

## Phase 6: Advanced Feature Testing

### 6.1 Material Operations (NO AUTO-CLEANUP)

**Console:**
```
AgentBridge.ListMaterials /Game 20
AgentBridge.GetMaterial /Engine/BasicShapes/BasicShapeMaterial
```

**Python (creates persistent material instance):**
```python
from agentbridge import AgentBridgeClient
client = AgentBridgeClient()

# List materials
materials = client.list_materials(filter_pattern="Wood", limit=20)
for m in materials:
    print(f"  {m.path}")

# Get material info
info = client.get_material_info("/Engine/BasicShapes/BasicShapeMaterial")
print(f"Material: {info.name}")
for param in info.scalar_parameters:
    print(f"  Scalar: {param.name} = {param.value}")

# Create material instance (if you have a base material)
# instance = client.create_material_instance("/Game/Materials/M_Base", "TestInstance")
```

### 6.2 PCG Operations

**Console (if PCG actors exist):**
```
AgentBridge.ListPCG Forest
AgentBridge.RegeneratePCG PCGActor_Name
```

### 6.3 CVar Manipulation

**Console:**
```
AgentBridge.ListCVars r.Shadow 10
AgentBridge.GetCVar r.ShadowQuality
AgentBridge.SetCVar r.ShadowQuality 3
```

**Artifact to Inspect:** Visual quality changes from modified CVars

### 6.4 DataAsset Operations

Test via HTTP client:
```python
# List data assets
assets = client.list_data_assets(path_filter="/Game/Data", limit=20)

# Get data table info
table_info = client.get_data_table("/Game/Data/MyDataTable")
```

---

## Phase 6: Wishlist Features Testing

Tests for the new asset, component, and file operations added in Sessions 9-10.

### 6.1 Automated Test Script

```bash
cd D:/tempo/TempoSample/Plugins/AgentBridge/Python
PYTHONPATH="D:/tempo/TempoSample/Plugins/Tempo/TempoCore/Content/Python/API/tempo" \
    D:/tempo/TempoSample/TempoEnv/Scripts/python.exe test_wishlist.py
```

This runs automated tests for:
- File operations (read/write/list/copy)
- Component transforms (get/set)
- Actor attachment (attach/detach)
- Asset operations (create/thumbnail)

### 6.2 Manual File Operations

```python
# Write a file
result = execute(client, "write_project_file", {
    "relative_path": "Saved/Test.txt",
    "content": "Hello World",
})

# Read it back
result = execute(client, "read_project_file", {
    "relative_path": "Saved/Test.txt",
})

# List directory
result = execute(client, "list_project_directory", {
    "relative_path": "Content",
    "pattern": "*.uasset",
    "limit": 20,
})
```

**Security checks:**
- Paths with `..` should be rejected
- Paths in `Binaries/` should be rejected
- Files with `.exe` extension should be rejected

### 6.3 Manual Component Operations

```python
# Spawn test actor
execute(client, "spawn_actor", {
    "class_name": "PointLight",
    "location": [0, 0, 500],
    "label": "TestLight",
})

# Get component transform
result = execute(client, "get_component_transform", {
    "actor_id": "TestLight",
    "component_name": "LightComponent0",
    "world_space": True,
})

# Set component transform (relative offset)
execute(client, "set_component_transform", {
    "actor_id": "TestLight",
    "component_name": "LightComponent0",
    "location": [50, 0, 0],
    "world_space": False,
})
```

### 6.4 Manual Actor Attachment

```python
# Spawn parent and child
execute(client, "spawn_actor", {
    "class_name": "StaticMeshActor", "location": [0, 0, 100], "label": "Parent"
})
execute(client, "spawn_actor", {
    "class_name": "PointLight", "location": [0, 0, 200], "label": "Child"
})

# Attach child to parent
execute(client, "attach_actor", {
    "child_actor_id": "Child",
    "parent_actor_id": "Parent",
    "location_rule": "keep_world",
})

# Detach child
execute(client, "detach_actor", {
    "actor_id": "Child",
})

# Cleanup
execute(client, "delete_actor", {"actor_id": "Child"})
execute(client, "delete_actor", {"actor_id": "Parent"})
```

### 6.5 Manual Asset Operations

```python
# Create a DataAsset (editor only)
result = execute(client, "create_asset", {
    "asset_class": "DataAsset",
    "package_path": "/Game/Test",
    "asset_name": "TestData",
})

# Get asset thumbnail
result = execute(client, "get_asset_thumbnail", {
    "asset_path": "/Engine/BasicShapes/Cube",
    "width": 128,
    "height": 128,
})
print(f"Image data: {len(result['image_data'])} base64 chars")
```

---

## Test Artifacts Cleanup

When you're done inspecting, clean up test artifacts manually:

### Option 1: Editor Console
```
# Find and delete test actors one by one
AgentBridge.QueryActors Test 50
```
Then select and delete in World Outliner.

### Option 2: Python Script

```python
from agentbridge import AgentBridgeClient
client = AgentBridgeClient()

# List all test artifacts
test_actors = [
    "TestLight_Console",
    "HTTP_TestLight",
    "HTTP_TestMesh",
    "gRPC_TestLight",
    "MCP_TestLight",
    "MCP_TestMesh",
]

for actor_name in test_actors:
    try:
        client.delete_actor(actor_name)
        print(f"Deleted: {actor_name}")
    except Exception as e:
        print(f"Not found or error: {actor_name}")

print("Cleanup complete!")
```

### Option 3: World Outliner
1. Search for "Test" in World Outliner
2. Select all matching actors
3. Delete (Del key)

### Option 4: Don't Save Level
Simply close the level without saving to discard all changes.

---

## Test Checklist

Use this checklist to track your testing progress:

### Console Commands
- [ ] ListWorlds
- [ ] Capabilities
- [ ] DumpActor
- [ ] DumpClass
- [ ] GetPath / SetPath
- [ ] QueryActors
- [ ] SpawnActor
- [ ] CallFunc
- [ ] ListMaterials / GetMaterial
- [ ] ListPCG
- [ ] GetCVar / SetCVar / ListCVars

### HTTP API
- [ ] Health check
- [ ] List worlds
- [ ] Query actors
- [ ] Spawn actor
- [ ] Set transform
- [ ] Delete actor
- [ ] List classes

### gRPC API
- [ ] ListWorlds
- [ ] QueryActors
- [ ] GetActor
- [ ] SpawnActor
- [ ] SetActorTransform
- [ ] GetPropertyPath
- [ ] SetPropertyPath
- [ ] ListClasses
- [ ] CallFunction
- [ ] DeleteActor

### Context Testing
- [ ] Editor capabilities verified
- [ ] PIE capabilities verified
- [ ] Transactions work in Editor
- [ ] Transactions fail gracefully in PIE
- [ ] World switching works
- [ ] PIE actors cleanup on stop

### Advanced Features
- [ ] Material listing
- [ ] Material parameter modification
- [ ] CVar get/set
- [ ] CVar listing

---

## Troubleshooting

### Server Not Responding

**HTTP (8080):**
```bash
# Check if server is running
curl http://localhost:8080/health

# Check Unreal log for errors
# D:\tempo\TempoSample\Saved\Logs\TempoSample.log
```

**gRPC (10001):**
```python
import grpc
channel = grpc.insecure_channel("localhost:10001")
try:
    grpc.channel_ready_future(channel).result(timeout=5)
    print("gRPC server is up")
except grpc.FutureTimeoutError:
    print("gRPC server not responding")
```

### Actor Not Found

1. Check exact spelling (case-sensitive)
2. Use QueryActors to find similar names
3. Try using GUID instead of name
4. Check if actor is in correct world (Editor vs PIE)

### Property Path Not Working

1. Use DumpActor to see available properties
2. Check component name: `LightComponent.Intensity` not `Light.Intensity`
3. Some properties are read-only

### Function Return Values

Known issue: Struct return values return defaults. Use property paths as workaround:
```
# Instead of:
AgentBridge.CallFunc MyActor K2_GetActorLocation

# Use:
AgentBridge.GetPath MyActor RootComponent.RelativeLocation
```

---

*Document Version: 1.0*
*Created: December 31, 2025*
*Purpose: Comprehensive manual testing with artifact inspection*
