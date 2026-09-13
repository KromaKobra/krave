# 4 · Node reference

**Sources:** `src/app/Node.h`, `src/app/Node.cpp`, `src/app/qml/panels/inspectors/*.qml`, `src/app/qml/panels/PropertyPanel.qml`, `src/core/SpectrumAnalyzer.h`

Every node type and every property. This is the lookup table — for *why* a node exists,
read its header comment in `Node.h`, which is the authoritative prose.

---

## How to read this

**Ten concrete types** over three base classes:

```
Node                          abstract — identity, name, graph position
├── VisualNode                draws; placement + compositing
│   ├── SpectrumNode          "spectrum"
│   ├── BackgroundNode        "background"
│   ├── TextNode              "text"
│   └── ImageNode             "image"
├── AudioSourceNode           abstract — owns a SpectrumAnalyzer
│   ├── AudioNode             "audio"      essential
│   └── SeparatorNode         "separator"
├── MergeNode                 "merge"      organises, draws nothing
├── TransformNode             "transform"  organises, draws nothing
├── BlurNode                  "blur"       organises, draws nothing
└── OutputNode                "output"     essential, the root of the render
```

`MergeNode`, `TransformNode`, `BlurNode` and `OutputNode` are deliberately **not**
`VisualNode`s. That single fact is what keeps them out of the render list with no
filtering code anywhere — see [render-list](render-list.md).

**Serialization is automatic and you cannot forget it.** Properties are declared
`Q_PROPERTY(… MEMBER … NOTIFY changed)`, and `Project.cpp` saves by walking the
`QMetaObject`. Every *writable, stored* property persists with zero extra code.
Read-only properties — `typeId`, `nodeId`, `bands`, `barCount`, `inactiveEdgeBars`,
`level`, `bass`, `mid`, `treble` — are skipped, because they are derived or live data.
→ [ADR-0020](../decisions/0020-member-properties-and-meta-walk.md)

**All properties share one `changed()` signal.** QML re-evaluates affected bindings on
it; the slight over-invalidation is far cheaper than a signal per property. This is why
a property edit reaches the canvas the same frame with no apply step.

**Nothing validates a property value.** Enum properties are plain `QString` with the
fallback applied at *use* time, and numeric ranges exist only as inspector slider
bounds. `setProperty` from a loaded file bypasses both — the bundled
`examples/SyrexAttemptv3-lightweight.mvproj` stores `minHz: -72`, well outside the
slider's 10..500. See [Enums and their fallbacks](#enums-and-their-fallbacks).

Column meanings: **Default** is the C++ member initializer. **Range** is the inspector
control's bounds, *not* a clamp. **Panel label** is the text beside the control in the
property panel, which often differs from the property name; "—" means **the property has
no interface at all** and can only be set by editing the file.

---

## `Node` — base

`src/app/Node.h:49-120`. Present on every node.

| Property | Type | Default | Range | Panel label | Notes |
|---|---|---|---|---|---|
| `typeId` | `QString` | per subclass | — | — | CONSTANT. Written as the `type` sibling key, not inside `props`. |
| `nodeId` | `int` | assigned on adoption | ≥ 1 | — | Read-only. Written as the `id` sibling key. **Never reused within a session** → [ADR-0007](../decisions/0007-node-ids-are-never-reused.md) |
| `name` | `QString` | per subclass, via `tr()` | — | Name | Card title. |
| `graphX` | `qreal` | `0.0` | — | — (drag) | Graph-space X. Negative values are legal. |
| `graphY` | `qreal` | `0.0` | — | — (drag) | Graph-space Y. |

`graphX`/`graphY` are properties rather than plain members so the drag handler writes
them directly *and* the meta-walk persists them — "the same bargain the rest of the
property set already makes" (`Node.h:58-60`).

---

## `VisualNode` — base for everything that draws

`src/app/Node.h:125-156`. Placement and compositing live here rather than on `Node`,
"because a separator has no position and an output has no opacity — and when they did,
the inspector showed seven dead controls for them".

| Property | Type | Default | Range | Panel label | Notes |
|---|---|---|---|---|---|
| `layerVisible` | `bool` | `true` | — | Visible | Also the `layerVisible` role in the node model. |
| `layerOpacity` | `qreal` | `1.0` | 0 … 1 | Opacity | Per-layer alpha. |
| `blendMode` | `QString` | `"normal"` | enum | Blend mode | `normal` · `add` · `screen`. → [ADR-0012](../decisions/0012-blend-via-premultiplied-alpha.md) |
| `posX` | `qreal` | `0.5` | -0.5 … 1.5 | Centre X | Layer **centre** X, normalized to canvas. |
| `posY` | `qreal` | `0.5` | -0.5 … 1.5 | Centre Y | Centre Y. `SpectrumNode` overrides to `0.75`, `TextNode` to `0.25`. |
| `sizeW` | `qreal` | `1.0` | 0 … 2 | Width | Width as a fraction of canvas. On a spectrum this is the **horizontal fill**: measured on the bundled bass preset, 0.7 fills 55% of the frame, 1.0 fills 78%, 1.28 reaches edge to edge. |
| `sizeH` | `qreal` | `1.0` | 0 … 2 | Height | Height. `SpectrumNode` overrides to `0.5`. |
| `layerRotation` | `qreal` | `0.0` | -180 … 180 | Rotation | Degrees. |

All geometry is normalized 0..1 so a project renders identically at 1280×720 preview and
3840×2160 export. → [ADR-0019](../decisions/0019-normalized-geometry.md)

> `PropertyPanel.qml:34` decides whether to show the Placement section by
> **duck-typing**: `selected.posX !== undefined`. Give a non-visual node a `posX` and it
> grows a placement section it cannot use.

---

## `AudioSourceNode` — base for things that run an FFT

`src/app/Node.h:162-193`.

| Property | Type | Access | Notes |
|---|---|---|---|
| `bands` | `QList<qreal>` | read-only | Live analysis output. Not serialized. |

Key method: `audioRoot()` walks the `audio` input chain back to the `AudioNode` holding
the PCM, with its own cycle guard. `nullptr` means "not plugged into anything", which is
how `SpectrumEngine` knows to skip the source entirely.
→ [ADR-0002](../decisions/0002-audio-edge-selects-a-source.md)

---

## `AudioNode` — `"audio"`

`src/app/Node.h:200-219`. **Essential** — exactly one exists, cannot be created or
deleted. The track itself, as a one-band analysis over the full range.

| Property | Type | Access | Notes |
|---|---|---|---|
| `level` | `qreal` | read-only | Overall loudness 0..1 — literally band 0. |

**It has no settable properties at all** beyond `name`/`graphX`/`graphY`. Its analysis
parameters are fixed in `Node.cpp:144-157`: `bandCount 1`, `20 Hz … 20 kHz`, Decibel,
`attackTau 0.01`, `decayTau 0.15`, `bandNormalization 0.0`.

It deliberately exposes `level` and **not** `bass`/`mid`/`treble`: with a single band the
centre frequency lands in exactly one of those three buckets and the other two read 0.0,
"which would be a trap rather than an API".

---

## `SeparatorNode` — `"separator"`

`src/app/Node.h:231-345`. **25 settable properties** — the largest surface in the
program, and the reason spectrum nodes carry only appearance. Draws nothing.

One separator can feed any number of spectrum nodes, and the FFT runs **once per
separator** rather than once per thing drawing it.

### Resolution

| Property | Type | Default | Range | Panel label | Notes |
|---|---|---|---|---|---|
| `bandCount` | `int` | `48` | 4 … 256 step 1 | Band count | Clamped to ≥ 1 in `analysisParams()`. |
| `windowMs` | `qreal` | `85.0` | 10 … 400 step 5 | Window | Analysis window length. Converted to samples at a **hard-coded 48 kHz** (`Node.cpp:181`). Sets frequency resolution: a tone spreads over ≈ `2 × rate / windowSamples` Hz. |
| `minHz` | `qreal` | `40.0` | 10 … 500 step 5 | Low cut | **May be negative** — that insets the lowest audible band from the display edge. → [ADR-0016](../decisions/0016-unresolvable-bands-are-flagged.md) |
| `maxHz` | `qreal` | `14000.0` | 200 … 22000 step 100 | High cut | |
| `hzLinearFactor` | `qreal` | `0.0` | 0 … 1 | Linear ←→ log spacing | 0 = logarithmic band spacing (equal width per octave), 1 = linear (equal width per Hz). |
| `averageCount` | `int` | `1` | 1 … 16 step 1 | Windows averaged | Welch averaging over half-overlapped windows. Costs one extra transform each. |

> `Node.h:238-239` claims `windowMs` is "Rounded up to a power of two internally". **It is
> not** — the transform is zero-padded instead, precisely so resolution is continuously
> adjustable. The comment is stale; `Node.cpp:176-180` and `SpectrumAnalyzer.h:61-66`
> are correct. → [ADR-0014](../decisions/0014-zero-pad-the-analysis-window.md)

### Conditioning

The **Mode** column matters: Linear amplitude mode ignores four of these outright, while
the inspector still shows all four sliders.

| Property | Type | Default | Range | Panel label | Mode | Notes |
|---|---|---|---|---|---|---|
| `bandMeasure` | `QString` | `"rms"` | enum | Band measure | both | `rms` averages the bins in a band; `peak` takes the loudest and is the single biggest source of frame-to-frame jitter. |
| `amplitudeMode` | `QString` | `"db"` | enum | Amplitude | — | `db` fills the display evenly but lifts the noise between partials; `linear` leaves a flat floor with discrete peaks. |
| `linearGain` | `qreal` | `30.0` | 0.5 … 60 step 0.5 | Gain | **linear only** | Magnitude multiplier to reach 0..1. |
| `linearFloor` | `qreal` | `0.0` | — | **none** | **linear only** | Black level, subtracted after the gain then rescaled. **No interface exists** — file-only. → [ADR-0015](../decisions/0015-linear-amplitude-and-expansion.md) |
| `lowFreqMagnitude` | `qreal` | `1.0` | 0 … 1 | Low-frequency level | both | Gain at the bottom of the range, ramping to 1.0 at the top. A straight ramp over **band index**, not octaves. |
| `tiltDbPerOctave` | `qreal` | `0.0` | 0 … 9 step 0.25 | Treble lift | both | Treble lift, compensating music's natural roll-off. |
| `bandNormalization` | `qreal` | `0.7` | 0 … 1 | Per-band auto-gain | **dB only** | Per-band auto-gain against each band's own slow-moving **average** — not its recent peak, which would flatten dynamics to nothing. |
| `normalizationTau` | `qreal` | `3.0` | 0.2 … 10 step 0.1 | Auto-gain adapt | **dB only** | Seconds for that reference to follow a change. |
| `floorDb` | `qreal` | `-78.0` | -100 … -20 step 1 | Floor | **dB only** | Bottom of the display window. |
| `ceilingDb` | `qreal` | `-4.0` | -40 … 6 step 1 | Ceiling | **dB only** | Top of the window. |
| `gateDb` | `qreal` | `8.0` | 0 … 24 step 1 | Gate | **dB only** | Soft gate width above the floor; larger means emptier gaps between hits. |
| `scalingCurve` | `QString` | `"power"` | enum | Scaling curve | both | `linear` · `log` · `power`. |
| `gamma` | `qreal` | `3.0` | 0.2 … 4 | Gamma | both | Exponent for `power`. Above 1 it is an **expander** — pushes mid-level bands down while leaving peaks tall. Inert unless the curve is `power`. |

### Motion

| Property | Type | Default | Range | Panel label | Notes |
|---|---|---|---|---|---|
| `smoothingMode` | `QString` | `"attackDecay"` | enum | Smoothing | `attackDecay` rises fast and falls slow; `symmetric` uses one constant both ways. |
| `smoothMs` | `qreal` | `60.0` | 0 … 400 step 5 | Smoothing | Symmetric mode only. |
| `attackMs` | `qreal` | `5.0` | 0 … 300 step 1 | Attack | Rise constant. |
| `decayMs` | `qreal` | `220.0` | 10 … 1500 step 5 | Decay | Fall constant. |
| `spatialSmoothing` | `qreal` | `0.15` | 0 … 1 | Neighbour smoothing | Blends each band toward its neighbours. Needs > 2 bands to do anything. |
| `floorLevel` | `qreal` | `0.0` | 0 … 0.3 step 0.001 | Resting level | Minimum drawn level, so idle bars stay visible. |

All three time values are authored in **milliseconds** and converted to seconds as time
constants. They are never per-frame factors.
→ [ADR-0010](../decisions/0010-smoothing-is-a-time-constant.md)

### Live data (read-only, not serialized)

`bands` (inherited), `bass`, `mid`, `treble`.

---

## `SpectrumNode` — `"spectrum"`

`src/app/Node.h:351-461`. The primary visualizer. Inherits all eight `VisualNode`
properties; constructor overrides `sizeH → 0.5`, `posY → 0.75`.

**It carries only how the bars look.** Everything about turning audio into numbers lives
on the separator wired to its `audio` input.

### Shape

| Property | Type | Default | Range | Panel label | Notes |
|---|---|---|---|---|---|
| `barWidthRatio` | `qreal` | `0.65` | 0.05 … 1 | Bar width | Bar width as a fraction of its slot. Not to be confused with `sizeW` ("Width"), which scales the whole row. |
| `cornerRadius` | `qreal` | `2.0` | 0 … 24 step 0.5 | Corner radius | Pixels. |
| `minHeight` | `qreal` | `0.005` | 0 … 0.5 step 0.001 | Min height | Fraction of the layer box. |
| `maxHeight` | `qreal` | `0.8` | 0.05 … 1 | Max height | |

### Placement

| Property | Type | Default | Range | Panel label | Notes |
|---|---|---|---|---|---|
| `orientation` | `QString` | `"linear"` | enum | Orientation | `linear` · `radial`. A retired third value `"mirrored"` is folded to `linear` + `mirrorBars` by the migration script. |
| `mirrorBars` | `bool` | `false` | — | Mirror bars | **Vertical** mirror: each bar grows both ways from its base line. Works in both orientations. |
| `mirrorSpectrum` | `bool` | `false` | — | Mirror spectrum | **Frequency-axis** mirror: band order repeated back on itself. Doubles `barCount`. Independent of `orientation`. |
| `bassInner` | `bool` | `false` | — | Bass in the centre | Which end meets in the centre. `false` puts the lowest band at both outer edges. Only meaningful with `mirrorSpectrum`. |
| `innerRadius` | `qreal` | `0.35` | 0.05 … 0.9 | Inner radius | Radial only: hole size as a fraction of the layer's smaller dimension. |
| `arc` | `qreal` | `0.0` | -1 … 1 | Arc | Linear only: bows the base line into an arc subtending `abs(arc) × 180°`. Positive = dome, negative = bowl. |
| `barsMatchArc` | `bool` | `false` | — | Bars match arc | Bars taper to fill their angular slot, so an arced spectrum has no wedge gaps. Only meaningful with `arc`. |
| `growUpward` | `bool` | `true` | — | **none** | Growth direction. Read by `BarLayerView.qml:295,350` but **no interface exists** — file-only. |

The two mirrors are easy to confuse and are explicitly disambiguated at `Node.h:365-377`:
`mirrorBars` is vertical, `mirrorSpectrum` is the frequency axis.

### Colour

| Property | Type | Default | Range | Panel label | Notes |
|---|---|---|---|---|---|
| `colorMode` | `QString` | `"gradient"` | enum | Mode | `solid` · `gradient` · `spectrum` (hue swept across the bars). |
| `colorA` | `QColor` | `#4cc9f0` | — | Colour A | Primary. |
| `colorB` | `QColor` | `#f72585` | — | Colour B | Gradient end. |
| `reactiveBrightness` | `qreal` | `0.0` | 0 … 2 | Reactive brightness | Extra brightness in proportion to a bar's level; 0 = off. |

### Live data (read-only, not serialized)

| Property | Type | Notes |
|---|---|---|
| `bands` | `QList<qreal>` | **Already mirrored** when `mirrorSpectrum` is set, so the view just draws it. |
| `barCount` | `int` | `bands.size()`. |
| `inactiveEdgeBars` | `int` | Bars at each outer edge that carry no distinct content and must not be drawn — they come from bands below the window's lowest resolvable frequency and all collapse onto one bin. → [ADR-0016](../decisions/0016-unresolvable-bands-are-flagged.md) |
| `bass` / `mid` / `treble` | `qreal` | Aggregates. |

`SpectrumNode` is **not** an `AudioSourceNode` — it runs no FFT, it is *fed* a frame by
`SpectrumEngine`. Adding ten spectrum nodes costs no extra analysis. An unconnected
audio input makes it draw **nothing**, with no fallback.

---

## `BackgroundNode` — `"background"`

`src/app/Node.h:464-476`. Flat colour behind everything else; a picture is an
`ImageNode`'s job.

| Property | Type | Default | Panel label |
|---|---|---|---|
| `color` | `QColor` | `#0b0b12` | Colour |

---

## `TextNode` — `"text"`

`src/app/Node.h:479-503`. Constructor overrides `posY → 0.25`.

| Property | Type | Default | Range | Panel label | Notes |
|---|---|---|---|---|---|
| `text` | `QString` | `"Track title"` | — | text field | Bound directly rather than via `target`/`propertyName`. A "Use track title" button fills it from `audioEngine.trackTitle`. |
| `fontFamily` | `QString` | `"Segoe UI"` | — | **none** | **No interface exists** — file-only. Note the default is a Windows font. |
| `fontScale` | `qreal` | `0.06` | 0.01 … 0.3 step 0.001 | Font size | Point size as a **fraction of canvas height**, so text scales with resolution. |
| `bold` | `bool` | `true` | — | Bold | |
| `color` | `QColor` | `#ffffff` | — | Colour | |
| `alignment` | `QString` | `"center"` | enum | Alignment | `left` · `center` · `right`. |

---

## `ImageNode` — `"image"`

`src/app/Node.h:510-537`. A still picture from disk. **Has no visual input** — stacking
one thing over another is what Merge is for.

| Property | Type | Default | Range | Panel label | Notes |
|---|---|---|---|---|---|
| `source` | `QString` | `""` | — | file dialog | A `file://` URL, stored **absolute**, exactly as the dialog hands it over. The inspector shows a missing-file state rather than relocating it. |
| `fillMode` | `QString` | `"crop"` | enum | Fill mode | `stretch` · `fit` · `crop`. |
| `cropX` | `qreal` | `0.0` | 0 … 1 | Left | Crop rect, **normalized to the source image** — so the node needs to know nothing about the file's pixel dimensions and survives the image being swapped for one of a different size. |
| `cropY` | `qreal` | `0.0` | 0 … 1 | Top | |
| `cropW` | `qreal` | `1.0` | 0.01 … 1 | Width | |
| `cropH` | `qreal` | `1.0` | 0.01 … 1 | Height | |

---

## `MergeNode` — `"merge"`

`src/app/Node.h:549-556`. **Zero properties, and no code on the class at all.**

This is the node that makes the graph a graph rather than a chain — until it existed,
every visual type had at most one visual input and the flatten could only produce a
straight line.

All of its behaviour is **port order**: input 0 is `background`, input 1 is
`foreground`, and `RenderList::flatten` walks inputs in index order. Swap the two edges
and the compositing order flips.
→ [ADR-0021](../decisions/0021-port-index-is-compositing-order.md)

It is deliberately not a `VisualNode` and has no inspector.

---

## `TransformNode` — `"transform"`

`src/app/Node.h:575-604`. Moves, scales and rotates its **entire upstream subgraph** as
one unit. Draws nothing, has no box of its own.

| Property | Type | Default | Range | Panel label | Notes |
|---|---|---|---|---|---|
| `offsetX` | `qreal` | `0.0` | -1 … 1 | Offset X | Translation as a fraction of canvas **width**. |
| `offsetY` | `qreal` | `0.0` | -1 … 1 | Offset Y | Fraction of canvas **height**. |
| `scaleX` | `qreal` | `1.0` | 0 … 4 | Scale X | |
| `scaleY` | `qreal` | `1.0` | 0 … 4 | Scale Y | |
| `rotation` | `qreal` | `0.0` | -180 … 180 | Rotation | Degrees about the pivot, clockwise. |
| `pivotX` | `qreal` | `0.5` | 0 … 1 | Pivot X | Fraction of the **canvas** — not of the group's bounding box, which no node knows. |
| `pivotY` | `qreal` | `0.5` | 0 … 1 | Pivot Y | |

It needs **no offscreen buffer**, and the argument matters: an affine map about a
canvas-relative pivot distributes over a group, so transforming each member individually
is mathematically identical to transforming the group. That exactness is why this node
has no group opacity and no group blend — those would *not* distribute.
→ [ADR-0013](../decisions/0013-transform-is-identity-blur-is-approximation.md)

---

## `BlurNode` — `"blur"`

`src/app/Node.h:633-676`. Three independently switchable effects plus a circle mask.
Draws nothing. **Every amount is a fraction of canvas height, never a pixel count**, so
a 4K export matches the preview.

| Property | Type | Default | Range | Panel label | Notes |
|---|---|---|---|---|---|
| `gaussianEnabled` | `bool` | `false` | — | Gaussian | Plain Gaussian softening. |
| `gaussianAmount` | `qreal` | `0.02` | 0 … 0.2 step 0.001 | Radius | Radius, fraction of canvas height. |
| `chromaBlurEnabled` | `bool` | `false` | — | Chroma blur | Blurs Cb/Cr only, leaving luma sharp — the look cheap video compression gives. |
| `chromaBlurAmount` | `qreal` | `0.05` | 0 … 0.3 step 0.001 | Chroma radius | |
| `chromaSplitEnabled` | `bool` | `false` | — | Chromatic separation | Radial RGB separation (chromatic aberration). |
| `chromaSplitAmount` | `qreal` | `0.01` | 0 … 0.05 step 0.001 | Separation | |
| `maskEnabled` | `bool` | `false` | — | Limit to circle | Circle that scales how much effect each pixel gets. |
| `maskX` | `qreal` | `0.5` | 0 … 1 | Centre X | Centre, fraction of canvas. |
| `maskY` | `qreal` | `0.5` | 0 … 1 | Centre Y | |
| `maskRadius` | `qreal` | `0.3` | 0 … 1 | Radius | Fraction of canvas **height**; the shader applies aspect correction so it stays circular. |
| `maskSoftness` | `qreal` | `0.5` | 0 … 1 | Soft edge | Fraction of the **radius**. |
| `maskInvert` | `bool` | `false` | — | Invert | Off: inside the circle is blurred. On: outside is — which is what the node was built for. |

**It blurs each upstream layer separately, not the composited group**, and unlike
Transform that is an *approximation* rather than an identity: convolution does not
distribute over a group, so two overlapping layers give `blur(A) over blur(B)`. A flat
Background upstream stays looking sharp, because blurring a solid colour returns it
unchanged. To smear two layers together, blur each arm **before** the Merge.
→ [ADR-0013](../decisions/0013-transform-is-identity-blur-is-approximation.md)

The selected blur node's mask circle is drawn as an editor guide on the canvas
(`CanvasStage.qml:175-223`).

---

## `OutputNode` — `"output"`

`src/app/Node.h:683-690`. **Essential**, zero properties. Exactly one exists and it
cannot be deleted.

Walking its visual input backwards produces the draw order — which is the whole reason
layering needs no array index. It is not a `VisualNode`, so it never enters the render
list itself.

---

## Enums and their fallbacks

Every enum property is a plain `QString`. **Nothing validates it on load or on write.**
The mapping happens at use time, and each has a permissive fallback — so a typo does not
error, it silently selects the fallback.

| Property | Values | Unknown value becomes | Mapped at |
|---|---|---|---|
| `blendMode` | `normal` · `add` · `screen` | treated as `normal` | `LayerFrame.qml:116-118` |
| `orientation` | `linear` · `radial` | — | `BarLayerView.qml` | Orientation 
| `colorMode` | `solid` · `gradient` · `spectrum` | — | `BarLayerView.qml` | Mode 
| `alignment` | `left` · `center` · `right` | — | `TextLayerView.qml` | Alignment 
| `fillMode` | `stretch` · `fit` · `crop` | — | `ImageLayerView.qml` | Fill mode 
| `bandMeasure` | `rms` · `peak` | **`Rms`** | `Node.cpp:187-188` | Band measure 
| `amplitudeMode` | `db` · `linear` | **`Decibel`** | `Node.cpp:189-190` | Amplitude 
| `scalingCurve` | `linear` · `log` · `power` | **`Power`** | `Node.cpp:205-210` | Scaling curve 
| `smoothingMode` | `attackDecay` · `symmetric` | **`AttackDecay`** | `Node.cpp:194-196` | Smoothing 

So `"dB"` silently means Decibel, and `"Power"` silently means Power for the wrong
reason. If you add an enum value, add it to the inspector's model *and* the mapping
function — the fallback will otherwise hide the omission.

---

## Properties with no interface

Set only by hand-editing a `.mvproj`. Each is real, serialized, and read by the
renderer:

| Property | Node | What it does |
|---|---|---|
| `linearFloor` | Separator | Black level in linear amplitude mode. Load-bearing for the "low body, tall spikes" look — see [ADR-0015](../decisions/0015-linear-amplitude-and-expansion.md). |
| `growUpward` | Spectrum | Bar growth direction. |
| `fontFamily` | Text | Font family; defaults to a Windows font. |

---

## See also

- [graph-rules](graph-rules.md) — which ports each type has, and what may connect
- [dsp-pipeline](dsp-pipeline.md) — where each Separator property lands in the DSP chain
- [project-format](project-format.md) — how these persist
- [guides/add-a-node-type](../guides/add-a-node-type.md) — adding an eleventh type
