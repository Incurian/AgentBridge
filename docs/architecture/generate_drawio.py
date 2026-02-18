#!/usr/bin/env python3
"""Convert Mermaid classDiagram from OVERVIEW.md to draw.io XML.

Generates a native .drawio file with editable class boxes, namespace
containers, and relationship edges. No external dependencies.

Usage:
    python docs/architecture/generate_drawio.py
    python docs/architecture/generate_drawio.py input.md output.drawio
"""

import re
import sys
import html
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path


# ── Data Model ───────────────────────────────────────────────────

@dataclass
class Member:
    visibility: str   # "+", "-", "#", "~", ""
    text: str
    is_method: bool

@dataclass
class MermaidClass:
    name: str
    display_name: str = ""
    annotation: str = ""
    fields: list = field(default_factory=list)
    methods: list = field(default_factory=list)
    namespace: str = ""

@dataclass
class Relationship:
    source: str
    target: str
    rel_type: str
    label: str = ""
    source_card: str = ""
    target_card: str = ""


# ── Layout Constants ─────────────────────────────────────────────

CHAR_W = 7.0
MEMBER_H = 16
HEADER_H = 26
ANNOT_H = 16
HR_H = 6
PAD_X = 12
PAD_Y = 8
CLASS_GAP_X = 30
CLASS_GAP_Y = 25
NS_PAD = 20
NS_TITLE_H = 30
NS_GAP_Y = 40
COL_GAP = 140
MAX_ROW_W = 1600
MIN_CLASS_W = 160

NS_META = {
    "L0_MCP_Server":           ("L0: MCP Server (Python)",          "#dae8fc", "#6c8ebf"),
    "L1_LiveServices":         ("L1: Live Services (Python)",       "#d5e8d4", "#82b366"),
    "L1_OfflineServices":      ("L1: Offline Services (Python)",    "#fff2cc", "#d6b656"),
    "L2_AgentBridgeServer":    ("L2: AgentBridgeServer (C++)",      "#f8cecc", "#b85450"),
    "L3_AgentBridgeScripting": ("L3: AgentBridgeScripting (C++)",   "#e1d5e7", "#9673a6"),
    "L4_AgentBridgeRuntime":   ("L4: AgentBridgeRuntime (C++)",     "#b6d7a8", "#6d8764"),
    "L5_AgentBridgeCore":      ("L5: AgentBridgeCore (C++)",        "#fff2cc", "#d6b656"),
    "L6_UnrealEngine":         ("L6: Unreal Engine APIs",           "#f5f5f5", "#666666"),
    "L1_BpToolkitScripts":     ("Offline: bp_toolkit (Python)",     "#ffe6cc", "#d79b00"),
    "L2_UAssetGUI":            ("Offline: UAssetGUI (.NET 8)",      "#f8cecc", "#b85450"),
    "L3_UAssetAPI":            ("Offline: UAssetAPI (C#)",          "#e1d5e7", "#9673a6"),
    "L4_Filesystem":           ("On-Disk Files",                    "#f5f5f5", "#666666"),
}

COLUMNS = {
    0: ["L0_MCP_Server", "L1_LiveServices", "L2_AgentBridgeServer",
        "L3_AgentBridgeScripting", "L4_AgentBridgeRuntime",
        "L5_AgentBridgeCore", "L6_UnrealEngine"],
    1: ["L1_OfflineServices", "L1_BpToolkitScripts", "L2_UAssetGUI",
        "L3_UAssetAPI", "L4_Filesystem"],
}

EDGE_STYLES = {
    "-->":  "endArrow=open;endFill=0;",
    "..>":  "dashed=1;dashPattern=8 4;endArrow=open;endFill=0;",
    "--|>": "endArrow=block;endFill=0;",
    "o--":  "startArrow=diamond;startFill=0;endArrow=none;endFill=0;",
    "*--":  "startArrow=diamond;startFill=1;endArrow=none;endFill=0;",
}


# ── Mermaid Parser ───────────────────────────────────────────────

def extract_mermaid(md_text):
    """Extract classDiagram block from markdown."""
    lines = []
    inside = False
    for line in md_text.split("\n"):
        if line.strip().startswith("```mermaid"):
            inside = True
            continue
        if inside and line.strip() == "```":
            break
        if inside:
            lines.append(line)
    return "\n".join(lines)


def _parse_member(cls, raw):
    """Parse one member line inside a class block."""
    line = html.unescape(raw.strip())
    if not line:
        return

    # Annotation like <<dataclass>>
    m = re.match(r"<<(.+?)>>", line)
    if m:
        cls.annotation = m.group(1)
        return

    # Visibility prefix
    vis = ""
    text = line
    if line[0] in "+-#~":
        vis = line[0]
        text = line[1:].strip()

    # Mermaid tildes -> angle brackets for generics
    text = re.sub(r"~([^~]+)~", r"<\1>", text)
    text = text.rstrip("$").strip()  # strip static marker

    is_method = bool(re.search(r"\(.*\)", text))
    member = Member(visibility=vis, text=text, is_method=is_method)
    if is_method:
        cls.methods.append(member)
    else:
        cls.fields.append(member)


_REL_PAT = re.compile(
    r"^(\w+)\s*"
    r'(?:"([^"]*)")?\s*'
    r"(--|>|-->|\.\.>|o--|\*--)\s*"
    r'(?:"([^"]*)")?\s*'
    r"(\w+)\s*"
    r"(?::\s*(.+))?$"
)


def _parse_rel(line):
    """Parse a relationship line."""
    m = _REL_PAT.match(line.strip())
    if not m:
        return None
    return Relationship(
        source=m.group(1), target=m.group(5), rel_type=m.group(3),
        source_card=m.group(2) or "", target_card=m.group(4) or "",
        label=(m.group(6) or "").strip(),
    )


def parse_mermaid(text):
    """Parse Mermaid classDiagram into classes, relationships, ns_classes."""
    classes = {}
    relationships = []
    ns_classes = {}

    cur_ns = None
    cur_cls = None

    for line in text.split("\n"):
        s = line.strip()
        if not s or s.startswith("%%") or s in ("classDiagram",) or s.startswith("direction"):
            continue

        # Namespace open
        m = re.match(r"namespace\s+(\w+)\s*\{", s)
        if m:
            cur_ns = m.group(1)
            ns_classes.setdefault(cur_ns, [])
            continue

        # Class open
        m = re.match(r'class\s+(\w+)(?:\["([^"]+)"\])?\s*\{', s)
        if m:
            name, display = m.group(1), m.group(2) or ""
            cur_cls = MermaidClass(name=name, display_name=display, namespace=cur_ns or "")
            classes[name] = cur_cls
            if cur_ns:
                ns_classes.setdefault(cur_ns, [])
                ns_classes[cur_ns].append(name)
            continue

        # Closing brace
        if s == "}":
            if cur_cls:
                cur_cls = None
            elif cur_ns:
                cur_ns = None
            continue

        # Inside class
        if cur_cls:
            _parse_member(cur_cls, s)
            continue

        # Relationship
        rel = _parse_rel(s)
        if rel:
            relationships.append(rel)

    return classes, relationships, ns_classes


# ── Layout Engine ────────────────────────────────────────────────

def _class_size(cls):
    """Calculate pixel (width, height) for a class box."""
    name = cls.display_name or cls.name
    all_lines = [name]
    if cls.annotation:
        all_lines.append(f"\u00ab{cls.annotation}\u00bb")
    for m in cls.fields:
        all_lines.append(f"{m.visibility}{m.text}")
    for m in cls.methods:
        all_lines.append(f"{m.visibility}{m.text}")

    max_len = max(len(l) for l in all_lines)
    w = max(MIN_CLASS_W, max_len * CHAR_W + PAD_X * 2)

    h = HEADER_H
    if cls.annotation:
        h += ANNOT_H
    h += HR_H + max(1, len(cls.fields)) * MEMBER_H
    h += HR_H + max(0, len(cls.methods)) * MEMBER_H
    h += PAD_Y
    return max(w, MIN_CLASS_W), max(h, 60)


def _layout_ns(cls_names, classes):
    """Arrange classes in rows within a namespace. Returns (positions, w, h)."""
    sizes = [(n, *_class_size(classes[n])) for n in cls_names]

    rows, row, row_w = [], [], 0
    for name, w, h in sizes:
        if row and row_w + w > MAX_ROW_W:
            rows.append(row)
            row, row_w = [], 0
        row.append((name, row_w, w, h))
        row_w += w + CLASS_GAP_X
    if row:
        rows.append(row)

    positions = {}
    y = NS_TITLE_H + NS_PAD
    total_w = 0
    for r in rows:
        row_h = max(h for _, _, _, h in r)
        for name, x, w, h in r:
            positions[name] = (NS_PAD + x, y, w, h)
        total_w = max(total_w, max(x + w for _, x, w, _ in r))
        y += row_h + CLASS_GAP_Y

    return positions, total_w + NS_PAD * 2, y + NS_PAD


def layout_diagram(classes, ns_classes):
    """Position all namespaces and classes. Returns (ns_geom, cls_geom)."""
    ns_internal = {}
    for ns, names in ns_classes.items():
        if names:
            ns_internal[ns] = _layout_ns(names, classes)

    # Stack namespaces in columns
    col_max_w = {0: 0, 1: 0}
    col_layouts = {0: [], 1: []}

    for col, ns_order in COLUMNS.items():
        y = 40
        for ns in ns_order:
            if ns not in ns_internal:
                continue
            _, tw, th = ns_internal[ns]
            col_layouts[col].append((ns, y, tw, th))
            col_max_w[col] = max(col_max_w[col], tw)
            y += th + NS_GAP_Y

    col_x = {0: 40, 1: 40 + col_max_w[0] + COL_GAP}

    ns_geom, cls_geom = {}, {}
    for col, layouts in col_layouts.items():
        bx = col_x[col]
        for ns, ny, tw, th in layouts:
            nw = max(tw, col_max_w[col])
            ns_geom[ns] = (bx, ny, nw, th)
            pos, _, _ = ns_internal[ns]
            for cn, (lx, ly, cw, ch) in pos.items():
                cls_geom[cn] = (bx + lx, ny + ly, cw, ch)

    return ns_geom, cls_geom


# ── HTML Label Builder ───────────────────────────────────────────

def _html_label(cls):
    """Build HTML label for a draw.io class cell."""
    name = html.escape(cls.display_name or cls.name)
    parts = [
        f'<p style="margin:0;padding:4px 8px;text-align:center;'
        f'font-weight:bold;border-bottom:1px solid #888;">{name}</p>'
    ]

    if cls.annotation:
        ann = html.escape(cls.annotation)
        parts.append(
            f'<p style="margin:0;padding:2px;text-align:center;'
            f'font-size:10px;font-style:italic;color:#666;">'
            f'\u00ab{ann}\u00bb</p>'
        )

    if cls.fields:
        fl = "<br/>".join(
            f"{html.escape(m.visibility)}{html.escape(m.text)}"
            for m in cls.fields
        )
        parts.append(
            f'<p style="margin:0;padding:2px 6px;text-align:left;'
            f'font-size:11px;border-top:1px solid #ccc;">{fl}</p>'
        )

    if cls.methods:
        ml = "<br/>".join(
            f"{html.escape(m.visibility)}{html.escape(m.text)}"
            for m in cls.methods
        )
        parts.append(
            f'<p style="margin:0;padding:2px 6px;text-align:left;'
            f'font-size:11px;border-top:1px solid #ccc;">{ml}</p>'
        )

    return "".join(parts)


# ── draw.io XML Generator ───────────────────────────────────────

def generate_drawio(classes, relationships, ns_classes, ns_geom, cls_geom):
    """Generate complete draw.io XML string."""
    # Compute page size from content
    max_x = max(x + w for x, y, w, h in list(ns_geom.values()) + list(cls_geom.values()))
    max_y = max(y + h for x, y, w, h in list(ns_geom.values()) + list(cls_geom.values()))
    page_w = str(int(max_x + 200))
    page_h = str(int(max_y + 200))

    mxfile = ET.Element("mxfile", host="app.diagrams.net", type="device")
    diagram = ET.SubElement(mxfile, "diagram", name="AgentBridge Architecture", id="arch")
    model = ET.SubElement(diagram, "mxGraphModel",
                          dx="1422", dy="794", grid="1", gridSize="10",
                          guides="1", tooltips="1", connect="1", arrows="1",
                          fold="1", page="1", pageScale="1",
                          pageWidth=page_w, pageHeight=page_h,
                          math="0", shadow="0")
    root = ET.SubElement(model, "root")
    ET.SubElement(root, "mxCell", id="0")
    ET.SubElement(root, "mxCell", id="1", parent="0")

    _id = [100]
    def nid():
        _id[0] += 1
        return str(_id[0])

    ns_ids, cls_ids = {}, {}

    # Namespace containers
    for ns, (x, y, w, h) in ns_geom.items():
        label, fill, stroke = NS_META.get(ns, (ns, "#f5f5f5", "#666"))
        sid = nid()
        ns_ids[ns] = sid
        style = (
            f"rounded=1;whiteSpace=wrap;html=1;fillColor={fill};"
            f"strokeColor={stroke};dashed=1;dashPattern=8 8;"
            f"verticalAlign=top;fontStyle=1;fontSize=13;"
            f"align=left;spacingLeft=10;spacingTop=2;"
            f"container=1;collapsible=0;"
        )
        cell = ET.SubElement(root, "mxCell", id=sid, value=label,
                             style=style, vertex="1", parent="1")
        geo = ET.SubElement(cell, "mxGeometry",
                            x=str(int(x)), y=str(int(y)),
                            width=str(int(w)), height=str(int(h)))
        geo.set("as", "geometry")

    # Class cells
    warnings = []
    for cn, co in classes.items():
        if cn not in cls_geom:
            warnings.append(f"  WARN: class '{cn}' has no layout position")
            continue

        ax, ay, cw, ch = cls_geom[cn]
        ns = co.namespace
        pid = ns_ids.get(ns, "1") if ns else "1"

        # Position relative to parent container
        if ns and ns in ns_geom:
            px, py = ns_geom[ns][0], ns_geom[ns][1]
            rx, ry = ax - px, ay - py
        else:
            rx, ry = ax, ay

        cid = nid()
        cls_ids[cn] = cid

        style = (
            "rounded=0;whiteSpace=wrap;html=1;overflow=hidden;"
            "fillColor=#ffffff;strokeColor=#333;"
            "fontSize=11;fontFamily=Helvetica;verticalAlign=top;"
            "align=center;spacingTop=0;"
        )

        tooltip = cn if co.display_name else ""
        attrs = dict(id=cid, value=_html_label(co), style=style,
                     vertex="1", parent=pid)
        if tooltip:
            attrs["tooltip"] = tooltip
        cell = ET.SubElement(root, "mxCell", **attrs)
        geo = ET.SubElement(cell, "mxGeometry",
                            x=str(int(rx)), y=str(int(ry)),
                            width=str(int(cw)), height=str(int(ch)))
        geo.set("as", "geometry")

    # Edges
    for rel in relationships:
        src = cls_ids.get(rel.source)
        tgt = cls_ids.get(rel.target)
        if not src or not tgt:
            miss = rel.source if not src else rel.target
            warnings.append(f"  WARN: edge skipped, class '{miss}' not found")
            continue

        eid = nid()
        base = EDGE_STYLES.get(rel.rel_type, EDGE_STYLES["-->"])
        style = (
            f"{base}strokeColor=#666;fontSize=10;fontColor=#666;"
            f"edgeStyle=orthogonalEdgeStyle;rounded=1;"
        )

        label = rel.label
        if rel.source_card or rel.target_card:
            label = f"{rel.source_card}  {label}  {rel.target_card}".strip()

        cell = ET.SubElement(root, "mxCell", id=eid, value=label,
                             style=style, edge="1", parent="1",
                             source=src, target=tgt)
        geo = ET.SubElement(cell, "mxGeometry", relative="1")
        geo.set("as", "geometry")

    ET.indent(mxfile, space="  ")
    xml_str = ET.tostring(mxfile, encoding="unicode", xml_declaration=True)
    return xml_str, warnings


# ── Main ─────────────────────────────────────────────────────────

def main():
    here = Path(__file__).parent
    inp = Path(sys.argv[1]) if len(sys.argv) > 1 else here / "OVERVIEW.md"
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else here / "agentbridge-architecture.drawio"

    md = inp.read_text(encoding="utf-8")
    mermaid = extract_mermaid(md)
    if not mermaid:
        print("ERROR: No ```mermaid block found.", file=sys.stderr)
        sys.exit(1)

    classes, rels, ns_cls = parse_mermaid(mermaid)
    print(f"Parsed: {len(classes)} classes, {len(rels)} relationships, {len(ns_cls)} namespaces")

    ns_geom, cls_geom = layout_diagram(classes, ns_cls)
    xml, warnings = generate_drawio(classes, rels, ns_cls, ns_geom, cls_geom)

    for w in warnings:
        print(w, file=sys.stderr)

    out.write_text(xml, encoding="utf-8")
    print(f"Output: {out} ({len(xml):,} bytes)")


if __name__ == "__main__":
    main()
