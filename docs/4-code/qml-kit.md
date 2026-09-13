# 4 · The QML kit — controls and singletons

**Sources:** `src/app/qml/controls/*.qml` (19), `src/app/qml/Theme.qml`, `Icons.qml`, `NodeTypes.qml`, `GraphLayout.qml`

Every panel is built from the same 19 controls, and every visual constant comes from one
of four singletons. Nothing downstream hard-codes a colour, a size or a font.

---

## The binding contract

Most controls bind **generically** through two properties:

```qml
ParamSlider { target: root.node; propertyName: "gamma"; from: 0.2; to: 4 }
```

They read `target[propertyName]` and write `target[propertyName] = v`. Because node
properties are `Q_PROPERTY(… MEMBER … NOTIFY changed)`, the write reaches the canvas the
same frame with no apply step.

**Every such control must tolerate `undefined`**, because inspectors are instantiated
unconditionally and hidden rather than loaded — so a control can briefly be pointed at a
node that has no such property. Each has an explicit fallback (`0`, `false`, `"#000000"`).

Controls also support an **unbound mode**: set `localValue` and listen to `activated`
instead of `target`/`propertyName`. In that mode the control never writes back, so the
caller's own binding survives. Used by the canvas zoom selector and the export dialog.

---

## The 19 controls

### Input

| Control | What it is | Key API |
|---|---|---|
| **`ParamSlider`** | Labelled slider paired with a number field for exact values | `label`, `info`, `details`, `target`, `propertyName`, `from`, `to`, `stepSize`, `sub` |
| **`NumberField`** | Numeric text entry | `value`, `from`, `to`, `stepSize`; reformats only when not focused |
| **`AppSlider`** | Bare styled slider | `from`, `to`, `value` |
| **`AppTextField`** | Single-line text entry | `text`, `onTextEdited` |
| **`ToggleRow`** | Label + switch on one row | `label`, `info`, `target`, `propertyName`, `sub` |
| **`SegmentedControl`** | Inline exclusive choice — the default for 2–3 options | `model` of `{text, value}`, `target`, `propertyName`, or `localValue` + `activated` |
| **`EnumCombo`** | Dropdown, for longer option lists | same contract as `SegmentedControl` |
| **`ColorField`** | Colour swatch + hex entry | `target`, `propertyName` |

### Buttons

| Control | What it is |
|---|---|
| **`AppButton`** | Standard button; `accent` variant for primary actions |
| **`IconButton`** | Square icon-only button, `Theme.iconButtonSize` |

### Structure and chrome

| Control | What it is |
|---|---|
| **`SectionHeader`** | Small caps heading that groups an inspector section |
| **`ParamLabel`** | Label + `info` tooltip + optional `details` — long explanations open a clickable `AppDialog` |
| **`AppToolTip`** | Styled tooltip |
| **`AppDialog`** | Modal shell used by the export dialog, errors and `details` popups |
| **`NoticeBar`** | Dismissable strip under the toolbar; carries `sessionWarning` |
| **`Tag`** | Small pill for read-only metadata (canvas size, fps, node type) |
| **`AppScrollBar`** | Styled scrollbar for the panel `Flickable`s |
| **`AppIcon`** | Renders an `Icons` path at a given size and colour; `filled` for solid glyphs |
| **`WaveformStrip`** | `Canvas` drawing a min/max peak envelope. Repaints on track change only |

> **`WaveformStrip` is worth copying as a pattern.** Playback progress is not a repaint —
> the timeline draws the strip twice and clips the second copy at the playhead, so
> advancing the cursor costs nothing.

---

## `Theme.qml` — the single source of visual truth

`pragma Singleton`. Every colour, size, radius and font in the tree comes from here.

**Utility.** `withAlpha(c, a)` → `Qt.rgba(c.r, c.g, c.b, a)`. Used pervasively **instead
of item `opacity`**, so a tint never fades an item's children.

| Group | Tokens |
|---|---|
| **Grounds** | `bg #161826`, `surface #232532`, `sunken #1b1d2b`, `stage #0d0e17`, `scrim` |
| **Ink** | `text #e9e9ed`, `textMuted` (.72), `textFaint` (.5), `textGhost` (.32), `divider` (.16), `dividerStrong` (.28) |
| **Accent** | `accent #9184d9` plus a full `accent100…accent900` ramp |
| **Neutral** | `neutral100…neutral900` |
| **Status** | `danger #e0899a`, `warning #d9b184` |
| **Interaction** | `hoverFill`, `pressFill`, `accentHoverFill`, `accentPressFill`, `accentSoftFill`, `selectionFill` — hovers are always *tints*, never new colours |
| **Spacing** | `space1 3`, `space2 6`, `space3 8`, `space4 11`, `space6 17`, `space8 22` (a compact 0.7× scale) |
| **Radii** | `radiusSm 4`, `radiusMd 8`, `radiusLg 14` |
| **Controls** | `controlHeight 30`, `controlHeightSm 26`, `rowHeight 40`, `iconButtonSize 28`, `topBarHeight 52`, `panelHeaderHeight 42`, `leftPanelWidth 264`, `rightPanelWidth 340`, `timelineHeight 148` |
| **Type** | `fontFamily`/`monoFamily` resolved once by `pickFont(candidates)` against `Qt.fontFamilies()`; sizes `Title 15`, `Body 13`, `Small 12`, `Micro 11`, `Kicker 10`; `weightMedium` |
| **Animation** | `durFast 90`, `durNormal 150` |

### Node-graph tokens

| Token | Value |
|---|---|
| `portVisual` | `#b07cff` — purple |
| `portAudio` | `#e8a05c` — orange |
| `portMask` | `#6fb6ef` — blue (reserved) |
| `portColor(kind)` | **the one switch on a kind string in the whole QML tree** |
| `graphBg`, `graphGrid`, `nodeFill`, `nodeHeaderFill` | graph surfaces |
| `nodeWidth 176`, `nodeHeight 62`, `nodeHeaderHeight 24` | card geometry |
| `portRadius 5`, `portSnapRadius 18` | port geometry and snap distance |

> **The graph never uses an accent *fill*.** Selection is a ring, because `accent`
> (`#9184d9`) sits right next to `portVisual` (`#b07cff`) and a flood would read as a
> port colour.

---

## `Icons.qml` — 24×24 SVG path data

Paths rather than raster assets, so they stay crisp at any DPI and take the colour of
whatever they sit in. Stroked with round caps unless `AppIcon.filled` is set.

| Group | Names |
|---|---|
| Transport | `play`, `pause`, `stop`, `skipBack` |
| Files | `filePlus`, `folderOpen`, `save`, `importArrow`, `exportArrow` |
| Graph | `nodes`, `separator`, `output` |
| Node types | `stack`, `waveform`, `bars`, `textT`, `sparkle`, `image`, `fill`, `merge`, `transform`, `blur` |
| Interface | `plus`, `close`, `check`, `chevronDown`, `arrowUp`, `arrowDown`, `trash`, `eye`, `eyeOff`, `speaker`, `speakerMute`, `sliders`, `frameCorners`, `warning`, `info`, `spinner`, `link` |

---

## `NodeTypes.qml` — presentation only

An array of `{typeId, title, label, icon, hint}` for the ten types, plus lookups
`entry()`, `icon()`, `label()`, `title()`, `hint()`, each with a graceful fallback.

**Ports, creatability and drawability are NOT here** — they live in the C++ port table,
because the flatten, cycle detection, the deletion cascade and link validation all read
them and all must run headless in the tests.
→ [ADR-0003](../decisions/0003-port-table-in-cpp.md)

The add menu's **model** is `nodeGraph.creatableTypes()`; this file only decorates it.

> Nothing mechanically checks that `NodeTypes.all` covers `creatableTypes()`. Add a node
> type and forget this file, and the add menu shows an undecorated entry.

---

## `GraphLayout.qml` — all node-editor geometry

This file exists for exactly one reason: **`mapToItem` is not binding-reactive**, so an
edge endpoint computed with it is correct on first paint and then silently never updates.
Every function here is plain arithmetic over `graphX`/`graphY`, which are real
`NOTIFY`-ing properties. → [qt-quirks](qt-quirks.md)

| Function | Returns |
|---|---|
| `inputPos(node, i, n)` | `(graphX + nodeWidth·(i+1)/(n+1), graphY)` — inputs spread across the **top** edge at even fractions, so two ports sit at ⅓ and ⅔ rather than at the corners |
| `outputPos(node)` | `(graphX + nodeWidth/2, graphY + nodeHeight)` — a single output centred on the **bottom** edge; flow is top-to-bottom, downstream is down |
| `curveBow(p1, p2)` | `max(28, abs(Δy) × 0.4)` — control-point offset scaling with vertical distance, with a floor so a zero-length edge is still a curve |
| `hitTestInput(graph, gx, gy, outKind, excludeId)` | Nearest **accepting** input within `snapRadius`, as `{nodeId, port, pos}`. Squared-distance comparison; skips `excludeId` because a node offering its own inputs reads as a bug even though `canLink` would refuse |
| `nodeWidth`, `nodeHeight`, `snapRadius` | proxies onto `Theme` |

---

## Registration

The four singletons are **QML files**, not C++ types. They are registered by CMake:

```cmake
set(app_qml_singletons qml/Theme.qml qml/Icons.qml qml/NodeTypes.qml qml/GraphLayout.qml)
set_source_files_properties(${app_qml_singletons} PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
```

plus `pragma Singleton` at the top of each file. Both halves are required.

Files in subdirectories need `import krave` to see them; `Main.qml` does not, because it
sits at the module root beside them.

---

## See also

- [qt-quirks](qt-quirks.md) — the patterns these controls follow and why
- [3 · editor-ui](../3-components/editor-ui.md) — how the panels assemble them
- [guides/add-a-node-type](../guides/add-a-node-type.md) — the inspector skeleton to copy
