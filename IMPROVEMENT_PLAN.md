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
| UB-006 | get_class_schema doesn't support structs | MEDIUM | Core | Pending |
| UB-007 | TArray schema missing element_type | MEDIUM | Core | Pending |
| UB-005 | DataAsset paths need `.AssetName` suffix | MEDIUM | MCP | Pending |
| ENH-002 | PCG workflow needs specific property guidance | LOW | MCP Help | Pending |

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

## Fix 3: Struct Schema Support (UB-006)

### Current Behavior

```python
get_class_schema("BiomeAsset")   # → NOT_FOUND
get_class_schema("FBiomeAsset")  # → NOT_FOUND
```

### Implementation Module: `AgentBridgeCore/TypeDiscovery.cpp`

```cpp
TSharedPtr<FAgentClassSchema> UTypeDiscovery::GetClassSchema(const FString& ClassName)
{
    // Current: Only searches UClass
    UClass* Class = FindClass(ClassName);
    if (Class) return BuildSchemaForClass(Class);

    // NEW: Also search UScriptStruct
    UScriptStruct* Struct = FindStruct(ClassName);
    if (Struct) return BuildSchemaForStruct(Struct);

    return nullptr;
}

UScriptStruct* FindStruct(const FString& Name)
{
    // Try exact name
    if (UScriptStruct* S = FindObject<UScriptStruct>(ANY_PACKAGE, *Name))
        return S;
    // Try with F prefix (UE convention)
    if (UScriptStruct* S = FindObject<UScriptStruct>(ANY_PACKAGE, *(TEXT("F") + Name)))
        return S;
    // Try without F prefix
    if (Name.StartsWith(TEXT("F")))
        if (UScriptStruct* S = FindObject<UScriptStruct>(ANY_PACKAGE, *Name.Mid(1)))
            return S;
    return nullptr;
}
```

### No MCP Changes Needed

The existing `get_class_schema` tool signature works - just extend the C++ to handle structs.

### Success Criteria

```python
get_class_schema("BiomeAsset")
# → {"class_name": "FBiomeAsset", "properties": [
#      {"name": "Mesh", "type_name": "TSoftObjectPtr<UStaticMesh>"},
#      {"name": "Weight", "type_name": "float"},
#      ...
#    ]}
```

---

## Fix 4: TArray Element Type (UB-007)

### Current Behavior

```python
get_class_schema("BiomeAssetTemplate")
# → properties: [{"name": "BiomeAssets", "type_name": "TArray", ...}]
```

### Implementation Module: `AgentBridgeCore/TypeDiscovery.cpp`

In schema property building:

```cpp
FAgentPropertySchema BuildPropertySchema(FProperty* Prop)
{
    FAgentPropertySchema Schema;
    Schema.Name = Prop->GetAuthoredName();

    if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
    {
        Schema.TypeName = TEXT("TArray");
        // NEW: Add element type
        Schema.ElementType = GetPropertyTypeName(ArrayProp->Inner);
    }
    else
    {
        Schema.TypeName = GetPropertyTypeName(Prop);
    }

    return Schema;
}
```

### Schema JSON Update

Add `element_type` field to property schema:

```json
{
  "name": "BiomeAssets",
  "type_name": "TArray",
  "element_type": "FBiomeAsset",
  "is_read_only": false
}
```

### Proto Update (if using gRPC schema)

```protobuf
message PropertySchema {
    string name = 1;
    string type_name = 2;
    bool is_read_only = 3;
    string element_type = 4;  // NEW: For TArray/TSet/TMap
}
```

---

## Fix 5: DataAsset Path Auto-Resolution (UB-005)

### Current Behavior

User must know to use `/Game/path/Asset.Asset` not `/Game/path/Asset`.

### Implementation Module: `Python/mcp/services/agentbridge.py`

```python
def _normalize_asset_path(path: str) -> str:
    """
    Auto-fix asset paths for Blueprint-derived assets.
    /Game/Foo/MyAsset → /Game/Foo/MyAsset.MyAsset (if needed)
    """
    if "." not in path.split("/")[-1]:
        # No extension - try adding asset name suffix
        asset_name = path.split("/")[-1]
        return f"{path}.{asset_name}"
    return path

def get_property(actor_id: str, path: str) -> dict:
    # If actor_id looks like an asset path, normalize it
    if actor_id.startswith("/Game/"):
        normalized = _normalize_asset_path(actor_id)
        result = client.get_property(normalized, path)
        if result.get("value") != "":
            return result
        # If still empty, try original path
        return client.get_property(actor_id, path)
    return client.get_property(actor_id, path)
```

### Success Criteria

```python
# Both should work identically:
get_property("/Game/freshtest/TestBiome", "BiomeDefinition.BiomeName")
get_property("/Game/freshtest/TestBiome.TestBiome", "BiomeDefinition.BiomeName")
```

---

## Fix 6: PCG Workflow Property Guidance (ENH-002)

### Problem

When sizing volumes to landscape, agents get 100+ properties and don't know which to use.

### Implementation Module: `Python/mcp/services/agentbridge.py` - Help System

Add specific property lists to workflow guidance:

```python
PCG_VOLUME_PROPERTIES = """
## Sizing a BoxComponent Volume to Landscape

### Key Properties (in order of use):

1. **Get landscape bounds:**
   ```
   bounds = get_landscape_bounds()
   # Returns: center, extent (half-size), min, max
   ```

2. **Set BoxComponent size** (component name is typically "Volume"):
   - `BoxExtent` (FVector) - Half-size in each axis
   - `RelativeScale3D` (FVector) - Set to (1,1,1) when using BoxExtent directly

3. **Position the actor:**
   - Use `set_actor_transform(location=bounds["center"])`

### Z Margin for PCG:
Add 5000+ units to BoxExtent.Z for spawn variation above terrain:
```python
tempo_set_vector_property(actor, component="Volume", property="BoxExtent",
    x=bounds["extent"][0],
    y=bounds["extent"][1],
    z=bounds["extent"][2] + 5000)  # Z margin for spawning
```

### Important Notes:
- BoxExtent is HALF-SIZE (full coverage = extent * 2)
- When scale=(1,1,1), BoxExtent directly controls the volume size
- Always reset scale to 1 when setting BoxExtent manually
"""
```

### Help Topic Addition

```python
HELP_TOPICS["pcg_volume"] = PCG_VOLUME_PROPERTIES
HELP_TOPICS["volume_sizing"] = PCG_VOLUME_PROPERTIES  # Alias
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
