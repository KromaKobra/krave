# ADR-0014 — Zero-pad the analysis window rather than round it to a power of two

**Status:** Accepted · **Source:** commit `32d7a8a`, `src/core/SpectrumAnalyzer.h:59-73` · **Pinned by:** `windowLengthSetsResolutionContinuously`

## Context

A radix-2 FFT needs a power-of-two transform size. The obvious move is to round the
analysis window up to one.

Measuring against a reference visualizer on a 20–70 Hz sine sweep showed why that is
wrong. The reference resolved each pure tone to a spike about two bars wide; ours smeared
across a third of the display. One of three causes:

> "Resolution was quantised. `windowMs` was rounded UP to a power of two, so only ~23 Hz
> (4096) or ~12 Hz (8192) were reachable and the reference's 17 Hz was not."

Window length **is** frequency resolution — a tone spreads over roughly
`2 × sampleRate / windowSamples` Hz. Rounding the window quantises resolution into
factor-of-two jumps with nothing in between.

## Decision

Keep the **true** window length exactly as authored, and **zero-pad it** out to the next
power of two for the transform.

```
m_windowLen = windowSamples                              // exactly what was asked for
m_fftSize   = nextPowerOfTwo(windowLen * padFactor)      // padFactor = 2
```

The Hann taper is computed over the **true window only**; the padding tail stays zero.

## Consequences

Resolution is now **continuously adjustable** via `windowMs`.

Padding also "places more bins under the same peak, which lets the per-band interpolation
locate it smoothly instead of in bin-sized steps".

**Normalisation follows the true window length**, not the padded size:

```
norm = 2.0f / (windowLen * 0.5f)
```

— so changing `padFactor` does not shift the amplitude scale.

Measured result from the commit, against the reference on the same sweep:

| | Hz/column | FWHM | 40 Hz lands at | peak height |
|---|---|---|---|---|
| reference | 0.95 | 17.1 Hz | column 115 | 0.971 |
| before | 0.48 | 40.4 Hz | column 133 | clipped |
| after | 0.99 | 19.8 Hz | column 118 | 0.961–0.978 |

**The zero padding must never be overwritten.** `rebuild()` zeroes the whole buffer once;
`analyze()` only ever writes `[0, windowLen)`:

> "Only the leading windowLen samples are filled; the padding beyond stays zero from
> rebuild() and must not be disturbed."

Any code that writes past `windowLen`, or resizes the frame buffer, silently destroys
this.

**Padding does not improve resolution** — only the true window does. Raising `padFactor`
buys CPU cost and smoother interpolation, nothing more. It is deliberately not exposed as
a node property.

The same commit also established that `minHz` may be negative
(→ [ADR-0016](0016-unresolvable-bands-are-flagged.md)) and that dB amplitude widens every
peak by lifting the Hann skirts (→ [ADR-0015](0015-linear-amplitude-and-expansion.md)).

`windowLengthSetsResolutionContinuously` asserts that a window between two powers of two
gives a peak width between them, rather than snapping to one.

> **Stale comment.** `Node.h:238-239` still claims `windowMs` is "Rounded up to a power of
> two internally". It is not, and the opposite is the whole point of this record.

## Revisiting it

Nothing to revisit. The cost is one larger transform; the benefit is a control that
actually does what its label says.
