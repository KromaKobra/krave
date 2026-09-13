# Guide · Add a node type

**Derived from** the commits that actually added nodes: `4e40c67` (merge, transform,
image) and `c5efbd9` (blur). Every file below appears in one of those diffs.

Read [4 · node-reference](../4-code/node-reference.md) and
[4 · graph-rules](../4-code/graph-rules.md) first.

---

## Decide what kind of node it is

This determines most of the steps:

| Kind | Base class | `visual` flag | Enters the draw list | Examples |
|---|---|---|---|---|
| **Draws something** | `VisualNode` | `true` | yes | spectrum, background, text, image |
| **Organises the graph** | `Node` | `false` | **no** | merge, transform, blur, output |
| **Produces band data** | `AudioSourceNode` | `false` | no | audio, separator |

An organising node that *encloses* a subgraph (like transform and blur) needs extra work in
`RenderList` — see step 5.

> **`NodeTypeSpec::visual` must agree with the class hierarchy.** `isVisual()` reads the
> flag, but `flatten` branches on `qobject_cast<VisualNode*>`. Nothing enforces agreement,
> and a mismatch means a node that is either never drawn or drawn without a delegate.

---

## The checklist

### 1 · `src/app/Node.h` — declare the class

Subclass the right base. Declare each setting as:

```cpp
Q_PROPERTY(qreal amount MEMBER m_amount NOTIFY changed)
```

`MEMBER` plus the shared `changed()` signal is what makes serialization and QML binding
free — [ADR-0020](../decisions/0020-member-properties-and-meta-walk.md). Override
`typeId()` to return your string literal.

**Write the header comment now, not later.** This is where the *why* lives; the docs link
here rather than duplicating it. Look at `TransformNode`'s and `BlurNode`'s headers for the
standard — they state what the node does, what it costs, and what it deliberately cannot
do.

### 2 · `src/app/Node.cpp` — constructor

Set the `tr()` display name and any default overrides (e.g. `SpectrumNode` sets
`m_sizeH = 0.5`). Audio sources also implement `analysisParams()` and `applyFrame()`.

### 3 · `src/app/NodePorts.cpp` — the table row

```cpp
{ QStringLiteral("mytype"),
  { visualIn() },              // inputs, IN INDEX ORDER — index order is compositing order
  true,                        // hasOutput
  PortKind::Visual,            // outputKind
  false,                       // essential
  true },                      // visual
```

Use the `visualIn()` / `audioIn()` helpers. Port **names** are serialized, so choose them
carefully — [ADR-0004](../decisions/0004-connections-keyed-by-port-name.md). Mark an input
`required` only if the node is useless without it —
[ADR-0009](../decisions/0009-required-marks-data-dependencies.md).

### 4 · `src/app/NodeGraph.cpp` — the factory

One branch in `createNode()`. There is no fallback: an unknown type returns `nullptr`.

### 5 · `src/app/RenderList.{h,cpp}` — only for enclosing nodes

**Skip this unless your node wraps a subgraph.** Transform and blur each needed a stack in
`Entry`, pushed before descending and popped after, plus a member in `operator==` so
re-plugging rebuilds the list. Both commits touched the header as well as the `.cpp`.

Read [4 · render-list](../4-code/render-list.md) and
[ADR-0013](../decisions/0013-transform-is-identity-blur-is-approximation.md) before adding a
third — the flat-list argument only holds for operations that distribute over a group.

### 6 · `src/app/qml/layers/XLayerView.qml` — only if it draws

Build on `LayerFrame`. Remember **`frame.width`/`frame.height` are the canvas**, not the
layer box, and all geometry arrives normalized 0..1 —
[ADR-0019](../decisions/0019-normalized-geometry.md).

**Do not import `krave` or touch a context property here.** The `layers/` subtree is
deliberately self-contained so `ExportController` can instantiate `VisualizerCanvas.qml`
standalone.

### 7 · `src/app/qml/VisualizerCanvas.qml` — dispatch

A `case` in the `typeId` switch plus a `Component`. Only needed if the node draws.

### 8 · `src/app/qml/layers/LayerFrame.qml` — only for enclosing nodes

Both node-adding commits touched this. An enclosing node needs its stack consumed here —
transforms fold into `composedMatrix()`, effects feed the `BlurChain` and rebind
`contentHost`.

### 9 · Inspector

Create `src/app/qml/panels/inspectors/XInspector.qml`. Copy `BackgroundInspector.qml` (31
lines) as the skeleton:

```qml
ColumnLayout {
    id: root
    property var node: null
    property string nodeType: ""
    Layout.fillWidth: true
    spacing: Theme.space4
    visible: root.nodeType === "mytype"

    SectionHeader { text: qsTr("…") }
    ParamSlider {
        label: qsTr("Amount"); info: qsTr("One line explaining it.")
        target: root.node; propertyName: "amount"; from: 0; to: 1
    }
}
```

Imports are always `QtQuick`, `QtQuick.Controls.Basic`, `QtQuick.Layouts`, `krave`,
`"../../controls"`.

Then add **one line** to `PropertyPanel.qml` beside the others:

```qml
XInspector { node: root.selected; nodeType: root.selectedType }
```

Inspectors are instantiated and hidden, not loaded — so **every control must tolerate
`undefined`**. Use `visible:` + `sub: true` for controls that depend on a toggle. Give each
control a one-line `info`; reserve the long `details` popup for genuinely subtle ones.

If your node is not a `VisualNode` but still needs placement controls, carry your own
Placement section the way `TransformInspector` and `BlurInspector` do — the shared one is
gated on `posX !== undefined`.

### 10 · `src/app/CMakeLists.txt` — **the step everybody misses**

Add every new `.qml` to `app_qml_files`. **There is no glob.** Omitting it is the single
easiest way to break this build, and the error is not obvious.

New shaders go in the `qt_add_shaders` block — see
[add-a-shader-effect](add-a-shader-effect.md).

### 11 · `src/app/qml/NodeTypes.qml` — presentation

Add `{typeId, title, label, icon, hint}`. Add a path to `Icons.qml` if you need a new
glyph. This file is **presentation only** — ports and creatability live in the C++ table
([ADR-0003](../decisions/0003-port-table-in-cpp.md)) — and nothing mechanically checks that
it covers `creatableTypes()`, so a missed entry shows as an undecorated add-menu row.

### 12 · `tests/tst_graph.cpp`

Extend the **exact type-set assertion** in `portTableCoversEveryNodeType`. It is spelled
out rather than derived, deliberately: "so adding or dropping a type is a deliberate edit
to this list."

Add cases for any new structural behaviour. `blursAccumulateOutermostFirst` is the model
for an enclosing node.

### 13 · `tests/tst_project.cpp`

Add a round-trip case pushing **every** new property off its default, so a missed field
cannot pass by luck. Compare via the meta-walk, as the existing cases do, so later
additions are covered automatically.

### 14 · Documentation

- [4 · node-reference](../4-code/node-reference.md) — a section for the type
- [4 · graph-rules](../4-code/graph-rules.md) — a row in the port grid
- A new [ADR](../decisions/README.md) if the node embodies a decision that spans files

---

## Also worth doing

- **An example project.** Both commits updated
  `examples/SyrexAttemptv3-lightweight.mvproj`. `everyExampleProjectLoads` then covers it.
- **`tools/migrate_v2_to_v3.py`**, if old files should map onto the new type (`4e40c67`
  touched it).

---

## Build and check

```bash
./scripts/build.sh --test
./scripts/build.sh --run
```

Then, in the app: add the node from the menu, wire it up, edit every property, save,
reopen, and confirm nothing reset. That last step is what catches a missing `MEMBER`.

> `console.log` prints nothing in this Qt build. Debug QML through a visible side effect.

---

## See also

- [add-a-node-property](add-a-node-property.md) — the much shorter path
- [4 · qt-quirks](../4-code/qt-quirks.md) — read before writing QML
- [testing-and-debugging](testing-and-debugging.md)
