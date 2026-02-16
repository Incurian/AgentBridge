# AgentBridgeRuntime

World context management, actor operations, and property path resolution for AgentBridge.

## Overview

AgentBridgeRuntime is a standalone Unreal Engine plugin that provides the core world
interaction layer used by the AgentBridge ecosystem. It sits between the low-level
reflection primitives (AgentBridgeCore) and the command/serialization layer
(AgentBridgeScripting).

## Capabilities

| Component | Description |
|-----------|-------------|
| `FWorldContextManager` | Multi-world support (Editor, PIE, Game) with capability queries |
| `FActorOperations` | Query, spawn, delete, and transform actors |
| `FWorldPartitionOps` | Streaming-aware queries, landscape bounds, data layers |
| `FTargetResolution` | Resolve actor/component targets from string identifiers |

Property path resolution (`FAgentPropertyPath`) is provided by the AgentBridgeCore dependency.

## Dependencies

- **AgentBridgeCore** (required) - Reflection primitives, property path resolution

## Console Commands

These commands are available in the Unreal console for testing:

| Command | Description |
|---------|-------------|
| `AgentBridge.ListWorlds` | List all world contexts |
| `AgentBridge.Capabilities` | Show current context capabilities |
| `AgentBridge.QueryActors <class> [limit]` | Query actors by class |
| `AgentBridge.IsPartitioned` | Check if world uses World Partition |
| `AgentBridge.QueryAllActors [pattern] [limit]` | Query including unloaded actors |
| `AgentBridge.QueryLandscape` | List landscape proxies |
| `AgentBridge.GetLandscapeBounds` | Get full landscape bounds |

## Part of AgentBridge

This plugin is one of four that make up the AgentBridge system:

| Plugin | Role |
|--------|------|
| AgentBridgeCore | Reflection primitives, type discovery |
| **AgentBridgeRuntime** | **World ops, actor ops, property paths** |
| AgentBridgeScripting | Command dispatch, JSON serialization |
| AgentBridgeServer | gRPC/HTTP server, proto definitions |

## Detailed Documentation

See [CLAUDE.md](CLAUDE.md) for
architecture details, code examples, thread safety notes, and known issues.
