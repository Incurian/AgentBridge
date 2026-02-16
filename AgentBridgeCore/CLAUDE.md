# AgentBridgeCore - AI Development Guide

> Low-level reflection primitives for reading/writing UE properties and invoking functions.

**Standalone UE plugin** - one of 4 independent AgentBridge plugins discovered by UBT.
This plugin has no dependencies on other AgentBridge plugins; it only depends on Engine
modules (Core, CoreUObject, Engine). Higher-level plugins (AgentBridgeRuntime,
AgentBridgeScripting, AgentBridgeServer) depend on this one.

---

## Plugin Structure

```
AgentBridgeCore/
|-- AgentBridgeCore.uplugin       # Plugin descriptor (EnabledByDefault: true)
|-- CLAUDE.md                     # This file
|-- README.md                     # Beginner-friendly documentation
|-- Source/AgentBridgeCore/
    |-- AgentBridgeCore.Build.cs  # Dependencies: Core, CoreUObject, Engine
    |-- Public/
    |   |-- AgentBridgeCore.h     # Module interface (FAgentBridgeCoreModule)
    |   |-- AgentBridgeTypes.h    # FAgentPropertyValue, FAgentClassInfo, FAgentFunctionSignature, etc.
    |   |-- PropertyAccessor.h    # FPropertyAccessor - read/write any FProperty
    |   |-- AgentPropertyPath.h   # FAgentPropertyPath - dot-notation path resolution
    |   |-- FunctionInvoker.h     # FFunctionInvoker - dynamic UFunction invocation
    |   |-- TypeDiscovery.h       # FTypeDiscovery - class/struct/enum discovery
    |-- Private/
        |-- AgentBridgeCore.cpp   # Module startup/shutdown (minimal - just logging)
        |-- AgentBridgeTypes.cpp  # Convenience constructors and extractors
        |-- PropertyAccessor.cpp  # All read/write logic for every FProperty type
        |-- AgentPropertyPath.cpp # Path parsing, segment resolution, nested traversal
        |-- FunctionInvoker.cpp   # Parameter marshaling, invocation, result extraction
        |-- TypeDiscovery.cpp     # Class lookup, BP normalization, schema enumeration
```

## Module Configuration

| Setting | Value |
|---------|-------|
| Module Type | `Runtime` |
| Loading Phase | `Default` |
| Enabled By Default | `true` |
| Can Contain Content | `false` |

---

## Purpose

This module provides the foundation for all property access and type discovery in
AgentBridge:

| Class | Responsibility |
|-------|---------------|
| `FPropertyAccessor` | Read/write any FProperty type recursively |
| `FAgentPropertyPath` | Parse and resolve dot-notation property paths |
| `FFunctionInvoker` | Dynamic UFunction invocation with parameter marshaling |
| `FTypeDiscovery` | Class/struct/enum discovery, Blueprint name normalization |

## Key Files

| File | Purpose |
|------|---------|
| `AgentBridgeTypes.h` | `FAgentPropertyValue`, `FAgentClassInfo`, `FAgentFunctionSignature`, `FAgentFunctionResult`, `FAgentEnumValue` |
| `PropertyAccessor.h/.cpp` | Read/write all FProperty types, type introspection, object reference serialization |
| `AgentPropertyPath.h/.cpp` | Path parsing, segment types (Property, ArrayIndex, MapKey), nested resolution |
| `FunctionInvoker.h/.cpp` | Function discovery, hidden param detection, invocation, result extraction |
| `TypeDiscovery.h/.cpp` | Class lookup, BP `_C` normalization, property/function enumeration |

---

## Critical Patterns

### Reading Properties

```cpp
// Core pattern - works for any container (Actor, Component, UObject, struct)
FAgentPropertyValue Value = FPropertyAccessor::ReadProperty(Container, Property, MaxDepth);
```

The Container can be a UObject pointer or a raw struct pointer. ReadProperty uses
`ContainerPtrToValuePtr` internally to compute the memory address of the value.

### Writing Properties

**Two variants exist for different use cases:**

```cpp
// VARIANT 1: When you have a container and want the offset calculated.
// Use this for top-level property access on a UObject.
FPropertyAccessor::WriteProperty(Container, Property, Value);

// VARIANT 2: When you already have the resolved value pointer.
// Use this after path resolution, which returns a direct pointer via ValuePtr.
FPropertyAccessor::WritePropertyDirect(ValuePtr, Property, Value);
```

**CRITICAL:** Use `WritePropertyDirect` when the pointer IS the value, not a container.
Path resolution (`FAgentPropertyPath::SetValue`) returns a `FPropertyPathResult` with
`ValuePtr` pointing directly to the target memory location. Calling `WriteProperty` on
that pointer would apply `ContainerPtrToValuePtr` again, double-offsetting and corrupting
the write. This was the root cause of the nested BP struct write bug.

**Internal write helpers:** All private write methods (`WriteBoolProperty`,
`WriteNumericProperty`, etc.) take a direct `ValuePtr` parameter. They must NOT call
`ContainerPtrToValuePtr` internally. `WriteProperty` computes the offset once and passes
the resolved pointer to the helpers. `WritePropertyDirect` passes the pointer through
directly.

### Array Traversal (CRITICAL)

When iterating over TArray properties, the element becomes the container for inner
properties. Getting this wrong is a common source of crashes:

```cpp
const void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(ContainerPtr);
FScriptArrayHelper ArrayHelper(ArrayProp, ArrayPtr);

for (int32 i = 0; i < ArrayHelper.Num(); i++)
{
    void* ElementPtr = ArrayHelper.GetRawPtr(i);
    // CRITICAL: ElementPtr IS the container for the Inner property.
    // Do NOT pass ContainerPtr here - the element's memory IS the value.
    TraverseProperty(ArrayProp->Inner, ElementPtr, Depth + 1);
}
```

### Map Traversal (SPARSE INDICES!)

Maps in UE use sparse storage - there can be gaps in the internal array. Always check
`IsValidIndex` before accessing elements:

```cpp
FScriptMapHelper MapHelper(MapProp, MapPtr);
for (int32 i = 0; i < MapHelper.GetMaxIndex(); ++i)
{
    if (MapHelper.IsValidIndex(i))  // CRITICAL: Maps have gaps!
    {
        const uint8* KeyPtr = MapHelper.GetKeyPtr(i);
        const uint8* ValuePtr = MapHelper.GetValuePtr(i);
    }
}
```

Note: Use `GetMaxIndex()` (not `Num()`) for the loop bound. `Num()` returns the count
of valid elements, but indices can go up to `GetMaxIndex() - 1` with gaps in between.

### Blueprint Property Names

Blueprint-generated properties have GUID suffixes appended to their names, like
`PropertyName_23_ABC123DEF`. When presenting property names externally or matching user
input, use the clean display name:

```cpp
// Get clean display name (strips GUID suffix)
FString DisplayName = Property->GetAuthoredName();

// Or use metadata (editor-only! Not available in packaged builds)
FString MetaDisplayName = Property->GetMetaData(TEXT("DisplayName"));
```

`FTypeDiscovery::GetDisplayPropertyName()` and `FPropertyAccessor::GetPropertyDisplayName()`
both wrap this logic. `FAgentPropertyPath::FindPropertyByName()` checks both C++ names and
display names when resolving path segments, so user-provided paths work with either format.

### Property Writability Check

The `IsPropertyWritable()` check only blocks `CPF_EditConst` properties. It intentionally
does NOT block `CPF_BlueprintReadOnly`, because that flag only prevents Blueprint scripts
from writing - C++ code and Editor operations can still modify these properties. This is
important because most component properties (Intensity, Color, etc.) have
`CPF_BlueprintReadOnly` set, but we need to write them from C++ via the reflection system.

```cpp
// Current check - intentionally permissive for C++ access:
const EPropertyFlags ReadOnlyFlags = CPF_EditConst;
return !Property->HasAnyPropertyFlags(ReadOnlyFlags);
```

---

## UObject Pointer Types

Reference table for the different object pointer types in UE's reflection system:

| C++ Type | Reflection Class | GC Behavior | Notes |
|----------|------------------|-------------|-------|
| `UObject*` / `TObjectPtr<>` | `FObjectProperty` | Prevents GC | Standard strong reference |
| `TSoftObjectPtr<>` | `FSoftObjectProperty` | Path-based, no GC prevention | Lazy loading, serialized as FSoftObjectPath |
| `TWeakObjectPtr<>` | `FWeakObjectProperty` | Auto-nulls when target is GC'd | Non-owning reference |
| `TSubclassOf<>` | `FClassProperty` | Prevents GC of UClass | Points to a UClass, not an instance |

All four types inherit from `FObjectPropertyBase`, enabling unified access:

```cpp
if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
{
    UObject* Value = ObjProp->GetObjectPropertyValue_InContainer(Instance);
    UClass* PropertyClass = ObjProp->PropertyClass;
}
```

When writing object references, values are resolved from string paths via
`FPropertyAccessor::ResolveObjectReference()`. The `set_property` MCP tool accepts
paths like `"/Game/Maps/Level.Level:PersistentLevel.Actor_0"` or class paths like
`"/Script/Engine.StaticMesh"`.

**Known limitation:** `TSoftObjectPtr` assignment does not work reliably through the
reflection system. Use `TObjectPtr` properties when possible. See Known Issues in the
parent CLAUDE.md.

---

## FAgentPropertyValue Transport Format

`FAgentPropertyValue` is the universal value container for moving property data across
module and network boundaries:

| Field | Used For |
|-------|----------|
| `Type` | `EAgentPropertyType` enum identifying the value type |
| `StringValue` | Primitive values (bool, int, float, string, name, text, enum, object path) |
| `BinaryValue` | Reserved for binary data (not currently used) |
| `ArrayValue` | Array and set elements (TArray of nested FAgentPropertyValue) |
| `StructValue` | Struct fields and map entries (TMap of field name to nested FAgentPropertyValue) |

Special struct types (FVector, FRotator, FTransform, FColor) are detected by
`TryReadSpecialStruct` / `TryWriteSpecialStruct` and serialized to a simpler format
rather than being expanded into individual fields.

---

## FAgentPropertyPath Segment Types

Paths are parsed into an array of `FPropertyPathSegment` values:

| Segment Type | Syntax | Example |
|-------------|--------|---------|
| `Property` | `name` | `"Location"`, `"Health"` |
| `ArrayIndex` | `[N]` | `"Components[0]"`, `"Items[3]"` |
| `MapKey` | `["key"]` | `"Stats[\"Strength\"]"` |

Segments can be chained with dots: `"Outer.Array[0].Map[\"Key\"].Value"`

Resolution walks the object's property chain, using `ContainerPtrToValuePtr` at each
step to compute the next memory address. For write operations, the final segment's
`ValuePtr` is passed to `WritePropertyDirect`.

---

## FFunctionInvoker Details

### Hidden Parameters

Many Blueprint functions have "hidden" parameters that users should not need to provide:

| Hidden Param | How It Is Filled |
|-------------|------------------|
| `WorldContextObject` | From the World parameter or derived from the target object |
| `self` | Set to the target instance automatically |

`FunctionNeedsWorldContext()` and `FunctionHasHiddenSelfPin()` detect these. The
invocation code fills them automatically before calling `UObject::ProcessEvent()`.

### Parameter Memory Layout

UE functions use a contiguous memory block for all parameters. `PrepareParameters()`
allocates this block, initializes defaults, and fills in user-provided values.
`ExtractResults()` reads return values and output parameters from the block after
execution. `CleanupParameters()` properly destroys complex types.

### Static vs Instance Calls

- `InvokeFunction()` calls a function on a specific object instance.
- `InvokeStaticFunction()` calls a function on the Class Default Object (CDO). Used for
  functions in static Blueprint libraries like `KismetSystemLibrary`.

---

## Console Commands

These are registered by AgentBridgeScripting but use Core under the hood:

| Command | Description |
|---------|-------------|
| `AgentBridge.DumpActor <name> [depth]` | Dump actor properties recursively |
| `AgentBridge.DumpClass <name>` | Dump class schema (properties + functions) |

---

## Dependencies

```csharp
// AgentBridgeCore.Build.cs
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
});
```

No plugin dependencies. No editor-only module dependencies. This is the leaf node in
the AgentBridge dependency graph.

---

## Resolved Issues

Historical context for bugs that were found and fixed. Understanding these helps avoid
re-introducing the same problems.

### Nested BP Struct Writes (WritePropertyDirect)

**Problem:** Writing to nested struct paths like `DefaultDefinition.BiomeColor` silently
failed. The value was not stored.

**Root Cause:** Path resolution returns a direct value pointer, but `WriteProperty()` was
calling `ContainerPtrToValuePtr()` on it, double-offsetting and corrupting the address.

**Solution:** Added `WritePropertyDirect()` that works with pre-resolved value pointers.
All internal write helpers now take value pointers directly, not containers. Path
resolution uses `WritePropertyDirect` for the final write.

### Nested Property SET (CPF_BlueprintReadOnly)

**Problem:** SET operations on component paths (`LightComponent0.Intensity`) and nested
paths (`RootComponent.RelativeLocation`) failed while GET worked fine.

**Root Cause:** `IsPropertyWritable()` was checking for `CPF_BlueprintReadOnly` flag. Most
component properties have this flag, but it only prevents Blueprint scripts from writing -
C++ code can still modify them.

**Solution:** Removed `CPF_BlueprintReadOnly` from the writable check. Only
`CPF_EditConst` is blocked:

```cpp
// OLD - Too restrictive (blocked component properties):
const EPropertyFlags ReadOnlyFlags = CPF_BlueprintReadOnly | CPF_EditConst;

// NEW - Only block truly immutable properties:
const EPropertyFlags ReadOnlyFlags = CPF_EditConst;
```

### FunctionInvoker Return Values

**Problem:** Function calls returned default/zero values for complex struct return types
(e.g., `K2_GetActorLocation` returned `(0,0,0)` instead of the actual location).

**Root Cause:** Issue in how `FFunctionInvoker::InvokeFunction` extracts return values
from the UFunction parameter block. The extraction logic did not correctly handle struct
return types.

**Workaround (in AgentBridgeScripting):** `CommandExecutor.cpp` has a
`GetFunctionToPropertyMap()` that redirects common getter functions to property access.
`K2_GetActorLocation`, `K2_GetActorRotation`, `GetActorScale3D`, etc. are transparently
redirected to read the corresponding property, so users get correct results without
knowing about the workaround. The underlying `FFunctionInvoker` extraction bug remains
but is effectively hidden.

### Object Reference Type Validation

**Problem:** `set_property` silently accepted type-mismatched object references (e.g.,
assigning a `UStaticMesh` to a property expecting `UMaterialInterface`). The value was
stored but the object was wrong, causing subtle runtime errors.

**Solution:** Added type checking in `WriteObjectProperty`. When the resolved object's
class does not match `FObjectPropertyBase::PropertyClass`, the write is rejected with a
clear error message. This was fixed in the C++ layer.

---

## Thread Safety

All public methods in this module must be called from the **Game Thread**. UObject
pointers and FProperty pointers are not safe to cache across frames without validation.
Higher-level modules (Runtime, Scripting, Server) are responsible for bouncing async
requests to the game thread before calling Core functions.

---

## Related Documentation

- **[README.md](README.md)** - Beginner-friendly documentation with UE reflection primer
- **[Parent CLAUDE.md](../CLAUDE.md)** - Project-wide development guide, known issues, SOPs
- **[Parent README.md](../README.md)** - Full AgentBridge ecosystem overview
