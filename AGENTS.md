# AGENTS.md - Router

This repository now uses two focused guides:

- `AGENTS_dev.md` - for developing AgentBridge (C++/Python implementation work)
- `AGENTS_use.md` - for using AgentBridge tools to control Unreal Editor

## Immediate Instruction (Required)

Before doing anything else, read one or both files immediately:

1. If you are changing code in this repo, read `AGENTS_dev.md` first.
2. If you are operating tools in Unreal (level/asset/sim workflows), read `AGENTS_use.md` first.
3. If your task includes both implementation and tool operation, read both in full.

Do not proceed with actions until the relevant guide(s) have been read.

## Topic Outline: `AGENTS_dev.md`

Covers:
- architecture boundaries and module responsibilities
- where business logic belongs (Server vs Scripting vs Runtime vs Core)
- canonical checklist for adding new gRPC + MCP tools
- build/run/test workflow and live validation requirements
- critical implementation rules (property writes, arrays/maps, world context, proto gotchas)
- documentation update contract
- known limitations and development guardrails
- module README references for deeper implementation details

Use when tasks include:
- adding RPCs, proto changes, command structs, or MCP tools
- modifying C++ plugin behavior or Python MCP behavior
- debugging transport/registration/conversion issues

## Topic Outline: `AGENTS_use.md`

Covers:
- startup prerequisites and initial `help()` workflow
- module/profile loading for tools
- critical usage rules (struct value format, volume sizing via `set_transform`, `/Game/` constraints)
- property, actor, transform, asset, and PCG-biome operating patterns
- troubleshooting map and efficiency guidance
- end-of-task safety verification checklist
- module README references plus top-level docs for further reading

Use when tasks include:
- controlling Unreal Editor through AgentBridge tools
- creating/modifying actors/assets via MCP
- running PCG/biome/simulation/editor workflows as an agent

## Additional Reference Docs

For deeper module-specific detail, see:
- `AgentBridgeCore/README.md`
- `AgentBridgeRuntime/README.md`
- `AgentBridgeScripting/README.md`
- `AgentBridgeServer/README.md`
- `mcp/README.md`
- `bp_toolkit/README.md`
