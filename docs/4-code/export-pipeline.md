# 4 · The export pipeline

**Sources:** `src/app/ExportController.h`, `src/app/ExportController.cpp`, `src/app/qml/panels/ExportDialog.qml`, `src/app/SpectrumEngine.cpp`

Renders the visualizer offscreen and pipes raw RGBA frames to ffmpeg's stdin.

---

## The guarantee

The offscreen window loads **the same `VisualizerCanvas.qml`** the preview uses and binds
it to **the same `RenderList`** over **the same `Node` objects**. The export cannot drift
from what the user saw, because there is only one render path.

Only the pacing differs: frames are produced as fast as the GPU and the pipe allow rather
than in real time. Two mechanisms keep that from changing the picture:

- **Time constants, not per-frame factors**, in the analyzer.
  → [ADR-0010](../decisions/0010-smoothing-is-a-time-constant.md)
- **A fixed-step `QAnimationDriver`**, so QML animations advance exactly `1/fps` per
  rendered frame. → [ADR-0011](../decisions/0011-fixed-step-animation-driver.md)

---

## Settings

| Property | Default | Notes |
|---|---|---|
| `width` / `height` | 1920 × 1080 | Seeded from the Project canvas by `setContext`. Rounded **down to even** at `start()` |
| `fps` | 30 | Clamped 1…240 at `start()` |
| `includeAudio` | `true` | |
| `videoCodec` | `"h264"` | or `"prores"` — ProRes is only valid in a `.mov` container |
| `quality` | 18 | x264 CRF, lower is better. **Ignored for ProRes** |

Read-only: `exporting`, `progress`, `framesDone`, `totalFrames`, `etaSeconds`,
`statusText`, `lastError`. Note `framesDone`, `totalFrames` and `etaSeconds` all notify
on `progressChanged` rather than on their own signals.

> `start()` mutates `width`/`height`/`fps` in place **without emitting
> `settingsChanged`**, so an odd width typed into the dialog is silently rounded for the
> render while the spin box keeps showing the odd number.

---

## `start()` — preconditions and setup

`ExportController.cpp:311`:

1. Validate wiring, `audio->hasTrack()`, ffmpeg availability, output path.
2. Round `w`/`h` to even; clamp `fps`.
3. `totalFrames = ceil(duration × fps)`.
4. **Stop the spectrum tick and pause playback** — both would fight the export for the
   layer band values, since all three write the same `Node` objects.
   `m_spectrumWasRunning` is remembered and restored in `finish()`.
5. `resetExportState()` — the export analyzers start from a clean envelope.
6. `buildScene()`, `startProcess()`, install the animation driver.
7. `m_frameTimer.start()` — **interval 0, repeating**.

---

## The offscreen scene

`buildScene()`, `ExportController.cpp:127-205`:

1. `QQuickRenderControl` + `QQuickWindow(renderControl)`, geometry `0,0,w,h`, background
   `project->canvasColor()`.
2. `QQmlComponent(m_qmlEngine, "qrc:/qt/qml/krave/VisualizerCanvas.qml")` created **in the
   engine's root context**, so every context property resolves.
3. `rootItem->setProperty("layers", m_graph->renderList())` — the same model.
4. `setParentItem(window->contentItem())` plus **explicit `setWidth`/`setHeight`**;
   offscreen windows do not lay out children automatically.
5. `renderControl->initialize()` → `m_rhi = renderControl->rhi()`.
6. **`QRhiTexture` RGBA8, 1 sample, `RenderTarget | UsedAsTransferSource`.**
7. `QRhiRenderBuffer(DepthStencil)`, `QRhiTextureRenderTarget`, and a compatible render
   pass descriptor.
8. `window->setRenderTarget(QQuickRenderTarget::fromRhiRenderTarget(...))`.

### Why the explicit texture

> "`QQuickWindow::grabWindow()` returns a null image for a render-control-backed window,
> so the export renders into an explicit texture and reads that back."
> — `ExportController.cpp:177-178`

Qt 5's `QQuickRenderControl::grab()` is gone. This is also the entire reason
`Qt6::GuiPrivate` is a hard link dependency: `QRhi` lives behind private Gui headers, and
the file needs `#include <rhi/qrhi.h>`.

**Anyone "simplifying" this back to `grabWindow()` gets null images.**

### Teardown

`renderControl->invalidate()` **first**, then the RHI resources in reverse (target → pass
→ depth → texture), then the root item, the window, and the render control.

---

## The frame loop

`renderNextFrame()` runs on a **zero-interval repeating timer** — one frame per
event-loop pass, so the editor UI keeps repainting and progress keeps updating.

Order per frame (`ExportController.cpp:380-454`):

1. Done check.
2. **Backpressure check** (below).
3. `process->state() != Running` → fail with ffmpeg's stderr.
4. `t = frameIndex / fps`, `dt = 1 / fps`.
5. `m_spectrum->renderFrameSync(t, dt)` — runs the export worker **synchronously on the
   GUI thread**, writing the same `Node` objects.
6. `animationDriver->step()`.
7. `polishItems()` → `beginFrame()` → `sync()` → `render()`.
8. `QRhiReadbackResult`; `batch->readBackTexture(m_texture)`.
9. **`endFrame()`** — "ends the offscreen frame and waits for GPU completion, so the
   readback data is populated by the time it returns".
10. Empty-readback check.
11. Optional vertical flip if `rhi->isYUpInFramebuffer()` (allocates a fresh buffer per
    frame; needed on OpenGL, not on D3D11).
12. `process->write(rgba)`.
13. `++m_frameIndex`, ETA, `progressChanged`.

### Backpressure

Rendering runs at roughly 280 fps; the encoder does not. Without throttling, queued
frames — 8 MB each at 1080p — would balloon memory.

A two-level hysteresis around `kWriteHighWaterMark = 32 MiB` ("roughly four 1080p
frames"):

| Event | Condition | Action |
|---|---|---|
| **Pause** | `process->bytesToWrite() >= 32 MiB` | `m_paused = true`; stop the timer |
| **Resume** | in `bytesWritten`, `bytesToWrite() < 16 MiB` | restart the timer |

> **Failure mode.** If ffmpeg wedges past the high-water mark, only `bytesWritten` ever
> clears `m_paused`. There is no watchdog, and the `state() != Running` check at step 3
> never runs because the timer is stopped — so the symptom is a frozen progress bar with
> no error until the user cancels. Flagged in
> [Known discrepancies](../README.md#known-discrepancies).

### The animation driver

```cpp
class FixedStepAnimationDriver : public QAnimationDriver {
    double m_stepMs = 1000.0 / max(1.0, fps);
    double m_elapsedMs = 0;
    void step() { m_elapsedMs += m_stepMs; advance(); }
    qint64 elapsed() const override { return qint64(m_elapsedMs); }
};
```

Installed for the duration of the export and **uninstalled first thing in `finish()`**,
"so the editor's animations resume normally even if the teardown below runs into
trouble".

---

## ffmpeg arguments

`buildFfmpegArgs()`, in order:

```
-y -hide_banner -loglevel error
-f rawvideo -pixel_format rgba -video_size {W}x{H} -framerate {fps} -i -
[withAudio]        -i {audio->trackPath()}
[prores]           -c:v prores_ks -profile:v 3 -pix_fmt yuv422p10le
[h264]             -c:v libx264 -preset medium -crf {quality} -pix_fmt yuv420p
[withAudio+prores] -c:a pcm_s16le
[withAudio+h264]   -c:a aac -b:a 320k
[withAudio]        -shortest
[else]             -an
[!prores]          -movflags +faststart
{outputPath}
```

Two choices are commented in place:

- `-pix_fmt yuv420p` because "some players and most hardware decoders will not touch
  4:4:4 H.264".
- `-shortest` because "frame count and audio length can differ by a fraction of a frame"
  — `totalFrames = ceil(duration × fps)`, so the last frame can be past the end of the
  track. That is safe only because `AudioBuffer::readMono` zero-pads.

> **The muxed audio is the original file, decoded a second time by ffmpeg** — not the
> 48 kHz f32 PCM that drove the analysis. Any A/V sync argument has to account for that.

`startProcess()` uses `SeparateChannels`, connects `bytesWritten` (the resume hook) and
`errorOccurred`, then `start()` + `waitForStarted(10000)`.

---

## Progress, cancellation, finish

- `progress()` = `frameIndex / totalFrames`, 0.0 when total ≤ 0.
- **ETA** is only computed once `m_clock.elapsed() > 0.25 s`:
  `perFrame × (totalFrames - frameIndex)`. Initialised to `-1.0`, which the dialog
  formats specially.
- `statusText`: `"Preparing…"` → `"Rendering…"` → `"Exported {filename}"` or the error.
- `cancel()` sets `lastError = "Export cancelled."` and calls `finish(false)`. The
  destructor cancels if still exporting.

`finish(bool success)`, in order:

1. Re-entrancy guard — `if (!m_exporting) return;` (matters because `fail()` calls
   `finish(false)` from inside `renderNextFrame()`).
2. Stop the timer, clear `m_paused`, **uninstall the animation driver**.
3. If the process is running: on success `closeWriteChannel()` — "close stdin so ffmpeg
   flushes and finalizes the container" — then `waitForFinished(120000)`, killing on
   timeout. On failure, `kill()` + `waitForFinished(2000)`.
4. Read stderr; downgrade to failure on a non-zero exit.
5. `deleteLater()` the process; `teardownScene()`.
6. **On failure, `QFile::remove(outputPath)`** — "never leave a half-written file behind
   pretending to be a finished export".
7. Clear `exporting`; set status; **restart the spectrum tick if it was running**; emit
   `finished(success, path)`.

---

## Two live scene graphs over one model

The export canvas and the preview canvas iterate the **same** `RenderList`. A topology
change would `beginResetModel` a model two `Repeater`s are walking.

Guarded twice: the export dialog is modal, **and** `Main.qml:91` sets
`enabled: !exportController.exporting` on the graph panel.

> If you add any other path that can mutate graph topology — a global shortcut, an undo
> stack, a file watcher — it must be guarded the same way.

---

## Testing

**There is none.** `ExportController` lives in the executable rather than in `mvapp`, so
no test target links it. The headless `--render --cancel-after N` mode that once provided
regression coverage was deleted in `5fa7fa8` along with all CLI parsing — see
[Known discrepancies](../README.md#known-discrepancies).

What is verified: `tst_fft::analyzerSmoothingIsFrameRateIndependent` pins the DSP half of
preview/export parity. The rest rests on the README's manual claim that "export frame 0
is pixel-identical in content to the preview at the same timestamp".

---

## See also

- [3 · export](../3-components/export.md) — the same pipeline at component scale
- [dsp-pipeline](dsp-pipeline.md) — why `dt` matters
- [qt-quirks](qt-quirks.md) — `grabWindow` and the other Qt constraints
