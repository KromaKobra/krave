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

One rule keeps that promise honest: smoothing and decay are stored as **time constants**, and the
blend factor is derived per frame as `1 - exp(-dt / tau)`. A per-frame factor would make a 30 fps
export decay at half the rate of a 60 Hz preview. `tst_fft` asserts this directly — decaying for
0.5 s at 30 fps and at 60 fps must land within 0.02 of each other.

## Requirements

| | |
|---|---|
| Qt | 6.8+ with **Quick, QuickControls2, Multimedia, Concurrent** and private Gui headers |
| Compiler | MSVC 2022+ (verified on Build Tools 2026, toolset 14.51) |
| CMake | 3.21+, Ninja |
| ffmpeg | on `PATH`, or set `MV_FFMPEG` to its full path |

ffmpeg is required for both import and export — it is the decoder as well as the encoder.

## Build

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
./build/Debug/bin/musicvisualizer.exe --audio track.mp3
```

- **Import audio…** loads mp3/wav/flac/aac/m4a/ogg. The waveform and a draggable playhead appear
  along the bottom; scrubbing seeks audio and visuals together.
- The **layer stack** (top right) adds, reorders, hides and deletes layers. Index order is the
  z-order, drawn back to front.
- The **property panel** binds directly to the selected layer's `Q_PROPERTY`s, so every edit shows
  on the canvas the same frame. There is no apply step.
- **Save / Save as…** writes a `.mvproj` file. It stores layers, their properties and the canvas
  settings — and deliberately **no audio path**, so projects stay portable as reusable templates.
- **Export video…** renders offscreen at the chosen resolution and pipes frames to ffmpeg.

### Rendering from the command line

The same pipeline, no window — useful for batch renders:

```bash
./build/Debug/bin/musicvisualizer.exe --render --audio track.mp3 --out out.mp4 --project look.mvproj --fps 60
```

Options: `--width`, `--height`, `--fps`, `--codec h264|prores`, `--no-audio`, `--project`.

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
  qml/               Main, VisualizerCanvas, layers/, panels/, controls/
tests/        ctest suites for the FFT/analyzer and for project round-tripping
```

`mvcore` links only `Qt6::Core`, which keeps the unit tests fast and headless and lets the export
path reuse the analysis code without pulling in the scene graph.

## Two Qt details worth knowing before editing this code

**`QQuickWindow::grabWindow()` returns a null image** when the window is backed by a
`QQuickRenderControl` (and Qt 5's `QQuickRenderControl::grab()` is gone). Export therefore renders
into an explicit `QRhiTexture` render target and reads it back — see `ExportController::buildScene`.
That needs `#include <rhi/qrhi.h>` and linking `Qt6::GuiPrivate`.

**A `Loader` always overwrites its item's `width` and `height`.** Any size binding on the loaded
item's root is silently clobbered. `LayerFrame.qml`'s root therefore spans the whole canvas and
places the layer in an inner box, which is also why `frame.width`/`frame.height` mean the canvas.

(Related: QML property names `layer` and `content` collide with `QQuickItem`'s FINAL `layer`
property — hence `layerData` and the `layerObject` model role.)

## Status

Working: audio import and playback, live FFT preview, EQ bar layer (linear / mirrored / radial),
background and text layers, the layer stack, live property editing, project save/load, and export
to MP4 (H.264) and MOV (ProRes 422 HQ) with progress, ETA and cancellation.

Not yet implemented: blend modes other than `normal` (Qt Quick has no per-item blend mode, so
Add/Screen/Multiply need a `ShaderEffect` pass), and particle layers.
