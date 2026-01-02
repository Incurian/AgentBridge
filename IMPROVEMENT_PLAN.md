# AgentBridge Improvement Plan

> Generated: January 2, 2026
> Based on: TEST_RESULTS.md findings from FreshMap_1 testing session

## Philosophy Reminder

**"Users and agents should not need to know implementation details. Tools should just work."**

Every fix below should be invisible to the user. No new parameters, no special syntax, no workarounds to document.

---

## Issue Summary

| ID | Issue | Severity | Module | Status |
|----|-------|----------|--------|--------|
| UB-004 | TArray setting fails | HIGH | Scripting + MCP | **✅ FIXED** |
| UB-008 | get_property returns empty for numeric/struct | HIGH | Scripting + MCP | **✅ FIXED** |
| UB-006 | get_class_schema doesn't support structs | MEDIUM | Scripting | **✅ FIXED** |
| UB-007 | TArray schema missing element_type | MEDIUM | Core + Proto | **✅ FIXED** |
| UB-005 | DataAsset paths need `.AssetName` suffix | MEDIUM | MCP | **✅ FIXED** |
| ENH-002 | PCG workflow needs specific property guidance | LOW | MCP Help | **✅ FIXED** |

---

## Fix 1: TArray Property Setting (UB-004) - ✅ FIXED

### Root Cause

**Two issues were discovered:**

1. **Python MCP Layer:** `_normalize_property_value()` used Python's `str()` on lists, producing
   single quotes like `['a', 'b']`. JSON requires double quotes: `["a", "b"]`.

2. **C++ Scripting Layer:** `JsonToPropertyValue()` wasn't parsing JSON arrays - just storing
   the raw string. `WriteArrayProperty()` needed the value pre-parsed into `FAgentPropertyValue`
   with `Type = Array` and populated `ArrayValue`.

### Solution Implemented

**Python (`agentbridge.py`):**
```python
# Changed from str(value) to json.dumps(value)
else:
    # Other array - return as JSON string (NOT str() which uses single quotes!)
    return json.dumps(value)
```

**C++ (`CommandExecutor.cpp`):**
```cpp
// Added JSON array parsing in JsonToPropertyValue()
if (Json.StartsWith(TEXT("[")) && Json.EndsWith(TEXT("]")))
{
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (FJsonSerializer::Deserialize(Reader, ParsedJson) && ParsedJson->Type == EJson::Array)
    {
        Value.Type = EAgentPropertyType::Array;
        // Parse each element into Value.ArrayValue...
    }
}
```

**C++ (`PropertyAccessor.cpp`):**
```cpp
// WriteArrayProperty now dispatches to type-specific writers
if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(InnerProp))
    bElementSuccess = WriteObjectProperty(ElementPtr, ObjProp, ElementValue);
else if (FStructProperty* StructProp = CastField<FStructProperty>(InnerProp))
    bElementSuccess = WriteStructProperty(ElementPtr, StructProp, ElementValue);
// ... etc for other types
```

### Verified Working

```python
# Set Tags (TArray<FName>) on an actor:
set_property("ArrayTestCube", "Tags", '["TestTag1", "TestTag2"]')
# Result: Tags = [TestTag1, TestTag2] ✓
```

---

## Fix 2: GET Property Returns Empty (UB-008) - ✅ FIXED

### Root Cause

**Two issues discovered:**

1. **C++ gRPC Layer:** `CommandExecutor::Execute(FGetPropertyPathCommand)` was setting
   `Response.TypeName` to a numeric enum value like `"3"` instead of a string like `"Float"`.
   The downstream `JsonToProtoPropertyValue()` uses `TypeName.Contains("Float")` to match
   types, so numeric values never matched.

2. **Python MCP Layer:** The `get_property` handler was only extracting `result.value.string_value`,
   ignoring typed fields like `float_value`, `vector_value`, `bool_value`, etc.

### Solution Implemented

**C++ (`CommandExecutor.cpp`):**
```cpp
// Added helper function to convert enum to string names
static FString PropertyTypeToString(EAgentPropertyType Type)
{
    switch (Type)
    {
    case EAgentPropertyType::Bool:   return TEXT("Bool");
    case EAgentPropertyType::Float:  return TEXT("Float");
    case EAgentPropertyType::Vector: return TEXT("Vector");
    // ... etc
    }
}

// Changed from numeric cast to string name
Response.TypeName = PropertyTypeToString(Result.Value.Type);  // "Float" not "3"
```

**Python (`agentbridge.py`):**
```python
def _extract_property_value(prop_value) -> Any:
    """Extract typed value from PropertyValue proto."""
    ptype = prop_value.type
    if ptype == 1:  # BOOL
        return prop_value.bool_value
    elif ptype == 3:  # FLOAT
        return prop_value.float_value
    elif ptype == 6:  # VECTOR
        v = prop_value.vector_value
        return {"x": v.x, "y": v.y, "z": v.z}
    # ... etc

# Updated handler to use typed extraction
value = _extract_property_value(result.value)
return {"path": args["path"], "value": value, "type": result.type_name}
```

### Verified Working

```python
# All return proper typed values:
get_property("PointLight", "LightComponent0.Intensity")  # → 5000.0
get_property("Cube", "RootComponent.RelativeLocation")   # → {"x": 0, "y": 0, "z": 100}
get_property("Actor", "bHidden")                          # → False
```

---

## Fix 3: Struct Schema Support (UB-006) - ✅ FIXED

### Root Cause

`get_class_schema` only searched `UClass` via `FTypeDiscovery::FindClassByName()`. Structs are
`UScriptStruct`, not `UClass`, so they were never found.

### Solution Implemented

**C++ (`CommandExecutor.cpp`):**
```cpp
// Added helper function to find UScriptStruct by name
static UScriptStruct* FindStructByName(const FString& Name)
{
    // Try exact name, with F prefix, without F prefix
    // Falls back to TObjectIterator search for thorough lookup
}

// In Execute(FGetClassSchemaCommand):
// First try as UClass (existing behavior)
UClass* Class = FTypeDiscovery::FindClassByName(Command.ClassName);

// If not found, try as UScriptStruct
if (!Class)
{
    UScriptStruct* Struct = FindStructByName(Command.ClassName);
    if (Struct)
    {
        // Build schema from struct properties (no functions)
        // ...
    }
}
```

### Verified Working

```python
get_class_schema("BiomeAsset")
# → {"class_name": "FBiomeAsset", "properties": [
#      {"name": "Mesh", "type_name": "TSoftObjectPtr<UStaticMesh>"},
#      {"name": "Weight", "type_name": "float"},
#      ...
#    ]}
```

---

## Fix 4: TArray Element Type (UB-007) - ✅ FIXED

### Root Cause

`get_class_schema` returned `type_name: "TArray"` without indicating what type the array contains.

### Solution Implemented

**1. Added fields to `FAgentPropertyInfo` (`AgentBridgeTypes.h`):**
```cpp
struct FAgentPropertyInfo
{
    // ... existing fields ...
    FString ElementType;  // For TArray/TSet/TMap: inner type name
    FString KeyType;      // For TMap: key type name
};
```

**2. Updated `BuildPropertyInfo` (`TypeDiscovery.cpp`):**
```cpp
if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
{
    Info.ElementType = FPropertyAccessor::GetPropertyTypeName(ArrayProp->Inner);
}
else if (FSetProperty* SetProp = CastField<FSetProperty>(Property))
{
    Info.ElementType = FPropertyAccessor::GetPropertyTypeName(SetProp->ElementProp);
}
else if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
{
    Info.KeyType = FPropertyAccessor::GetPropertyTypeName(MapProp->KeyProp);
    Info.ElementType = FPropertyAccessor::GetPropertyTypeName(MapProp->ValueProp);
}
```

**3. Updated proto (`AgentBridge.proto`):**
```protobuf
message PropertyInfo {
    // ... existing fields ...
    string element_type = 9;  // For TArray/TSet/TMap
    string key_type = 10;     // For TMap
}
```

**4. Updated gRPC serialization (`AgentBridgeServiceSubsystem.cpp`):**
- Added `set_element_type()` and `set_key_type()` calls

### Verified Working

```python
get_class_schema("BiomeAssetTemplate")
# → properties: [{"name": "BiomeAssets", "type_name": "TArray", "element_type": "FBiomeAsset", ...}]
```

---

## Fix 5: DataAsset Path Auto-Resolution (UB-005) - ✅ FIXED

### Root Cause

Unreal asset paths have two forms:
- **Package path:** `/Game/Folder/MyAsset` (what users naturally type)
- **Object path:** `/Game/Folder/MyAsset.MyAsset` (what Unreal's API expects)

Users shouldn't need to know this implementation detail.

### Solution Implemented

**Python (`agentbridge.py`):**
```python
def _normalize_asset_path(path: str) -> str:
    """
    Auto-fix asset paths by adding the .AssetName suffix if missing.
    '/Game/Biomes/Forest' -> '/Game/Biomes/Forest.Forest'
    """
    if not path or not path.startswith("/"):
        return path  # Not an asset path

    final_part = path[path.rfind("/") + 1:]
    if "." in final_part:
        return path  # Already has object name

    return f"{path}.{final_part}"

# In get_property/set_property handlers:
actor_id = _normalize_asset_path(args["actor_id"])
result = safe_call(client.get_property, actor_id, args["path"])
if isinstance(result, dict) and "error" in result:
    # Fallback to original path if normalized fails
    if actor_id != args["actor_id"]:
        result = safe_call(client.get_property, args["actor_id"], args["path"])
```

### Verified Working

```python
# Both now work identically:
get_property("/Game/freshtest/TestBiome", "BiomeDefinition.BiomeName")
get_property("/Game/freshtest/TestBiome.TestBiome", "BiomeDefinition.BiomeName")
```

---

## Fix 6: PCG Workflow Property Guidance (ENH-002) - ✅ FIXED

### Problem

When sizing volumes to landscape, agents get 100+ properties and don't know which to use.

### Solution Implemented

Added dedicated help topics in `agentbridge.py` with focused property guidance:

**1. Enhanced "workflows" topic** with detailed PCG volume sizing steps:
- Step-by-step instructions for sizing BoxComponent volumes
- Key properties: `BoxExtent`, `RelativeScale3D`
- Warning that BoxExtent is HALF-SIZE
- Z margin recommendation for PCG spawn variation

**2. New help topics:**
- `help(topic="pcg_volume")` - Dedicated guide for PCG volume sizing
- `help(topic="volume_sizing")` - General BoxComponent sizing guide

### Key Guidance Added

```
KEY PROPERTIES ON BOXCOMPONENT:
- BoxExtent (FVector): HALF-SIZE in each axis
- RelativeScale3D (FVector): Set to 1,1,1 when using BoxExtent directly

COMMON MISTAKES:
- Using "BoxComponent" (class) instead of "Volume" (instance name)
- Forgetting BoxExtent is HALF-SIZE, not full size
- Not resetting RelativeScale3D when setting BoxExtent
- Forgetting Z margin for PCG spawn variation
```

### Verified Working

```python
help(topic="pcg_volume")   # Returns focused PCG volume guidance
help(topic="volume_sizing") # Returns general BoxComponent sizing
help(topic="workflows")     # Now includes detailed PCG section
```

---

## Implementation Priority

| Order | Fix | Effort | Unblocks |
|-------|-----|--------|----------|
| 1 | UB-004: TArray<UObject*> | High | PCG workflow completion |
| 2 | UB-008: GET returns empty | Medium | Symmetric read/write |
| 3 | UB-007: TArray element_type | Low | Better discovery |
| 4 | UB-006: Struct schema | Medium | Full introspection |
| 5 | UB-005: Asset path auto-fix | Low | Simpler DataAsset access |
| 6 | ENH-002: PCG property guidance | Low | Clearer workflows |

---

## Testing Checklist

After implementing fixes:

- [ ] `set_property("Actor", "ArrayOfObjects", "['/Game/Path']")` succeeds
- [ ] `get_property("Light", "LightComponent0.Intensity")` returns float value
- [ ] `get_class_schema("BiomeAsset")` returns struct properties
- [ ] TArray properties show `element_type` in schema
- [ ] `/Game/Foo/Asset` auto-resolves to `/Game/Foo/Asset.Asset`
- [ ] `help(topic="pcg_volume")` returns focused guidance

---

## Module Summary

| Module | Changes |
|--------|---------|
| **AgentBridgeCore** | Struct schema lookup, TArray element_type |
| **AgentBridgeRuntime** | Fix GET for float/vector/color, TArray<UObject*> SET |
| **Python MCP** | Path auto-resolution, GET fallback to Tempo, help topics |

---

*Plan owner: Claude Code session*
*Review: User approved priorities on 2026-01-02*
