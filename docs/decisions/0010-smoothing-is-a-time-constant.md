# ADR-0010 — Smoothing is a time constant, never a per-frame factor

**Status:** Accepted · **Source:** `src/core/SpectrumAnalyzer.h:152-154` · **Pinned by:** `analyzerSmoothingIsFrameRateIndependent`

## Context

Bars rise and fall with a smoothing envelope. The conventional implementation is a
per-frame blend factor:

```
smoothed += (target - smoothed) * 0.2      // 0.2 per frame
```

That is simple, and it is wrong here — because **preview and export run at different frame
rates**. Preview ticks at 60 Hz; export renders at whatever `fps` the user chose, commonly
30.

A fixed per-frame factor makes a 30 fps export decay at **half the rate** of the 60 Hz
preview. The exported video visibly disagrees with what the user approved.

## Decision

Store attack, decay and symmetric smoothing as **time constants in seconds**, and derive
the per-frame factor from the actual frame interval:

```cpp
blendFactor(tau, dt) = 1 - exp(-dt / tau)        // returns 1.0 for tau <= 0
```

`analyze()` takes `dt` as a parameter: `1/60` for preview, `1/fps` for export.

## Consequences

> "Smoothing expressed as TIME CONSTANTS in seconds, deliberately not as per-frame
> factors. A per-frame factor would make a 30 fps export decay at half the rate of a
> 60 Hz preview and the two would visibly disagree."

This is **the DSP half of the preview/export parity guarantee**. The rendering half is the
fixed-step animation driver → [ADR-0011](0011-fixed-step-animation-driver.md).

Consequences throughout:
- The Separator inspector authors times in **milliseconds** (`attackMs`, `decayMs`,
  `smoothMs`), converted to seconds in `analysisParams()`. That is also the right unit
  perceptually — "220 ms fall" is meaningful, "0.2 per frame" is not.
- The same derivation is used for the auto-gain reference (`normalizationTau`), so that
  too is frame-rate independent.
- `SpectrumEngine` must pass a **real** `dt`, measured from a `QElapsedTimer` — and clamp
  it to 0.25 s, because a stalled GUI thread would otherwise deliver a multi-second `dt`
  and snap every envelope straight to its target.
- Any new temporal behaviour in the analyzer must follow the same pattern. A raw per-frame
  constant anywhere in `analyze()` silently reintroduces the bug.

`analyzerSmoothingIsFrameRateIndependent` decays for a fixed 0.5 s at 60 fps and at
30 fps and requires the results to agree within 0.02. Its comment calls it "the core
guarantee behind preview/export parity".

## Revisiting it

There is no reason to. The exponential form costs one `exp()` per band per frame and buys
exact frame-rate independence.

The thing to watch is not this decision but its **neighbours**: the parity promise also
depends on the animation driver, the shared `RenderList`, and the shared `Node` objects.
This test covers only the analyzer.
