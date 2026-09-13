# Krave

A desktop music visualizer in the spirit of Avee Player: import a track, wire up a
frequency-reactive visualizer as a **node graph**, preview it live against the audio, save
the configuration as a reusable project, and export a video with the visualizer burned in.

C++17 core, Qt Quick / QML front end, ffmpeg for decode and encode.

## The idea it is built on

The whole decoded track lives in memory and the playhead position is always known, so
**spectrum analysis is a pure function of `(pcmBuffer, timeSeconds)`** rather than a tap on
a live audio stream. Everything else follows from that:

- Preview asks for the spectrum at the playback cursor, 60 times a second.
- Export asks for the spectrum at `frameIndex / fps`, as fast as the GPU allows.
- Both render **the same `VisualizerCanvas.qml`** bound to **the same `Node` objects**, so
  the exported video cannot drift from what you previewed.

The scene is a **directed graph, not a stack**. Nodes have typed ports; an edge's colour
*is* its type — purple carries a rendered image, orange carries audio. Layering is a
consequence of topology: every visual node has at most one visual input, so the subgraph
reaching the **Output** node is a chain, and walking it backwards produces the draw order.

## Documentation

Full documentation lives in **[`docs/`](docs/README.md)**, organised on the
[C4 model](https://c4model.com/) — four levels of zoom, so you can enter at the scope that
matches what you are doing.

| I want to… | Go to |
|---|---|
| Understand what this is | [1 · Context](docs/1-context/README.md) |
| See how it is put together | [2 · Containers](docs/2-containers/README.md) |
| Work on a subsystem | [3 · Components](docs/3-components/) |
| Look up an exact value | [4 · Code](docs/4-code/) |
| Add a node type | [guides/add-a-node-type](docs/guides/add-a-node-type.md) |
| Know *why* something is so | [decisions/](docs/decisions/README.md) — 22 ADRs |
| Avoid the traps | [4 · qt-quirks](docs/4-code/qt-quirks.md) |

## Requirements

| | |
|---|---|
| Qt | 6.8+ with **Quick, QuickControls2, Multimedia, Concurrent, ShaderTools** and private Gui headers |
| Compiler | MSVC 2022+ (verified on Build Tools 2026, toolset 14.51) or GCC 13+ / Clang 17+ |
| CMake | 3.21+, Ninja |
| ffmpeg | on `PATH`, or set `MV_FFMPEG` to its full path |

ffmpeg is required for both import and export — it is the decoder as well as the encoder.

## Build

### Linux / Nix

The flake pins the whole toolchain, so nothing needs to be installed globally.

```bash
nix build           # -> ./result/bin/krave, ffmpeg baked onto its PATH
nix run             # build and launch
```

For day-to-day work there is a build script, which re-execs itself inside `nix develop`
when cmake is not on `PATH`:

```bash
./scripts/build.sh                  # debug build
./scripts/build.sh --config release
./scripts/build.sh --test           # build then run ctest
./scripts/build.sh --run            # build then launch
./scripts/build.sh --clean
```

Or drive the presets by hand from inside the shell:

```bash
nix develop
cmake --preset linux-debug && cmake --build --preset linux-debug && ctest --preset linux-debug
./build/linux-debug/bin/krave
```

An in-tree binary must be launched **from inside the shell** — it is not wrapped, so it
finds Qt's plugins and QML modules only through the environment the shell exports. The
`nix build` output is wrapped and runs anywhere.

### Windows

`cmake` and `ninja` ship inside the VS Build Tools and are usually not on `PATH`, and Ninja
needs the MSVC environment. The build script handles all of that:

```bash
pwsh ./scripts/build.ps1
pwsh ./scripts/build.ps1 -Config Release -Test
```

Full build notes, VS Code / VSCodium setup and the preset table:
**[guides/build-and-run](docs/guides/build-and-run.md)**.

## Using it

```bash
./build/linux-debug/bin/krave        # build/Debug/bin/krave.exe on Windows
```

- **Import audio** (`Ctrl+I`) loads mp3/wav/flac/aac/m4a/ogg. A waveform, a time ruler and
  a draggable playhead fill the transport dock along the bottom; scrubbing seeks audio and
  visuals together.
- The **node graph** (left) is where the scene is built. Drag from an output to an input to
  connect. Middle-drag pans, including over nodes. Double-click to rename, `Del` to delete.
- The **property panel** (right) binds directly to the selected node's properties, so every
  edit shows on the canvas the same frame. There is no apply step.
- The **canvas** letterboxes the project's aspect ratio and toggles between Fit and 100%.
  Composition guides are an editor overlay only — they never reach an exported frame.
- **Save / Save as…** writes a `.mvproj` (format version 3). It stores the nodes, their
  properties, their positions and their connections, plus the canvas settings — and
  deliberately **no audio path**, so projects stay portable as reusable templates. Older
  files are rejected; `tools/migrate_v2_to_v3.py` converts them.
- The editor **reopens where you left it** — last project, last track, volume. That state
  lives in `session.json` under the platform config directory, *not* in the project.
- **Export video…** (`Ctrl+E`) renders offscreen at the chosen resolution and pipes frames
  to ffmpeg.

Shortcuts: `Space` play/pause, `Ctrl+N/O/S/Shift+S` document, `Ctrl+I` import, `Ctrl+E`
export, `Del` delete the selected node.

> **There are no command-line flags.** `main.cpp` does no argument parsing.

## Layout

```
src/core/     mvcore — Qt Core only, no GUI. Decode, FFT, analysis, tool discovery.
src/app/      the document model (mvapp) + the engines + the Qt Quick application
  qml/        Main, VisualizerCanvas, singletons, graph/, layers/, panels/, controls/
  shaders/    the four fragment shaders
tests/        ctest suites: FFT/analyzer, graph topology, project round-tripping, session
docs/         the C4 documentation tree
tools/        migrate_v2_to_v3.py
```

See [2 · Containers](docs/2-containers/README.md) for what links what and why.

## Status

**Implemented:** audio import and playback (mp3/wav/flac/aac/m4a/ogg), live FFT preview,
the node graph editor (pan, drag, drag-to-connect with type checking and cycle rejection,
add and delete), the spectrum node (linear or radial, mirrored and arced), background, text
and image nodes, the merge and transform nodes, the blur node (Gaussian, chroma blur and
chromatic separation, with a circle mask), visibility, `add`/`screen` blend modes, live
property editing, project save/load, and export to MP4 (H.264) and MOV (ProRes 422 HQ) with
resolution and frame-rate settings, optional audio, progress, ETA and cancellation.

**Not implemented:** `multiply` blending, video backgrounds, zoom in the graph editor, any
command-line interface, and any effect that needs a group render target rather than a
per-layer pass. Each is explained in [decisions/](docs/decisions/README.md).

**Removed in the move to a graph:** the particle layer, and the background layer's still
image. Both are recoverable from git; the still image's role now belongs to an Image node.

**Verified end to end:** duration matches `ffprobe` within one frame across all six input
formats; export frame 0 is pixel-identical in content to the preview at the same timestamp;
cancelling mid-render leaves no partial file.
