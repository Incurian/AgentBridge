# AgentBridgeScripting

Command dispatch, JSON serialization, and business logic for the AgentBridge ecosystem.

## Overview

AgentBridgeScripting provides the command/response abstraction layer that sits between
the transport layer (gRPC/HTTP in AgentBridgeServer) and the engine operations
(AgentBridgeCore, AgentBridgeRuntime). It defines 50+ command/response struct types and
implements all business logic in a single central dispatcher (`CommandExecutor`).

## What This Plugin Provides

- **AgentCommands.h** -- All command and response structs (world ops, actor ops, property
  access, class discovery, asset management, file operations, Blueprint node editing)
- **CommandExecutor** -- Central dispatch that routes commands to the appropriate runtime
  operations, handles JSON serialization/deserialization, and converts between C++ types
  and the agent-facing value representations
- **JSON value conversion** -- Bidirectional conversion for Bool, Int, Float, String,
  Vector, Rotator, Transform, Color, Arrays, and Structs

## Key Architectural Constraint

**ALL business logic goes in CommandExecutor.cpp, NOT in AgentBridgeServer.**

AgentBridgeServer includes gRPC headers that conflict with certain UE headers
(AssetRegistry, Editor, ImageWrapper, etc.). The Server module contains only thin
handlers that convert between proto messages and command structs. Any logic that
needs UE editor or engine headers must live here in the Scripting plugin.

## Dependencies

| Plugin | Purpose |
|--------|---------|
| AgentBridgeCore | Reflection primitives (FProperty traversal, type discovery) |
| AgentBridgeRuntime | World context, actor operations, property path resolution |

## Part of AgentBridge

This plugin is one of four that make up the AgentBridge system:

```
AgentBridgeCore      -- Reflection primitives
AgentBridgeRuntime   -- World ops, actor ops, property paths
AgentBridgeScripting -- Command dispatch, JSON serialization (this plugin)
AgentBridgeServer    -- gRPC/HTTP server, proto definitions
```

A wrapper plugin (`AgentBridge.uplugin`) ties them together via dependency chain.

## Detailed Documentation

See [CLAUDE.md](CLAUDE.md) for implementation details, command
reference, resolved issues, and development patterns.
