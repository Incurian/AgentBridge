# AgentBridgeServer

gRPC and HTTP server plugin exposing AgentBridge to external AI agents.

## Overview

AgentBridgeServer is a standalone Unreal Engine plugin that provides the network
interface for AgentBridge. It implements 38 gRPC RPCs via the Tempo scripting
infrastructure and an HTTP/JSON fallback server for testing.

This plugin sits at the top of the AgentBridge dependency chain. It is part of
the [AgentBridge](../README.md) ecosystem, which was split from a monolithic
plugin into four standalone plugins following the Tempo multi-plugin pattern.

## Architecture

**Thin handlers only.** All business logic lives in AgentBridgeScripting
(CommandExecutor). This plugin converts between protobuf messages and internal
command structs, then delegates to lower layers.

```
External Agent --> gRPC (port 10001) --> AgentBridgeServiceSubsystem
                  HTTP  (port 8080) --> AgentHttpServer
                                            |
                                            v
                                    AgentBridgeScripting
                                    (CommandExecutor)
```

## Header Conflict Warning

This plugin CANNOT include certain UE headers due to Windows SDK conflicts with
gRPC headers. Problematic headers include `AssetRegistry/AssetRegistryModule.h`,
`Editor.h`, `LevelEditor.h`, and `IImageWrapper.h`. Any functionality requiring
these headers must be placed in AgentBridgeScripting/CommandExecutor.cpp instead.

## Dependencies

| Plugin | Purpose |
|--------|---------|
| AgentBridgeCore | Reflection primitives |
| AgentBridgeRuntime | World ops, actor ops, property paths |
| AgentBridgeScripting | Command dispatch, JSON serialization |
| TempoCore | gRPC infrastructure, scripting framework |

## Ports

| Protocol | Port | Notes |
|----------|------|-------|
| gRPC | 10001 | Via Tempo scripting infrastructure |
| HTTP | 8080 | JSON fallback for testing |

## Key Files

| File | Purpose |
|------|---------|
| `AgentBridge.proto` | gRPC service definition (38 RPCs) |
| `AgentBridgeServiceSubsystem.h/.cpp` | gRPC handler implementations |
| `AgentHttpServer.h/.cpp` | HTTP/JSON fallback server |

## Detailed Documentation

See [CLAUDE.md](CLAUDE.md) for
implementation details, header conflict specifics, proto generation, and value
conversion internals.
