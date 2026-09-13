# ADR-0011 — Export installs a fixed-step animation driver

**Status:** Accepted · **Source:** commit `e8749d7`, `src/app/ExportController.cpp:35-62` · **Pinned by:** **none** — see below

## Context

QML animations — `Behavior`s, transitions, `NumberAnimation`, anything a user adds to a
layer — advance against Qt's animation driver, which follows the **wall clock**.

Export does not run in real time. It renders as fast as the GPU and the pipe allow,
commonly several times faster than playback.

So a two-second render of a twelve-second animation plays the whole twelve seconds of
motion during those two seconds, and the exported video shows animation running wildly
fast relative to the audio.

## Decision

Install a `QAnimationDriver` subclass for the duration of the export that advances
**exactly `1/fps` per rendered frame**, regardless of wall-clock time:

```cpp
class FixedStepAnimationDriver : public QAnimationDriver {
    double m_stepMs = 1000.0 / max(1.0, fps);
    double m_elapsedMs = 0;
    void step() { m_elapsedMs += m_stepMs; advance(); }
    qint64 elapsed() const override { return qint64(m_elapsedMs); }
};
```

`renderNextFrame()` calls `step()` once per frame, between the analysis and the render.

## Consequences

> "Installing this decouples every QML animation from wall-clock time for the duration of
> the export. Rendering runs several times faster than real time, so without it a particle
> system would burn through twelve seconds of motion in the two seconds the render
> actually took."

This is the **rendering half** of preview/export parity; the DSP half is
[ADR-0010](0010-smoothing-is-a-time-constant.md).

It is installed in `start()` and **uninstalled first thing in `finish()`** — before the
process teardown — "so the editor's animations resume normally even if the teardown below
runs into trouble". An export that fails must not leave the editor's animations frozen.

The driver is global while installed, so the **editor's own** animations are also stepped
at `1/fps` during an export. That is invisible in practice because the export dialog is
modal.

Note the decision predates the node graph: it arrived with the particle layer, which has
since been removed. The driver survives because the reasoning never depended on particles
— it applies to any QML animation, including ones a user adds today.

## Revisiting it

**There is no test.** The original commit verified it by measurement — "mean luma at the
same timestamp differs by 0.6% between a 30 fps and a 60 fps render of the same project" —
but that was a manual check, and the `--render` headless mode that made such checks
scriptable was deleted in `5fa7fa8`.

A pinning test would render a project containing an animation at 30 and at 60 fps and
compare frames at equal timestamps. It needs a headless render entry point to exist first.
That is the real backlog item here.
