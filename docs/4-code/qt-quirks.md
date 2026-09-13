# 4 · Qt and QML constraints this codebase is shaped by

**Sources:** `src/app/qml/GraphLayout.qml`, `src/app/qml/layers/LayerFrame.qml`, `src/app/qml/layers/BlurChain.qml`, `src/app/qml/graph/NodeCard.qml`, `src/app/RenderList.cpp`, `src/app/ExportController.cpp`, `src/core/SpectrumAnalyzer.cpp`

**Read this before you touch any QML in this project.** Several files here look
over-engineered until you know which Qt behaviour forced them. Every item below was
found the hard way and is commented at its call site.

---

## The eight hard constraints

### 1 · `layer` is `FINAL` on `QQuickItem`

A delegate cannot expose a model role named `layer` as a required property, and no
component can declare a property called `layer`. It is a hard error, not a style choice.

Consequences, all deliberate:
- `RenderList`'s role is **`layerObject`**, not `layer` (`RenderList.cpp:46-48`).
- `LayerFrame`'s backing property is **`layerData`** (`LayerFrame.qml:28-30`).

### 2 · `mapToItem` is not binding-reactive

QML cannot track an `x`/`y` dependency through `mapToItem`. An edge endpoint computed
that way is **correct on first paint and then silently never updates again** — no
warning, no error, just a connector that stops following its node.

This is the entire reason `GraphLayout.qml` exists as a file: every endpoint in the node
editor is plain arithmetic over `graphX`/`graphY`, which are real `NOTIFY`-ing
properties.

> **`mapToItem` is fine inside an event handler**, where it runs once instead of being
> re-evaluated. `NodeCard`'s press handler uses it legitimately.

### 3 · `drag.target` destroys the position binding

`NodeCard.x` is a **binding** to `node.graphX`. `drag.target` assigns `card.x` directly
and permanently destroys that binding: the node moves on screen, nothing reaches the
model, and it snaps back on the next re-evaluation or reload.

The codebase drags by writing the model in `onPositionChanged` instead:

```qml
onPressed:          grab = mapToItem(canvas.content, mouse.x, mouse.y)   // handler: fine
onPositionChanged:  node.graphX = startX + (p.x - grab.x)
onReleased:         canvas.snapToGrid(node)                              // snap on release only
```

Snapping only on release matters too — snapping during the drag makes the node feel
notchy and the edges twitch.

### 4 · A `Loader` overwrites its item's `width` and `height`

Any size binding on the loaded item's root is silently clobbered.

So `LayerFrame`'s root **spans the whole canvas** and places the actual layer in an inner
box. This is also why `frame.width` / `frame.height` mean *the canvas*, not the layer —
which is the single most confusing thing about reading `layers/` until you know it.

It is also why `VisualizerCanvas` **assigns** `layerData`/`layerTransforms`/`layerEffects`
in `onLoaded` rather than binding them.

### 5 · `QQuickWindow::grabWindow()` returns null under a `QQuickRenderControl`

And Qt 5's `QQuickRenderControl::grab()` is gone. Export therefore renders into an
explicit `QRhiTexture` render target and reads it back, which is why the file needs
`#include <rhi/qrhi.h>` and the target links `Qt6::GuiPrivate`.
See [export-pipeline](export-pipeline.md).

### 6 · A QML file cannot instantiate itself by type name

`BlurChain` is recursive — one level per enclosing blur node — so it recurses through
`Loader { source: "BlurChain.qml" }`.

### 7 · Inline components cannot see the enclosing document's ids

Hence `BlurPass.qml` exists as a separate file, `BlurChain` uses ordinary `Component`s,
and `BarLayerView`'s `FanBars` threads a `view` property in explicitly rather than
reaching outward.

### 8 · A list handed to a delegate after construction must be captured with `Qt.binding`

Copying it instead silently freezes the value (`BlurChain.qml:102-108`).

---

## Two that are not Qt's fault but bite just as hard

### `setParams` runs every frame

`SpectrumAnalyzer::setParams` receives the full struct on **every tick**, so it must
distinguish "buffers need rebuilding" from "a value changed". Rebuilding unconditionally
clears the smoothing envelope and the auto-gain reference every frame — which silently
degrades attack/decay into a plain gain and stops auto-gain ever converging, **with no
error and a plausible-looking picture**.

Pinned by `tst_fft::analyzerKeepsStateWhenParamsAreResent`. Full classification in
[dsp-pipeline](dsp-pipeline.md#the-biggest-trap-setparams-runs-every-frame).

### `console.log` prints nothing in this Qt build

Debug QML through side effects — a visible property, a colour change, a `Text` element —
not through the console.

---

## Recurring patterns you should match

These are conventions, not constraints, but the codebase is consistent about them and new
code should be too.

| Pattern | Why | Example |
|---|---|---|
| **Instantiate-and-hide, not `Loader`, for inspectors** | Tearing the panel down on every selection change costs more than a handful of idle bindings | `PropertyPanel.qml:265-271` |
| **Every control tolerates `undefined`** | The corollary of the above — each inspector is pointed at nodes lacking its properties | `ParamSlider.qml:37-42` |
| **`target` + `propertyName` generic binding** | One control works for any node property; a parallel unbound `localValue` + `activated` mode exists for non-model uses | `ParamSlider`, `ToggleRow`, `SegmentedControl`, `EnumCombo`, `ColorField` |
| **Reformat-unless-focused** | A drag and a keystroke must not fight: `onTextEdited` applies per keystroke, `onValueChanged` reformats only when not focused. `textEdited` is used precisely because it does not fire for programmatic assignment | `NumberField.qml:67-74` |
| **`Number(x.toPrecision(15))`** | Kills float noise on snapped values | `NumberField.qml:35-43` |
| **press / positionChanged-guarded-on-`pressed` / released / canceled** | The one drag idiom. There is no `DragHandler` or `WheelHandler` anywhere | `GraphCanvas`, `NodeCard`, `PortDot`, `TimelinePanel` |
| **`acceptedButtons` + z-order as event routing** | No hit-test special cases: `panArea` sits below `content` and takes `Left \| Middle`, cards take `Left` only — so a middle-drag over a card falls through and pans | `GraphCanvas.qml:176-184` |
| **`required property` on every delegate** | The codebase never relies on implicit delegate context | everywhere |
| **`Theme.withAlpha(c, a)` instead of item `opacity`** | A tint must not fade the item's children | `Theme.qml:19-21` |
| **Selection is a ring or hairline, never a flood** | In the graph especially, because `accent` neighbours `portVisual` | `NodeCard.qml:36-39` |
| **Window `Shortcut`s gated on `window.typing`** | So bare Space/Delete do not fire while a text field has focus | `Main.qml:28-32` |
| **`Canvas` + `requestPaint()` on discrete triggers only** | The waveform repaints on track change, never on playhead movement — progress is a **clip rectangle** over a second copy, so scrubbing costs no repaint | `TimelinePanel.qml:232-242` |
| **Fallbacks never reach geometry as `NaN`** | `BarLayerView` splits `hasBands` from `barCount`: zero bands must draw **nothing**, not one bar filling the layer | `BarLayerView.qml:24-32` |

---

## Import rules

- Files in **subdirectories** say `import krave` to see the root singletons.
- **`Main.qml` does not** — it sits at the module root beside `Theme.qml`, so implicit
  same-directory resolution supplies them.
- **The entire `layers/` subtree and `VisualizerCanvas.qml` import neither `krave` nor any
  context property.** That self-containment is exactly what lets `ExportController`
  instantiate `VisualizerCanvas.qml` standalone in an offscreen window. Do not break it
  by reaching for `Theme` in a layer view.

---

## Build gotcha

**All 49 `.qml` files are listed explicitly in `src/app/CMakeLists.txt`. There is no
glob.** Adding a file without adding it to `app_qml_files` is the single easiest way to
break this build, and the error is not obvious.

---

## See also

- [qml-kit](qml-kit.md) — the controls and singletons these patterns apply to
- [3 · editor-ui](../3-components/editor-ui.md) — where each constraint shows up
- [3 · rendering](../3-components/rendering.md) — `LayerFrame` and the blur chain in context
