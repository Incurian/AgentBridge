# UAsset Dissection Guide for AI Agents

**Purpose:** This guide enables any AI agent to systematically analyze Unreal Engine Blueprint assets exported to JSON format. Follow these steps to understand any Blueprint's architecture, logic, and implementation patterns.

## Prerequisites

You need:
1. A `.json` file exported from a `.uasset` using UAssetAPI
2. Python 3.8+ for parsing (stdlib only)
3. Access to the codebase's `.claude/archive/` directory

If you only have a `.uasset` file, the user must first export it to JSON using UAssetAPI/UAssetGUI.

---

## Phase 1: Initial Assessment (5 minutes)

### 1.1 Check File Size

```bash
ls -lh *.json
```

- **< 5 MB:** Can read directly with reasonable context
- **5-50 MB:** Needs parsing into smaller chunks
- **> 50 MB:** Definitely needs parsing; consider selective extraction

### 1.2 Verify JSON Validity

```python
import json
with open('asset.json', 'r') as f:
    data = json.load(f)
print(f"Keys: {list(data.keys())}")
print(f"Exports: {len(data.get('Exports', []))}")
print(f"NameMap: {len(data.get('NameMap', []))}")
```

### 1.3 Identify Asset Type

Look at Export 0's ObjectName:
- Contains "Pawn" → Character/Pawn Blueprint
- Contains "Actor" → Actor Blueprint
- Contains "Widget" → UI Widget Blueprint
- Contains "AnimBP" → Animation Blueprint
- Contains "Component" → Component Blueprint

---

## Phase 2: Parse and Organize (10 minutes)

### 2.1 Create Parse Script

Create or copy `parse_blueprint.py`:

```python
#!/usr/bin/env python3
"""
Generic Blueprint parser - splits UAssetAPI JSON into navigable hierarchy.
Usage: python parse_blueprint.py input.json output_dir/
"""

import json
import os
import re
from pathlib import Path
from collections import defaultdict
import sys

def sanitize_filename(name: str) -> str:
    """Convert a name to a safe filename."""
    name = re.sub(r'[<>:"/\\|?*\s]', '_', name)
    name = name.replace('_GEN_VARIABLE', '')
    name = name.strip('_')
    return name or "unnamed"

def extract_array_indices(data_list, prop_name):
    """Extract array of object indices from a Data list property."""
    if not isinstance(data_list, list):
        return []
    for item in data_list:
        if isinstance(item, dict) and item.get("Name") == prop_name:
            value = item.get("Value", [])
            if isinstance(value, list):
                return [v.get("Value") for v in value if isinstance(v, dict)]
    return []

def get_export_name(export):
    """Get the display name of an export."""
    return export.get("ObjectName", export.get("Name", "Unknown"))

def extract_comments(export):
    """Extract human-readable comments from an export."""
    comments = []
    data = export.get("Data", [])
    if not isinstance(data, list):
        return comments
    for item in data:
        if not isinstance(item, dict):
            continue
        name = item.get("Name", "")
        value = item.get("Value", "")
        if isinstance(value, str) and len(value) > 10:
            if not re.match(r'^[0-9A-Fa-f\-]+$', value):
                comments.append({"property": name, "text": value[:500]})
    return comments

def main():
    if len(sys.argv) < 2:
        print("Usage: python parse_blueprint.py input.json [output_dir]")
        sys.exit(1)

    input_file = Path(sys.argv[1])
    output_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else input_file.parent / f"{input_file.stem}_parsed"

    print(f"Loading {input_file}...")
    with open(input_file, 'r', encoding='utf-8') as f:
        data = json.load(f)

    # Create directories
    output_dir.mkdir(exist_ok=True)
    (output_dir / "graphs").mkdir(exist_ok=True)
    (output_dir / "functions").mkdir(exist_ok=True)
    (output_dir / "components").mkdir(exist_ok=True)

    exports = data.get("Exports", [])
    namemap = data.get("NameMap", [])

    # Save metadata
    metadata = {
        "source": str(input_file),
        "exports_count": len(exports),
        "namemap_count": len(namemap),
    }
    with open(output_dir / "_metadata.json", 'w') as f:
        json.dump(metadata, f, indent=2)

    # Save NameMap (grep-friendly)
    with open(output_dir / "_namemap.txt", 'w') as f:
        for i, name in enumerate(namemap):
            f.write(f"{i:04d}: {name}\n")

    # Get graph indices from Export 0
    if exports:
        main_data = exports[0].get("Data", [])
        ubergraph_indices = set(extract_array_indices(main_data, "UbergraphPages"))
        function_indices = set(extract_array_indices(main_data, "FunctionGraphs"))
    else:
        ubergraph_indices = set()
        function_indices = set()

    # Categorize and save exports
    all_comments = []
    summary = {"graphs": [], "functions": [], "components": []}

    for i, export in enumerate(exports):
        name = get_export_name(export)
        comments = extract_comments(export)
        if comments:
            all_comments.append({"index": i, "name": name, "comments": comments})

        # Determine category
        if i in ubergraph_indices or (i - 1) in ubergraph_indices:
            safe = sanitize_filename(name)
            with open(output_dir / "graphs" / f"{safe}.json", 'w') as f:
                json.dump({"index": i, "name": name, "data": export}, f, indent=2)
            summary["graphs"].append(name)
        elif i in function_indices or (i - 1) in function_indices:
            safe = sanitize_filename(name)
            with open(output_dir / "functions" / f"{safe}.json", 'w') as f:
                json.dump({"index": i, "name": name, "data": export}, f, indent=2)
            summary["functions"].append(name)
        elif "_GEN_VARIABLE" in name or "Component" in name:
            safe = sanitize_filename(name)
            with open(output_dir / "components" / f"{safe}.json", 'w') as f:
                json.dump({"index": i, "name": name, "data": export}, f, indent=2)
            summary["components"].append(name)

    # Save comments
    with open(output_dir / "_comments.json", 'w') as f:
        json.dump(all_comments, f, indent=2)

    # Save summary
    with open(output_dir / "_summary.json", 'w') as f:
        json.dump(summary, f, indent=2)

    print(f"\nOutput: {output_dir}/")
    print(f"  Graphs: {len(summary['graphs'])}")
    print(f"  Functions: {len(summary['functions'])}")
    print(f"  Components: {len(summary['components'])}")
    print(f"  Comments: {len(all_comments)}")

if __name__ == "__main__":
    main()
```

### 2.2 Run Parser

```bash
python parse_blueprint.py asset.json asset_parsed/
```

---

## Phase 3: Discover Structure (15 minutes)

### 3.1 Read Summary

```bash
cat asset_parsed/_summary.json | head -100
```

This gives you the list of graphs, functions, and components.

### 3.2 Search NameMap for Keywords

Based on what you're analyzing, search for relevant terms:

```bash
# For a VR pawn
grep -i "motion\|controller\|hmd\|vr\|hand" asset_parsed/_namemap.txt

# For a character
grep -i "movement\|jump\|crouch\|sprint" asset_parsed/_namemap.txt

# For a weapon
grep -i "fire\|reload\|ammo\|damage" asset_parsed/_namemap.txt

# For UI
grep -i "button\|text\|panel\|click" asset_parsed/_namemap.txt
```

### 3.3 Identify Key Systems

Look for these common patterns in NameMap:

| Pattern | Indicates |
|---------|-----------|
| `bIs*`, `bCan*`, `bHas*` | Boolean state flags |
| `*Velocity`, `*Speed` | Physics/movement |
| `*Component` | Component references |
| `*Timer`, `*Delay` | Timing systems |
| `On*`, `Event*` | Events/delegates |
| `*Trace`, `*Hit` | Raycasting/collision |
| `*Montage`, `*Anim*` | Animation |

### 3.4 Read Comments

Blueprint authors often leave documentation in comments:

```bash
cat asset_parsed/_comments.json | python -c "
import json, sys
data = json.load(sys.stdin)
for item in data:
    for c in item.get('comments', []):
        if len(c['text']) > 20:
            print(f\"[{item['name']}] {c['text'][:200]}\")
"
```

---

## Phase 4: Deep Dive into Systems (30+ minutes)

### 4.1 Component Hierarchy

Read component files to understand the scene structure:

```bash
ls asset_parsed/components/
```

For each major component, note:
- Parent (attachment)
- Location/Rotation/Scale
- Key properties

Document as a tree:
```
RootComponent
├── CameraComponent
│   └── SpringArmComponent
├── MeshComponent
└── CollisionComponent
```

### 4.2 Function Analysis

For each important function:

1. **Read the function file:**
   ```bash
   cat asset_parsed/functions/FunctionName.json | python -m json.tool | head -200
   ```

2. **Look for:**
   - Input parameters (K2Node_FunctionEntry)
   - Return values (K2Node_FunctionResult)
   - Called functions (K2Node_CallFunction)
   - Branches (K2Node_IfThenElse)
   - Variable access (K2Node_VariableGet/Set)

3. **Document the logic:**
   ```
   FunctionName(Input1, Input2) → ReturnValue
   ├─ If (Condition)
   │  ├─ True: DoSomething()
   │  └─ False: DoOther()
   └─ Return Result
   ```

### 4.3 Event Graph Analysis

Event graphs contain the runtime logic. Key events:

| Event | When it fires |
|-------|---------------|
| BeginPlay | Actor spawned |
| Tick | Every frame |
| EndPlay | Actor destroyed |
| OnComponentHit | Physics collision |
| Input* | Player input |

For each graph:
1. Find the entry event
2. Trace the execution flow
3. Document branches and conditions

### 4.4 State Machine Identification

Look for:
- Enum variables (state types)
- Switch statements on enums
- State transition functions

Common patterns:
```
EMovementState: Idle, Walking, Running, Jumping
EWeaponState: Ready, Firing, Reloading, Empty
EAIState: Patrol, Chase, Attack, Flee
```

---

## Phase 5: Create Documentation

### 5.1 Summary Document (BLUEPRINT_SUMMARY.md)

Create a comprehensive summary:

```markdown
# [Blueprint Name] Analysis

## Overview
- **Type:** [Pawn/Actor/Widget/etc.]
- **Purpose:** [What this Blueprint does]
- **Key Systems:** [List major systems]

## Component Hierarchy
[Tree diagram of components]

## State Variables
| Variable | Type | Purpose |
|----------|------|---------|
| bIsActive | bool | Whether actor is active |
| CurrentState | EState | Current state enum |
| ...

## Key Functions
### FunctionName
- **Purpose:** What it does
- **Inputs:** param1 (type), param2 (type)
- **Returns:** type
- **Called by:** Other functions that call this
- **Calls:** Functions this calls

## Event Flow
### Tick
1. Check state
2. If active, process logic
3. Update components

## Key Patterns
- [Notable design patterns used]
- [Unusual or clever implementations]

## Notes for Implementation
- [Things to watch out for]
- [Dependencies]
- [Potential improvements]
```

### 5.2 Execution Flow Document (EXECUTION_FLOW.md)

Create tree diagrams of logic flow:

```markdown
# Execution Flow

## Main Tick
```
Event Tick
├─ Check bIsActive
│  └─ False → Return
├─ UpdateMovement()
│  ├─ GetInputVector()
│  ├─ ApplyMovement()
│  └─ CheckCollision()
└─ UpdateVisuals()
```

## State Transitions
```
Idle → Walking: Input detected
Walking → Running: Sprint pressed
Running → Jumping: Jump pressed
Jumping → Falling: Peak reached
Falling → Idle: Ground detected
```
```

### 5.3 Reference Index System

Use indices to create bidirectional links between documentation and BP source:

| Index | Format | Example | Lookup |
|-------|--------|---------|--------|
| Export | `E###` | `E386: Floating_SearchFootClamp` | `--find "SearchFootClamp"` |
| Comment | `C###` | `C182: "Clamp foot if normal acceptable"` | `--comments` |
| NameMap | `[####]` | `[0782] FootMayClamp` | `--find "FootMayClamp"` |

**Document with indices:**

```markdown
## Foot Clamping

> **BP Reference:** Exports E386, E387, E420 | Comments C182, C288-289

The foot clamp system (E387: `Floating_TryFootClamp`) checks surface normals
before clamping. See C182: "Clamp foot if normal acceptable".
```

**Why this matters:**
- Future agents can verify claims by running `--find` queries
- Documentation stays synchronized with BP structure
- Indices are stable anchors for a visual graph format

---

## Phase 6: Compare to Existing Code (if applicable)

If implementing the Blueprint in C++:

### 6.1 Create Mapping Table

| Blueprint | C++ Equivalent | Status |
|-----------|----------------|--------|
| RootComponent | RootComponent (USceneComponent) | ✅ Exists |
| bIsWalking | bIsWalking (bool) | ❌ Missing |
| GetLocoDirection() | GetLocomotionDirection() | ⚠️ Partial |

### 6.2 Identify Gaps

List what's missing:
1. Critical (blocks core functionality)
2. Important (affects quality)
3. Nice to have (polish)

### 6.3 Create Implementation Plan

Break into incremental chunks:
```
Chunk 1: Core state machine
Chunk 2: Movement input
Chunk 3: Collision response
...
```

Each chunk should be testable independently.

---

## Quick Reference: JSON Structure

### Export Structure
```json
{
  "$type": "NormalExport",
  "ObjectName": "ComponentName_GEN_VARIABLE",
  "ClassIndex": {"Index": -42},
  "OuterIndex": {"Index": 0},
  "Data": [
    {"Name": "PropertyName", "Value": ...}
  ]
}
```

### Common Data Types in Value
```json
// Boolean
{"Name": "bEnabled", "Value": true}

// Number
{"Name": "Speed", "Value": 600.0}

// Vector
{"Name": "Location", "Value": {"X": 0, "Y": 0, "Z": 100}}

// Rotator
{"Name": "Rotation", "Value": {"Pitch": 0, "Yaw": 90, "Roll": 0}}

// Object Reference
{"Name": "Mesh", "Value": {"Index": 42}}

// Enum
{"Name": "State", "Value": "Walking"}

// Array
{"Name": "Items", "Value": [{"Value": ...}, {"Value": ...}]}
```

### K2Node Reference

| Node Type | Purpose | Key Properties |
|-----------|---------|----------------|
| K2Node_Event | Event entry | EventReference |
| K2Node_CallFunction | Function call | FunctionReference |
| K2Node_VariableGet | Read variable | VariableReference |
| K2Node_VariableSet | Write variable | VariableReference |
| K2Node_IfThenElse | Branch | Condition pin |
| K2Node_SwitchEnum | Switch | Enum type |
| K2Node_MacroInstance | Macro call | MacroGraphReference |
| K2Node_Timeline | Timeline | TimelineName |

---

## Workflow Summary

```
1. ASSESS    → Check size, verify JSON, identify type
2. PARSE     → Run parser, create organized output
3. DISCOVER  → Search NameMap, read summary, find patterns
4. DEEP DIVE → Analyze components, functions, events
5. DOCUMENT  → Create summary and flow documents
6. COMPARE   → Map to existing code, identify gaps, plan implementation
```

---

## Example Session

```bash
# 1. Parse the asset
python parse_blueprint.py MyPawn.json MyPawn_parsed/

# 2. Quick overview
cat MyPawn_parsed/_summary.json

# 3. Search for movement systems
grep -i "movement\|velocity\|speed" MyPawn_parsed/_namemap.txt

# 4. List components
ls MyPawn_parsed/components/

# 5. Read main event graph
cat MyPawn_parsed/graphs/EventGraph.json | head -500

# 6. Check for author comments
cat MyPawn_parsed/_comments.json

# 7. Create documentation
# (Write BLUEPRINT_SUMMARY.md and EXECUTION_FLOW.md)
```

---

## Tools Available

| Tool | Location | Purpose |
|------|----------|---------|
| parse_blueprint.py | This guide (copy above) | Split JSON into navigable chunks |
| parse_pawn_json.py | .claude/archive/ | VR pawn-specific parser (example) |
| jq | System tool | JSON querying |
| grep | System tool | Pattern searching |

---

## Tips for Agents

1. **Start with NameMap** - It's your index to everything
2. **Read comments first** - Authors often document their intent
3. **Follow the hierarchy** - Components → Functions → Events
4. **Document as you go** - Don't try to understand everything first
5. **Focus on one system at a time** - Don't get lost in complexity
6. **Use grep liberally** - The NameMap is your search index
7. **Compare to known patterns** - UE has standard conventions

---

*Last updated: 2026-01-01*
