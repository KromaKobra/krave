# ADR-0016 — Bands that collapse onto one bin are flagged, not drawn

**Status:** Accepted · **Source:** commit `0ab8710`, `src/core/SpectrumAnalyzer.h:173-180` · **Pinned by:** `unresolvableLowBandsAreFlagged`

## Context

`minHz` is allowed to be **negative**. That sounds like nonsense until you see why: the
reference visualizer maps its outer edge to about −72 Hz, so its lowest *audible* band sits
inset from the display edge with a flat margin outside it.

Allowing it produced a visible artefact — a slab at each edge of the display. Measuring run
lengths in a rendered frame showed "a 70 px run at a single identical height where every
bar is 10 px. Seven bands were reporting the same value."

The cause: bands below the transform's lowest representable frequency are all clamped onto
bin 1, so they carry identical values and merge visually into one wide block.

## Decision

Report the boundary rather than hiding it. `SpectrumFrame::activeFrom` is the index of the
first band that maps to a **distinct** FFT bin; `SpectrumNode::inactiveEdgeBars` exposes it
to QML, and the bar view skips those bands in both the linear and radial paths.

## Consequences

> "They are reported here so a view can leave them out and show a gap instead, keeping
> every drawn bar the same width."

The space they occupied becomes a clean gap, and **every drawn bar stays the same width** —
which is the actual goal. Verified on a rendered frame: 70 px empty at each edge, all drawn
bars 10 px.

The mirrored case needs handling too: with `mirrorSpectrum`, the dead bands appear at
**both** edges.

The commit also settled a related question — no new property was needed to control how wide
the row is:

> "No new scale property was needed — the bar layer's existing `sizeW` ('Width' in the
> panel) already controls horizontal fill. Measured on the bass preset: 0.7 fills 55% of the
> frame, 1.0 fills 78%, 1.28 reaches edge to edge."

Two structural details this rests on:
- Band bin edges are clamped to `[1, bins-1]`, so **bin 0 (DC) is never used** — and that
  clamp is exactly what causes the collapse.
- The logarithmic placement term is guarded against negative `minHz`, since `log` is
  undefined there.

## Revisiting it

The alternative — clamping `minHz` to ≥ 1 Hz — was the original behaviour and it removes the
inset-margin look entirely. That was tried and rejected against the reference.

The other alternative is to have the view *merge* the duplicate bands into one wide bar
rather than dropping them. That is the artefact this record exists to remove.

If a view ever ignores `inactiveEdgeBars`, the slab comes straight back, silently.
