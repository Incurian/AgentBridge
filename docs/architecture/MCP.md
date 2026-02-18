# Python MCP Server - External Agent Interface

**Location:** `mcp/` (git submodule)
**Language:** Python 3.8+
**Dependencies:** grpcio, protobuf (via TempoEnv)

The MCP server is the entry point for external AI agents. It implements the MCP protocol
(JSON-RPC over stdio) and routes tool calls to gRPC services or local Python handlers.

## Class Diagram

```mermaid
classDiagram
    direction TB

    class MCPServer {
        -services: Dict~str, ServiceModule~
        -tool_to_service: Dict~str, str~
        -clients: Dict~str, Any~
        -enabled_modules: List~str~
        +__init__(host, port, profile, modules)
        +run()
        +handle_message(message) dict
        -_handle_initialize() dict
        -_handle_tools_list() dict
        -_handle_tools_call(name, args) dict
        -_handle_load_modules(modules) dict
        -_get_client(service_name) Any
    }

    class ServiceModule {
        <<dataclass>>
        +name: str
        +description: str
        +tools: List~Dict~
        +execute: Callable
        +connect: Callable
    }

    class FilteredServiceModule {
        +name: str
        +description: str
        +tools: List~Dict~ (filtered)
        +execute: Callable
        +connect: Callable
    }

    class AgentBridgeGrpcClient {
        -_channel: grpc.Channel
        -_stub: AgentBridgeServiceStub
        -address: str
        +__init__(host, port)
        +connect()
        +_ensure_connected()
        +list_worlds()
        +query_actors(class_name, name_pattern, tag, limit)
        +get_actor(actor_id, include_properties, include_components)
        +spawn_actor(class_name, location, rotation, scale, label)
        +delete_actor(actor_id)
        +set_actor_transform(...)
        +get_property(actor_id, path)
        +set_property(actor_id, path, value)
        +call_function(call, parameters)
        +call_asset_function(asset_path, function_name, ...)
        +list_classes(base_class_name, name_pattern, limit)
        +get_class_schema(class_name, include_inherited, include_functions)
        +create_blueprint_node(blueprint_path, node_type, ...)
        +connect_blueprint_pins(blueprint_path, src_node, src_pin, tgt_node, tgt_pin)
        +list_blueprint_nodes(blueprint_path, graph_name)
        +list_blueprint_pins(blueprint_path, node_id)
        +get_streaming_state(actor_guid)
    }

    class ModuleRegistry["services/__init__.py"] {
        +MODULES: Dict~str, Dict~ (8 module definitions)
        +PROFILES: Dict~str, List~ (6 profile presets)
        +register_service(ServiceModule)$
        +get_profile_modules(profile) List~str~$
        +get_filtered_services(modules) Dict$
        -_import_all_services()$
    }

    class AgentBridgeServiceDef["agentbridge.py"] {
        <<ServiceModule, 57 tools>>
        +TOOLS: List~Dict~ (tool schemas)
        +execute(client, tool_name, args) str
        +connect(host, port) AgentBridgeGrpcClient
        -_execute_impl(client, tool_name, args) dict
        -_get_help_text(topic) str
    }

    class TempoServiceDefs["tempo_*.py (11 modules)"] {
        <<ServiceModule group, 30 tools>>
        tempo_core / tempo_time
        tempo_actor_control
        tempo_world_state
        tempo_geographic
        tempo_sensors
        tempo_movement
        tempo_map_query
        tempo_labels
        tempo_agents_editor
        tempo_core_editor
    }

    class BpToolkitServiceDef["bp_toolkit.py"] {
        <<ServiceModule, 14 tools>>
        +TOOLS: List~Dict~ (tool schemas)
        +execute(client, tool_name, args) str
        -_find_bp_toolkit() Optional~Path~
        -_handle_export_asset(args) dict
        -_handle_import_asset(args) dict
        -_handle_set_property(args) dict
        -_handle_clone_asset(args) dict
        -_handle_parse(args) dict
    }

    class WorldInfo {
        <<dataclass>>
        +world_type: str
        +world_name: str
        +pie_instance: int
        +has_begun_play: bool
        +actor_count: int
    }

    class ActorInfo {
        <<dataclass>>
        +guid: str
        +path: str
        +name: str
        +label: str
        +class_name: str
        +location: tuple
        +rotation: tuple
        +scale: tuple
        +is_hidden: bool
    }

    MCPServer "1" o-- "*" ServiceModule : loads
    MCPServer --> ModuleRegistry : uses for routing
    ServiceModule <|-- FilteredServiceModule : wraps

    ModuleRegistry --> AgentBridgeServiceDef : registers
    ModuleRegistry --> TempoServiceDefs : registers
    ModuleRegistry --> BpToolkitServiceDef : registers (optional)

    AgentBridgeServiceDef ..> AgentBridgeGrpcClient : creates
    AgentBridgeGrpcClient ..> WorldInfo : returns
    AgentBridgeGrpcClient ..> ActorInfo : returns
```

## Module & Profile System

### Modules (8 defined)

| Module | Tools | Description |
|--------|-------|-------------|
| `core` | 6 | Help, world listing, console commands |
| `classes` | 17 | Actors, properties, components, transforms, assets |
| `editor` | 7 | PIE, simulate, level management |
| `world_partition` | 7 | Streaming, landscape queries |
| `files` | 4 | Project file I/O |
| `bp_toolkit` | 26 | Blueprint/PCG live + offline tools |
| `tempo_sim` | 28 | Simulation, time, sensors, navigation |

### Profiles (6 presets)

| Profile | Modules | Total Tools | Use Case |
|---------|---------|-------------|----------|
| `core` | core | 6 | Absolute minimum |
| `standard` | core, classes, world_partition | 30 | Default editor work |
| `editor` | core, classes, editor, world_partition, files | 41 | Full editor |
| `scripting` | core, classes, editor, files, bp_toolkit | 60 | With BP/PCG editing |
| `simulation` | core, classes, tempo_sim | 51 | PIE/runtime |
| `full` | all modules | ~100 | Everything |

## Service Registration Pattern

Each service file follows this pattern:

```python
# 1. Define tools
TOOLS = [{"name": "tool_name", "description": "...", "inputSchema": {...}}, ...]

# 2. Define client class
class MyClient:
    def __init__(self, host, port): ...
    def method(self, ...): ...

# 3. Define execute function
def execute(client, tool_name, args):
    result = _execute_impl(client, tool_name, args)
    return json.dumps(result, indent=2)

# 4. Define connect function
def connect(host, port):
    return MyClient(host, port)

# 5. Register
register_service(ServiceModule(name="...", tools=TOOLS, execute=execute, connect=connect))
```

## Request Flow

```
stdin (JSON-RPC) -> MCPServer.run() -> handle_message()
    -> _handle_tools_call(tool_name, args)
    -> tool_to_service[tool_name] -> service_name
    -> _get_client(service_name) -> lazy gRPC client
    -> service.execute(client, tool_name, args)
    -> gRPC call to Unreal Engine
    -> response JSON -> stdout
```

## Configuration

```bash
python -m mcp --host localhost --port 10001 --profile full
```

| Option | Default | Description |
|--------|---------|-------------|
| `--host` | localhost | gRPC server host |
| `--port` | 10001 | gRPC server port |
| `--profile` | full | Profile name |
| `--modules` | (from profile) | Override module list |
| `--debug` | false | Enable debug logging |

## Files

| File | Contents |
|------|----------|
| `mcp/__init__.py` | Package metadata |
| `mcp/__main__.py` | CLI entry point |
| `mcp/server.py` | `MCPServer` class |
| `mcp/client.py` | `AgentBridgeGrpcClient`, dataclasses |
| `mcp/services/__init__.py` | Module registry, profiles |
| `mcp/services/base.py` | Tempo API path detection, utilities |
| `mcp/services/agentbridge.py` | AgentBridge tools (57) |
| `mcp/services/bp_toolkit.py` | Offline asset tools (14) |
| `mcp/services/tempo_*.py` | Tempo service tools (11 files, 30 tools) |
