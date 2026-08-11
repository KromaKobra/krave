# Music Visualizer

A desktop music visualizer in the spirit of Avee Player: import a track, build a layered
frequency-reactive visualizer, preview it live against the audio, save the configuration as a
reusable project, and export a video with the visualizer burned in.

C++17 core, Qt Quick / QML front end, ffmpeg for decode and encode.

## The idea it is built on

The whole decoded track lives in memory and the playhead position is always known, so **spectrum
analysis is a pure function of `(pcmBuffer, timeSeconds)`** rather than a tap on a live audio
stream. Everything else follows from that:

- Preview asks for the spectrum at the playback cursor, 60 times a second.
- Export asks for the spectrum at `frameIndex / fps`, as fast as the GPU allows.
- Both render **the same `VisualizerCanvas.qml`** bound to **the same `Layer` objects**, so the
  exported video cannot drift from what you previewed.

Two rules keep that promise honest:

- Smoothing and decay are stored as **time constants**, with the blend factor derived per frame as
  `1 - exp(-dt / tau)`. A per-frame factor would make a 30 fps export decay at half the rate of a
  60 Hz preview. `tst_fft` asserts this directly — decaying for 0.5 s at 30 fps and at 60 fps must
  land within 0.02 of each other.
- Export installs a **fixed-step `QAnimationDriver`**, so every QML animation (particle systems
  included) advances exactly 1/fps per rendered frame. Rendering runs several times faster than
  real time, so on the wall clock a particle system would burn through twelve seconds of motion in
  the two seconds the render actually took.

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
- The **layer stack** (left) adds, reorders, renames (double-click), hides and deletes layers.
  Index order is the z-order, drawn back to front.
- The **property panel** (right) binds directly to the selected layer's `Q_PROPERTY`s, so every
  edit shows on the canvas the same frame. There is no apply step. Every slider is paired with a
  number field for exact values.
- The **canvas** letterboxes the project's aspect ratio, and toggles between Fit and 100%.
  Composition guides (thirds plus a title-safe box) are an editor overlay only — they are drawn
  outside the canvas item, so they never reach an exported frame.
- Shortcuts: `Space` play/pause, `Ctrl+N/O/S/Shift+S` document, `Ctrl+I` import, `Ctrl+E` export,
  `Del` delete the selected layer.
- **Save / Save as…** writes a `.mvproj` file. It stores layers, their properties and the canvas
  settings — and deliberately **no audio path**, so projects stay portable as reusable templates.
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
  SpectrumEngine     worker-thread analysis; writes bands into the Layer objects
  Layer/LayerModel   layer types and the ordered stack
  Project            QMetaObject-driven JSON save/load
  ExportController   offscreen QQuickRenderControl -> ffmpeg stdin
  qml/               Main, VisualizerCanvas
    Theme/Icons/LayerTypes  singletons: design tokens, the icon paths, layer metadata
    layers/          one view per layer type, composited by VisualizerCanvas
    panels/          the docked regions: top bar, layer list, canvas stage, properties, transport
    controls/        the widget kit every panel is built from
tests/        ctest suites for the FFT/analyzer and for project round-tripping
```

`mvcore` links only `Qt6::Core`, which keeps the unit tests fast and headless and lets the export
path reuse the analysis code without pulling in the scene graph.

## The Audio provider layer

"What the spectrum really is" and "what looks good" are different problems. A
faithful FFT readout jitters every frame, slopes steeply off toward the treble, and
never sits still — every bar hovers at a similar mid height and the whole row reads
as busy noise rather than as music.

So all audio conditioning lives on its own **Audio provider** layer, and bar layers
just draw what it hands them. Bar layers carry only appearance. One provider can
feed any number of bar layers, and the FFT runs once per provider rather than once
per thing drawing it.

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

## Layer types

| Type | Reacts to audio | Notable properties |
|---|---|---|
| `audio` | provides the data | see above. Draws nothing |
| `bars` | fed by a provider | width, corner radius, min/max height, linear / radial, **mirror bars** (each bar grows both ways from its base), **arc** (linear only: bows the base line into a dome or a bowl, optionally tapering the bars to fill the fan), **mirror spectrum** (reflects the frequency axis, with bass at the edges or in the centre), solid / gradient / spectrum colour, reactive brightness |
| `particles` | yes, own analyzer | driving band (bass/mid/treble), idle and reactive emission rates, colour, size, lifetime, speed, direction, spread, gravity |
| `text` | no | text, font, size (relative to canvas height), colour, alignment |
| `background` | no | colour, image, fill mode |

Every layer also has position, size, rotation, opacity, visibility and blend mode. Geometry is
normalized 0..1, so a project renders identically at preview size and at 4K.

**Horizontal fill** is the bar layer's `sizeW` ("Width" in the panel). With a negative `minHz` the
unresolvable bands leave a gap at each edge, and `sizeW` scales the whole row into or past it —
measured on the bundled bass preset: 0.7 fills 55% of the frame, 1.0 fills 78%, 1.28 reaches edge
to edge.

**Blend modes.** Qt Quick has no per-item blend mode. `add` and `screen` work by rendering the layer
to a texture and running a `ShaderEffect` that exploits premultiplied source-over compositing:
`dst = src.rgb + (1 - src.a) * dst`, so forcing `src.a` to 0 yields exact addition, and setting it to
the channel maximum approximates Screen. True `multiply` needs the destination as a shader input,
which the scene graph does not expose, so it is not offered.

## Three Qt details worth knowing before editing this code

**`QQuickWindow::grabWindow()` returns a null image** when the window is backed by a
`QQuickRenderControl` (and Qt 5's `QQuickRenderControl::grab()` is gone). Export therefore renders
into an explicit `QRhiTexture` render target and reads it back — see `ExportController::buildScene`.
That needs `#include <rhi/qrhi.h>` and linking `Qt6::GuiPrivate`.

**A `Loader` always overwrites its item's `width` and `height`.** Any size binding on the loaded
item's root is silently clobbered. `LayerFrame.qml`'s root therefore spans the whole canvas and
places the layer in an inner box, which is also why `frame.width`/`frame.height` mean the canvas.

(Related: the QML property name `layer` collides with `QQuickItem`'s FINAL `layer` property — hence
`layerData` and the `layerObject` model role.)

**Analyzer parameters are re-sent every frame.** `SpectrumAnalyzer::setParams` must
therefore distinguish "buffers need rebuilding" from "a value changed". Rebuilding
unconditionally clears the smoothing envelope and the auto-gain reference on every
frame, which silently degrades attack/decay from a filter into a plain gain and stops
auto-gain ever converging — with no error and a plausible-looking picture.
`tst_fft::analyzerKeepsStateWhenParamsAreResent` guards this.

**`ImageParticle`'s built-in sprite does not load** in this Qt build. Particles simulate correctly
but draw nothing, with no warning. The app therefore ships `assets/glowdot.png` and sets `source:`
explicitly. If particles ever vanish again, check that first — the system itself is rarely at fault.

## Status

Implemented: audio import and playback (mp3/wav/flac/aac/m4a/ogg), live FFT preview, EQ bar layer
(linear or radial, mirrored and arced), audio-reactive particle layer, background and text layers, the layer
stack with reordering and visibility, `add`/`screen` blend modes, live property editing, project
save/load, a headless `--render` mode, and export to MP4 (H.264) and MOV (ProRes 422 HQ) with
resolution and frame-rate settings, optional audio, progress, ETA and cancellation.

Not implemented: `multiply` blending (see above), and video backgrounds — the background layer takes
colours and still images only.

Verified end to end: duration matches `ffprobe` within one frame across all six input formats;
export frame 0 is pixel-identical in content to the preview at the same timestamp; particle
simulation reaches the same state at a given timestamp whether rendered at 30 or 60 fps; cancelling
mid-render leaves no partial file.
