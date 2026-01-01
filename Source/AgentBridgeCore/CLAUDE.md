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

## Known Issues

### FunctionInvoker Return Values

Function calls return default values (0 for structs, "" for strings) instead of actual return values.

**Workaround:** Use property queries instead of function calls when possible.

**Status:** Won't fix (by design) - complex UE limitation.

## Stretch Goals

| Feature | Effort | Notes |
|---------|--------|-------|
| Fix struct return values | High | Requires understanding UE4/5 return value marshaling |
| UObject property access | Medium | Allow property access on DataAssets, not just actors |

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
