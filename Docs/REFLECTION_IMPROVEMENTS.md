# Reflection Improvements Implementation Plan

> Improving AgentBridge to support component/UObject reflection and static function calls.
> Created: December 31, 2025

---

## Executive Summary

Testing with a naive Claude revealed that AgentBridge's reflection capabilities are too narrow for advanced workflows. This document outlines improvements across three phases:

1. **Bug Fixes** - Fix get_actor response not including properties/components
2. **Expand Reflection** - Support non-Actor classes (components, UObjects)
3. **Static Functions** - Call Blueprint Function Library methods (e.g., KismetRenderingLibrary)

---

## Phase 1: Bug Fixes (Quick Wins)

### Issue: `get_actor` ignores include_properties/include_components flags

**Root Cause:** The MCP execute handler only extracts `actor_info`, ignoring `properties` and `components` from the gRPC response.

**Location:** `Python/mcp/services/agentbridge.py:909-921`

**Current Code:**
```python
elif tool_name == "get_actor":
    result = safe_call(
        client.get_actor,
        actor_id=args["actor_id"],
        include_properties=args.get("include_properties", False),
        include_components=args.get("include_components", False),
    )
    if isinstance(result, dict) and "error" in result:
        return result
    if result.HasField("actor"):
        actor = client._parse_actor_descriptor(result.actor.actor_info)
        return {"found": True, "actor": _actor_to_dict(actor)}  # <-- Bug: Only actor_info!
    return {"found": False, "error": f"Actor '{args['actor_id']}' not found"}
```

**Fix:** Include properties and components from response:
```python
elif tool_name == "get_actor":
    result = safe_call(
        client.get_actor,
        actor_id=args["actor_id"],
        include_properties=args.get("include_properties", False),
        include_components=args.get("include_components", False),
    )
    if isinstance(result, dict) and "error" in result:
        return result
    if result.HasField("actor"):
        actor = client._parse_actor_descriptor(result.actor.actor_info)
        response = {"found": True, "actor": _actor_to_dict(actor)}

        # Include properties if requested and present
        if result.actor.properties:
            response["properties"] = {
                kv.key: _property_value_to_dict(kv.value)
                for kv in result.actor.properties
            }

        # Include components if requested and present
        if result.actor.components:
            response["components"] = [
                {
                    "name": c.name,
                    "class_name": c.class_name,
                    "is_scene_component": c.is_scene_component,
                }
                for c in result.actor.components
            ]

        return response
    return {"found": False, "error": f"Actor '{args['actor_id']}' not found"}
```

**Files to Modify:**
- `Python/mcp/services/agentbridge.py` - Fix execute handler for get_actor

---

### Issue: `get_class_schema` returns empty properties/functions

**Root Cause:** C++ implementation has a TODO and only returns success without populating schema.

**Location:** `Source/AgentBridgeScripting/Private/CommandExecutor.cpp:734-751`

**Current Code:**
```cpp
void FCommandExecutor::Execute(const FGetClassSchemaCommand& Command, FAgentResponseBase& Response)
{
    // ... class lookup ...

    // TODO: Return full schema in response
    Response.bSuccess = true;
    Response.ExecutionTimeMs = EndTiming(StartTime);
}
```

**Fix:** Use PropertyAccessor and TypeDiscovery to populate schema:
```cpp
void FCommandExecutor::Execute(const FGetClassSchemaCommand& Command, FGetClassSchemaResponse& Response)
{
    double StartTime = StartTiming();
    Response.CommandId = Command.CommandId;

    UClass* Class = FTypeDiscovery::FindClassByName(Command.ClassName);
    if (!Class)
    {
        Response.bSuccess = false;
        Response.ErrorMessage = FString::Printf(TEXT("Class '%s' not found"), *Command.ClassName);
        Response.ExecutionTimeMs = EndTiming(StartTime);
        return;
    }

    // Class info
    Response.ClassName = Class->GetName();
    Response.ParentClassName = Class->GetSuperClass() ? Class->GetSuperClass()->GetName() : TEXT("");
    Response.bIsBlueprint = Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
    Response.bIsAbstract = Class->HasAnyClassFlags(CLASS_Abstract);

    // Properties
    for (TFieldIterator<FProperty> PropIt(Class,
             Command.bIncludeInherited ? EFieldIteratorFlags::IncludeSuper : EFieldIteratorFlags::ExcludeSuper);
         PropIt; ++PropIt)
    {
        FProperty* Prop = *PropIt;
        if (!Prop->HasAnyPropertyFlags(CPF_Deprecated))
        {
            FPropertySchemaInfo Info;
            Info.Name = Prop->GetName();
            Info.DisplayName = Prop->GetDisplayNameText().ToString();
            Info.TypeName = Prop->GetCPPType();
            Info.bReadOnly = Prop->HasAnyPropertyFlags(CPF_BlueprintReadOnly);
            Info.bBlueprintVisible = Prop->HasAnyPropertyFlags(CPF_BlueprintVisible);
            Response.Properties.Add(Info);
        }
    }

    // Functions
    if (Command.bIncludeFunctions)
    {
        for (TFieldIterator<UFunction> FuncIt(Class,
                 Command.bIncludeInherited ? EFieldIteratorFlags::IncludeSuper : EFieldIteratorFlags::ExcludeSuper);
             FuncIt; ++FuncIt)
        {
            UFunction* Func = *FuncIt;
            if (Func->HasAnyFunctionFlags(FUNC_BlueprintCallable))
            {
                FFunctionSchemaInfo Info;
                Info.Name = Func->GetName();
                Info.bIsStatic = Func->HasAnyFunctionFlags(FUNC_Static);
                Info.bIsPure = Func->HasAnyFunctionFlags(FUNC_BlueprintPure);
                Response.Functions.Add(Info);
            }
        }
    }

    Response.bSuccess = true;
    Response.ExecutionTimeMs = EndTiming(StartTime);
}
```

**Files to Modify:**
- `Source/AgentBridgeScripting/Public/AgentCommands.h` - Add FGetClassSchemaResponse
- `Source/AgentBridgeScripting/Public/CommandExecutor.h` - Change signature to use FGetClassSchemaResponse
- `Source/AgentBridgeScripting/Private/CommandExecutor.cpp` - Implement full schema extraction
- `Source/AgentBridgeServer/Private/AgentBridgeServiceSubsystem.cpp` - Use FGetClassSchemaResponse

---

## Phase 2: Expand Reflection Scope

The current reflection is limited to AActor subclasses. We need to support:
- UActorComponent subclasses (SceneCaptureComponent2D, PointLightComponent, etc.)
- UObject subclasses (TextureRenderTarget2D, MaterialInstance, etc.)

### Changes Required

#### Proto Changes (`AgentBridge.proto`)

No changes needed - `list_classes` and `get_class_schema` already have `base_class_name`/`class_name` parameters that can accept any UClass name.

#### C++ Changes

1. **TypeDiscovery::FindClassByName** - Already works for all UClass types (tested)

2. **CommandExecutor::Execute(FListClassesCommand)** - Already supports any base class (line 759-770):
   ```cpp
   UClass* BaseClass = AActor::StaticClass();
   if (!Command.BaseClassName.IsEmpty())
   {
       BaseClass = FTypeDiscovery::FindClassByName(Command.BaseClassName);
       // ...
   }
   ```

3. **No C++ changes needed** - The infrastructure already supports non-Actor classes!

#### MCP Tool Changes

Update tool descriptions and defaults to clarify capability:

**`list_classes`** - Update description:
```python
{
    "name": "list_classes",
    "description": "List available classes (actors, components, or any UObject). "
                   "Use base_class_name='ActorComponent' for components, "
                   "'Object' for all types.",
    "inputSchema": {
        "type": "object",
        "properties": {
            "base_class_name": {
                "type": "string",
                "description": "Base class filter: 'Actor' (default), 'ActorComponent', 'Object', or specific class name",
                "default": "Actor",
            },
            # ...
        },
    },
}
```

**`get_class_schema`** - Update description:
```python
{
    "name": "get_class_schema",
    "description": "Get schema (properties/functions) for ANY class - actors, components, or UObjects. "
                   "Works with 'SceneCaptureComponent2D', 'TextureRenderTarget2D', etc.",
    # ...
}
```

**Files to Modify:**
- `Python/mcp/services/agentbridge.py` - Update TOOLS list descriptions

---

## Phase 3: Static Function Support

Many useful operations are in static Blueprint Function Libraries:
- `UKismetRenderingLibrary::CreateRenderTarget2D`
- `UKismetRenderingLibrary::ExportRenderTarget`
- `UKismetSystemLibrary::ExecuteConsoleCommand`

### Proto Changes (`AgentBridge.proto`)

Add new RPC and messages:

```protobuf
message CallStaticFunctionRequest {
  string class_name = 1;              // "KismetRenderingLibrary"
  string function_name = 2;           // "CreateRenderTarget2D"
  repeated PropertyKeyValue parameters = 3;
}

message CallStaticFunctionResponse {
  PropertyValue return_value = 1;
  repeated PropertyKeyValue out_parameters = 2;
}

service AgentBridgeService {
  // ... existing RPCs ...

  //--- Static Function Invocation ---
  rpc CallStaticFunction(CallStaticFunctionRequest) returns (CallStaticFunctionResponse);
}
```

### C++ Implementation

1. **New Command** (`AgentCommands.h`):
```cpp
struct AGENTBRIDGESCRIPTING_API FCallStaticFunctionCommand : FAgentCommandBase
{
    FCallStaticFunctionCommand() { Type = EAgentCommandType::CallStaticFunction; }

    FString ClassName;     // Blueprint Library class name
    FString FunctionName;
    TMap<FString, FAgentPropertyValue> Parameters;
};
```

2. **CommandExecutor** (`CommandExecutor.cpp`):
```cpp
void FCommandExecutor::Execute(const FCallStaticFunctionCommand& Command, FFunctionCallResponse& Response)
{
    // 1. Find the class (e.g., UKismetRenderingLibrary)
    UClass* Class = FTypeDiscovery::FindClassByName(Command.ClassName);
    if (!Class)
    {
        Response.ErrorMessage = FString::Printf(TEXT("Class '%s' not found"), *Command.ClassName);
        return;
    }

    // 2. Find the static function
    UFunction* Func = Class->FindFunctionByName(*Command.FunctionName);
    if (!Func || !Func->HasAnyFunctionFlags(FUNC_Static))
    {
        Response.ErrorMessage = FString::Printf(TEXT("Static function '%s' not found"), *Command.FunctionName);
        return;
    }

    // 3. Get CDO as "this" for static calls
    UObject* CDO = Class->GetDefaultObject();

    // 4. Invoke using FunctionInvoker
    Response = FFunctionInvoker::Invoke(CDO, Func, Command.Parameters);
}
```

3. **gRPC Handler** (`AgentBridgeServiceSubsystem.cpp`):
```cpp
void UAgentBridgeServiceSubsystem::CallStaticFunction(
    const CallStaticFunctionRequest& Request,
    const TResponseDelegate<CallStaticFunctionResponse>& ResponseContinuation)
{
    FCallStaticFunctionCommand Cmd;
    Cmd.ClassName = UTF8_TO_TCHAR(Request.class_name().c_str());
    Cmd.FunctionName = UTF8_TO_TCHAR(Request.function_name().c_str());
    // Convert parameters...

    FFunctionCallResponse CmdResponse;
    FCommandExecutor::Execute(Cmd, CmdResponse);

    // Build response...
}
```

### MCP Tool

```python
{
    "name": "call_static_function",
    "description": "Call a static Blueprint library function. "
                   "Example: KismetRenderingLibrary::CreateRenderTarget2D",
    "inputSchema": {
        "type": "object",
        "properties": {
            "class_name": {
                "type": "string",
                "description": "Blueprint library class (e.g., 'KismetRenderingLibrary')",
            },
            "function_name": {
                "type": "string",
                "description": "Static function name (e.g., 'CreateRenderTarget2D')",
            },
            "parameters": {
                "type": "object",
                "description": "Function parameters as key-value pairs",
                "additionalProperties": True,
            },
        },
        "required": ["class_name", "function_name"],
    },
}
```

**Files to Modify:**
- `Source/AgentBridgeServer/Public/AgentBridge.proto` - Add RPC and messages
- `Source/AgentBridgeScripting/Public/AgentCommands.h` - Add command struct
- `Source/AgentBridgeScripting/Private/CommandExecutor.cpp` - Add handler
- `Source/AgentBridgeServer/Public/AgentBridgeServiceSubsystem.h` - Add declaration
- `Source/AgentBridgeServer/Private/AgentBridgeServiceSubsystem.cpp` - Add gRPC handler
- `Python/mcp/services/agentbridge.py` - Add MCP tool

---

## Implementation Order

1. **Phase 1a: Fix get_actor MCP bug** (Python only, no build required)
2. **Phase 1b: Fix get_class_schema** (C++ changes, requires build)
3. **Phase 2: Expand reflection** (Python only - just documentation)
4. **Phase 3: Static functions** (Proto + C++ + Python, requires build)
5. **Update help text** (Python only)

---

## Testing Checklist

### Phase 1 Tests
- [ ] `get_actor("PointLight", include_properties=True)` returns properties
- [ ] `get_actor("PointLight", include_components=True)` returns components
- [ ] `get_class_schema("PointLight")` returns properties
- [ ] `get_class_schema("PointLight", include_functions=True)` returns functions

### Phase 2 Tests
- [ ] `list_classes(base_class_name="ActorComponent")` finds components
- [ ] `get_class_schema("SceneCaptureComponent2D")` returns properties
- [ ] `list_classes(base_class_name="Object", name_pattern="*RenderTarget*")` finds render targets

### Phase 3 Tests
- [ ] `call_static_function("KismetSystemLibrary", "PrintString", {"String": "Hello"})` works
- [ ] `call_static_function("KismetRenderingLibrary", "CreateRenderTarget2D", {...})` creates asset

---

## Help Text Updates

Add to `_get_help_text()` in agentbridge.py:

**Overview section:**
```
NEW CAPABILITIES:
- list_classes now supports base_class_name="ActorComponent" or "Object"
- get_class_schema works on ANY class, not just Actors
- call_static_function for Blueprint library calls (KismetRenderingLibrary, etc.)
```

**Workflows section:**
```
Creating a render target:
1. call_static_function(
     class_name="KismetRenderingLibrary",
     function_name="CreateRenderTarget2D",
     parameters={"WorldContextObject": null, "Width": 1024, "Height": 1024}
   )
```

---

*Document Version: 1.0*
*Author: Claude Code*
