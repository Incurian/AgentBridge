# AgentBridgeCore Module

> Low-level reflection primitives for reading/writing UE properties and invoking functions.

## Purpose

This module provides the foundation for all property access and type discovery:
- `FPropertyAccessor` - Read/write any FProperty type recursively
- `FFunctionInvoker` - Dynamic UFunction invocation with parameter marshaling
- `FTypeDiscovery` - Class/struct/enum discovery, BP name normalization

## Key Files

| File | Purpose |
|------|---------|
| `AgentBridgeTypes.h` | `FAgentPropertyValue`, `FAgentClassInfo`, `FAgentFunctionSignature` |
| `PropertyAccessor.h/.cpp` | Read/write all FProperty types |
| `FunctionInvoker.h/.cpp` | UFunction invocation |
| `TypeDiscovery.h/.cpp` | Class discovery, BP normalization |

## Critical Patterns

### Reading Properties

```cpp
// Core pattern - works for any container (Actor, Component, UObject)
FAgentPropertyValue Value = FPropertyAccessor::ReadProperty(Container, Property, MaxDepth);
```

### Writing Properties

**Two variants exist for different use cases:**

```cpp
// When you have a container and want the offset calculated:
FPropertyAccessor::WriteProperty(Container, Property, Value);

// When you already have the resolved value pointer (e.g., from path resolution):
FPropertyAccessor::WritePropertyDirect(ValuePtr, Property, Value);
```

**CRITICAL:** Use `WritePropertyDirect` when the pointer IS the value, not a container.
This was the fix for nested BP struct writes - path resolution returns value pointers.

### Array Traversal (CRITICAL)

The element becomes the container for inner properties:

```cpp
const void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(ContainerPtr);
FScriptArrayHelper ArrayHelper(ArrayProp, ArrayPtr);

for (int32 i = 0; i < ArrayHelper.Num(); i++)
{
    void* ElementPtr = ArrayHelper.GetRawPtr(i);
    // CRITICAL: Element IS the container for Inner property
    TraverseProperty(ArrayProp->Inner, ElementPtr, Depth + 1);
}
```

### Map Traversal (SPARSE INDICES!)

Maps have gaps - always check `IsValidIndex`:

```cpp
FScriptMapHelper MapHelper(MapProp, MapPtr);
for (int32 i = 0; i < MapHelper.GetMaxIndex(); ++i)
{
    if (MapHelper.IsValidIndex(i))  // CRITICAL: Maps have gaps
    {
        const uint8* KeyPtr = MapHelper.GetKeyPtr(i);
        const uint8* ValuePtr = MapHelper.GetValuePtr(i);
    }
}
```

### Blueprint Property Names

BP properties have GUID suffixes like `PropertyName_23_abc123`:

```cpp
// Get clean display name
FString DisplayName = Property->GetAuthoredName();

// Or use metadata (editor-only!)
FString MetaDisplayName = Property->GetMetaData(TEXT("DisplayName"));
```

## UObject Pointer Types

| Type | Reflection Class | GC Behavior |
|------|------------------|-------------|
| `UObject*` / `TObjectPtr<>` | `FObjectProperty` | Prevents GC |
| `TSoftObjectPtr<>` | `FSoftObjectProperty` | Path-based, no GC prevention |
| `TWeakObjectPtr<>` | `FWeakObjectProperty` | Auto-nulls when target GC'd |
| `TSubclassOf<>` | `FClassProperty` | Prevents GC of UClass |

Unified access:
```cpp
if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
{
    UObject* Value = ObjProp->GetObjectPropertyValue_InContainer(Instance);
    UClass* PropertyClass = ObjProp->PropertyClass;
}
```

## Recent Fixes

### Nested BP Struct Writes (Session 19)

**Problem:** Writing to nested struct paths like `DefaultDefinition.BiomeColor` silently failed.

**Root Cause:** Path resolution returns a direct value pointer, but `WriteProperty()` was calling
`ContainerPtrToValuePtr()` on it, corrupting the offset.

**Solution:** Added `WritePropertyDirect()` that works with pre-resolved value pointers.
All internal write helpers now take value pointers directly, not containers.

## Known Limitations

### FunctionInvoker Return Values - AUTO-FIXED

Function calls were returning default/zero values for complex struct return types.

**Solution (Session 19):**
- Added `GetFunctionToPropertyMap()` in CommandExecutor.cpp
- Common getters automatically redirect to property access
- K2_GetActorLocation, K2_GetActorRotation, GetActorScale3D, etc. all work correctly now
- Users get expected results without knowing about the workaround

**Root Cause (for reference):** Issue in how `FFunctionInvoker::InvokeFunction` extracts return
values from the UFunction parameter block. Not fixed in FunctionInvoker itself, but transparently
worked around at the command layer.

## Stretch Goals

| Feature | Effort | Notes |
|---------|--------|-------|
| UObject property access | Medium | Allow property access on DataAssets, not just actors |
| Test struct return values | Low | May already work after WritePropertyDirect fix |

## Testing

Console commands for testing this module:
- `AgentBridge.DumpActor <name> [depth]` - Dump actor properties
- `AgentBridge.DumpClass <name>` - Dump class schema

## Dependencies

```csharp
// AgentBridgeCore.Build.cs
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
});
```
