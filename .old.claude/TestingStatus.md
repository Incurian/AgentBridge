# AgentBridge Testing Status

This document tracks all implemented features and their testing status.

**Legend:**
- **Tested (Editor)** - Tested with Unreal Editor running
- **Tested (Python)** - Python client tests pass (requires editor)
- **Untested** - Not yet tested
- **Needs Editor** - Cannot test autonomously, requires live editor
- **Code Review** - Code written, logic reviewed, not runtime tested

---

## Core Reflection (AgentBridgeCore)

| Feature | Status | Notes |
|---------|--------|-------|
| PropertyAccessor - Read primitives | Tested (Editor) | Bool, int, float, string, name, text |
| PropertyAccessor - Read vectors | Tested (Editor) | FVector, FRotator, FTransform |
| PropertyAccessor - Read arrays | Tested (Editor) | TArray of various types |
| PropertyAccessor - Read structs | Tested (Editor) | Nested struct traversal |
| PropertyAccessor - Read maps | Tested (Editor) | TMap with sparse indices |
| PropertyAccessor - Read objects | Tested (Editor) | UObject*, TObjectPtr, soft refs |
| PropertyAccessor - Write primitives | Tested (Editor) | Via SetPath command |
| PropertyAccessor - Write complex | Tested (Editor) | Arrays, structs |
| FunctionInvoker - Get signature | Tested (Editor) | Via DumpClass command |
| FunctionInvoker - Invoke function | Tested (Editor) | Via CallFunc command |
| FunctionInvoker - Return values | **Known Issue** | Returns default values, not actual |
| TypeDiscovery - Find class | Tested (Editor) | BP and C++ classes |
| TypeDiscovery - List properties | Tested (Editor) | Via DumpClass |

---

## Runtime Operations (AgentBridgeRuntime)

| Feature | Status | Notes |
|---------|--------|-------|
| WorldContextManager | Tested (Editor) | ListWorlds command |
| WorldContextManager - PIE | Tested (Editor) | Works in Play-In-Editor |
| ActorOperations - Query | Tested (Editor/Python) | QueryActors command |
| ActorOperations - Spawn | Tested (Editor/Python) | SpawnActor command |
| ActorOperations - Delete | Tested (Editor/Python) | DeleteActor command |
| ActorOperations - Transform | Tested (Editor/Python) | SetTransform command |
| AgentPropertyPath - Read | Tested (Editor) | GetPath command |
| AgentPropertyPath - Write | Tested (Editor) | SetPath command |
| AgentPropertyPath - Nested | Tested (Editor) | "Component.Property.X" |
| GetCapabilities | Tested (Editor) | Capabilities command |

---

## Console Commands (AgentBridgeDebug)

| Command | Status | Notes |
|---------|--------|-------|
| AgentBridge.DumpActor | Tested (Editor) | Property inspection |
| AgentBridge.DumpClass | Tested (Editor) | Class schema |
| AgentBridge.ListWorlds | Tested (Editor) | World enumeration |
| AgentBridge.Capabilities | Tested (Editor) | Context capabilities |
| AgentBridge.GetPath | Tested (Editor) | Property path read |
| AgentBridge.SetPath | Tested (Editor) | Property path write |
| AgentBridge.QueryActors | Tested (Editor) | Actor queries |
| AgentBridge.SpawnActor | Tested (Editor) | Actor spawning |
| AgentBridge.CallFunc | Tested (Editor) | Function calls |
| AgentBridge.ListMaterials | Code Review | Needs editor test |
| AgentBridge.GetMaterial | Code Review | Needs editor test |
| AgentBridge.SetMaterialParam | Code Review | Needs editor test |
| AgentBridge.ListPCG | Code Review | Needs editor test |

---

## HTTP Server (AgentBridgeServer)

| Feature | Status | Notes |
|---------|--------|-------|
| Health check | Tested (Python) | /health endpoint |
| ListWorlds | Tested (Python) | test_client.py |
| QueryActors | Tested (Python) | test_client.py |
| SpawnActor | Tested (Python) | test_client.py |
| DeleteActor | Tested (Python) | test_client.py |
| SetTransform | Tested (Python) | test_client.py |
| GetProperty | Tested (Python) | test_client.py |
| SetProperty | Tested (Python) | test_client.py |
| CallFunction | Tested (Python) | Returns default values |
| GetCapabilities | Tested (Python) | test_client.py |
| GetActorDetails | Tested (Python) | test_client.py |
| GetClassInfo | Tested (Python) | test_client.py |

---

## Python Client (agentbridge package)

| Method | Status | Notes |
|--------|--------|-------|
| health_check() | Tested | Works |
| list_worlds() | Tested | Works |
| query_actors() | Tested | Works |
| spawn_actor() | Tested | Works |
| delete_actor() | Tested | Works |
| set_actor_transform() | Tested | Works |
| get_property() | Tested | Works |
| set_property() | Tested | Works |
| call_function() | Tested | Returns default values |
| get_capabilities() | Tested | Works |
| get_actor_details() | Tested | Works |
| get_class_info() | Tested | Works |
| get_actor_location() | Tested | Workaround for CallFunc issue |
| list_data_assets() | Code Review | Needs editor test |
| get_data_asset_details() | Code Review | Needs editor test |
| get_data_table_rows() | Code Review | Needs editor test |
| capture_viewport() | Code Review | Needs editor test |
| capture_scene() | Code Review | Needs editor test |
| analyze_audio_at_location() | Code Review | Needs editor test |
| capture_audio() | Code Review | Needs editor test |
| list_materials() | Code Review | Needs editor test |
| get_material_info() | Code Review | Needs editor test |
| create_material_instance() | Code Review | Needs editor test |
| set_material_parameter() | Code Review | Needs editor test |
| apply_material_to_actor() | Code Review | Needs editor test |
| list_pcg_actors() | Code Review | Needs editor test |
| regenerate_pcg() | Code Review | Needs editor test |
| set_pcg_parameter() | Code Review | Needs editor test |
| get_cvar() | Code Review | Needs editor test |
| set_cvar() | Code Review | Needs editor test |
| list_cvars() | Code Review | Needs editor test |

---

## gRPC Server (AgentBridgeServiceSubsystem)

| Feature | Status | Notes |
|---------|--------|-------|
| ListWorlds | Needs Editor | test_grpc.py ready |
| SetTargetWorld | Needs Editor | test_grpc.py ready |
| QueryActors | Needs Editor | test_grpc.py ready |
| GetActor | Needs Editor | test_grpc.py ready |
| SpawnActor | Needs Editor | test_grpc.py ready |
| DeleteActor | Needs Editor | test_grpc.py ready |
| SetActorTransform | Needs Editor | test_grpc.py ready |
| SetActorProperties | Needs Editor | test_grpc.py ready |
| GetPropertyPath | Needs Editor | test_grpc.py ready |
| SetPropertyPath | Needs Editor | test_grpc.py ready |
| CallFunction | Needs Editor | Known issue with return values |
| FindClass | Needs Editor | test_grpc.py ready |
| GetClassSchema | Needs Editor | test_grpc.py ready |
| ListClasses | Needs Editor | test_grpc.py ready |
| Value Conversions | Code Complete | JSON↔Proto in both directions |

### gRPC Value Conversion (December 31, 2025)

Added full bidirectional value conversion between JSON (used by CommandExecutor) and Protobuf PropertyValue types:

**JSON → Proto (`JsonToProtoPropertyValue`):**
- Bool, Int, Float, String types
- Vector (`{"X":..., "Y":..., "Z":...}`)
- Rotator (`{"Pitch":..., "Yaw":..., "Roll":...}`)
- Transform (nested Location, Rotation, Scale)
- Color (`{"r":..., "g":..., "b":..., "a":...}`)
- Arrays (recursively converts elements)
- Structs/Maps (converts to key-value pairs)

**Proto → JSON (`ProtoPropertyValueToJson`):**
- All PropertyValue types to JSON string format
- Used for SetActorProperties, SetPropertyPath, CallFunction parameters

---

## Extended Features

### DataAsset Support

| Feature | C++ Implementation | HTTP Endpoint | Python Client | Console Cmd |
|---------|-------------------|---------------|---------------|-------------|
| List data assets | Done | Done | Done | Code Review |
| Get asset details | Done | Done | Done | Code Review |
| Query data table | Done | Done | Done | N/A |

### Viewport/Scene Capture

| Feature | C++ Implementation | HTTP Endpoint | Python Client | Console Cmd |
|---------|-------------------|---------------|---------------|-------------|
| Capture viewport | Done | Done | Done | Code Review |
| Capture scene | Done | Done | Done | N/A |

### Audio Capture

| Feature | C++ Implementation | HTTP Endpoint | Python Client | Console Cmd |
|---------|-------------------|---------------|---------------|-------------|
| Analyze location | Done | Done | Done | N/A |
| Capture audio | Done | Done | Done | N/A |

### Material Operations

| Feature | C++ Implementation | HTTP Endpoint | Python Client | Console Cmd |
|---------|-------------------|---------------|---------------|-------------|
| List materials | Done | Done | Done | Done |
| Get material info | Done | Done | Done | Done |
| Create instance | Done | Done | Done | N/A |
| Set parameter | Done | Done | Done | Done |
| Apply to actor | Done | Done | Done | N/A |

### PCG Operations

| Feature | C++ Implementation | HTTP Endpoint | Python Client | Console Cmd |
|---------|-------------------|---------------|---------------|-------------|
| List PCG actors | Done | Done | Done | Done |
| Regenerate | Done | Done | Done | N/A |
| Set parameter | Done | Done | Done | N/A |

### Console Variable (CVar) Operations

| Feature | C++ Implementation | HTTP Endpoint | Python Client | Console Cmd |
|---------|-------------------|---------------|---------------|-------------|
| Get CVar | Done | Pending | Done | Done |
| Set CVar | Done | Pending | Done | Done |
| List CVars | Done | Pending | Done | Done |

---

## Known Issues

1. **FunctionInvoker Return Values**: Function calls return default values instead of actual return values. Workaround: use property queries instead.

2. **Material Commands**: New console commands (ListMaterials, GetMaterial, SetMaterialParam, ListPCG) need live editor testing.

3. **Extended Python Methods**: All new Python client methods for DataAssets, Capture, Audio, Materials, PCG, and CVars need live editor testing.

4. **CVar HTTP Endpoints**: HTTP endpoint handlers for CVar operations not yet implemented (console commands work).



---

## Build Verification (December 31, 2025)

**Issues Found and Fixed:**

1. **Name Collision (FIXED)**: `EMaterialParameterType` and `FMaterialParameterInfo` collided with UE types. Renamed to `EAgentMaterialParamType` and `FAgentMaterialParamInfo`.

2. **UE 5.6 API Change (FIXED)**: `IImageWrapper::GetCompressed()` now returns data directly instead of output parameter.

3. **Type Mismatch (FIXED)**: `UMaterialInstanceDynamic::Create()` ternary fixed with explicit `UObject*` cast.

4. **Argument Order (FIXED)**: `FPropertyAccessor::ReadProperty()` calls corrected to (Container, Property).

5. **Const Qualification (FIXED)**: `DataTable->GetRowStruct()` returns `const UScriptStruct*`.

**Build Status**: PASSED (UE 5.6 / TempoSample)

---

## Testing Requirements

### Autonomous Testing (No Editor)
- Python type definitions compile
- Python client methods have correct signatures
- Code review for logic errors

### Editor Testing (Manual)
- Console commands work correctly
- HTTP endpoints return valid data (test_client.py)
- gRPC endpoints return valid data (test_grpc.py)
- Python client methods succeed with editor running

### Test Scripts Available
| Script | Protocol | Command |
|--------|----------|---------|
| `test_client.py` | HTTP/JSON | `python test_client.py` |
| `test_grpc.py` | gRPC/Protobuf | `python test_grpc.py [--port 50051]` |

### Full Integration Testing
- MCP server exposes tools correctly
- Claude Code can use tools effectively
- End-to-end agent workflows work

---

*Last Updated: December 31, 2025*
