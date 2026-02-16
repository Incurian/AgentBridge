# AgentBridgeCore

Low-level Unreal Engine reflection primitives for reading/writing properties,
invoking functions, and discovering types.

---

## Table of Contents

- [What Is This Plugin?](#what-is-this-plugin)
- [What Is "Reflection" in Unreal Engine?](#what-is-reflection-in-unreal-engine)
- [Where Does This Fit in AgentBridge?](#where-does-this-fit-in-agentbridge)
- [Plugin Structure](#plugin-structure)
- [Key Classes](#key-classes)
  - [FPropertyAccessor](#fpropertyaccessor---readwrite-any-property)
  - [FAgentPropertyPath](#fagentpropertypath---dot-notation-property-paths)
  - [FFunctionInvoker](#ffunctioninvoker---call-any-function-dynamically)
  - [FTypeDiscovery](#ftypediscovery---find-classesstructsenums)
  - [FAgentPropertyValue](#fagentpropertyvalue---transport-format-for-values)
  - [FAgentPropertyInfo and Friends](#fagentpropertyinfo-and-friends---metadata-types)
- [Dependencies](#dependencies)
- [Module Configuration](#module-configuration)
- [Console Commands for Debugging](#console-commands-for-debugging)
- [Further Reading](#further-reading)

---

## What Is This Plugin?

AgentBridgeCore is a **standalone Unreal Engine plugin** that provides the foundation layer
for the entire AgentBridge ecosystem. It answers a simple question: *how do you let an
external program read and write any property on any UObject in Unreal Engine?*

The answer is Unreal's **reflection system** - a set of C++ classes that describe, at
runtime, what properties and functions exist on any object. AgentBridgeCore wraps this
reflection system into clean, reusable C++ utilities that the rest of AgentBridge builds on.

This plugin has **no dependencies on other AgentBridge plugins**. It only depends on
standard Engine modules (Core, CoreUObject, Engine). The other three AgentBridge plugins
(Runtime, Scripting, Server) all depend on this one, but this one stands alone at the
bottom of the stack.

---

## What Is "Reflection" in Unreal Engine?

If you are new to Unreal Engine or C++ game engines, "reflection" might be an unfamiliar
term. Here is a quick primer.

In standard C++, once your code is compiled, all the type information (variable names,
class hierarchies, function signatures) is gone. The compiled binary only has raw memory
addresses. There is no built-in way to ask "what properties does this object have?" at
runtime.

Unreal Engine solves this by generating extra metadata for any class, struct, or function
that uses the `UCLASS()`, `USTRUCT()`, `UPROPERTY()`, or `UFUNCTION()` macros. This
metadata is called the **reflection system** (or sometimes the "property system"), and it
lets you do things like:

- **Enumerate properties at runtime**: "This actor has a `Health` float, a `Location`
  FVector, and a `Tags` TArray of FStrings."
- **Read and write property values by name**: Given the string `"Health"`, find the
  matching FProperty, compute its memory offset, and read or write the float value.
- **Call functions by name**: Given the string `"K2_SetActorLocation"`, find the matching
  UFunction, set up its parameters in memory, and invoke it.
- **Discover the class hierarchy**: "PointLight inherits from Light, which inherits from
  Actor."

The key classes in UE's reflection system that this plugin works with are:

| UE Class | What It Represents |
|----------|-------------------|
| `FProperty` | A single property (variable) on a class or struct. Knows its type, offset, and flags. |
| `FBoolProperty`, `FFloatProperty`, `FStructProperty`, `FArrayProperty`, etc. | Specific property type subclasses. Each knows how to read/write its own type. |
| `UFunction` | A function that can be called via reflection. Knows its parameters and return type. |
| `UClass` | A class in the UE type system. Knows its properties, functions, and parent class. |
| `UScriptStruct` | A struct in the UE type system. Like UClass but for value types. |
| `UEnum` | An enumeration. Knows its named values. |

AgentBridgeCore wraps all of these into higher-level utilities (`FPropertyAccessor`,
`FFunctionInvoker`, `FTypeDiscovery`) that handle the many edge cases, type conversions,
and Blueprint quirks so the rest of the codebase does not have to.

---

## Where Does This Fit in AgentBridge?

AgentBridge consists of 4 separate Unreal Engine plugins. There is no wrapper plugin -
Unreal Build Tool (UBT) discovers all 4 independently by scanning the `Plugins/AgentBridge/`
directory recursively.

The dependency chain flows in one direction:

```
AgentBridgeServer   (gRPC/HTTP server, proto definitions)
        |
        v
AgentBridgeScripting (command dispatch, JSON serialization)
        |
        v
AgentBridgeRuntime   (world context, actor operations, component resolution)
        |
        v
AgentBridgeCore      (reflection primitives)  <-- YOU ARE HERE
```

Each higher-level plugin adds capabilities:

| Plugin | What It Adds |
|--------|-------------|
| **AgentBridgeCore** | Can read/write any property, invoke functions, discover types. Knows nothing about actors, worlds, or networking. |
| **AgentBridgeRuntime** | Adds world context - can find actors by name/class, resolve component paths, handle transforms. Uses Core for the actual property reads/writes. |
| **AgentBridgeScripting** | Adds command dispatch - takes JSON commands like `{"action":"get_property","actor":"MyLight","path":"Intensity"}` and routes them to the right Runtime/Core calls. |
| **AgentBridgeServer** | Adds networking - exposes everything via gRPC (port 10001) and HTTP (port 8080) so external programs (AI agents, Python scripts) can control the editor remotely. |

---

## Plugin Structure

```
AgentBridgeCore/
|-- AgentBridgeCore.uplugin       # Plugin descriptor (discovered by UBT)
|-- CLAUDE.md                     # Deep implementation docs for AI assistants
|-- README.md                     # This file
|-- Source/AgentBridgeCore/
    |-- AgentBridgeCore.Build.cs  # Build configuration (dependencies)
    |-- Public/                   # Header files (public API)
    |   |-- AgentBridgeCore.h     # Module interface
    |   |-- AgentBridgeTypes.h    # Shared data types (FAgentPropertyValue, etc.)
    |   |-- PropertyAccessor.h    # Property read/write
    |   |-- AgentPropertyPath.h   # Dot-notation path resolution
    |   |-- FunctionInvoker.h     # Dynamic function calls
    |   |-- TypeDiscovery.h       # Class/struct/enum discovery
    |-- Private/                  # Implementation files
        |-- AgentBridgeCore.cpp   # Module startup/shutdown
        |-- AgentBridgeTypes.cpp  # Type constructors and helpers
        |-- PropertyAccessor.cpp  # Property read/write implementation
        |-- AgentPropertyPath.cpp # Path parsing and resolution
        |-- FunctionInvoker.cpp   # Function invocation implementation
        |-- TypeDiscovery.cpp     # Discovery implementation
```

---

## Key Classes

### FPropertyAccessor - Read/Write Any Property

**File:** `Public/PropertyAccessor.h`, `Private/PropertyAccessor.cpp`

This is the core workhorse. Given a pointer to any UObject (or struct) and an FProperty
describing one of its fields, `FPropertyAccessor` can:

- **Read** the property's current value and convert it to an `FAgentPropertyValue` (our
  transport format that can be serialized to JSON or gRPC).
- **Write** a new value from an `FAgentPropertyValue` back into the property, handling
  type conversion where possible (e.g., string `"123"` to int32).

It handles every FProperty type that Unreal supports:

| Property Type | Examples |
|---------------|----------|
| Numeric | `int8`, `int32`, `int64`, `uint8`, `float`, `double` |
| String-like | `FString`, `FName`, `FText` |
| Boolean | `bool` |
| Struct | `FVector`, `FRotator`, `FTransform`, `FColor`, custom structs |
| Container | `TArray<>`, `TMap<>`, `TSet<>` |
| Object reference | `UObject*`, `TObjectPtr<>`, `TSoftObjectPtr<>`, `TWeakObjectPtr<>`, `TSubclassOf<>` |
| Enum | C++ enums, Blueprint enums |

Key methods:

```cpp
// Read a property value
FAgentPropertyValue Value = FPropertyAccessor::ReadProperty(MyActor, SomeProperty);

// Write a property value (normal - calculates memory offset from container)
bool Ok = FPropertyAccessor::WriteProperty(MyActor, SomeProperty, NewValue);

// Write a property value (direct - when you already have the exact memory address)
// Used after path resolution, which returns a direct pointer
bool Ok = FPropertyAccessor::WritePropertyDirect(ResolvedPtr, SomeProperty, NewValue);
```

**Why two write methods?** When you access a top-level property on an object, Unreal needs
to calculate where in memory that property lives (using `ContainerPtrToValuePtr`). But when
you use dot-notation paths like `"MyStruct.InnerField"`, the path resolution code already
computes the exact address. Calling `ContainerPtrToValuePtr` again would apply the offset
twice, corrupting the write. `WritePropertyDirect` skips the offset calculation for this case.

### FAgentPropertyPath - Dot-Notation Property Paths

**File:** `Public/AgentPropertyPath.h`, `Private/AgentPropertyPath.cpp`

This class lets you access deeply nested properties using string paths like those used in
many scripting languages:

```
"Location"                       -> Actor's Location property
"Location.X"                     -> X component of Location vector
"Components[0]"                  -> First element of Components array
"Inventory[3].Name"              -> Name property of 4th inventory item
"Stats[\"Strength\"]"            -> Value for "Strength" key in a TMap
"Equipment[\"Weapon\"].Damage"   -> Nested access through a map into a struct
```

It parses these paths into segments, then walks through the object's memory following each
segment until it reaches the target value.

Key methods:

```cpp
// Read a nested value
FPropertyPathResult Result = FAgentPropertyPath::GetValue(MyActor, "RootComponent.RelativeLocation.X");
if (Result.bSuccess)
{
    double X = Result.Value.AsFloat();
}

// Write a nested value
bool Ok = FAgentPropertyPath::SetValue(MyActor, "RootComponent.RelativeLocation.X", FAgentPropertyValue(42.0));

// Check if a path exists
bool Exists = FAgentPropertyPath::PathExists(MyActor, "RootComponent.RelativeLocation.X");
```

### FFunctionInvoker - Call Any Function Dynamically

**File:** `Public/FunctionInvoker.h`, `Private/FunctionInvoker.cpp`

This class lets you call any `UFUNCTION` on any object by name, passing parameters as
key-value pairs. It handles all the tricky parts of UE function invocation:

- Allocating and initializing parameter memory blocks
- Filling in "hidden" parameters like WorldContext and Self
- Converting `FAgentPropertyValue` parameters to native types
- Extracting return values and output parameters after the call

Key methods:

```cpp
// Find and invoke a function
UFunction* Func = FFunctionInvoker::FindFunction(MyActor->GetClass(), "K2_SetActorLocation");

TMap<FString, FAgentPropertyValue> Params;
Params.Add("NewLocation", FAgentPropertyValue::FromVector(FVector(100, 0, 0)));
Params.Add("bSweep", FAgentPropertyValue::FromBool(false));

FAgentFunctionResult Result = FFunctionInvoker::InvokeFunction(MyActor, Func, Params);
if (Result.bSuccess)
{
    // Function executed successfully
}
```

**Current limitation:** The gRPC layer (`call_function` MCP tool) currently only supports
zero-argument void-return functions. Full argument support is planned for a future release.
The C++ layer (`FFunctionInvoker`) supports full argument passing - it is only the gRPC
transport that is limited.

### FTypeDiscovery - Find Classes/Structs/Enums

**File:** `Public/TypeDiscovery.h`, `Private/TypeDiscovery.cpp`

This class lets you explore UE's type system at runtime. You can:

- Find a class by name (handles C++ prefixes, Blueprint `_C` suffixes, full paths)
- List all classes derived from a base class
- Get all properties and functions on a class
- Find and inspect structs and enums

Key methods:

```cpp
// Find a class - handles many input formats
UClass* A = FTypeDiscovery::FindClassByName("StaticMeshActor");    // Short name
UClass* B = FTypeDiscovery::FindClassByName("AStaticMeshActor");   // With prefix
UClass* C = FTypeDiscovery::FindClassByName("BP_MyActor");         // Blueprint (adds _C)
UClass* D = FTypeDiscovery::FindClassByName("/Script/Engine.StaticMeshActor"); // Full path

// List all actor classes
TArray<UClass*> ActorClasses = FTypeDiscovery::GetAllClassesOfType(AActor::StaticClass());

// Get property list
TArray<FAgentPropertyInfo> Props = FTypeDiscovery::GetClassProperties(SomeClass);

// Get function signatures
TArray<FAgentFunctionSignature> Funcs = FTypeDiscovery::GetClassFunctions(SomeClass);
```

### FAgentPropertyValue - Transport Format for Values

**File:** `Public/AgentBridgeTypes.h`, `Private/AgentBridgeTypes.cpp`

This struct is the universal value container used to move property values across module
boundaries (and eventually across the network via gRPC). It can hold any type that UE's
reflection system supports:

- Primitive values are stored in `StringValue` (parsed on demand)
- Vectors, rotators, and transforms have dedicated constructors
- Nested structs use `StructValue` (a map of field name to nested FAgentPropertyValue)
- Arrays use `ArrayValue` (a list of nested FAgentPropertyValue)

```cpp
// Create values
FAgentPropertyValue BoolVal = FAgentPropertyValue::FromBool(true);
FAgentPropertyValue FloatVal = FAgentPropertyValue::FromFloat(3.14);
FAgentPropertyValue VecVal = FAgentPropertyValue::FromVector(FVector(1, 2, 3));
FAgentPropertyValue ObjVal = FAgentPropertyValue::FromObject(SomeActor);

// Extract values
bool B = BoolVal.AsBool();
double F = FloatVal.AsFloat();
FVector V = VecVal.AsVector();
```

### FAgentPropertyInfo and Friends - Metadata Types

**File:** `Public/AgentBridgeTypes.h`

Several small structs carry metadata about the type system:

| Struct | What It Describes |
|--------|-------------------|
| `FAgentPropertyInfo` | A single property - name, type, whether it is read-only, its category |
| `FAgentClassInfo` | A class - name, path, parent class, whether it is Blueprint-generated |
| `FAgentFunctionSignature` | A function - name, parameters, return type, whether it is static |
| `FAgentFunctionResult` | The result of a function call - success flag, return value, out params |
| `FAgentEnumValue` | A single enum value - name, display name, numeric value |

These types are designed to be easily serialized for transport over gRPC or HTTP.

---

## Dependencies

This plugin depends only on standard Unreal Engine modules:

```csharp
// AgentBridgeCore.Build.cs
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core",
    "CoreUObject",
    "Engine",
});
```

It has **no dependencies** on:
- Other AgentBridge plugins (Runtime, Scripting, Server)
- Tempo or any other third-party plugins
- Editor-only modules

This makes it safe to use in both editor and runtime (packaged game) contexts.

---

## Module Configuration

| Setting | Value |
|---------|-------|
| Module Type | `Runtime` |
| Loading Phase | `Default` |
| Enabled By Default | `true` |
| Can Contain Content | `false` |

The plugin is a **Runtime** module, meaning it is available in both editor and packaged
builds. It loads during the **Default** phase, which is appropriate since it has no special
initialization ordering requirements - it just provides static utility functions.

---

## Console Commands for Debugging

AgentBridge registers several console commands that use AgentBridgeCore under the hood.
These are useful for debugging property access and type discovery:

```
AgentBridge.DumpActor <name> [depth]
```
Dumps all properties on the named actor, recursing into nested structs up to the specified
depth (default 3). Useful for seeing what properties are available and what their current
values are.

```
AgentBridge.DumpClass <name>
```
Dumps the schema (property list and function list) for the named class. Useful for
understanding what a class exposes to the reflection system.

**Note:** These commands are registered by the AgentBridgeScripting plugin, not by Core
directly. Core provides the underlying property/type reading logic that the commands use.

---

## Further Reading

- **[CLAUDE.md](CLAUDE.md)** - Deep implementation documentation for AI assistants and
  developers. Covers critical code patterns (array traversal, map iteration, Blueprint
  property names, WritePropertyDirect), UObject pointer type reference, and resolved issues.
- **[Parent README](../README.md)** - Overview of the entire AgentBridge ecosystem,
  including all 4 plugins, the MCP server, and the bp_toolkit.
- **[Parent CLAUDE.md](../CLAUDE.md)** - Project-wide development guide with build
  instructions, testing protocols, and architecture diagrams.
