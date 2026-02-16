# AgentBridgeServer

gRPC and HTTP server plugin that exposes AgentBridge to external AI agents.

## What This Plugin Does

AgentBridgeServer is the **network layer** of the AgentBridge system. It is the entry point
where external programs (AI agents, Python scripts, automation tools) connect to Unreal Engine.

Without this plugin, there would be no way for code running outside of Unreal Engine to
communicate with the engine. AgentBridgeServer opens two network ports and listens for
incoming requests, translates them into commands that Unreal understands, and sends the
results back.

**It sits at the top of the AgentBridge dependency chain:**

```
AgentBridgeServer (this plugin - network layer)
    |
    v
AgentBridgeScripting (command dispatch, all business logic)
    |
    v
AgentBridgeRuntime (world operations, actor manipulation)
    |
    v
AgentBridgeCore (low-level reflection, type discovery)
```

Each of these is a standalone Unreal Engine plugin with its own `.uplugin` file. There is
no wrapper plugin - Unreal Build Tool (UBT) discovers all four independently by scanning the
`Plugins/AgentBridge/` directory.

---

## Key Concepts for Beginners

### What is gRPC?

gRPC (Google Remote Procedure Call) is a framework that lets one program call functions in
another program over the network. Think of it like a phone call between two programs:

- The **client** (e.g., a Python script) says "please run QueryActors with these parameters"
- The **server** (this plugin, inside Unreal) runs the function and sends back the results
- Communication happens over TCP, so the client and server can be on different machines

gRPC uses a compact binary format, making it fast. AgentBridge's gRPC server runs on
**port 10001** (provided by the Tempo plugin's infrastructure).

### What is Protobuf?

Protocol Buffers (protobuf) is the message format used by gRPC. Instead of sending human-readable
JSON text, protobuf sends compact binary data. Messages are defined in `.proto` files, which
describe the structure (like a schema). A code generator then creates C++ and Python classes
from these definitions.

For example, the `AgentBridge.proto` file defines messages like:

```protobuf
message QueryActorsRequest {
  string class_name = 1;
  string name_pattern = 2;
  int32 limit = 3;
}
```

This gets compiled into C++ classes that the server uses, and Python classes that the client uses.

### What is MCP?

MCP (Model Context Protocol) is a standard created by Anthropic for AI agents to use tools.
When you use AgentBridge with Claude Code, the flow is:

1. Claude Code sees a tool like `query_actors`
2. It calls the MCP server (a Python process)
3. The MCP server converts this to a gRPC call
4. The gRPC call reaches AgentBridgeServer inside Unreal Engine

MCP is the outermost layer - it wraps gRPC calls in a format that AI agents understand.

### What is Tempo?

Tempo is a simulation framework plugin for Unreal Engine. AgentBridge uses Tempo's gRPC
infrastructure rather than implementing its own. Specifically:

- Tempo provides `FTempoScriptingServer` which manages gRPC services
- AgentBridgeServer implements `ITempoScriptable` to register its service
- Tempo handles all the networking details (port binding, request routing, threading)

This means AgentBridgeServer does not need to manage sockets, threads, or gRPC server
lifecycle directly. Tempo handles all of that.

### Why "Thin Handlers"?

This plugin contains ONLY thin handler methods. The actual logic (finding actors, setting
properties, creating assets) lives in AgentBridgeScripting. There is an important reason
for this:

**gRPC headers conflict with certain Unreal Engine headers.** The gRPC library includes
Windows SDK headers that define macros like `InterlockedIncrement`, which clash with Unreal's
`FPlatformAtomics`. This means any file that includes gRPC headers cannot also include certain
UE headers like `AssetRegistry/AssetRegistryModule.h` or `Editor.h`.

By keeping handler code thin (just converting protobuf messages to command structs and back),
the actual business logic can live in AgentBridgeScripting where all UE headers are available.

---

## Architecture

### Data Flow

```
External AI Agent (Claude Code, LLM, Python script)
         |
         v
MCP Server (Python process, ~100 tools)
         |
         |--- gRPC (port 10001, primary, high performance)
         |         |
         |         v
         |    AgentBridgeServiceSubsystem (UWorldSubsystem)
         |         |
         |         v
         |    Converts protobuf <-> command structs
         |
         |--- HTTP  (port 8080, fallback, for testing)
                   |
                   v
              AgentHttpServer (FAgentHttpServer singleton)
                   |
                   v
              Converts JSON <-> command structs
                   |
                   v
         FCommandExecutor (in AgentBridgeScripting)
                   |
                   v
         AgentBridgeRuntime / AgentBridgeCore
                   |
                   v
            Unreal Engine 5.6
```

### How a Request Flows (gRPC Example)

1. Python MCP server sends a `QueryActors` gRPC request to port 10001
2. Tempo's gRPC server receives it and routes it to `UAgentBridgeServiceSubsystem`
3. The handler method `QueryActors()` is called on the game thread
4. It creates an `FQueryActorsCommand` struct and fills it from the protobuf request
5. It calls `FCommandExecutor::Execute(Cmd, Response)` (in AgentBridgeScripting)
6. CommandExecutor does the actual work (finding actors, filtering, etc.)
7. The handler converts the response struct back to a protobuf `QueryActorsResponse`
8. The response is sent back through gRPC to the Python client

### How a Request Flows (HTTP Example)

1. External client sends `POST /agentbridge/execute` with a JSON body
2. `FAgentHttpServer::HandleExecute()` receives the request
3. It passes the raw JSON to `FCommandExecutor::ExecuteJson()`
4. CommandExecutor parses the JSON, executes the command, and returns JSON
5. The JSON response is sent back to the client

---

## Plugin Structure

```
AgentBridgeServer/
|-- AgentBridgeServer.uplugin          Plugin descriptor
|-- CLAUDE.md                          AI assistant documentation
|-- README.md                          This file
|-- Source/AgentBridgeServer/
    |-- AgentBridgeServer.Build.cs     Build rules (extends TempoModuleRules)
    |-- Public/
    |   |-- AgentBridge.proto          gRPC service definition (888 lines)
    |   |-- AgentBridgeServer.h        Module interface
    |   |-- AgentBridgeServiceSubsystem.h   gRPC handler declarations
    |   |-- AgentHttpServer.h          HTTP server interface
    |   |-- ProtobufGenerated/         Auto-generated, DO NOT EDIT
    |       |-- AgentBridgeServer/
    |           |-- AgentBridge.pb.h       Generated message classes
    |           |-- AgentBridge.grpc.pb.h  Generated service stubs
    |-- Private/
        |-- AgentBridgeServer.cpp          Module startup (starts HTTP server)
        |-- AgentBridgeServiceSubsystem.cpp  gRPC handler implementations
        |-- AgentHttpServer.cpp            HTTP server implementation
        |-- ProtobufGenerated/             Auto-generated, DO NOT EDIT
            |-- AgentBridgeServer/
                |-- AgentBridge.pb.cc      Generated message implementations
                |-- AgentBridge.grpc.pb.cc Generated service implementations
```

---

## Ports

| Protocol | Port  | Purpose                                       |
|----------|-------|-----------------------------------------------|
| gRPC     | 10001 | Primary communication (via Tempo infrastructure) |
| HTTP     | 8080  | JSON fallback for testing and simple clients   |

The gRPC port is managed by Tempo and is always available when the editor is running with
Tempo enabled. The HTTP server starts automatically when the module loads.

---

## Proto File Overview

The `AgentBridge.proto` file (888 lines) defines the entire gRPC API:

- **105 message types** - Request/response structures for all operations
- **51 RPC methods** - Organized into categories:
  - World Operations (2 RPCs)
  - Actor Discovery (2 RPCs)
  - Actor Manipulation (5 RPCs)
  - Property Path Operations (2 RPCs)
  - Function Invocation (2 RPCs)
  - Type Discovery (3 RPCs)
  - World Partition and Streaming (7 RPCs)
  - Console Commands (2 RPCs)
  - Asset Operations (5 RPCs)
  - Component Operations (6 RPCs)
  - Unified Transform/Attachment (4 RPCs)
  - File Operations (5 RPCs)
  - Blueprint Node Operations (6 RPCs)

The proto file imports two Tempo proto files:
- `TempoScripting/Empty.proto` - Empty message for RPCs with no response data
- `TempoScripting/Geometry.proto` - Vector and Rotation types

### Proto Generation

Proto files are compiled into C++ code automatically during the build process. You do not
need to run anything manually in most cases. The pipeline is:

1. `Build.sh` calls `PreBuild.bat`
2. `PreBuild.bat` calls Tempo's `GenProtos.sh`
3. `GenProtos.sh` recursively scans the project for `.proto` files
4. `protoc` compiles each proto file into `.pb.h`, `.pb.cc`, `.grpc.pb.h`, `.grpc.pb.cc`
5. Generated files are placed in `ProtobufGenerated/` directories
6. Files use `REPLACE_IF_STALE` - they are only updated when content changes

If you need to regenerate manually (rarely needed):

```bash
cd <PROJECT_ROOT>/Plugins/Tempo/TempoCore/Scripts
./GenProtos.sh
```

**Important:** Proto files must use LF line endings (not CRLF). The protoc compiler silently
fails on CRLF files. If you edit a proto file on Windows, verify line endings with
`file AgentBridge.proto` - it should say "ASCII text", not "CRLF line terminators".

---

## Header Conflict Explanation

This is the most important technical constraint in this plugin:

**gRPC headers conflict with certain Unreal Engine headers on Windows.** Specifically,
`TempoScriptingServer.h` includes `<grpcpp/grpcpp.h>` before `CoreMinimal.h`. This causes
Windows SDK macros (like `InterlockedIncrement`) to conflict with UE's `FPlatformAtomics`.

### Headers That CANNOT Be Included

- `AssetRegistry/AssetRegistryModule.h`
- `Editor.h`, `LevelEditor.h`
- `IImageWrapper.h`
- `ThumbnailRendering/ThumbnailManager.h`
- Any header that transitively includes `IoBuffer.h`

### What This Means in Practice

If you need to write code that uses the Asset Registry, editor subsystems, or image processing,
that code MUST go in **AgentBridgeScripting** (specifically `CommandExecutor.cpp`), not in this
plugin. The handler in AgentBridgeServer should only convert protobuf messages to command
structs, call CommandExecutor, and convert the response back.

---

## Dependencies

### Plugin Dependencies (in AgentBridgeServer.uplugin)

| Plugin               | Purpose                                |
|----------------------|----------------------------------------|
| AgentBridgeCore      | Reflection primitives                  |
| AgentBridgeRuntime   | World ops, actor ops, property paths   |
| AgentBridgeScripting | Command dispatch, JSON serialization   |
| TempoCore            | gRPC infrastructure, scripting framework |

### Module Dependencies (in AgentBridgeServer.Build.cs)

The Build.cs extends `TempoModuleRules` (not the standard `ModuleRules`), which automatically
adds gRPC-related dependencies:

```csharp
public class AgentBridgeServer : TempoModuleRules
{
    PublicDependencyModuleNames.AddRange(new string[] {
        "Core", "CoreUObject", "Engine",
        "HTTPServer", "Json",
        "AgentBridgeCore", "AgentBridgeRuntime", "AgentBridgeScripting",
        "TempoScripting",
    });

    PrivateDependencyModuleNames.AddRange(new string[] {
        "TempoCoreShared",
    });

    // UnrealEd is only available in editor builds
    if (Target.bBuildEditor)
    {
        PrivateDependencyModuleNames.Add("UnrealEd");
    }
}
```

### Module Type and Loading

- **Module Type:** Runtime (not Editor - this supports PIE and packaged game)
- **Loading Phase:** Default
- **Enabled by Default:** true

---

## HTTP Server

The HTTP server is a simpler alternative to gRPC, useful for testing and simple scripts
that do not want to use gRPC/protobuf.

### Endpoints

| Method | Path                       | Description                |
|--------|----------------------------|----------------------------|
| POST   | `/agentbridge/execute`     | Execute a single command   |
| POST   | `/agentbridge/batch`       | Execute multiple commands  |
| GET    | `/agentbridge/health`      | Health check               |
| GET    | `/agentbridge/schema`      | API schema documentation   |

### Security

- Binds to localhost only (not accessible from other machines)
- No authentication (assumes trusted local environment)

### Thread Safety

HTTP requests arrive on worker threads. The `HandleExecute` method passes JSON directly to
`FCommandExecutor::ExecuteJson()`, which handles game thread dispatch internally.

---

## How to Add a New RPC

This is a step-by-step guide for adding a new gRPC RPC to AgentBridge. It involves changes
across multiple plugins.

### Step 1: Define Proto Messages

Edit `Source/AgentBridgeServer/Public/AgentBridge.proto`:

```protobuf
// Add request and response messages
message MyNewFeatureRequest {
  string some_param = 1;
  int32 another_param = 2;
}

message MyNewFeatureResponse {
  bool success = 1;
  string result = 2;
}

// Add RPC to the service definition
service AgentBridgeService {
  // ... existing RPCs ...
  rpc MyNewFeature(MyNewFeatureRequest) returns (MyNewFeatureResponse);
}
```

### Step 2: Regenerate Proto Files

Run the build, which will automatically regenerate proto files. Or manually:

```bash
cd <PROJECT_ROOT>/Plugins/Tempo/TempoCore/Scripts
./GenProtos.sh
```

### Step 3: Add Command Struct (in AgentBridgeScripting)

Edit `AgentBridgeScripting/Source/AgentBridgeScripting/Public/AgentCommands.h`:

```cpp
struct FMyNewFeatureCommand : public FAgentCommandBase
{
    FString SomeParam;
    int32 AnotherParam = 0;
};

struct FMyNewFeatureResponse : public FAgentResponseBase
{
    FString Result;
};
```

### Step 4: Implement Logic (in AgentBridgeScripting)

Edit `AgentBridgeScripting/Source/AgentBridgeScripting/Private/CommandExecutor.cpp`:

```cpp
void FCommandExecutor::Execute(const FMyNewFeatureCommand& Cmd, FMyNewFeatureResponse& Response)
{
    // Your actual logic here - you can include any UE header
    Response.bSuccess = true;
    Response.Result = TEXT("Done");
}
```

### Step 5: Add Handler Declaration (in this plugin)

Edit `Source/AgentBridgeServer/Public/AgentBridgeServiceSubsystem.h`:

```cpp
// Add forward declaration at the top
namespace AgentBridgeServer
{
    class MyNewFeatureRequest;
    class MyNewFeatureResponse;
}

// Add handler method in the class
void MyNewFeature(
    const AgentBridgeServer::MyNewFeatureRequest& Request,
    const TResponseDelegate<AgentBridgeServer::MyNewFeatureResponse>& ResponseContinuation);
```

### Step 6: Implement Thin Handler (in this plugin)

Edit `Source/AgentBridgeServer/Private/AgentBridgeServiceSubsystem.cpp`:

```cpp
void UAgentBridgeServiceSubsystem::MyNewFeature(
    const MyNewFeatureRequest& Request,
    const TResponseDelegate<MyNewFeatureResponse>& ResponseContinuation)
{
    // Convert proto -> command struct
    FMyNewFeatureCommand Cmd;
    Cmd.SomeParam = UTF8_TO_TCHAR(Request.some_param().c_str());
    Cmd.AnotherParam = Request.another_param();

    // Execute via CommandExecutor
    FMyNewFeatureResponse CmdResponse;
    FCommandExecutor::Execute(Cmd, CmdResponse);

    // Convert response -> proto
    MyNewFeatureResponse Response;
    Response.set_success(CmdResponse.bSuccess);
    Response.set_result(TCHAR_TO_UTF8(*CmdResponse.Result));

    if (CmdResponse.bSuccess)
    {
        ResponseContinuation.ExecuteIfBound(Response, grpc::Status_OK);
    }
    else
    {
        ResponseContinuation.ExecuteIfBound(Response,
            grpc::Status(grpc::INTERNAL, TCHAR_TO_UTF8(*CmdResponse.ErrorMessage)));
    }
}
```

### Step 7: Register the Handler (EASY TO FORGET!)

In `RegisterScriptingServices()` inside `AgentBridgeServiceSubsystem.cpp`, add a new entry
to the `RegisterService` call:

```cpp
SimpleRequestHandler(&AgentBridgeAsyncService::RequestMyNewFeature,
    &UAgentBridgeServiceSubsystem::MyNewFeature),
```

If you forget this step, the RPC will compile but never get called - requests will hang or
return "unimplemented".

### Step 8: Add Python Client + MCP Tool (in mcp submodule)

Add a method in `mcp/services/agentbridge.py` and register it as an MCP tool.

---

## Detailed Documentation

See [CLAUDE.md](CLAUDE.md) for implementation details including:

- Complete header conflict analysis
- Value conversion function reference
- gRPC service registration pattern
- HTTP server internals
- Tempo integration specifics

## Parent Documentation

See the [AgentBridge README](../README.md) for the full system overview, tool reference,
usage examples, and quick start guide.
