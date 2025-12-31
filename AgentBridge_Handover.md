# AgentBridge Plugin — Complete Implementation Handover

> **Purpose**: Unreal Engine 5.6 plugin exposing editor/runtime state to external AI agents via gRPC + MCP.  
> **Primary Use Case**: "Build me a level" — agents need full read/write/discover capabilities.  
> **Constraints**: Editor-first (runtime-ready architecture), high latency tolerance, single world initially, streaming desirable.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Module Structure](#module-structure)
3. [Critical Technical Gotchas](#critical-technical-gotchas)
   - [Blueprint vs C++ Reflection](#blueprint-vs-c-reflection)
   - [UObject Pointer Handling](#uobject-pointer-handling)
   - [Nested Arrays and Structs](#nested-arrays-and-structs)
   - [Components vs Actors](#components-vs-actors)
   - [PIE and Runtime Contexts](#pie-and-runtime-contexts)
4. [Module 1: AgentBridgeCore (Reflection Primitives)](#module-1-agentbridgecore)
5. [Module 2: AgentBridgeRuntime (Abstraction Layer)](#module-2-agentbridgeruntime)
6. [Module 3: AgentBridgeScripting (High-Level API)](#module-3-agentbridgescripting)
7. [Module 4: AgentBridgeServer (gRPC)](#module-4-agentbridgeserver)
8. [Module 5: Python Client](#module-5-python-client)
9. [Module 6: MCP Server](#module-6-mcp-server)
10. [Extended Features](#extended-features)
    - [DataAsset Support](#dataasset-support)
    - [Viewport/Scene Capture](#viewportscene-capture)
    - [Audio Capture](#audio-capture)
    - [Material Operations](#material-operations)
    - [PCG Operations](#pcg-operations)
11. [Debugging Utilities](#debugging-utilities)
12. [Autonomous Compilation Setup](#autonomous-compilation-setup)
13. [Implementation Order](#implementation-order)
14. [Risk Mitigation](#risk-mitigation)

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        External Agents                          │
│                    (Claude, other LLMs)                         │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      MCP Server (Python)                        │
│              Tools: spawn, modify, query, etc.                  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Python gRPC Client                           │
│              agentbridge.AgentBridgeClient                      │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼ (gRPC over localhost:50051)
┌─────────────────────────────────────────────────────────────────┐
│                 AgentBridgeServer (UE Module)                   │
│         gRPC service, game thread dispatch, streaming           │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│               AgentBridgeScripting (UE Module)                  │
│         High-level commands, undo support, validation           │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                AgentBridgeRuntime (UE Module)                   │
│      World context, actor ops, property paths, BP/C++ glue      │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                 AgentBridgeCore (UE Module)                     │
│         FProperty access, UFunction invoke, type discovery      │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Unreal Engine 5.6                            │
│              Reflection System, World, Actors                   │
└─────────────────────────────────────────────────────────────────┘
```

---

## Module Structure

```
Plugins/AgentBridge/
├── AgentBridge.uplugin
├── Source/
│   ├── AgentBridgeCore/           # Module 1: Reflection primitives
│   │   ├── AgentBridgeCore.Build.cs
│   │   ├── Public/
│   │   │   ├── PropertyAccessor.h
│   │   │   ├── FunctionInvoker.h
│   │   │   ├── TypeDiscovery.h
│   │   │   └── AgentBridgeTypes.h
│   │   └── Private/
│   │       ├── PropertyAccessor.cpp
│   │       ├── FunctionInvoker.cpp
│   │       └── TypeDiscovery.cpp
│   │
│   ├── AgentBridgeRuntime/        # Module 2: Abstraction & helpers
│   │   ├── AgentBridgeRuntime.Build.cs
│   │   ├── Public/
│   │   │   ├── WorldContextManager.h
│   │   │   ├── ActorOperations.h
│   │   │   ├── ComponentOperations.h
│   │   │   ├── PropertyPath.h
│   │   │   ├── ClassNormalization.h
│   │   │   └── TransactionWrapper.h
│   │   └── Private/
│   │       └── ...
│   │
│   ├── AgentBridgeScripting/      # Module 3: High-level operations
│   │   ├── AgentBridgeScripting.Build.cs
│   │   ├── Public/
│   │   │   ├── AgentCommand.h
│   │   │   ├── WorldCommands.h
│   │   │   ├── ManipulationCommands.h
│   │   │   ├── DiscoveryCommands.h
│   │   │   └── CommandExecutor.h
│   │   └── Private/
│   │       └── ...
│   │
│   └── AgentBridgeServer/         # Module 4: gRPC server
│       ├── AgentBridgeServer.Build.cs
│       ├── Public/
│       │   ├── AgentBridgeServer.h
│       │   └── AgentBridgeServiceImpl.h
│       └── Private/
│           └── ...
│
├── Protos/                        # Protobuf definitions
│   ├── common.proto
│   ├── world.proto
│   ├── actors.proto
│   ├── properties.proto
│   ├── functions.proto
│   ├── streaming.proto
│   └── service.proto
│
└── Python/                        # Module 5+: Python client & MCP
    ├── agentbridge/
    │   ├── __init__.py
    │   ├── client.py
    │   ├── types.py
    │   ├── async_client.py
    │   ├── generated/             # protoc output (gitignored)
    │   └── mcp_server.py
    ├── pyproject.toml
    ├── generate_protos.py
    └── README.md
```

---

## Critical Technical Gotchas

### Blueprint vs C++ Reflection

#### The `_C` Suffix Problem

Blueprint classes have TWO objects:
- `BP_MyActor` — the `UBlueprint` asset (editor-only)
- `BP_MyActor_C` — the `UBlueprintGeneratedClass` (runtime class)

**Always use the `_C` suffix when loading Blueprint classes programmatically:**

```cpp
// WRONG - references the asset, fails in packaged builds
LoadObject<UClass>(nullptr, TEXT("/Game/BP_MyActor.BP_MyActor"));

// CORRECT - references the generated class
UClass* Class = LoadClass<AActor>(nullptr, TEXT("/Game/BP_MyActor.BP_MyActor_C"));
```

#### Property Name Mangling

Blueprint variables have internal names with GUID suffixes like `PropertyName_23_abc123`. This enables safe renaming without breaking connections.

**To get clean display names:**
```cpp
FString DisplayName = Property->GetAuthoredName();
// Or check metadata:
FString MetaDisplayName = Property->GetMetaData(TEXT("DisplayName"));
```

**To find properties by display name, build a lookup cache:**
```cpp
// In ClassNormalization.cpp
FProperty* FClassNormalization::FindPropertyByDisplayName(UClass* Class, const FString& DisplayName)
{
    for (TFieldIterator<FProperty> PropIt(Class); PropIt; ++PropIt)
    {
        FString AuthoredName = PropIt->GetAuthoredName();
        if (AuthoredName.Equals(DisplayName, ESearchCase::IgnoreCase))
        {
            return *PropIt;
        }
    }
    return nullptr;
}
```

#### Blueprint vs Native Detection

```cpp
bool bIsBlueprintClass = Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
bool bIsBlueprintProperty = !Property->GetOwnerClass()->IsNative();
bool bIsNativeFunction = Function->HasAnyFunctionFlags(FUNC_Native);
bool bIsBlueprintFunction = Function->Script.Num() > 0;
```

#### Property Access — UE 5.1+ Preferred Methods

```cpp
// PREFERRED - respects BlueprintGetter/Setter if defined
float Value;
Property->GetValue_InContainer(Container, &Value);
Property->SetValue_InContainer(Container, &NewValue);

// DIRECT MEMORY - use when you need the pointer
void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Instance);
```

#### Metadata is Editor-Only

**Property metadata is stripped in shipping builds.** Design runtime functionality around property flags (`CPF_*`) instead:

```cpp
// WORKS IN SHIPPING
bool bReadOnly = Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly | CPF_EditConst);
bool bReplicated = Property->HasAnyPropertyFlags(CPF_Net);

// EDITOR ONLY - returns empty in shipping
FString Category = Property->GetMetaData(TEXT("Category"));
```

---

### UObject Pointer Handling

#### The Five Pointer Types

| Type | Reflection Class | Use Case | GC Behavior |
|------|------------------|----------|-------------|
| `UObject*` / `TObjectPtr<>` | `FObjectProperty` / `FObjectPtrProperty` | Strong runtime reference | Prevents GC of target |
| `TSoftObjectPtr<>` | `FSoftObjectProperty` | Asset reference, lazy load | Path-based, no GC prevention |
| `TWeakObjectPtr<>` | `FWeakObjectProperty` | Non-owning runtime ref | Auto-nulls when target GC'd |
| `TSubclassOf<>` | `FClassProperty` | Class reference | Prevents GC of UClass |
| `TLazyObjectPtr<>` | `FLazyObjectProperty` | Cross-level references | GUID-based resolution |

**Unified access pattern:**
```cpp
if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
{
    UObject* Value = ObjProp->GetObjectPropertyValue_InContainer(Instance);
    UClass* PropertyClass = ObjProp->PropertyClass;  // Expected type
}
```

#### Serializing Object References for Transport

**Use FSoftObjectPath for maximum compatibility:**
```cpp
// Serialize
FSoftObjectPath SoftPath(MyObject);
FString PathString = SoftPath.ToString();
// Result: "/Game/Maps/Level.Level:PersistentLevel.MyActor_0"

// Deserialize
FSoftObjectPath ParsedPath;
ParsedPath.SetPath(PathString);
UObject* Resolved = ParsedPath.ResolveObject();  // nullptr if not loaded
```

**Resolution strategies:**
```cpp
// Fast - only finds already-loaded objects
UObject* Obj = FindObject<UObject>(nullptr, *Path);

// Will load from disk synchronously - USE SPARINGLY
UObject* Obj = LoadObject<UObject>(nullptr, *Path);

// Async loading - preferred for assets
FStreamableManager& Manager = UAssetManager::GetStreamableManager();
Manager.RequestAsyncLoad(SoftPath, FStreamableDelegate::CreateLambda([](){ 
    // Asset loaded 
}));
```

#### Thread Safety for External Access

**Most UObject operations require game thread.** Use this pattern for async:

```cpp
TWeakObjectPtr<UObject> WeakRef(MyObject);

AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakRef]()
{
    // Do non-UObject work here...
    
    // When you need UObject access, bounce to game thread
    AsyncTask(ENamedThreads::GameThread, [WeakRef]()
    {
        if (UObject* Obj = WeakRef.Get())
        {
            // Safe to access UObject here
        }
    });
});
```

**Pinning pattern (UE5):**
```cpp
TWeakObjectPtr<UObject> WeakRef(MyObject);
AsyncTask(ENamedThreads::AnyThread, [WeakRef]()
{
    if (TStrongObjectPtr<UObject> Pinned = WeakRef.Pin())
    {
        // Object won't be GC'd while Pinned exists
        // But still do UObject ops on game thread!
    }
});
```

#### Validating Object References

```cpp
// Check if object is valid and not pending kill
if (IsValid(MyObject))
{
    // Safe to use
}

// For weak pointers
if (WeakPtr.IsValid())
{
    UObject* Obj = WeakPtr.Get();
}
```

---

### Nested Arrays and Structs

#### Recursive Traversal Pattern

The critical insight: **at each nesting level, the element becomes the container for inner properties.**

```cpp
void TraverseProperty(const FProperty* Property, const void* ContainerPtr, int32 Depth = 0)
{
    // Handle arrays
    if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
    {
        const void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(ContainerPtr);
        FScriptArrayHelper ArrayHelper(ArrayProp, ArrayPtr);
        
        for (int32 i = 0; i < ArrayHelper.Num(); i++)
        {
            void* ElementPtr = ArrayHelper.GetRawPtr(i);
            // CRITICAL: Element IS the container for Inner property
            TraverseProperty(ArrayProp->Inner, ElementPtr, Depth + 1);
        }
        return;
    }
    
    // Handle structs
    if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        const void* StructPtr = StructProp->ContainerPtrToValuePtr<void>(ContainerPtr);
        
        for (TFieldIterator<FProperty> PropIt(StructProp->Struct); PropIt; ++PropIt)
        {
            // StructPtr is now the container
            TraverseProperty(*PropIt, StructPtr, Depth + 1);
        }
        return;
    }
    
    // Handle maps - SPARSE INDICES!
    if (const FMapProperty* MapProp = CastField<FMapProperty>(Property))
    {
        const void* MapPtr = MapProp->ContainerPtrToValuePtr<void>(ContainerPtr);
        FScriptMapHelper MapHelper(MapProp, MapPtr);
        
        for (int32 i = 0; i < MapHelper.GetMaxIndex(); ++i)
        {
            if (MapHelper.IsValidIndex(i))  // CRITICAL: Maps have gaps
            {
                const uint8* KeyPtr = MapHelper.GetKeyPtr(i);
                const uint8* ValuePtr = MapHelper.GetValuePtr(i);
                TraverseProperty(MapProp->KeyProp, KeyPtr, Depth + 1);
                TraverseProperty(MapProp->ValueProp, ValuePtr, Depth + 1);
            }
        }
        return;
    }
    
    // Handle sets - also sparse
    if (const FSetProperty* SetProp = CastField<FSetProperty>(Property))
    {
        const void* SetPtr = SetProp->ContainerPtrToValuePtr<void>(ContainerPtr);
        FScriptSetHelper SetHelper(SetProp, SetPtr);
        
        for (int32 i = 0; i < SetHelper.GetMaxIndex(); ++i)
        {
            if (SetHelper.IsValidIndex(i))
            {
                const uint8* ElementPtr = SetHelper.GetElementPtr(i);
                TraverseProperty(SetProp->ElementProp, ElementPtr, Depth + 1);
            }
        }
        return;
    }
    
    // Leaf property - read value
    // ... handle primitives, objects, etc.
}
```

#### Container Helper Construction

**Common crash: passing wrong pointer level to helpers.**

```cpp
// CORRECT
const void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(ContainerPtr);
FScriptArrayHelper ArrayHelper(ArrayProp, ArrayPtr);

// WRONG - passing outer container instead of array pointer
// FScriptArrayHelper ArrayHelper(ArrayProp, ContainerPtr);  // CRASH
```

#### Dynamic Struct Memory Allocation

```cpp
UScriptStruct* Struct = /* ... */;

// Allocate
uint8* Memory = (uint8*)FMemory::Malloc(
    Struct->GetStructureSize(),
    Struct->GetMinAlignment()
);

// Initialize (REQUIRED for non-POD)
Struct->InitializeStruct(Memory);

// Use...
for (TFieldIterator<FProperty> PropIt(Struct); PropIt; ++PropIt)
{
    // Memory is the container
    void* ValuePtr = PropIt->ContainerPtrToValuePtr<void>(Memory);
}

// Copy
Struct->CopyScriptStruct(DestMemory, Memory);

// Cleanup (REQUIRED for non-POD - prevents leaks)
Struct->DestroyStruct(Memory);
FMemory::Free(Memory);
```

---

### Components vs Actors

#### Reflection Access is Identical

FProperty access works the same way for Actors, Components, and plain UObjects:

```cpp
void InspectObject(UObject* Obj)  // Works for AActor*, UActorComponent*, or any UObject*
{
    for (TFieldIterator<FProperty> PropIt(Obj->GetClass()); PropIt; ++PropIt)
    {
        FProperty* Property = *PropIt;
        void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Obj);
        // Same access pattern regardless of object type
    }
}
```

#### Component Discovery Methods

```cpp
// Get all components of a type - prefer TInlineComponentArray to avoid heap alloc
TInlineComponentArray<UStaticMeshComponent*> MeshComps;
Actor->GetComponents<UStaticMeshComponent>(MeshComps);

// Find first component of type - O(n) with early exit
UStaticMeshComponent* Mesh = Actor->FindComponentByClass<UStaticMeshComponent>();

// Iterate with callback (can early-exit by returning false)
Actor->ForEachComponent<USceneComponent>(false, [](USceneComponent* Comp)
{
    // Process component
    return true;  // Continue iteration
});
```

#### Component Hierarchy Traversal

```cpp
void TraverseComponentTree(USceneComponent* Root, int32 Depth = 0)
{
    if (!Root) return;
    
    // Process Root...
    UE_LOG(LogTemp, Log, TEXT("%s%s"), 
        *FString::ChrN(Depth * 2, ' '), *Root->GetName());
    
    // Recurse children
    for (USceneComponent* Child : Root->GetAttachChildren())
    {
        TraverseComponentTree(Child, Depth + 1);
    }
}

// Start from actor's root
TraverseComponentTree(Actor->GetRootComponent());
```

#### Registration State Affects Property Validity

| State | Transform | World Ptr | Physics/Render | Other Actors |
|-------|-----------|-----------|----------------|--------------|
| Pre-RegisterComponent | ✓ Valid | ✗ Null | ✗ Not setup | ✗ |
| Post-RegisterComponent | ✓ Valid | ✓ Valid | ✓ Valid | ⚠ Partial |
| Post-BeginPlay | ✓ Valid | ✓ Valid | ✓ Valid | ✓ Valid |

```cpp
// Check if component is fully registered
if (Component->IsRegistered())
{
    // Safe to access World, physics, render proxies
}
```

#### Runtime Component Creation

```cpp
// Runtime-created components MUST be manually registered
UStaticMeshComponent* NewComp = NewObject<UStaticMeshComponent>(OwnerActor);
NewComp->SetupAttachment(OwnerActor->GetRootComponent());  // BEFORE registration
NewComp->SetStaticMesh(SomeMesh);
NewComp->RegisterComponent();  // REQUIRED - or component won't render/tick
```

#### Component Creation Method Detection

```cpp
EComponentCreationMethod Method = Component->CreationMethod;
switch (Method)
{
    case EComponentCreationMethod::Native:
        // Created via CreateDefaultSubobject in C++ constructor
        break;
    case EComponentCreationMethod::SimpleConstructionScript:
        // Added in Blueprint Components panel
        break;
    case EComponentCreationMethod::UserConstructionScript:
        // Created in Blueprint Construction Script
        break;
    case EComponentCreationMethod::Instance:
        // Created at runtime via NewObject
        break;
}
```

#### Transform Space Considerations

```cpp
// World space (absolute)
FVector WorldLoc = Component->GetComponentLocation();
FRotator WorldRot = Component->GetComponentRotation();
FVector WorldScale = Component->GetComponentScale();

// Local space (relative to parent)
FVector LocalLoc = Component->GetRelativeLocation();
FRotator LocalRot = Component->GetRelativeRotation();
FVector LocalScale = Component->GetRelativeScale3D();

// Setting transforms
Component->SetWorldTransform(NewWorldTransform);
Component->SetRelativeTransform(NewLocalTransform);

// Force update if needed
Component->UpdateComponentToWorld();
```

---

### PIE and Runtime Contexts

#### Context-Aware Operations (Phase 4)

AgentBridge provides transparent handling of different world contexts through the `FWorldContextCapabilities` system. Agents can query capabilities via the `GetCapabilities` command:

```cpp
// Get capabilities for current context
FWorldContextCapabilities Caps = FWorldContextManager::Get().GetCapabilities();

if (Caps.bCanUseTransactions)
{
    // Safe to use undo/redo
    FScopedAgentTransaction Trans(LOCTEXT("Modify", "Agent Modify"));
    // ...
}
else
{
    // Skip transaction - we're in PIE or packaged build
    UE_LOG(LogAgentBridge, Log, TEXT("Transactions unavailable: %s"),
           *Caps.TransactionUnavailableReason);
}
```

**Capability Matrix:**
| Feature | Editor | PIE | Packaged | Check |
|---------|--------|-----|----------|-------|
| Property iteration | ✓ | ✓ | ✓ | `bCanIterateProperties` |
| Function invocation | ✓ | ✓ | ✓ | `bCanInvokeFunctions` |
| Spawn/Destroy | ✓ | ✓ | ✓ | `bCanSpawnActors` |
| SetActorLabel | ✓ | ✓ | ✗ | `bCanSetActorLabel` |
| Transactions | ✓ | ✗ | ✗ | `bCanUseTransactions` |
| Metadata | ✓ | ✓ | ✗ | `bHasPropertyMetadata` |

#### World Type Detection

```cpp
UWorld* World = GetWorld();

// Check world type enum
switch (World->WorldType)
{
    case EWorldType::Editor:
        // Level editing, no gameplay
        break;
    case EWorldType::PIE:
        // Play In Editor
        break;
    case EWorldType::Game:
        // Standalone game or packaged
        break;
    case EWorldType::EditorPreview:
        // Mesh viewer, animation preview, etc.
        break;
}

// Check if gameplay is active
if (World->HasBegunPlay())
{
    // Game is running (PIE or standalone)
}
```

**Critical:** `GIsEditor` remains TRUE during PIE. Use `World->WorldType` for accurate detection.

#### Multiple PIE Worlds

```cpp
// Iterate ALL world contexts (handles multiple PIE clients)
for (const FWorldContext& Context : GEngine->GetWorldContexts())
{
    if (Context.WorldType == EWorldType::PIE)
    {
        UWorld* PIEWorld = Context.World();
        int32 PIEInstance = Context.PIEInstance;
        bool bIsServer = Context.RunAsDedicated || 
                         (PIEWorld->GetNetMode() < ENetMode::NM_Client);
    }
}
```

#### Editor vs Runtime API Availability

| Feature | Editor | PIE | Packaged |
|---------|--------|-----|----------|
| FProperty iteration | ✓ | ✓ | ✓ |
| Property metadata | ✓ | ✓ | ✗ Stripped |
| UFunction invocation | ✓ | ✓ | ✓ |
| GEditor pointer | ✓ | ✓ | ✗ Null |
| Transactions/Undo | ✓ | ✗ | ✗ |
| Asset loading | ✓ | ✓ | ✓ (if cooked) |

#### Compile-Time vs Runtime Checks

```cpp
#if WITH_EDITOR
    // This code doesn't exist in packaged builds
    GEditor->BeginTransaction(...);
#endif

// vs

if (GIsEditor)
{
    // This code EXISTS in packaged builds but branch never taken
    // GIsEditor is always false in packaged builds
}
```

**Use `WITH_EDITOR` for editor-only functionality. Use `GIsEditor` for runtime behavior branching.**

---

## Module 1: AgentBridgeCore

### PropertyAccessor.h

```cpp
#pragma once
#include "CoreMinimal.h"
#include "AgentBridgeTypes.h"

/**
 * Low-level property read/write using UE reflection.
 * Handles all FProperty types including nested containers.
 */
class AGENTBRIDGECORE_API FPropertyAccessor
{
public:
    // Core read - converts any property to transport format
    static FAgentPropertyValue ReadProperty(
        const void* Container, 
        FProperty* Property,
        int32 MaxDepth = 10
    );
    
    // Core write - sets property from transport format
    static bool WriteProperty(
        void* Container,
        FProperty* Property,
        const FAgentPropertyValue& Value
    );
    
    // Type introspection
    static EAgentPropertyType GetPropertyType(FProperty* Property);
    static FString GetPropertyTypeName(FProperty* Property);  // Human-readable
    static bool IsPropertyWritable(FProperty* Property);
    
    // Object reference handling
    static FString SerializeObjectReference(UObject* Object);
    static UObject* ResolveObjectReference(const FString& Reference, UClass* ExpectedClass = nullptr);
    
private:
    // Recursive handlers for complex types
    static FAgentPropertyValue ReadArrayProperty(const void* Container, FArrayProperty* Prop, int32 MaxDepth);
    static FAgentPropertyValue ReadStructProperty(const void* Container, FStructProperty* Prop, int32 MaxDepth);
    static FAgentPropertyValue ReadMapProperty(const void* Container, FMapProperty* Prop, int32 MaxDepth);
    static FAgentPropertyValue ReadObjectProperty(const void* Container, FObjectPropertyBase* Prop);
    
    static bool WriteArrayProperty(void* Container, FArrayProperty* Prop, const FAgentPropertyValue& Value);
    static bool WriteStructProperty(void* Container, FStructProperty* Prop, const FAgentPropertyValue& Value);
    static bool WriteMapProperty(void* Container, FMapProperty* Prop, const FAgentPropertyValue& Value);
    static bool WriteObjectProperty(void* Container, FObjectPropertyBase* Prop, const FAgentPropertyValue& Value);
};
```

### FunctionInvoker.h

```cpp
#pragma once
#include "CoreMinimal.h"
#include "AgentBridgeTypes.h"

/**
 * Dynamic UFunction invocation with parameter marshaling.
 * Handles hidden pins (WorldContext, self) automatically.
 */
class AGENTBRIDGECORE_API FFunctionInvoker
{
public:
    // Discover callable functions
    static TArray<UFunction*> GetCallableFunctions(
        UClass* Class,
        bool bIncludeParents = true,
        bool bBlueprintOnly = false
    );
    
    // Get function signature for external consumption
    static FAgentFunctionSignature GetFunctionSignature(UFunction* Function);
    
    // Check for hidden parameters
    static bool FunctionNeedsWorldContext(UFunction* Function);
    static FString GetWorldContextParamName(UFunction* Function);
    static bool FunctionHasHiddenSelfPin(UFunction* Function);
    
    // Invoke with automatic parameter handling
    static FAgentFunctionResult InvokeFunction(
        UObject* Target,
        UFunction* Function,
        const TMap<FString, FAgentPropertyValue>& Params,
        UWorld* WorldContext = nullptr
    );
    
    // Invoke on CDO for static-like functions
    static FAgentFunctionResult InvokeStaticFunction(
        UClass* Class,
        UFunction* Function,
        const TMap<FString, FAgentPropertyValue>& Params,
        UWorld* WorldContext = nullptr
    );
    
private:
    // Allocate and populate parameter memory
    static void* PrepareParameters(
        UFunction* Function,
        const TMap<FString, FAgentPropertyValue>& Params,
        UObject* WorldContextObject
    );
    
    // Extract return value and out params
    static FAgentFunctionResult ExtractResults(
        UFunction* Function,
        void* ParamBuffer
    );
    
    // Cleanup parameter memory
    static void CleanupParameters(UFunction* Function, void* ParamBuffer);
};
```

### TypeDiscovery.h

```cpp
#pragma once
#include "CoreMinimal.h"
#include "AgentBridgeTypes.h"

/**
 * Class, struct, and enum discovery and introspection.
 * Normalizes BP vs C++ naming differences.
 */
class AGENTBRIDGECORE_API FTypeDiscovery
{
public:
    // Class discovery
    static TArray<UClass*> GetAllClassesOfType(
        UClass* BaseClass,
        bool bIncludeAbstract = false,
        bool bBlueprintOnly = false
    );
    
    static UClass* FindClassByName(const FString& Name);  // Handles BP_Name or BP_Name_C
    static FAgentClassInfo GetClassInfo(UClass* Class);
    static TArray<FAgentPropertyInfo> GetClassProperties(
        UClass* Class,
        bool bIncludeParents = true,
        bool bIncludeHidden = false
    );
    
    // Struct discovery
    static UScriptStruct* FindStructByName(const FString& Name);
    static TArray<FAgentPropertyInfo> GetStructProperties(UScriptStruct* Struct);
    static bool IsUserDefinedStruct(UScriptStruct* Struct);
    
    // Enum discovery
    static UEnum* FindEnumByName(const FString& Name);
    static TArray<FAgentEnumValue> GetEnumValues(UEnum* Enum);
    static bool IsUserDefinedEnum(UEnum* Enum);
    
    // Name normalization utilities
    static FString GetDisplayClassName(UClass* Class);
    static FString GetDisplayPropertyName(FProperty* Property);
    static FString NormalizeClassName(const FString& Input);  // Adds/removes _C as needed
};
```

### AgentBridgeTypes.h

```cpp
#pragma once
#include "CoreMinimal.h"
#include "AgentBridgeTypes.generated.h"

UENUM(BlueprintType)
enum class EAgentPropertyType : uint8
{
    None,
    Bool,
    Int8, Int16, Int32, Int64,
    UInt8, UInt16, UInt32, UInt64,
    Float, Double,
    String,      // FString
    Name,        // FName
    Text,        // FText
    Vector,      // FVector
    Rotator,     // FRotator
    Transform,   // FTransform
    Color,       // FColor / FLinearColor
    Object,      // UObject* and TObjectPtr<>
    SoftObject,  // TSoftObjectPtr<>
    WeakObject,  // TWeakObjectPtr<>
    Class,       // TSubclassOf<> / UClass*
    Struct,      // Any UScriptStruct
    Enum,        // Any UEnum (byte value + name)
    Array,       // TArray<T>
    Map,         // TMap<K,V>
    Set,         // TSet<T>
    Unknown
};

USTRUCT()
struct AGENTBRIDGECORE_API FAgentPropertyValue
{
    GENERATED_BODY()
    
    UPROPERTY()
    EAgentPropertyType Type = EAgentPropertyType::None;
    
    UPROPERTY()
    FString StringValue;  // Most primitives serialize here
    
    UPROPERTY()
    TArray<uint8> BinaryValue;  // For types needing binary
    
    UPROPERTY()
    TArray<FAgentPropertyValue> ArrayValue;  // For arrays, sets
    
    UPROPERTY()
    TMap<FString, FAgentPropertyValue> StructValue;  // For structs, maps
    
    // Convenience constructors
    static FAgentPropertyValue FromBool(bool Value);
    static FAgentPropertyValue FromInt(int64 Value);
    static FAgentPropertyValue FromFloat(double Value);
    static FAgentPropertyValue FromString(const FString& Value);
    static FAgentPropertyValue FromVector(const FVector& Value);
    static FAgentPropertyValue FromObject(UObject* Object);
    
    // Convenience extractors
    bool AsBool() const;
    int64 AsInt() const;
    double AsFloat() const;
    FString AsString() const;
    FVector AsVector() const;
};

USTRUCT()
struct AGENTBRIDGECORE_API FAgentPropertyInfo
{
    GENERATED_BODY()
    
    UPROPERTY()
    FString PropertyName;
    
    UPROPERTY()
    FString DisplayName;  // Cleaned up for external use
    
    UPROPERTY()
    EAgentPropertyType Type = EAgentPropertyType::None;
    
    UPROPERTY()
    FString TypeName;  // Detailed type string (e.g., "TArray<FVector>")
    
    UPROPERTY()
    bool bIsReadOnly = false;
    
    UPROPERTY()
    bool bIsEditorOnly = false;
    
    UPROPERTY()
    FString Category;
    
    UPROPERTY()
    FString Description;
};

USTRUCT()
struct AGENTBRIDGECORE_API FAgentClassInfo
{
    GENERATED_BODY()
    
    UPROPERTY()
    FString ClassName;
    
    UPROPERTY()
    FString DisplayName;
    
    UPROPERTY()
    FString ClassPath;  // Full path for loading
    
    UPROPERTY()
    bool bIsBlueprintClass = false;
    
    UPROPERTY()
    bool bIsAbstract = false;
    
    UPROPERTY()
    FString ParentClassName;
    
    UPROPERTY()
    TArray<FString> ImplementedInterfaces;
};

USTRUCT()
struct AGENTBRIDGECORE_API FAgentFunctionSignature
{
    GENERATED_BODY()
    
    UPROPERTY()
    FString FunctionName;
    
    UPROPERTY()
    TArray<FAgentPropertyInfo> Parameters;
    
    UPROPERTY()
    FAgentPropertyInfo ReturnValue;
    
    UPROPERTY()
    bool bIsStatic = false;
    
    UPROPERTY()
    bool bIsBlueprintCallable = false;
    
    UPROPERTY()
    bool bNeedsWorldContext = false;
    
    UPROPERTY()
    FString Description;
};

USTRUCT()
struct AGENTBRIDGECORE_API FAgentFunctionResult
{
    GENERATED_BODY()
    
    UPROPERTY()
    bool bSuccess = false;
    
    UPROPERTY()
    FString ErrorMessage;
    
    UPROPERTY()
    FAgentPropertyValue ReturnValue;
    
    UPROPERTY()
    TMap<FString, FAgentPropertyValue> OutParams;
};

USTRUCT()
struct AGENTBRIDGECORE_API FAgentEnumValue
{
    GENERATED_BODY()
    
    UPROPERTY()
    FString Name;
    
    UPROPERTY()
    FString DisplayName;
    
    UPROPERTY()
    int64 Value = 0;
};
```

---

## Module 2: AgentBridgeRuntime

### WorldContextManager.h

```cpp
#pragma once
#include "CoreMinimal.h"

/**
 * Manages target world selection across editor, PIE, and game contexts.
 */
class AGENTBRIDGERUNTIME_API FWorldContextManager
{
public:
    static FWorldContextManager& Get();
    
    // Get the current target world
    UWorld* GetTargetWorld() const;
    
    // Explicit world selection (for multi-PIE support)
    void SetTargetWorldOverride(UWorld* World);
    void ClearTargetWorldOverride();
    
    // Context queries
    bool IsEditorWorld() const;
    bool IsPIEWorld() const;
    bool IsGameWorld() const;
    bool IsGameplayActive() const;  // PIE or Game with HasBegunPlay
    
    // Multi-world enumeration
    TArray<UWorld*> GetAllPIEWorlds() const;
    int32 GetPIEInstanceCount() const;
    
private:
    FWorldContextManager() = default;
    
    UWorld* ResolveWorld() const;  // Heuristic selection
    
    TWeakObjectPtr<UWorld> WorldOverride;
};
```

### ActorOperations.h

```cpp
#pragma once
#include "CoreMinimal.h"
#include "AgentBridgeTypes.h"

USTRUCT()
struct AGENTBRIDGERUNTIME_API FActorSpawnParams
{
    GENERATED_BODY()
    
    UPROPERTY()
    FString ClassPath;
    
    UPROPERTY()
    FTransform Transform;
    
    UPROPERTY()
    FString ActorLabel;  // Editor display name
    
    UPROPERTY()
    FString FolderPath;  // /MyFolder/SubFolder
    
    UPROPERTY()
    TMap<FString, FAgentPropertyValue> InitialProperties;
};

USTRUCT()
struct AGENTBRIDGERUNTIME_API FActorReference
{
    GENERATED_BODY()
    
    UPROPERTY()
    FString Guid;  // Most stable identifier
    
    UPROPERTY()
    FString Path;  // GetPathName() result
    
    UPROPERTY()
    FString Name;  // GetName() result
    
    UPROPERTY()
    FString Label;  // GetActorLabel() - editor only
    
    UPROPERTY()
    FString ClassName;
    
    // Resolution helpers
    AActor* Resolve() const;
    static FActorReference FromActor(AActor* Actor);
    bool IsValid() const;
};

class AGENTBRIDGERUNTIME_API FActorOperations
{
public:
    // Finding actors
    static TArray<FActorReference> QueryActors(
        UClass* ClassFilter = nullptr,
        const FString& NamePattern = TEXT(""),
        const FString& Tag = TEXT(""),
        const FBox* BoundsFilter = nullptr,
        int32 Limit = 1000
    );
    
    static AActor* ResolveActorReference(const FActorReference& Ref);
    static AActor* FindActorByGuid(const FGuid& Guid);
    static AActor* FindActorByLabel(const FString& Label);
    
    // CRUD operations
    static AActor* SpawnActor(const FActorSpawnParams& Params, FString& OutError);
    static bool DestroyActor(AActor* Actor);
    static AActor* DuplicateActor(AActor* Source, const FTransform& NewTransform);
    
    // Modification
    static bool SetActorTransform(AActor* Actor, const FTransform& Transform);
    static bool SetActorProperties(AActor* Actor, const TMap<FString, FAgentPropertyValue>& Properties);
    static bool SetActorLabel(AActor* Actor, const FString& NewLabel);
    static bool SetActorFolder(AActor* Actor, const FString& FolderPath);
    static bool ReparentActor(AActor* Actor, AActor* NewParent);  // Attachment
};
```

### PropertyPath.h

```cpp
#pragma once
#include "CoreMinimal.h"
#include "AgentBridgeTypes.h"

/**
 * Resolves property paths like "Mesh.RelativeLocation.X" or "Items[3].Name"
 */
class AGENTBRIDGERUNTIME_API FPropertyPath
{
public:
    // Parse and validate path
    static bool ParsePath(const FString& Path, TArray<FPropertyPathSegment>& OutSegments);
    
    // Read value at path
    static bool ReadPropertyPath(
        UObject* Root,
        const FString& Path,
        FAgentPropertyValue& OutValue
    );
    
    // Write value at path
    static bool WritePropertyPath(
        UObject* Root,
        const FString& Path,
        const FAgentPropertyValue& Value
    );
    
    // Validate path exists
    static bool ValidatePath(UClass* Class, const FString& Path, FString& OutError);
    
private:
    static bool ResolvePath(
        void* Container,
        UStruct* ContainerType,
        const TArray<FPropertyPathSegment>& Segments,
        int32 SegmentIndex,
        void*& OutContainer,
        FProperty*& OutProperty
    );
};

struct FPropertyPathSegment
{
    FString Name;
    int32 ArrayIndex = INDEX_NONE;  // -1 if not array access
    FString MapKey;  // Empty if not map access
    
    bool IsArrayAccess() const { return ArrayIndex != INDEX_NONE; }
    bool IsMapAccess() const { return !MapKey.IsEmpty(); }
};
```

### TransactionWrapper.h

```cpp
#pragma once
#include "CoreMinimal.h"

/**
 * RAII wrapper for editor transactions (undo support).
 * No-op in non-editor builds.
 */
class AGENTBRIDGERUNTIME_API FScopedAgentTransaction
{
public:
    explicit FScopedAgentTransaction(const FText& Description);
    ~FScopedAgentTransaction();
    
    // Mark objects as modified (required for undo)
    void Modify(UObject* Object);
    
    // Cancel transaction (won't commit on destruction)
    void Cancel();
    
    // Check if transactions are supported in current context
    static bool AreTransactionsSupported();
    
private:
#if WITH_EDITOR
    int32 TransactionIndex = INDEX_NONE;
    bool bCancelled = false;
#endif
};

// Usage:
// {
//     FScopedAgentTransaction Trans(LOCTEXT("SpawnActor", "Agent: Spawn Actor"));
//     Trans.Modify(Actor);
//     // ... make changes ...
// }  // Transaction commits here, Ctrl+Z will undo
```

### ClassNormalization.h

```cpp
#pragma once
#include "CoreMinimal.h"

/**
 * Handles BP vs C++ naming differences.
 */
class AGENTBRIDGERUNTIME_API FClassNormalization
{
public:
    // Class name resolution
    // "BP_MyActor" -> finds BP_MyActor_C
    // "AMyActor" -> finds AMyActor
    // "/Game/BP_MyActor.BP_MyActor_C" -> loads from path
    static UClass* ResolveClassName(const FString& Name);
    
    // Get clean display name (strips _C suffix, etc.)
    static FString GetDisplayClassName(UClass* Class);
    
    // Property name handling
    // BP properties have GUID suffixes: "MyVar_23_abc123" -> "MyVar"
    static FString GetDisplayPropertyName(FProperty* Property);
    static FProperty* FindPropertyByDisplayName(UClass* Class, const FString& DisplayName);
    
    // Build property lookup cache for a class
    static TMap<FString, FProperty*> BuildPropertyDisplayNameMap(UClass* Class);
    
    // Path utilities
    static FString GetClassPath(UClass* Class);
    static bool IsBlueprintClass(UClass* Class);
    static bool IsValidClassName(const FString& Name);
};
```

---

## Module 3: AgentBridgeScripting

### AgentCommand.h

```cpp
#pragma once
#include "CoreMinimal.h"
#include "AgentBridgeTypes.h"

UENUM()
enum class EAgentCommandStatus : uint8
{
    Success,
    PartialSuccess,  // Some operations succeeded
    Failed,
    InvalidParams,
    NotSupported,
    Unauthorized
};

USTRUCT()
struct AGENTBRIDGESCRIPTING_API FAgentCommandResult
{
    GENERATED_BODY()
    
    UPROPERTY()
    EAgentCommandStatus Status = EAgentCommandStatus::Failed;
    
    UPROPERTY()
    FString Message;
    
    UPROPERTY()
    FString JsonPayload;  // Flexible result data
    
    static FAgentCommandResult Ok(const FString& Payload = TEXT(""));
    static FAgentCommandResult Fail(const FString& Message);
    static FAgentCommandResult InvalidParams(const FString& Message);
};

/**
 * Base class for agent commands.
 */
class AGENTBRIDGESCRIPTING_API IAgentCommand
{
public:
    virtual ~IAgentCommand() = default;
    
    virtual FAgentCommandResult Execute() = 0;
    virtual FString GetCommandName() const = 0;
    virtual FString GetDescription() const { return TEXT(""); }
    virtual bool SupportsUndo() const { return true; }
    virtual bool RequiresEditorContext() const { return false; }
};
```

### CommandExecutor.h

```cpp
#pragma once
#include "CoreMinimal.h"
#include "AgentCommand.h"

/**
 * Executes commands on the game thread with proper error handling.
 */
class AGENTBRIDGESCRIPTING_API FAgentCommandExecutor
{
public:
    static FAgentCommandExecutor& Get();
    
    // Execute command (ensures game thread)
    FAgentCommandResult ExecuteCommand(TUniquePtr<IAgentCommand> Command);
    
    // Execute from serialized form (for gRPC)
    FAgentCommandResult ExecuteFromJson(const FString& CommandName, const FString& ParamsJson);
    
    // Queue command from another thread
    TFuture<FAgentCommandResult> QueueCommand(TUniquePtr<IAgentCommand> Command);
    
    // Registration
    void RegisterCommandFactory(const FString& Name, TFunction<TUniquePtr<IAgentCommand>(const FString&)> Factory);
    
    // Must be called from game thread tick
    void ProcessPendingCommands();
    
private:
    FAgentCommandExecutor() = default;
    
    TMap<FString, TFunction<TUniquePtr<IAgentCommand>(const FString&)>> CommandFactories;
    TQueue<TPair<TPromise<FAgentCommandResult>, TUniquePtr<IAgentCommand>>> PendingCommands;
    FCriticalSection PendingCommandsLock;
};
```

---

## Module 4: AgentBridgeServer

### Proto Definitions (Protos/service.proto)

```protobuf
syntax = "proto3";
package agentbridge;

import "common.proto";

service AgentBridge {
    // World operations
    rpc GetWorldInfo(GetWorldInfoRequest) returns (GetWorldInfoResponse);
    rpc QueryActors(QueryActorsRequest) returns (QueryActorsResponse);
    rpc InspectActor(InspectActorRequest) returns (InspectActorResponse);
    
    // Actor manipulation
    rpc SpawnActor(SpawnActorRequest) returns (SpawnActorResponse);
    rpc ModifyActor(ModifyActorRequest) returns (ModifyActorResponse);
    rpc DeleteActors(DeleteActorsRequest) returns (DeleteActorsResponse);
    rpc CallFunction(CallFunctionRequest) returns (CallFunctionResponse);
    
    // Component operations
    rpc GetComponents(GetComponentsRequest) returns (GetComponentsResponse);
    rpc InspectComponent(InspectComponentRequest) returns (InspectComponentResponse);
    rpc ModifyComponent(ModifyComponentRequest) returns (ModifyComponentResponse);
    
    // Discovery
    rpc ListClasses(ListClassesRequest) returns (ListClassesResponse);
    rpc InspectClass(InspectClassRequest) returns (InspectClassResponse);
    rpc FindAssets(FindAssetsRequest) returns (FindAssetsResponse);
    
    // Streaming (for watching changes)
    rpc Subscribe(SubscribeRequest) returns (stream StreamEvent);
    
    // Batch operations
    rpc ExecuteBatch(BatchRequest) returns (BatchResponse);
}
```

### AgentBridgeServer.h

```cpp
#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class AGENTBRIDGESERVER_API FAgentBridgeServerModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    
    void StartServer(int32 Port = 50051);
    void StopServer();
    bool IsServerRunning() const;
    int32 GetPort() const;
    
    // Settings
    void SetPort(int32 NewPort);
    void SetReadOnly(bool bReadOnly);  // Disable write operations
    
private:
    void OnEditorInitialized();
    void RegisterSettings();
    void Tick(float DeltaTime);
    
    TUniquePtr<class FAgentBridgeServerImpl> ServerImpl;
    FDelegateHandle TickHandle;
    int32 CurrentPort = 50051;
    bool bAutoStart = true;
};
```

---

## Extended Features

### DataAsset Support

Query and inspect UDataAsset instances and DataTables in the project.

**Python Client Methods:**
```python
# List all data assets
assets = client.list_data_assets(class_filter="GameDataAsset", limit=50)

# Get detailed asset info
details = client.get_data_asset_details("/Game/Data/DA_PlayerStats")

# Query data table rows
rows = client.get_data_table_rows("/Game/Data/DT_Items", row_filter="Weapon")
```

**Console Commands:**
- `AgentBridge.ListDataAssets [ClassFilter] [Limit]` - List data assets
- `AgentBridge.GetDataAsset <Path>` - Inspect data asset

### Viewport/Scene Capture

Capture viewport images and scene depth/normals for AI vision capabilities.

**Python Client Methods:**
```python
# Capture viewport (returns PNG base64)
capture = client.capture_viewport(width=1920, height=1080)

# Capture from scene capture component
result = client.capture_scene("SceneCaptureActor", capture_type="color")  # or "depth", "normals"
```

**Console Commands:**
- `AgentBridge.CaptureViewport [Width] [Height]` - Capture editor viewport

### Audio Capture

Capture and analyze audio for understanding scene sounds.

**Python Client Methods:**
```python
# Analyze audio at a location
analysis = client.analyze_audio_at_location(x=100, y=200, z=300, radius=1000)

# Capture audio stream
audio = client.capture_audio(duration_ms=1000, sample_rate=44100)
```

### Material Operations

Discover, inspect, and modify materials on actors.

**Python Client Methods:**
```python
# List materials in project
materials = client.list_materials(filter_pattern="Wood", limit=20)

# Get material details
info = client.get_material_info("/Game/Materials/M_Wood")

# Create dynamic material instance
instance = client.create_material_instance("/Game/Materials/M_Wood", "MyInstance")

# Set material parameter on actor
client.set_material_parameter("MyActor", "BaseColor", (1.0, 0.5, 0.2, 1.0), "Vector")

# Apply material to actor
client.apply_material_to_actor("MyActor", "/Game/Materials/M_Wood")
```

**Console Commands:**
- `AgentBridge.ListMaterials [Filter] [Limit]` - List project materials
- `AgentBridge.GetMaterial <Path>` - Get material info and parameters
- `AgentBridge.SetMaterialParam <Actor> <Param> <Value> [Type]` - Set material parameter

### PCG Operations

Interact with Procedural Content Generation actors.

**Python Client Methods:**
```python
# List PCG actors
actors = client.list_pcg_actors(pattern="Forest")

# Regenerate PCG graph
result = client.regenerate_pcg("PCG_ForestGenerator")

# Set PCG parameter
client.set_pcg_parameter("PCG_ForestGenerator", "Density", "0.5")
```

**Console Commands:**
- `AgentBridge.ListPCG [Pattern]` - List PCG actors in world

---

## Debugging Utilities

Build these utilities early—they'll save enormous time during development.

### Console Commands

```cpp
// Register in module StartupModule()
static FAutoConsoleCommandWithWorldAndArgs DumpActorCmd(
    TEXT("AgentBridge.DumpActor"),
    TEXT("Dump reflection data for an actor. Usage: AgentBridge.DumpActor <ActorName>"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FAgentBridgeDebug::DumpActor)
);

static FAutoConsoleCommandWithWorldAndArgs DumpClassCmd(
    TEXT("AgentBridge.DumpClass"),
    TEXT("Dump class schema. Usage: AgentBridge.DumpClass <ClassName>"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FAgentBridgeDebug::DumpClass)
);

static FAutoConsoleCommandWithWorldAndArgs TestPropertyPathCmd(
    TEXT("AgentBridge.TestPath"),
    TEXT("Test property path resolution. Usage: AgentBridge.TestPath <ActorName> <PropertyPath>"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FAgentBridgeDebug::TestPropertyPath)
);

static FAutoConsoleCommand ListWorldsCmd(
    TEXT("AgentBridge.ListWorlds"),
    TEXT("List all world contexts"),
    FConsoleCommandDelegate::CreateStatic(&FAgentBridgeDebug::ListWorlds)
);
```

### Debug Functions

```cpp
// AgentBridgeDebug.h
class AGENTBRIDGERUNTIME_API FAgentBridgeDebug
{
public:
    // Dump all properties of an object with values
    static void DumpObject(UObject* Object, int32 MaxDepth = 3);
    
    // Dump class schema (properties + functions) without values
    static void DumpClassSchema(UClass* Class);
    
    // Test property path resolution
    static void TestPropertyPath(UObject* Object, const FString& Path);
    
    // Show BP vs native differences for a class
    static void AnalyzeClass(UClass* Class);
    
    // List all functions with their signatures
    static void DumpFunctions(UClass* Class);
    
    // Console command handlers
    static void DumpActor(const TArray<FString>& Args, UWorld* World);
    static void DumpClass(const TArray<FString>& Args, UWorld* World);
    static void TestPropertyPath(const TArray<FString>& Args, UWorld* World);
    static void ListWorlds();
    
    // Logging helpers
    static FString PropertyToDebugString(FProperty* Property, const void* ValuePtr);
    static FString PropertyFlagsToString(EPropertyFlags Flags);
    static FString FunctionFlagsToString(EFunctionFlags Flags);
};
```

### Useful Built-in Commands

Reference these existing UE commands during debugging:

```
obj dump <ObjectName>          - Dump all properties of an object
obj list class=<ClassName>     - List all instances of a class
displayall <Class> <Property>  - Show property across all instances
obj refs name=<ObjectName>     - Show what references an object
obj classes                    - List all classes
obj types                      - List loaded types
```

---

## Autonomous Compilation Setup

To enable Claude Code to compile and test the plugin without human intervention:

### Windows Environment Requirements

1. **Visual Studio 2022** with:
   - MSVC v143 toolset
   - Windows 10/11 SDK (10.0.18362 or later)
   - .NET Framework 4.6.2 targeting pack

2. **Environment Variables** (add to system PATH):
   ```
   SET UE_ROOT=C:\Program Files\Epic Games\UE_5.7
   SET PATH=%PATH%;%UE_ROOT%\Engine\Build\BatchFiles
   SET PATH=%PATH%;%UE_ROOT%\Engine\Binaries\Win64
   ```

### Compilation Commands

```batch
:: Compile plugin (Development Editor)
"%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" ^
    MyProjectEditor Win64 Development ^
    -Project="C:\Projects\MyProject\MyProject.uproject" ^
    -WaitMutex -FromMsBuild

:: Compile plugin (Shipping for packaging test)
"%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" ^
    MyProject Win64 Shipping ^
    -Project="C:\Projects\MyProject\MyProject.uproject" ^
    -WaitMutex

:: Clean build
"%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" ^
    MyProjectEditor Win64 Development ^
    -Project="C:\Projects\MyProject\MyProject.uproject" ^
    -Clean
```

### Running Automated Tests

```batch
:: Run automation tests headlessly
"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
    "C:\Projects\MyProject\MyProject.uproject" ^
    -ExecCmds="Automation RunTests Project.AgentBridge" ^
    -unattended -nopause -NullRHI ^
    -TestExit="Automation Test Queue Empty" ^
    -ReportOutputPath="C:\Projects\MyProject\TestResults"
```

### Log File Locations

```
Build logs:     %UE_ROOT%\Engine\Programs\UnrealBuildTool\Log.txt
Editor logs:    <Project>\Saved\Logs\<Project>.log
Test reports:   <Project>\Saved\Automation\Reports\
Crash dumps:    <Project>\Saved\Crashes\
```

### Common Error Patterns

| Error Pattern | Cause | Fix |
|--------------|-------|-----|
| `Expected a GENERATED_BODY()` | Missing or misplaced macro | Add GENERATED_BODY() after access specifier |
| `Unrecognized type 'X'` | Type not reflected or missing include | Add USTRUCT/UCLASS or #include |
| `LNK2019 unresolved external` | Missing module dependency | Add to PublicDependencyModuleNames in .Build.cs |
| `error C2027: use of undefined type` | Missing forward declaration or include | Add forward decl or include header |
| `.generated.h must be last` | Include order wrong | Move .generated.h to end of includes |

### Recommended Build.cs Dependencies

```csharp
// AgentBridgeCore.Build.cs
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
});

// AgentBridgeRuntime.Build.cs
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "AgentBridgeCore",
});

PrivateDependencyModuleNames.AddRange(new string[] {
    "UnrealEd",  // For editor operations
});

// AgentBridgeServer.Build.cs
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "AgentBridgeCore",
    "AgentBridgeRuntime",
    "AgentBridgeScripting",
    // gRPC module - depends on how you integrate it
});
```

---

## Implementation Order

### Phase 1: Foundation (Week 1-2)
1. Create plugin structure with all module folders and Build.cs files
2. Implement `FAgentPropertyValue` type and basic serialization
3. Implement `FPropertyAccessor` for primitive types (bool, int, float, string, vector)
4. Implement `FTypeDiscovery::FindClassByName` with BP/C++ normalization
5. **Build debug console commands early** — you'll need them
6. Write unit tests for reflection primitives

### Phase 2: Complex Types (Week 2-3)
7. Extend `FPropertyAccessor` for arrays using `FScriptArrayHelper`
8. Extend for structs with recursive traversal
9. Extend for maps/sets with sparse index handling
10. Implement object pointer serialization/resolution
11. Implement `FPropertyPath` for nested access (e.g., "Mesh.Materials[0].Color")
12. Add `FClassNormalization` for BP property name cleanup

### Phase 3: Actor Operations (Week 3-4)
13. Implement `FWorldContextManager`
14. Implement `FActorOperations` (query, spawn, delete, modify)
15. Implement `FComponentOperations`
16. Implement `FScopedAgentTransaction` for undo support
17. Build `FAgentBridgeDebug::DumpActor` to validate everything works

### Phase 4: Function Invocation (Week 4-5)
18. Implement `FFunctionInvoker::GetFunctionSignature`
19. Implement hidden parameter detection (WorldContext, etc.)
20. Implement `FFunctionInvoker::InvokeFunction` with param marshaling
21. Test with various function signatures (in/out params, return values)

### Phase 5: Command Layer (Week 5-6)
22. Implement command pattern base classes
23. Implement all query commands (GetWorldInfo, QueryActors, InspectActor, etc.)
24. Implement all manipulation commands (SpawnActor, ModifyActor, etc.)
25. Implement `FAgentCommandExecutor` with game thread dispatch

### Phase 6: gRPC Integration (Week 6-7)
26. Study Tempo's gRPC integration (check your installation)
27. Integrate gRPC library into build
28. Generate proto stubs
29. Implement server wrapper with threading
30. Implement service methods routing to commands
31. Test basic RPC calls from Python

### Phase 7: Streaming & Polish (Week 7-8)
32. Implement world event delegates (actor spawn/destroy)
33. Implement property change polling
34. Implement streaming RPC
35. Build Python client package
36. Implement MCP server
37. Documentation and examples

---

## Risk Mitigation

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| gRPC integration complexity | High | High | Study Tempo first; have HTTP/JSON fallback plan |
| Thread safety crashes | Medium | High | Strict game-thread policy; use TWeakObjectPtr |
| BP property name chaos | High | Medium | Build robust normalization + caching early |
| GC invalidating references | Medium | High | Always use TWeakObjectPtr for stored refs |
| Editor-only code in runtime | Low | Medium | Test with Shipping builds early and often |
| Deep nested struct serialization | Medium | Medium | Implement depth limits; lazy loading |
| Performance on large worlds | Medium | Low | Cache property lookups; batch operations |

---

## Quick Reference: Key Patterns

### Safe Object Access
```cpp
TWeakObjectPtr<AActor> WeakActor = SomeActor;
// Later...
if (AActor* Actor = WeakActor.Get())
{
    // Safe to use
}
```

### Property Value Reading
```cpp
FProperty* Prop = Class->FindPropertyByName(PropName);
void* Container = Object;  // or struct memory
float Value;
Prop->GetValue_InContainer(Container, &Value);
```

### Array Iteration
```cpp
FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop);
void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(Container);
FScriptArrayHelper Helper(ArrayProp, ArrayPtr);
for (int32 i = 0; i < Helper.Num(); i++)
{
    void* ElementPtr = Helper.GetRawPtr(i);
    // Element is container for Inner property
}
```

### Map Iteration (Sparse!)
```cpp
FMapProperty* MapProp = CastField<FMapProperty>(Prop);
void* MapPtr = MapProp->ContainerPtrToValuePtr<void>(Container);
FScriptMapHelper Helper(MapProp, MapPtr);
for (int32 i = 0; i < Helper.GetMaxIndex(); i++)
{
    if (Helper.IsValidIndex(i))  // CRITICAL
    {
        void* Key = Helper.GetKeyPtr(i);
        void* Value = Helper.GetValuePtr(i);
    }
}
```

### Game Thread Dispatch
```cpp
AsyncTask(ENamedThreads::GameThread, [WeakObj = TWeakObjectPtr<UObject>(MyObj)]()
{
    if (UObject* Obj = WeakObj.Get())
    {
        // Safe UObject work here
    }
});
```

### Transaction Wrapper
```cpp
{
    FScopedAgentTransaction Trans(LOCTEXT("ModifyActor", "Agent: Modify Actor"));
    Trans.Modify(Actor);
    Actor->SetActorLocation(NewLocation);
}  // Undo point created
```

---

*Document Version: 3.0*
*Last Updated: December 2024*
*Target Engine: Unreal Engine 5.6*
*Extended Features: DataAssets, Capture, Audio, Materials, PCG*
