# 3 · The editor UI

**Sources:** `src/app/qml/Main.qml`, `src/app/qml/panels/*.qml`, `src/app/qml/panels/inspectors/*.qml`, `src/app/qml/graph/*.qml`, `src/app/qml/controls/*.qml`

The window: what instantiates what, and how the node editor works.

---

## The component tree

```
ApplicationWindow (Main.qml)
├── TopBar                    brand · project name + unsaved dot · loading spinner ·
│                             ffmpeg-missing warning · New/Open/Import/Save/Save as/Export
│                             → owns no state; emits signals up to Main
├── NoticeBar                 sessionWarning — a moved track, reported quietly
├── SplitView
│   ├── NodeGraphPanel        header + GraphCanvas + footer (add · arrange · reset view)
│   │   └── GraphCanvas       grid · edges · rubber band · NodeCard × n
│   ├── CanvasStage           zoom control · Flickable → letterboxed frame
│   │   ├── VisualizerCanvas  ← the render list
│   │   ├── guides            thirds + title-safe box (editor overlay only)
│   │   ├── maskGuide         the selected blur node's circle
│   │   └── fpsMeter
│   └── PropertyPanel         Node section · Placement · the 7 inspectors
└── TimelinePanel             transport · ruler · waveform + playhead · scrub area

+ 7 window Shortcuts · 3 FileDialogs · ExportDialog · error dialog
```

**Signals up, state nowhere in the chrome.** `TopBar` owns nothing; `Main.qml` funnels
the menu, the buttons and the shortcuts through one set of functions so they cannot
drift.

Composition guides are drawn **outside** the canvas item, so they never reach an exported
frame.

---

## How C++ reaches QML

**Eight context properties, set in `main.cpp`. That is the only mechanism** — no
`QML_ELEMENT`, no `qmlRegisterType`, no C++ singleton anywhere.

`audioEngine` · `nodeGraph` · `spectrumEngine` · `project` · `exportController` ·
`ffmpegAvailable` · `ffmpegPath` · `sessionWarning`

Two model role sets:

| Model | Roles |
|---|---|
| `nodeGraph` | `nodeObject`, `name`, `typeId`, `layerVisible` |
| `nodeGraph.renderList` | `layerObject`, `layerTransforms`, `layerEffects` |

The four singletons (`Theme`, `Icons`, `NodeTypes`, `GraphLayout`) are **QML files**
registered through CMake, not C++ types → [4 · qml-kit](../4-code/qml-kit.md).

---

## The property panel

### Inspectors are instantiated and hidden, not loaded

All seven exist at all times; each gates itself with `visible: root.nodeType === "…"`.
Tearing the panel down on every selection change costs more than a handful of idle
bindings.

**The corollary is a hard requirement:** every control must tolerate being pointed at a
node that lacks its property, so each has an explicit `undefined` fallback.

### Binding

Controls bind generically through `target` + `propertyName`, reading `target[propertyName]`
and writing it back. Because node properties are `MEMBER` with a shared `changed()`
signal, an edit reaches the canvas the same frame — there is no apply step.

### The shared sections

Visibility, opacity, blend mode and the five placement sliders are shown only when
`root.placed` — computed as **`selected.posX !== undefined`**, i.e. duck-typed on a
`VisualNode`-only property. That is why a separator, audio or output selection shows only
the Node section.

> Give a non-visual node a `posX` and it silently grows a placement section it cannot use.

`TransformInspector` and `BlurInspector` therefore carry their **own** placement sections,
since those nodes are not `VisualNode`s. **Merge has no inspector at all** — its
compositing *is* its port order.

Adding an inspector: see
[guides/add-a-node-type](../guides/add-a-node-type.md).

---

## The node editor

### Coordinate frames

`viewport` (clips, owns pan) → `content`, a **zero-sized `Item`** that is a coordinate
frame rather than a box. `content.x/y` are assigned imperatively by the pan handler and
never bound.

Helpers: `toGraph(x,y)`, `fromGraph(x,y)`, `viewportCentreInGraph()` — the last is where a
new node is placed.

> **Adding zoom** would be `content.scale` + `transformOrigin: TopLeft` + a division in
> those two helpers, and nothing else.

### Event routing is z-order, not hit testing

- `panArea` sits below `content` and accepts `Left | Middle`.
- `NodeCard`'s drag area and `PortDot`'s area accept **`Left` only**.

So a middle press *anywhere* — including over a card — falls through and pans. A left
press that reaches `panArea` means empty space, so it clears the selection. No special
cases.

### Dragging a node

`NodeCard.x/y` are **bindings** to `node.graphX/graphY`. `drag.target` is deliberately not
used — it assigns `card.x` directly and permanently destroys that binding.

```
onPressed          select; grab = mapToItem(canvas.content, …)   ← handler: mapToItem is fine
onPositionChanged  node.graphX = startX + (p.x - grab.x)
onReleased         canvas.snapToGrid(node)                        ← snap on release only
```

The arithmetic is correct even though the card moves under the cursor: the card moved by
*d*, so `mouse.x` dropped by *d* while `card.x` rose by *d*.

The drag area is inset top and bottom by `portRadius × 2` so it does not eat the port dots
straddling the card edges.

### Drag-to-connect

In-flight link state lives on the **canvas**, not on the dot that started it, so every
other dot can read it: `linking`, `linkFromId`, `linkKind`, `linkStart`, `linkEnd`,
`linkTarget`.

| Gesture | Result |
|---|---|
| Press an **output** | `beginLink(nodeId, kind, at)` |
| Press a **connected input** | Reads `node.sourceId(port)`, takes the kind from the **source's output** (so dragging off a mask input still offers visual targets), then `unlink` + `beginLink`. One gesture gives both "move this connection" and "delete it" |
| Drag | `hitTestInput(...)`; `linkEnd` **snaps to the target's centre**, so the drop is confirmed before the button comes up. The rubber band switches from dashed to solid |
| Release | `nodeGraph.canLink(...)` then `link(...)` — **C++ is authoritative**, and it is what catches cycles |

**Highlighting costs zero hit testing.** Each dot independently computes `compatible`
(`linking && !isOutput && port.accepts.indexOf(linkKind) >= 0 && nodeId !== linkFromId`)
and `dimmed`, then swells or fades with a `Behavior`.

`starved` marks a **`required`** input that is unconnected while not linking, drawn as a
dashed amber ring. Required rather than merely unconnected, or nearly every graph would
carry a warning and people would learn to ignore it.
→ [ADR-0009](../decisions/0009-required-marks-data-dependencies.md)

**Shape says direction, colour says type:** inputs are circles, the output is a rounded
square; colour is always `Theme.portColor(kind)`. The visible dot is 10 px; the hit area
is 20×20.

### Edge geometry

A `Shape` + one `PathCubic`:

```
p1  = GraphLayout.outputPos(fromNode)
p2  = GraphLayout.inputPos(toNode, portIndex, portCount)
bow = max(28, abs(p2.y - p1.y) × 0.4)
c1  = (p1.x, p1.y + bow)      c2 = (p2.x, p2.y - bow)
```

Control points are **purely vertical** because the graph flows top-to-bottom: the curve
leaves an output downward and enters an input downward, so two edges arriving at
neighbouring ports stay visually separate.

**Every endpoint is plain arithmetic over `graphX`/`graphY`** — never `mapToItem`, which
is not binding-reactive and would leave edges correct on first paint and silently frozen
afterwards. That constraint is the entire reason `GraphLayout.qml` exists.
→ [4 · qt-quirks](../4-code/qt-quirks.md)

The edge `Repeater` binds to `nodeGraph.connections`, whose `NOTIFY` is
`topologyChanged` **only** — so delegates rebuild on a rewire and never on a drag.

---

## The timeline

Transport, a time ruler, a draggable playhead and a waveform. Scrubbing seeks audio and
visuals together.

The waveform is drawn **twice**: a neutral copy, and an accent copy clipped at the
playhead. So advancing the cursor costs **no repaint at all** — the `Canvas` repaints only
on track change or resize.

---

## The canvas stage

Letterboxes the project's aspect ratio and toggles between Fit and 100%. The selected blur
node's mask circle is reproduced as an editor guide in QML, mirroring the shader's aspect
correction.

`NodeGraphPanel` is `enabled: !exportController.exporting`, on top of the export dialog's
modality — because two live scene graphs iterate one `RenderList`, and a topology change
would `beginResetModel` a model two `Repeater`s are walking.

---

## Shortcuts

`Space` play/pause · `Ctrl+N/O/S/Shift+S` document · `Ctrl+I` import · `Ctrl+E` export ·
`Del` delete selected.

Window `Shortcut`s beat the focused item, so bare `Space` and `Delete` are gated on
`window.typing` — otherwise they would fire while a text field has focus.

---

## See also

- [4 · qml-kit](../4-code/qml-kit.md) — the 19 controls and 4 singletons
- [4 · qt-quirks](../4-code/qt-quirks.md) — read before editing any QML
- [4 · graph-rules](../4-code/graph-rules.md) — what `canLink` actually checks
