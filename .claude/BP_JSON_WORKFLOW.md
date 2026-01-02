# Blueprint-to-JSON Workflow Documentation

This document describes the complete workflow for exporting Unreal Engine Blueprint assets to JSON format and analyzing them programmatically.

## Overview

Blueprints are binary `.uasset` files that can't be directly read. Using **UAssetAPI**, we export them to human-readable JSON, then use custom parsing tools to organize the data for analysis.

```
.uasset (binary) → UAssetAPI → .json (47MB+) → parse_pawn_json.py → pawn_parsed/ (organized)
```

## Prerequisites

### UAssetAPI

**What it is:** A C#/.NET library that reads/writes Unreal Engine asset files.

**Installation:**
1. Download from: https://github.com/atenfyr/UAssetAPI/releases
2. Use UAssetGUI for interactive exploration, or the CLI for batch exports
3. Works on Windows, macOS, Linux via .NET

**Version used:** 1.0.2.0 (compatible with UE 5.7)

### Python Environment

```bash
# No external dependencies - uses only stdlib
python --version  # 3.8+ recommended
```

## Step 1: Export Blueprint to JSON

### Using UAssetGUI (Interactive)

1. Open UAssetGUI
2. File → Open → Select your `.uasset` file
3. Configure for your UE version (5.7 = Unreal 5.7)
4. File → Save As → Choose `.json` extension
5. Place output in `.claude/archive/` directory

### Using UAssetAPI CLI (Batch)

```bash
# Single file
UAssetCLI.exe export MyBlueprint.uasset MyBlueprint.json

# With version specification
UAssetCLI.exe export MyBlueprint.uasset MyBlueprint.json --version VER_UE5_4
```

### Output Size Warning

Large Blueprints produce massive JSON files:
- Simple actor: ~1-5 MB
- Complex pawn (like VR template): **47+ MB**
- Character with animation: 100+ MB

**Always gitignore these files** - add to `.gitignore`:
```
.claude/archive/*.json
```

## Step 2: Understand JSON Structure

The UAssetAPI JSON has these top-level keys:

```json
{
  "$type": "UAsset",
  "Info": "UAssetAPI v1.0.2.0",
  "NameMap": [...],      // All names/identifiers in the asset
  "Imports": [...],      // References to other packages
  "Exports": [...],      // The actual Blueprint content
  "DependsMap": [...],   // Dependencies
  "SoftPackageReferenceList": [...],
  "AssetRegistryData": [...]
}
```

### NameMap

A flat array of all string identifiers used in the asset:
```json
"NameMap": [
  "RootBox",
  "VROO",
  "Camera",
  "bIsWalking",
  "PawnVelocity",
  ...
]
```

**Use for:** Discovering what variables/functions exist via grep.

### Exports

The main content array. Export 0 is the Blueprint class itself. Subsequent exports are:
- Component templates
- Function graphs
- Event graphs (UbergraphPages)
- K2Nodes (visual scripting nodes)
- Variables

Each export has:
```json
{
  "$type": "NormalExport",
  "ObjectName": "RootBox_GEN_VARIABLE",
  "ClassIndex": {...},
  "Data": [...]
}
```

### Data Arrays

The `Data` field contains property/value pairs:
```json
"Data": [
  {
    "Name": "RelativeLocation",
    "Value": { "X": 0.0, "Y": 0.0, "Z": 0.0 }
  },
  {
    "Name": "bAbsoluteLocation",
    "Value": true
  }
]
```

## Step 3: Parse with Python Script

### Using parse_pawn_json.py

```bash
cd .claude/archive
python parse_pawn_json.py
```

**Output:**
```
Loading pawn.json...
Loaded. Keys: ['$type', 'Info', 'NameMap', ...]
Found 24 UbergraphPages
Found 45 FunctionGraphs

Categories:
  ubergraphs: 24
  functions: 45
  components: 87
  nodes: 1523
  other: 412

Done! Output in pawn_parsed/
```

### Output Directory Structure

```
pawn_parsed/
├── _metadata.json          # Export info, counts
├── _namemap_full.txt       # Complete NameMap (grep-friendly)
├── _namemap_organized.json # Names grouped by category
├── _comments.json          # Human-readable comments from nodes
├── _index.json             # Quick lookup of all graphs/functions
├── graphs/
│   ├── EventGraph.json
│   ├── Walk.json
│   ├── Inputs.json
│   └── ...
├── functions/
│   ├── Jump_Calculations.json
│   ├── Get_Loco_Direction.json
│   └── ...
└── components/
    ├── RootBox.json
    ├── Camera.json
    ├── MC_Left.json
    └── ...
```

## Step 4: Navigate and Search

### Quick Lookup

```bash
# List all graphs
cat _index.json | jq '.ubergraphs'

# List all functions
cat _index.json | jq '.functions'

# List all components
cat _index.json | jq '.components'
```

### Search NameMap

```bash
# Find movement-related names
grep -i "loco\|walk\|float" _namemap_full.txt

# Find collision variables
grep -i "collision\|trace\|hit" _namemap_full.txt

# Find specific function references
grep -i "jump" _namemap_full.txt
```

### Search by Category

```bash
# Use organized namemap
cat _namemap_organized.json | jq '.locomotion'
cat _namemap_organized.json | jq '.collision'
cat _namemap_organized.json | jq '.hands'
```

### Find Function Implementation

```bash
# Search in functions directory
grep -l "Jump" functions/*.json

# Read specific function
cat functions/Jump_Calculations.json | jq '.data.Data'
```

## Step 5: Understand Blueprint Patterns

### Function Graphs

Function graphs contain:
- Input parameters (K2Node_FunctionEntry)
- Output values (K2Node_FunctionResult)
- Execution flow (K2Node_CallFunction, K2Node_IfThenElse, etc.)

Example structure:
```json
{
  "export_index": 42,
  "name": "Jump_Calculations",
  "data": {
    "ObjectName": "Jump_Calculations",
    "Data": [
      {"Name": "FunctionFlags", "Value": "FUNC_Public|FUNC_BlueprintCallable"},
      // ... nodes
    ]
  }
}
```

### Event Graphs (UbergraphPages)

Event graphs are split into "pages" for organization:
- `EventGraph` - Main tick, BeginPlay, etc.
- `Walk` - Walking regime logic
- `Inputs` - Input handling
- `Hands` - Hand processing

### Component Templates

Components have their default values in the Data array:
```json
{
  "base_name": "Camera",
  "exports": [{
    "data": {
      "Data": [
        {"Name": "bUsePawnControlRotation", "Value": false},
        {"Name": "bLockToHmd", "Value": true}
      ]
    }
  }]
}
```

### K2Node Types

Common node types you'll encounter:

| K2Node Type | Purpose |
|-------------|---------|
| K2Node_Event | Event entry point (Tick, BeginPlay) |
| K2Node_CallFunction | Calls a function |
| K2Node_VariableGet | Reads a variable |
| K2Node_VariableSet | Writes a variable |
| K2Node_IfThenElse | Branch node |
| K2Node_Select | Switch/select |
| K2Node_MakeStruct | Create struct |
| K2Node_BreakStruct | Decompose struct |

### Node Comments

Blueprint authors often leave comments. These are invaluable:
```bash
# View all comments
cat _comments.json | jq '.[] | select(.comments | length > 0)'

# Search for specific comment
grep -i "velocity" _comments.json
```

## Step 6: Document Findings

After analysis, create documentation like:

1. **SUMMARY.md** - Component hierarchy, variables, functions
2. **EXECUTION_FLOW.md** - Tree-style execution diagrams
3. **IMPLEMENTATION_PLAN.md** - How to replicate in C++

See `pawn_parsed/PAWN_SUMMARY.md` and `pawn_parsed/EXECUTION_FLOW.md` for examples.

## Common Patterns

### Finding Variable Default Values

```bash
# Search for variable in exports
grep -l "bIsWalking" components/*.json functions/*.json

# Or search raw JSON
grep -B2 -A2 "bIsWalking" ../pawn.json | head -50
```

### Understanding Execution Flow

1. Find the event (e.g., "Event Tick")
2. Trace K2Node_CallFunction references
3. Follow the "Then" pins through branches
4. Map out the decision tree

### Identifying Function Parameters

Look for K2Node_FunctionEntry in function JSON:
```json
{
  "$type": "K2Node_FunctionEntry",
  "Data": [
    {"Name": "InputParams", "Value": [
      {"ParamName": "DeltaTime", "ParamType": "float"}
    ]}
  ]
}
```

## Extending the Parser

### Adding New Categories to _namemap_organized.json

Edit `parse_pawn_json.py`:

```python
namemap_organized = {
    # Add new category
    "climbing": [n for n in namemap if "Climb" in n or "Grip" in n],
    # ...existing categories...
}
```

### Extracting Additional Metadata

Add new extraction functions:
```python
def extract_function_signatures(exports):
    """Extract function input/output parameters."""
    signatures = {}
    for i, export in enumerate(exports):
        if "FunctionEntry" in export.get("$type", ""):
            # Parse parameters
            pass
    return signatures
```

## Troubleshooting

### "No exports found"
- Check JSON is valid: `python -c "import json; json.load(open('pawn.json'))"`
- Ensure UAssetAPI version matches UE version

### "Empty categories"
- Blueprint may not use that feature
- Category filter may need adjustment

### Large file performance
- Use `jq` for JSON parsing (faster than Python for queries)
- Consider splitting truly massive files into smaller chunks

## Related Files

| File | Purpose |
|------|---------|
| `.claude/archive/pawn.json` | Source JSON (gitignored) |
| `.claude/archive/parse_pawn_json.py` | Parser script |
| `.claude/archive/pawn_parsed/` | Organized output |
| `.claude/archive/pawn_parsed/PAWN_SUMMARY.md` | Analysis document |
| `.claude/archive/pawn_parsed/EXECUTION_FLOW.md` | Execution diagrams |

## Version History

| Date | Change |
|------|--------|
| 2026-01-01 | Initial workflow documentation |
