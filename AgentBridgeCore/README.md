# AgentBridgeCore

Low-level UE reflection primitives for reading/writing properties and invoking functions.

## Overview

AgentBridgeCore is a standalone Unreal Engine plugin that provides the foundation layer
for the AgentBridge ecosystem. It exposes UE reflection capabilities (FProperty traversal,
UFunction invocation, class/type discovery) as reusable C++ utilities.

This plugin has **no dependencies on other AgentBridge plugins** -- it only depends on
Engine modules (Core, CoreUObject, Engine). Higher-level plugins build on top of it:

```
AgentBridgeServer  ->  AgentBridgeScripting  ->  AgentBridgeRuntime  ->  AgentBridgeCore
```

## Key Components

| Class | Purpose |
|-------|---------|
| `FPropertyAccessor` | Read/write any FProperty type recursively, including arrays, maps, structs |
| `FFunctionInvoker` | Dynamic UFunction invocation with parameter marshaling |
| `FTypeDiscovery` | Class/struct/enum discovery, Blueprint name normalization |
| `FAgentPropertyPath` | Property path resolution with dot-notation traversal |
| `FAgentPropertyValue` | Type-safe property value container for serialization |

## Dependencies

- **Engine modules only:** Core, CoreUObject, Engine
- **No plugin dependencies** -- this is the base of the AgentBridge dependency chain

## Module Type

- **Type:** Runtime
- **Loading Phase:** Default

## Documentation

See [CLAUDE.md](CLAUDE.md) for detailed development documentation including:
- Critical code patterns (array traversal, map iteration, BP property names)
- UObject pointer type reference
- Recent fixes and known issues
- Testing commands

## Part of AgentBridge

This plugin is part of the [AgentBridge](../README.md) ecosystem, which exposes Unreal Engine
editor and runtime state to external AI agents via gRPC and MCP. AgentBridgeCore provides the
reflection primitives that all other AgentBridge plugins depend on.
