# Music Visualizer

A desktop music visualizer in the spirit of Avee Player: import a track, wire up a
frequency-reactive visualizer as a **node graph**, preview it live against the audio, save the
configuration as a reusable project, and export a video with the visualizer burned in.

C++17 core, Qt Quick / QML front end, ffmpeg for decode and encode.

## The idea it is built on

The whole decoded track lives in memory and the playhead position is always known, so **spectrum
analysis is a pure function of `(pcmBuffer, timeSeconds)`** rather than a tap on a live audio
stream. Everything else follows from that:

- Preview asks for the spectrum at the playback cursor, 60 times a second.
- Export asks for the spectrum at `frameIndex / fps`, as fast as the GPU allows.
- Both render **the same `VisualizerCanvas.qml`** bound to **the same `Node` objects**, so the
  exported video cannot drift from what you previewed.

Two rules keep that promise honest:

- Smoothing and decay are stored as **time constants**, with the blend factor derived per frame as
  `1 - exp(-dt / tau)`. A per-frame factor would make a 30 fps export decay at half the rate of a
  60 Hz preview. `tst_fft` asserts this directly — decaying for 0.5 s at 30 fps and at 60 fps must
  land within 0.02 of each other.
- Export installs a **fixed-step `QAnimationDriver`**, so every QML animation advances exactly
  1/fps per rendered frame. Rendering runs several times faster than real time, so on the wall
  clock an animation would burn through twelve seconds of motion in the two seconds the render
  actually took.

## Requirements

| | |
|---|---|
| Qt | 6.8+ with **Quick, QuickControls2, Multimedia, Concurrent, ShaderTools** and private Gui headers |
| Compiler | MSVC 2022+ (verified on Build Tools 2026, toolset 14.51) or GCC 13+ / Clang 17+ |
| CMake | 3.21+, Ninja |
| ffmpeg | on `PATH`, or set `MV_FFMPEG` to its full path |

ffmpeg is required for both import and export — it is the decoder as well as the encoder.

## Build (Linux / Nix)

The flake pins the whole toolchain — Qt, cmake, ninja, ffmpeg — so nothing needs to be installed
globally.

```bash
nix build           # -> ./result/bin/krave, ffmpeg baked onto its PATH
nix run             # build and launch
```

For day-to-day work there is a build script — the counterpart to `scripts/build.ps1`. It re-execs
itself inside `nix develop` when cmake is not on `PATH`, so it works from a bare terminal:

```bash
./scripts/build.sh                  # debug build
./scripts/build.sh --config release
./scripts/build.sh --test           # build then run ctest
./scripts/build.sh --run            # build then launch the app
./scripts/build.sh --clean
```

Or drive the presets by hand from inside the shell:

```bash
nix develop
cmake --preset linux-debug && cmake --build --preset linux-debug && ctest --preset linux-debug
./build/linux-debug/bin/krave
```

The shell also carries `clangd` (it reads the `compile_commands.json` the preset emits), `gdb`,
`qmllint` and `ffmpeg`. An in-tree binary must be launched **from inside the shell** — it is not
wrapped, so it finds Qt's plugins and QML modules only through the `QT_PLUGIN_PATH` /
`QML2_IMPORT_PATH` the shell exports. The `nix build` output is wrapped and runs anywhere.

### VS Code

`.vscode/tasks.json` and `launch.json` carry both hosts; the Linux debug configs are the ones
suffixed `(Linux)`. Every build writes the dev shell's Qt environment to `build/<preset>/dev.env`,
which those configs read as their `envFile` — that is what lets a debug session find the QML
modules without VS Code itself being started from the shell.

Debugging uses **CodeLLDB** (`vadimcn.vscode-lldb`), not the usual `cppdbg`: that type comes from
`ms-vscode.cpptools`, which is proprietary and absent from Open VSX, so it cannot be installed on
VSCodium at all. On NixOS take CodeLLDB from nixpkgs rather than the extensions panel — the
marketplace build ships an unpatched prebuilt binary that will not run:

```nix
programs.vscode = {
  enable = true;
  package = pkgs.vscodium;
  mutableExtensionsDir = true;   # keeps hand-installed extensions working
  profiles.default.extensions = with pkgs.vscode-extensions; [ vadimcn.vscode-lldb ];
};
```

To install it system-wide, add the repo as a flake input in `/etc/nixos/flake.nix` and put
`inputs.krave.packages.${pkgs.system}.default` in `environment.systemPackages`.

Building without Nix works too, given Qt 6.8+ and ffmpeg on `PATH`:

```bash
cmake --preset linux-debug && cmake --build --preset linux-debug
```

## Build (Windows)

`cmake` and `ninja` ship inside the VS Build Tools and are usually not on `PATH`, and Ninja needs
the MSVC environment. The build script handles all of that:

```bash
pwsh ./scripts/build.ps1
```

```bash
pwsh ./scripts/build.ps1 -Config Release -Test
```

If you already have a Developer PowerShell with Qt on `CMAKE_PREFIX_PATH`, the CMake presets work
directly:

```bash
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
```

## Using it

Launch the editor, optionally opening straight onto a track:

```bash
./build/linux-debug/bin/krave --audio track.mp3   # build/Debug/bin/krave.exe on Windows
```

- **Import audio** loads mp3/wav/flac/aac/m4a/ogg. A waveform, a time ruler and a draggable
  playhead fill the transport dock along the bottom; scrubbing seeks audio and visuals together.
- The **node graph** (left) is where the scene is built. Nodes are rectangles with typed ports;
  drag from an output to an input to connect them. Middle-drag pans, including over nodes.
  Double-click a node to rename it, `Del` to delete it.
- The **property panel** (right) binds directly to the selected node's `Q_PROPERTY`s, so every
  edit shows on the canvas the same frame. There is no apply step. Every slider is paired with a
  number field for exact values.
- The **canvas** letterboxes the project's aspect ratio, and toggles between Fit and 100%.
  Composition guides (thirds plus a title-safe box) are an editor overlay only — they are drawn
  outside the canvas item, so they never reach an exported frame.
- Shortcuts: `Space` play/pause, `Ctrl+N/O/S/Shift+S` document, `Ctrl+I` import, `Ctrl+E` export,
  `Del` delete the selected node.
- **Save / Save as…** writes a `.mvproj` file (format version 3). It stores the nodes, their
  properties, their graph positions and the connections between them, plus the canvas settings —
  and deliberately **no audio path**, so projects stay portable as reusable templates. Older files
  are rejected; `tools/migrate_v2_to_v3.py` converts them.
- The editor **reopens where you left it**: the last project, the last track and the playback
  volume come back on launch. That state lives in `session.json` under the platform's app-config
  directory (`~/.config/kroma/Krave/` on Linux), *not* in the project — which is what keeps the
  `.mvproj` portable while still saving you the re-import every session. A remembered file that
  has since moved is reported in a dismissable strip under the toolbar and simply not loaded.
- **Export video…** renders offscreen at the chosen resolution and pipes frames to ffmpeg.

## Layout

```
src/core/     mvcore — Qt Core only, no GUI. Decode, FFT, analysis, tool discovery.
  AudioBuffer        interleaved f32 PCM; mono windows, peak envelopes
  AudioDecoder       ffmpeg subprocess -> PCM, on a worker thread
  Fft                iterative radix-2, precomputed twiddles and bit-reversal
  SpectrumAnalyzer   Hann window -> log bands -> dB -> time-constant smoothing
src/app/      the Qt Quick application
  AudioEngine        QAudioSink playback, transport, playhead
  SpectrumEngine     worker-thread analysis; writes bands into the Node objects
  Node               the node types: Visual, AudioSource, Spectrum, Separator, ...
  NodePorts          the port table: what each type has, and what each input accepts
  NodeGraph          the document: nodes, edges, validation, the deletion cascade
  RenderList         the flatten: Output's visual chain, as an ordered draw list
  Project            QMetaObject-driven JSON save/load
  Session            what to reopen on launch: last project, last track, volume
  ExportController   offscreen QQuickRenderControl -> ffmpeg stdin
  qml/               Main, VisualizerCanvas
    Theme/Icons/NodeTypes/GraphLayout  singletons: design tokens, icon paths, node
                     presentation, and all of the editor's geometry maths
    graph/           the node editor: canvas, cards, ports, edges
    layers/          one view per visual node type, composited by VisualizerCanvas
    panels/          the docked regions: top bar, graph, canvas stage, properties, transport
    controls/        the widget kit every panel is built from
tests/        ctest suites for the FFT/analyzer, the graph topology, and project round-tripping
```

`mvcore` links only `Qt6::Core`, which keeps the unit tests fast and headless and lets the export
path reuse the analysis code without pulling in the scene graph.

## The graph

The scene is a directed graph, not a stack. Nodes have typed ports; an edge's colour
**is** its type — purple carries a rendered image, orange carries audio.

**Layering is a consequence of topology.** Every visual node has at most one visual
input, so the subgraph reaching the **Output** node is a chain, and walking that
input backwards produces the draw order: upstream is behind, downstream is in front.
`RenderList` does exactly that walk, which is why `VisualizerCanvas.qml` still
consumes an ordered list and is almost unchanged from when there was a stack.

**An audio edge selects a source; it does not carry numbers.** A **Separator** needs
PCM to run an FFT over, not its input's band values, so its audio input names *which*
audio to analyse and `audioRoot()` walks that chain back to the **Audio** node
holding the samples. Two consequences worth knowing:

- Chained separators do **not** cascade. Two in series are two independent analyses
  of the same PCM with their own band counts and smoothing. It is allowed — refusing
  it would be a special case in an otherwise uniform type system — but it is not a
  two-stage filter.
- The Audio node is itself just an analyser, with one band over the full range. So
  "one bar = overall volume" falls out of the type system rather than needing a node
  of its own.

Cycles are the one genuinely new failure mode: nothing in a stack could be cyclic.
`NodeGraph::canLink` rejects them by walking every input port, audio included, and
the flatten carries its own path-set guard anyway because a hand-edited file never
goes through `canLink`.

## The Separator node

"What the spectrum really is" and "what looks good" are different problems. A
faithful FFT readout jitters every frame, slopes steeply off toward the treble, and
never sits still — every bar hovers at a similar mid height and the whole row reads
as busy noise rather than as music.

So all audio conditioning lives on its own **Separator** node, and spectrum nodes
just draw what it hands them. Spectrum nodes carry only appearance. One separator can
feed any number of spectrum nodes, and the FFT runs once per separator rather than
once per thing drawing it.

| Control | What it fixes |
|---|---|
| Band measure (average / peak) | `peak` follows individual partials and is the biggest single source of frame-to-frame jitter; `rms` averages the bins in a band |
| Linear vs log spacing | Log gives equal width per octave (a wide overview); linear gives equal width per Hz. Linear over a narrow low range resolves a bass line's harmonics into separate spikes instead of smearing them into one mound |
| Amplitude: dB vs linear | dB fills the display evenly but lifts the noise between partials as much as the partials themselves, so it reads as one continuous mound. Linear leaves a flat floor with discrete peaks standing on it |
| Window (ms) | Sets frequency resolution: a pure tone spreads over roughly `2 × sampleRate / windowSamples` Hz. The window is **zero-padded** to a power of two rather than rounded to one, so resolution is continuously adjustable — rounding would quantise it into factor-of-two jumps (4096 samples ≈ 23 Hz, 8192 ≈ 12 Hz, nothing between) |
| Windows averaged | A single FFT is a high-variance estimate and the floor between peaks looks like grass. Averaging several half-overlapped windows (Welch) cuts that noise |
| Gain / black level (linear mode) | Gain scales magnitude to 0..1; the black level is subtracted after it and the rest rescaled. Quiet passages carry rumble in every band, so without a black level the display never rests at the baseline and loud passages then have to climb from an already-high floor |
| Curve + gamma (linear mode) | With gamma **above** 1 this is an *expander*: it pushes mid-level bands toward the floor while leaving peaks near full height. That is what produces a low flat body with tall spikes, rather than the whole display rising together when the track gets loud |
| Low-frequency level | Bass dominates any mix; pulling the bottom of the range down spreads the picture across the width instead of piling it at the low end |
| Min frequency | May be **negative**, which insets the lowest audible band from the edge instead of starting there. Bands under the transform's lowest representable frequency all collapse onto the same bin and would draw as one wide slab, so they are reported via `SpectrumFrame::activeFrom` and the bar view leaves them out — giving a clean gap with every drawn bar the same width |
| Smoothing: symmetric vs attack/decay | Attack/decay makes every bar fall at the same rate whatever the music does, which reads as mechanical. Symmetric follows the signal's own shape |
| Treble lift (dB/octave) | Music rolls off with frequency, leaving the outer bars stuck near the floor |
| Per-band auto-gain | References each band to its own slow-moving average, so a quiet band is as animated as a loud one. Referenced to the average and **not** the recent peak — a peak reference pins every band at its own maximum and flattens the dynamics to nothing |
| Floor / ceiling / gate (dB) | Sets how much of the range is used and how empty the gaps between beats read |
| Curve + gamma | The main lever for *punch* — how far peaks stand above the resting level |
| Attack / decay | Rise fast, fall slow |
| Neighbour smoothing | Turns a comb of unrelated spikes into one moving contour |
| Resting level | Keeps idle bars visible |

## The Blur node

The one node that changes pixels rather than arranging them. It carries three
effects, each switchable on its own — a **Gaussian blur**, a **chroma blur**
(Cb/Cr softened, luma left sharp, the look cheap video compression gives) and a
**radial RGB separation** — plus an optional **circle** that scales how much
effect each pixel gets. Invert the circle and the middle of the frame stays
sharp while everything toward the edges fringes and smears, which is what the
node was built for.

**It blurs each upstream layer separately, not the composited group.** That is
the honest caveat and it is not an implementation detail that can be fixed
later without a different design. A transform can be pushed down onto every
member of a group because an affine map distributes over a group; a convolution
does not. So two overlapping layers under one blur give `blur(A) over blur(B)`
rather than `blur(A over B)`. Where it shows:

- Overlapping upstream layers — their *edges against each other* stay crisp,
  because each was blurred before they met.
- A flat **Background** upstream stays looking sharp, since blurring a solid
  colour returns the same solid colour.

Neither is wrong-looking on its own, and the alternative — an offscreen render
target per group — is the same cost `TransformNode`'s header already identifies
as what a group opacity or group blend would need. To smear two layers
*together*, put a blur on each arm before the **Merge** rather than one after it.

The effect sits inside the layer's blend/opacity group and therefore **inside**
any enclosing Transform: a blurred layer in a scaled transform gets its blur and
its mask circle scaled with it, exactly as `add` and `screen` already behave.

Every radius is a **fraction of canvas height**, never a pixel count, so the 4K
export matches the preview. The passes are `layer.enabled` only while their
effect is switched on, so an all-off blur node costs no offscreen buffer at all,
and two blur nodes on one path nest rather than conflict (`BlurChain.qml`
recurses through a `Loader`).

## Node types

| Type | In | Out | Notable properties |
|---|---|---|---|
| `output` | visual | — | none. The root of the render; exactly one exists and it cannot be deleted |
| `audio` | — | audio | none. The track, as one band over the full range. Cannot be deleted |
| `separator` | audio | audio | see above. Draws nothing |
| `spectrum` | visual, audio | visual | width, corner radius, min/max height, linear / radial, **mirror bars** (each bar grows both ways from its base), **arc** (linear only: bows the base line into a dome or a bowl, optionally tapering the bars to fill the fan), **mirror spectrum** (reflects the frequency axis, with bass at the edges or in the centre), solid / gradient / spectrum colour, reactive brightness |
| `text` | visual | visual | text, font, size (relative to canvas height), colour, alignment |
| `background` | — | visual | colour |
| `image` | — | visual | file, fill mode, crop rectangle (normalized to the source) |
| `merge` | visual (background), visual (foreground) | visual | none. Stacks two arms of the graph; port order *is* compositing order. Draws nothing |
| `transform` | visual | visual | offset, scale, rotation, pivot — all applied to everything upstream as one unit. Draws nothing |
| `blur` | visual | visual | Gaussian / chroma-blur / chromatic-separation toggles and amounts, circle mask (centre, radius, soft edge, invert). See above. Draws nothing |

Nodes that draw also have position, size, rotation, opacity, visibility and blend mode. Geometry is
normalized 0..1, so a project renders identically at preview size and at 4K. Nodes that do **not**
draw have none of that — a separator has no position, and the inspector shows it none.

**Horizontal fill** is the spectrum node's `sizeW` ("Width" in the panel). With a negative `minHz` the
unresolvable bands leave a gap at each edge, and `sizeW` scales the whole row into or past it —
measured on the bundled bass preset: 0.7 fills 55% of the frame, 1.0 fills 78%, 1.28 reaches edge
to edge.

**Blend modes.** Qt Quick has no per-item blend mode. `add` and `screen` work by rendering the node
to a texture and running a `ShaderEffect` that exploits premultiplied source-over compositing:
`dst = src.rgb + (1 - src.a) * dst`, so forcing `src.a` to 0 yields exact addition, and setting it to
the channel maximum approximates Screen. True `multiply` needs the destination as a shader input,
which the scene graph does not expose, so it is not offered.

## Four Qt details worth knowing before editing this code

**`QQuickWindow::grabWindow()` returns a null image** when the window is backed by a
`QQuickRenderControl` (and Qt 5's `QQuickRenderControl::grab()` is gone). Export therefore renders
into an explicit `QRhiTexture` render target and reads it back — see `ExportController::buildScene`.
That needs `#include <rhi/qrhi.h>` and linking `Qt6::GuiPrivate`.

**A `Loader` always overwrites its item's `width` and `height`.** Any size binding on the loaded
item's root is silently clobbered. `LayerFrame.qml`'s root therefore spans the whole canvas and
places the layer in an inner box, which is also why `frame.width`/`frame.height` mean the canvas.

(Related: the QML property name `layer` collides with `QQuickItem`'s FINAL `layer` property — hence
`layerData` and the `layerObject` model role.)

**`mapToItem` is not binding-reactive.** QML cannot track an `x`/`y` dependency through it, so an
edge endpoint computed that way is correct on first paint and then never follows the node again —
silently, with no warning. Every endpoint in the node editor is therefore plain arithmetic over
`graphX`/`graphY`, which is the entire reason `GraphLayout.qml` exists as a file. `mapToItem` is
fine inside an event handler, where it runs once instead of being re-evaluated.

(Related: **do not use `drag.target`** to move a node card. It assigns `card.x` directly and
permanently destroys the `x: node.graphX` binding — the node moves on screen, nothing reaches the
model, and it snaps back on reload.)

**Analyzer parameters are re-sent every frame.** `SpectrumAnalyzer::setParams` must
therefore distinguish "buffers need rebuilding" from "a value changed". Rebuilding
unconditionally clears the smoothing envelope and the auto-gain reference on every
frame, which silently degrades attack/decay from a filter into a plain gain and stops
auto-gain ever converging — with no error and a plausible-looking picture.
`tst_fft::analyzerKeepsStateWhenParamsAreResent` guards this.

## Status

Implemented: audio import and playback (mp3/wav/flac/aac/m4a/ogg), live FFT preview, the node graph
editor (pan, drag, drag-to-connect with type checking and cycle rejection, add and delete), the
spectrum node (linear or radial, mirrored and arced), background, text and image nodes, the merge
and transform nodes, the blur node (Gaussian, chroma blur and chromatic separation, with a circle
mask), visibility, `add`/`screen` blend modes, live property editing, project save/load, a headless
`--render` mode, and export to MP4 (H.264) and MOV (ProRes 422 HQ) with resolution and frame-rate
settings, optional audio, progress, ETA and cancellation.

Not implemented: `multiply` blending (see above), video backgrounds, zoom in the graph editor, and
any effect that needs a group render target rather than a per-layer pass — the blur node documents
where that line falls.

Removed in the move to a graph: the particle layer, and the background layer's still image. Both
are recoverable from git when they are revisited as nodes of their own.

Verified end to end: duration matches `ffprobe` within one frame across all six input formats;
export frame 0 is pixel-identical in content to the preview at the same timestamp; cancelling
mid-render leaves no partial file.
