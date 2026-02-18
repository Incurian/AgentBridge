# AgentBridge - Unified Architecture

## Complete Class Diagram

```mermaid
%%{init: {'theme': 'dark', 'themeVariables': {'fontSize': '12px'}, 'flowchart': {'rankSpacing': 40}}}%%
classDiagram
    direction TB

    %% =============================================
    %% LAYER 0: PYTHON MCP SERVER (Entry Point)
    %% =============================================

    namespace L0_MCP_Server {
        class MCPServer {
            -services: Dict~str,ServiceModule~
            -tool_to_service: Dict~str,str~
            -clients: Dict~str,Any~
            +run() stdio JSON-RPC loop
            +handle_message(msg)
            -_handle_tools_call(name, args)
            -_handle_tools_list()
            -_get_client(service) Any
        }

        class ServiceModule {
            &lt;&lt;dataclass&gt;&gt;
            +name: str
            +description: str
            +tools: List~Dict~
            +execute: Callable
            +connect: Callable
        }
    }

    MCPServer "1" o-- "*" ServiceModule : loads

    %% =============================================
    %% LAYER 1: PYTHON SERVICES (Two Paths Diverge)
    %% =============================================

    namespace L1_LiveServices {
        class AgentBridgeService["agentbridge.py (57 live tools)"] {
            +execute(client, tool_name, args)
            +connect(host, port) Client
            -_get_help_text(topic) str
        }

        class AgentBridgeGrpcClient {
            -_channel: grpc.Channel
            -_stub: AgentBridgeServiceStub
            +query_actors(class_name, pattern, limit)
            +spawn_actor(class_name, location, ...)
            +get_property(actor_id, path)
            +set_property(actor_id, path, value)
            +call_function(call, parameters)
            +list_classes(base, pattern, limit)
            +get_class_schema(class_name)
            +create_blueprint_node(bp, type, ...)
            +connect_blueprint_pins(bp, src, dst)
            +list_blueprint_nodes(bp, graph)
        }

        class TempoClients["Tempo Service Clients (30 tools)"] {
            TempoCoreClient
            TempoTimeClient
            TempoActorControlClient
            TempoWorldStateClient
            TempoGeographicClient
            TempoSensorsClient
            TempoMapQueryClient
        }
    }

    namespace L1_OfflineServices {
        class BpToolkitService["bp_toolkit.py (14 offline tools)"] {
            +execute(client, tool_name, args)
            -_find_bp_toolkit() Path
            -_handle_export_asset(args)
            -_handle_import_asset(args)
            -_handle_set_property(args)
            -_handle_get_property(args)
            -_handle_clone_asset(args)
            -_handle_add_comment(args)
            -_handle_parse(args)
            -_handle_find(args)
            -_handle_query(args)
        }
    }

    MCPServer --> AgentBridgeService : routes 57 tools
    MCPServer --> BpToolkitService : routes 14 tools
    MCPServer --> TempoClients : routes 30 tools
    AgentBridgeService ..> AgentBridgeGrpcClient : creates

    %% =============================================
    %% LAYER 2: AGENTBRIDGE SERVER (C++ Network)
    %% =============================================

    namespace L2_AgentBridgeServer {
        class FAgentBridgeServerModule {
            &lt;&lt;IModuleInterface&gt;&gt;
            +StartupModule()
            +ShutdownModule()
        }

        class UAgentBridgeServiceSubsystem {
            &lt;&lt;UWorldSubsystem, ITempoScriptable&gt;&gt;
            +Initialize(Collection)
            +Deinitialize()
            +RegisterScriptingServices(Server)
            -HandleQueryActors(Req, Resp)
            -HandleSpawnActor(Req, Resp)
            -HandleGetPropertyPath(Req, Resp)
            -HandleSetPropertyPath(Req, Resp)
            -HandleCallFunction(Req, Resp)
            -HandleCreateBlueprintNode(Req, Resp)
            - 51 thin gRPC handlers total
        }

        class FAgentHttpServer {
            &lt;&lt;singleton&gt;&gt;
            -HttpRouter: IHttpRouter
            +Get()$ FAgentHttpServer
            +Start(Port)
            +Stop()
            -HandleExecute(Req, Resp)
            -HandleBatch(Req, Resp)
        }

        class ProtoMessages["Proto Messages (AgentBridge.proto)"] {
            PropertyValue
            ActorDescriptor
            ActorTransform
            ClassSchema
            BlueprintNodeInfo
            StreamingActorInfo
            51 Request/Response pairs
        }
    }

    AgentBridgeGrpcClient ..> UAgentBridgeServiceSubsystem : gRPC port 10001
    AgentBridgeGrpcClient ..> ProtoMessages : serializes/deserializes
    FAgentBridgeServerModule --> FAgentHttpServer : starts on port 8080
    UAgentBridgeServiceSubsystem ..> ProtoMessages : converts to/from

    %% =============================================
    %% LAYER 3: AGENTBRIDGE SCRIPTING (Dispatch)
    %% =============================================

    namespace L3_AgentBridgeScripting {
        class FCommandExecutor {
            &lt;&lt;static, 7400 lines&gt;&gt;
            +ExecuteJson(json)$ FString
            +Execute(FQueryActorsCommand)$
            +Execute(FSpawnActorCommand)$
            +Execute(FGetPropertyPathCommand)$
            +Execute(FSetPropertyPathCommand)$
            +Execute(FCallFunctionCommand)$
            +Execute(FCreateBlueprintNodeCommand)$
            +Execute(FConnectBlueprintPinsCommand)$
            + 50 plus overloaded Execute
            -ResolveActor(name)$ AActor*
            -ResolveObject(name)$ UObject*
            -BuildActorInfo(Actor)$ FActorInfo
        }

        class EAgentCommandType {
            &lt;&lt;enum, 116 plus values&gt;&gt;
            QueryActors
            SpawnActor
            GetPropertyPath
            SetPropertyPath
            CallFunction
            CreateBlueprintNode
        }

        class FAgentCommandBase {
            +CommandId: FString
            +Type: EAgentCommandType
        }

        class FAgentResponseBase {
            +bSuccess: bool
            +ErrorMessage: FString
            +ExecutionTimeMs: double
        }

        class CommandStructs["50+ Command Structs"] {
            FQueryActorsCommand
            FSpawnActorCommand
            FGetPropertyPathCommand
            FSetPropertyPathCommand
            FCallFunctionCommand
            FCreateBlueprintNodeCommand
        }

        class ResponseStructs["Info and Response Structs"] {
            FActorInfo
            FBlueprintNodeInfo
            FBlueprintPinInfo
            FWorldInfo
        }
    }

    UAgentBridgeServiceSubsystem --> FCommandExecutor : all 51 handlers delegate
    FAgentHttpServer --> FCommandExecutor : HTTP delegates
    FCommandExecutor ..> FAgentCommandBase : executes
    FCommandExecutor ..> FAgentResponseBase : returns
    FAgentCommandBase --> EAgentCommandType : typed by
    CommandStructs --|> FAgentCommandBase : extend
    FCommandExecutor ..> ResponseStructs : builds

    %% =============================================
    %% LAYER 4: AGENTBRIDGE RUNTIME (World Ops)
    %% =============================================

    namespace L4_AgentBridgeRuntime {
        class FWorldContextManager {
            &lt;&lt;singleton&gt;&gt;
            -WorldOverride: TWeakObjectPtr~UWorld~
            +Get()$ FWorldContextManager
            +GetTargetWorld() UWorld*
            +SetTargetWorldOverride(World)
            +IsEditorWorld() bool
            +IsPIEWorld() bool
            +GetCapabilities() Caps
        }

        class FActorOperations {
            &lt;&lt;static&gt;&gt;
            +QueryActors(Params)$ TArray~FActorReference~
            +FindActorByName(name)$ AActor*
            +SpawnActor(Params)$ AActor*
            +DuplicateActor(Source)$ AActor*
            +DestroyActor(Actor)$ bool
            +SetActorTransform(Actor, T)$ bool
            +AttachActor(Child, Parent)$ bool
        }

        class TargetResolution {
            &lt;&lt;namespace&gt;&gt;
            +Parse(target)$ FTargetInfo
            +Resolve(World, target)$ FResolvedTarget
            +FindComponent(Actor, name)$ USceneComponent*
        }

        class FWorldPartitionOps {
            &lt;&lt;static&gt;&gt;
            +IsWorldPartitioned()$ bool
            +QueryAllActors(Params)$ TArray
            +GetActorStreamingState(Guid)$ State
            +QueryLandscapeProxies()$ TArray
            +GetLandscapeBounds()$ FLandscapeBounds
            +GetDataLayers()$ TArray~FName~
        }

        class FActorReference {
            +Guid: FString
            +Path: FString
            +Name: FString
            +Label: FString
            +ClassName: FString
            +Resolve(World) AActor*
        }

        class FStreamingActorReference {
            +StreamingState: EActorStreamingState
            +EditorBounds: FBox
            +DataLayers: TArray~FName~
            +Transform: FTransform
        }

        class FResolvedTarget {
            +Actor: AActor*
            +Component: USceneComponent*
            +Error: FString
        }

        class FAgentBridgeDebug {
            &lt;&lt;static, 30 console commands&gt;&gt;
            +RegisterCommands()$
            +DumpObject(Object)$
            +DumpClassSchema(Class)$
        }
    }

    FStreamingActorReference --|> FActorReference
    FCommandExecutor --> FActorOperations : actor CRUD
    FCommandExecutor --> FWorldContextManager : world selection
    FCommandExecutor --> TargetResolution : Actor to Component
    FCommandExecutor --> FWorldPartitionOps : streaming queries
    FActorOperations --> FWorldContextManager : resolves world
    FActorOperations ..> FActorReference : returns
    TargetResolution ..> FResolvedTarget : returns
    TargetResolution --> FActorOperations : FindActorByName
    FWorldPartitionOps --> FWorldContextManager : resolves world
    FWorldPartitionOps ..> FStreamingActorReference : returns
    FAgentBridgeDebug --> FActorOperations : uses
    FAgentBridgeDebug --> FWorldContextManager : uses

    %% =============================================
    %% LAYER 5: AGENTBRIDGE CORE (Reflection)
    %% =============================================

    namespace L5_AgentBridgeCore {
        class FPropertyAccessor {
            &lt;&lt;static&gt;&gt;
            +ReadProperty(Container, Prop)$ Value
            +WriteProperty(Container, Prop, Val)$
            +WritePropertyDirect(Ptr, Prop, Val)$
            +GetPropertyType(Prop)$ Type
            +SerializeObjectReference(Obj)$ str
            +ResolveObjectReference(str)$ UObject*
            -ReadStructProperty()$
            -ReadArrayProperty()$
            -ReadMapProperty()$
            -TryReadSpecialStruct()$
        }

        class FAgentPropertyPath {
            &lt;&lt;static&gt;&gt;
            +ParsePath(str)$ TArray~Segment~
            +GetValue(Object, path)$ Result
            +SetValue(Object, path, val)$ bool
            +PathExists(Object, path)$ bool
        }

        class FTypeDiscovery {
            &lt;&lt;static&gt;&gt;
            +FindClassByName(name)$ UClass*
            +GetAllClassesOfType(Base)$ TArray
            +GetClassInfo(Class)$ ClassInfo
            +GetClassProperties(Class)$ TArray
            +GetClassFunctions(Class)$ TArray
            +FindStructByName(name)$ UScriptStruct*
            +NormalizeClassName(name)$ FString
        }

        class FFunctionInvoker {
            &lt;&lt;static&gt;&gt;
            +FindFunction(Class, name)$ UFunction*
            +GetFunctionSignature(Func)$ Sig
            +InvokeFunction(Target, Func, Params)$ Result
            +InvokeStaticFunction(Class, Func, Params)$ Result
            -PrepareParameters()$
            -ExtractResults()$
        }

        class FAgentPropertyValue {
            +Type: EAgentPropertyType
            +StringValue: FString
            +ArrayValue: TArray~FAgentPropertyValue~
            +StructValue: TMap~str,FAgentPropertyValue~
            +FromBool() FromInt() FromVector()$
            +AsBool() AsInt() AsVector()
        }

        class EAgentPropertyType {
            &lt;&lt;enum&gt;&gt;
            Bool Int32 Int64
            Float Double
            String Name Text
            Vector Rotator Transform Color
            Object SoftObject Class
            Struct Enum Array Map Set
        }

        class FAgentPropertyInfo {
            +PropertyName: FString
            +Type: EAgentPropertyType
            +TypeName: FString
            +bIsReadOnly: bool
        }

        class FAgentClassInfo {
            +ClassName: FString
            +ClassPath: FString
            +ParentClassName: FString
            +bIsBlueprintClass: bool
        }

        class FAgentFunctionSignature {
            +FunctionName: FString
            +Parameters: TArray~FAgentPropertyInfo~
            +ReturnValue: FAgentPropertyInfo
            +bIsStatic: bool
        }

        class FAgentFunctionResult {
            +bSuccess: bool
            +ReturnValue: FAgentPropertyValue
            +OutParams: TMap
        }

        class FPropertyPathSegment {
            +Type: EPropertyPathSegmentType
            +Name: FString
            +Index: int32
        }
    }

    FAgentPropertyValue --> EAgentPropertyType : typed by
    FAgentPropertyInfo --> EAgentPropertyType : typed by
    FAgentFunctionSignature o-- FAgentPropertyInfo : params
    FAgentFunctionResult o-- FAgentPropertyValue : return

    FAgentPropertyPath --> FPropertyAccessor : read/write via
    FAgentPropertyPath ..> FPropertyPathSegment : parses into
    FFunctionInvoker --> FPropertyAccessor : param marshaling
    FFunctionInvoker ..> FAgentFunctionResult : returns
    FTypeDiscovery ..> FAgentClassInfo : returns
    FTypeDiscovery ..> FAgentPropertyInfo : returns
    FTypeDiscovery ..> FAgentFunctionSignature : returns

    FCommandExecutor --> FAgentPropertyPath : property GET/SET
    FCommandExecutor --> FTypeDiscovery : class discovery
    FCommandExecutor --> FFunctionInvoker : function calls and PCG ops
    FActorOperations --> FPropertyAccessor : property access
    FActorOperations --> FTypeDiscovery : class loading
    FAgentBridgeDebug --> FPropertyAccessor : inspection
    FAgentBridgeDebug --> FTypeDiscovery : schema dump

    %% =============================================
    %% UE ENGINE APIS (External Dependencies)
    %% =============================================

    namespace L6_UnrealEngine {
        class FProperty {
            &lt;&lt;UE Reflection&gt;&gt;
            +ContainerPtrToValuePtr()
            +GetName() FName
        }

        class UClass {
            &lt;&lt;UE Reflection&gt;&gt;
            +FindFunctionByName()
            +GetDefaultObject()
        }

        class UFunction {
            &lt;&lt;UE Reflection&gt;&gt;
            +ParmsSize: int32
            +ProcessEvent()
        }

        class UWorld {
            &lt;&lt;UE Engine&gt;&gt;
            +SpawnActor()
        }

        class UK2Node {
            &lt;&lt;UE Editor, WITH_EDITOR&gt;&gt;
            +AllocateDefaultPins()
            +CreateNewGuid()
        }

        class UEdGraphSchema_K2 {
            &lt;&lt;UE Editor&gt;&gt;
            +TryCreateConnection(Pin1, Pin2)
        }

        class UBlueprint {
            &lt;&lt;UE Editor&gt;&gt;
            +GeneratedClass: UClass*
        }

        class UPCGGraph {
            &lt;&lt;UE Engine&gt;&gt;
            +AddNodeOfType(Class)
            +AddEdge(From, Pin, To, Pin)
            +RemoveNode(Node)
            +GetInputNode()
        }

        class ALandscapeProxy {
            &lt;&lt;UE Engine&gt;&gt;
            +GetLandscapeBounds()
        }
    }

    FPropertyAccessor --> FProperty : reads/writes via reflection
    FTypeDiscovery --> UClass : enumerates
    FFunctionInvoker --> UFunction : invokes
    FActorOperations --> UWorld : spawns/queries
    FWorldContextManager --> UWorld : manages
    FWorldPartitionOps --> ALandscapeProxy : queries bounds
    FCommandExecutor ..> UK2Node : creates (live BP tools)
    FCommandExecutor ..> UEdGraphSchema_K2 : connects pins
    FCommandExecutor ..> UBlueprint : loads/modifies
    FFunctionInvoker ..> UPCGGraph : invokes methods (live PCG tools)

    %% =============================================
    %% OFFLINE PATH: bp_toolkit Python Scripts
    %% =============================================

    namespace L1_BpToolkitScripts {
        class AssetModifier["AssetModifier (bp_builder.py)"] {
            -path: Path
            -data: dict
            -_namemap_set: set
            -_import_cache: dict
            +asset_type: str
            +get_property(path, export_idx) Any
            +set_property(path, value, export_idx)
            +list_properties(export_idx) List
            +list_graphs() List
            +add_comment(graph, text, x, y)
            +clone_node(node_name, offset)
            +clone_asset(new_name, folder)
            +add_name(name) int
            +find_import(class_name) int
            +save(output_path) Path
        }

        class AssetParser["AssetParser (asset_parser.py)"] {
            +detect_asset_type(data) str
            +find_in_asset(data, pattern) List
            +query_asset(path, query_type) Any
        }

        class BpParser["BpParser (bp_parser.py)"] {
            +parse_blueprint(json, out_dir) dict
            -extract_call_graph(nodes) List
            -categorize_namemap(names) dict
        }

        class BpExport["BpExport (bp_export.py)"] {
            +export_uasset_to_json(uasset, json, ver) bool
            +import_json_to_uasset(json, uasset, ver) bool
            +get_uassetgui_path() Path
        }
    }

    BpToolkitService --> AssetModifier : lazy import
    BpToolkitService --> AssetParser : lazy import
    BpToolkitService --> BpParser : lazy import
    BpToolkitService --> BpExport : lazy import
    AssetModifier ..> AssetParser : detect_asset_type

    %% =============================================
    %% OFFLINE PATH: UAssetGUI CLI (.NET 8)
    %% =============================================

    namespace L2_UAssetGUI {
        class UAssetGUI_CLI["UAssetGUI.exe (.NET 8 CLI)"] {
            +tojson(source, dest, version)
            +fromjson(source, dest, version)
        }
    }

    BpExport --> UAssetGUI_CLI : subprocess call

    %% =============================================
    %% OFFLINE PATH: UAssetAPI Library (C# .NET 8)
    %% =============================================

    namespace L3_UAssetAPI {
        class UAsset["UAsset (INameMap)"] {
            +NameMap: List~FString~
            +Imports: List~Import~
            +Exports: List~Export~
            +CustomVersionContainer: List
            +PackageFlags: EPackageFlags
            +HasUnversionedProperties: bool
            +AddNameReference(str) int
            +SearchNameReference(str) int
            +Read(reader)
            +Write(writer)
        }

        class Import_CS["Import"] {
            +ObjectName: FName
            +OuterIndex: FPackageIndex
            +ClassPackage: FName
            +ClassName: FName
        }

        class Export_CS["Export (base)"] {
            +ObjectName: FName
            +ClassIndex: FPackageIndex
            +OuterIndex: FPackageIndex
            +SuperIndex: FPackageIndex
            +ObjectFlags: EObjectFlags
            +Extras: byte[]
        }

        class NormalExport_CS["NormalExport"] {
            +Data: List~PropertyData~
            +ObjectGuid: Guid?
        }

        class StructExport_CS["StructExport"] {
            +SuperStruct: FPackageIndex
            +Children: FPackageIndex[]
            +LoadedProperties: FProperty[]
            +ScriptBytecode: KismetExpression[]
        }

        class ClassExport_CS["ClassExport"] {
            +ClassFlags: EClassFlags
            +FuncMap: TMap~FName,FPackageIndex~
            +Interfaces: InterfaceReference[]
            +ClassDefaultObject: FPackageIndex
        }

        class FunctionExport_CS["FunctionExport"] {
            &lt;&lt;function-specific data&gt;&gt;
        }

        class PropertyData_CS["PropertyData (abstract, 60+ subtypes)"] {
            +Name: FName
            +ArrayIndex: int
            +PropertyGuid: Guid?
            +PropertyTagFlags: Flags
        }

        class NumericProps["Numeric Properties"] {
            IntPropertyData
            FloatPropertyData
            DoublePropertyData
            BytePropertyData
            BoolPropertyData
        }

        class StringProps["String Properties"] {
            StrPropertyData
            NamePropertyData
            TextPropertyData
        }

        class ObjectProps["Object Properties"] {
            ObjectPropertyData
            SoftObjectPropertyData
            ClassPropertyData
            WeakObjectPropertyData
        }

        class CollectionProps["Collection Properties"] {
            ArrayPropertyData
            MapPropertyData
            SetPropertyData
        }

        class StructPropertyData_CS["StructPropertyData"] {
            +StructType: string
            +Value: List~PropertyData~
        }

        class FName_CS["FName"] {
            +Value: FString
            +Index: int
            +Number: int
            +Asset: INameMap
        }

        class FPackageIndex_CS["FPackageIndex"] {
            +Index: int
            +IsImport() bool (negative)
            +IsExport() bool (positive)
            +IsNull() bool (zero)
            +ToImport(asset) Import
            +ToExport(asset) Export
        }

        class MainSerializer_CS["MainSerializer"] {
            +Read(reader, name) PropertyData
            +Write(data, writer)
            +ReadFProperty(reader) FProperty
        }

        class KismetSerializer_CS["KismetSerializer"] {
            +ReadExpression() KismetExpression
            +WriteExpression()
        }

        class JsonConverters["JSON Converters"] {
            FNameJsonConverter
            FNameTypeConverter
            FPackageIndexJsonConverter
            TMapJsonConverter
        }
    }

    UAssetGUI_CLI --> UAsset : uses library
    UAsset "1" o-- "*" Import_CS : contains
    UAsset "1" o-- "*" Export_CS : contains
    UAsset "1" o-- "*" FName_CS : NameMap

    NormalExport_CS --|> Export_CS
    StructExport_CS --|> NormalExport_CS
    ClassExport_CS --|> StructExport_CS
    FunctionExport_CS --|> StructExport_CS

    NormalExport_CS "1" o-- "*" PropertyData_CS : Data array

    NumericProps --|> PropertyData_CS
    StringProps --|> PropertyData_CS
    ObjectProps --|> PropertyData_CS
    CollectionProps --|> PropertyData_CS
    StructPropertyData_CS --|> PropertyData_CS
    StructPropertyData_CS "1" o-- "*" PropertyData_CS : nested Value

    Import_CS --> FName_CS : references names
    Import_CS --> FPackageIndex_CS : OuterIndex
    Export_CS --> FName_CS : ObjectName
    Export_CS --> FPackageIndex_CS : ClassIndex OuterIndex
    PropertyData_CS --> FName_CS : Name

    MainSerializer_CS --> PropertyData_CS : creates
    StructExport_CS --> KismetSerializer_CS : bytecode
    UAsset --> JsonConverters : JSON serialization
    UAsset --> MainSerializer_CS : binary serialization

    %% =============================================
    %% FILESYSTEM
    %% =============================================

    namespace L4_Filesystem {
        class UAssetFiles[".uasset / .uexp files (binary)"] {
            &lt;&lt;binary on disk&gt;&gt;
        }
        class JsonFiles[".json files (intermediate)"] {
            &lt;&lt;text on disk&gt;&gt;
        }
    }

    UAsset --> UAssetFiles : reads/writes binary
    UAsset --> JsonFiles : reads/writes JSON
    AssetModifier --> JsonFiles : loads/saves
```

## Data Flow - Three Execution Paths

```
                          EXTERNAL AI AGENT (Claude, etc.)
                                     |
          +-------------- MCPServer (Python, JSON-RPC over stdio) ---------------+
          |                          |                                            |
   ~57 live tools           14 offline tools                             ~30 Tempo tools
          |                          |                                            |
  agentbridge.py             bp_toolkit.py                               tempo_*.py
  AgentBridgeGrpcClient      (lazy Python imports)                       TempoXxxClient
          |                          |                                            |
          | gRPC                     | Python-only                                | gRPC
          |                          |                                            |
  +-------v-----------+   +---------v-----------+                       +--------v--------+
  | AgentBridgeService|   | AssetModifier       |                       | Tempo gRPC      |
  | Subsystem (C++)   |   | AssetParser         |                       | Services (C++)  |
  | 51 thin handlers  |   | BpParser            |                       +-----------------+
  +-------+-----------+   | BpExport            |
          |               +---------+-----------+
  +-------v-----------+             | subprocess
  | FCommandExecutor  |   +---------v-----------+
  | (Scripting,static)|   | UAssetGUI.exe       |
  | 50+ Execute()     |   | (.NET 8 CLI)        |
  +--+----------+-----+   +---------+-----------+
     |          |                    |
  +--v---+ +---v----+    +----------v----------+
  |Runtim| | Core   |    | UAssetAPI (C#)      |
  |ActOps| |PropAccs|    | UAsset              |
  |WrldCx| |PropPath|    |   NameMap           |
  |Target| |TypeDisc|    |   Imports           |
  |WP Ops| |FuncInv |    |   Exports           |
  +--+---+ +---+----+    |     NormalExport    |
     |         |          |     StructExport    |
     +----+----+          |     ClassExport     |
          |               |   PropertyData(60+) |
  +-------v-----------+   |   MainSerializer    |
  | UE Reflection     |   |   KismetSerializer  |
  | FProperty, UClass |   +----------+----------+
  | UFunction, UWorld  |              |
  | UK2Node, UPCGGraph |   +----------v----------+
  +--------------------+   | .uasset / .json     |
    IN-MEMORY (live)       | files (on disk)     |
                           +---------------------+
                             ON-DISK (offline)
```

## Layer Summary

| Layer | Module | Language | Key Classes | Role |
|-------|--------|----------|-------------|------|
| 0 | MCP Server | Python | `MCPServer`, `ServiceModule` | JSON-RPC stdio interface for AI agents |
| 1a | agentbridge.py | Python | `AgentBridgeGrpcClient` | gRPC client, 57 live tools |
| 1b | bp_toolkit.py | Python | `BpToolkitService` | 14 offline asset tools |
| 1c | tempo_*.py | Python | `TempoXxxClient` (11 modules) | 30 Tempo simulation tools |
| 2 | AgentBridgeServer | C++ | `UAgentBridgeServiceSubsystem`, `FAgentHttpServer` | gRPC/HTTP network layer |
| 3 | AgentBridgeScripting | C++ | `FCommandExecutor`, `FAgentCommandBase` | Command dispatch, JSON serialization |
| 4 | AgentBridgeRuntime | C++ | `FActorOperations`, `FWorldContextManager`, `TargetResolution`, `FWorldPartitionOps` | World operations, actor management |
| 5 | AgentBridgeCore | C++ | `FPropertyAccessor`, `FAgentPropertyPath`, `FTypeDiscovery`, `FFunctionInvoker` | UE reflection primitives |
| 6 | UE Engine | C++ | `FProperty`, `UClass`, `UFunction`, `UWorld`, `UK2Node`, `UPCGGraph` | Unreal Engine APIs |
| O1 | bp_toolkit scripts | Python | `AssetModifier`, `AssetParser`, `BpParser`, `BpExport` | Offline asset manipulation |
| O2 | UAssetGUI | C# .NET 8 | CLI binary | Binary-to-JSON conversion |
| O3 | UAssetAPI | C# .NET 8 | `UAsset`, `Export`, `PropertyData`, `FName`, `FPackageIndex` | UE binary format parser/writer |

## Live vs Offline Tool Paths

| Aspect | Live Path (gRPC) | Offline Path (bp_toolkit) |
|--------|-------------------|---------------------------|
| **Requires editor** | Yes | No |
| **Tool count** | 57 (AgentBridge) + 30 (Tempo) | 14 |
| **Languages** | Python - C++ | Python - C# |
| **Data format** | Protobuf over gRPC | JSON files on disk |
| **Latency** | Milliseconds | Seconds (export/import cycle) |
| **Capabilities** | Full actor/property/BP/PCG manipulation | Asset parse/modify/clone |
| **BP editing** | Create nodes, connect pins (live) | Clone nodes, add comments (offline) |
| **PCG editing** | Add/connect/delete nodes (live) | Property modification (offline) |
| **Limitation** | Editor must be running | K2Node pin data opaque (Extras blob) |

## Design Principles

1. **Dependencies flow downward only.** Server depends on Scripting depends on Runtime depends on Core. Never upward.

2. **Static utility classes dominate.** `FPropertyAccessor`, `FAgentPropertyPath`, `FTypeDiscovery`, `FFunctionInvoker`, `FActorOperations`, `FCommandExecutor` are all stateless. Only `FWorldContextManager` (singleton) and `UAgentBridgeServiceSubsystem` (UE-managed) hold state.

3. **`FAgentPropertyValue` is the universal transport type.** Every layer converts to/from this single struct for property values - Core creates them from `FProperty` reflection, Scripting serializes to JSON, Server converts to protobuf.

4. **Thin handler pattern isolates gRPC.** All 51 handlers in `UAgentBridgeServiceSubsystem` only do proto-to-struct conversion, delegate to `FCommandExecutor`, then struct-to-proto. This exists because gRPC headers conflict with many UE editor headers.

5. **UAssetAPI mirrors UE reflection.** Its `PropertyData` hierarchy (60+ types) parallels UE's `FProperty` tree. Its `FName`/`FPackageIndex` types mirror UE's own name map and package index system. This symmetry enables offline round-tripping.

---

*See per-layer documents for detailed class descriptions and implementation notes.*
