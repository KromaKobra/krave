# ADR-0015 — Linear amplitude, a black level, and expansion rather than compression

**Status:** Accepted · **Source:** commits `39b2603`, `6f7ce60`, `527da07` · **Pinned by:** `bandNormalizationFlattensWithoutCrushingDynamics` (partial)

## Context

"What the spectrum really is" and "what looks good" are different problems. A faithful FFT
readout jitters every frame, slopes steeply toward the treble, and never sits still —
every bar hovers at a similar mid height and the row reads as busy noise rather than as
music.

The work behind this record was a series of measured comparisons against a reference
visualizer, frame by frame over the same song.

## Decision

Offer **linear amplitude** alongside dB, add a **black level** in linear mode, and default
the curve to **expansion** (gamma > 1) rather than compression.

All of it lives on the `SeparatorNode`, so spectrum nodes carry only appearance.

## Consequences

### Why linear amplitude at all

> "dB fills the display evenly but lifts the noise between partials as much as the
> partials themselves, so it reads as one continuous mound. Linear leaves a flat floor
> with discrete peaks standing on it."

It also affects apparent resolution: "A Hann skirt 40 dB below the peak is invisible at 1%
in linear magnitude, but in a 67 dB window it still draws a tall bar."

### Why a black level

> "Quiet passages still carry rumble in every band; without this the display never rests at
> the baseline" — and loud passages then have to climb from an already-high floor.

### Why expansion, not compression

This one was got **wrong first**, and the record is worth keeping. An initial attempt used
compression (gamma < 1) to lift quiet detail; measurement showed it made things worse
(mean body error 0.136 → 0.194) because the quiet body was already too tall.

> "EXPANSION rather than compression: gamma above 1 pushes mid-level bands down while
> leaving peaks near their height."

Fitting the measured transfer gave floor ≈ 0.02, gamma ≈ 1.7, gain ≈ 1.45:

| Peak height (fraction of frame) | intro | drop |
|---|---|---|
| reference | 0.147 | 0.538 |
| before | 0.233 | 0.786 |
| after | 0.108 | 0.462 |

Mean body error against the reference across twelve probe points: 0.087 → 0.026.

### What linear mode ignores

`bandNormalization`, `gateDb`, `floorDb` and `ceilingDb` **do nothing** in linear mode —
"quiet content simply stays near zero, which is what leaves a flat floor with sharp peaks
standing on it". The inspector still shows all four sliders.
→ [node-reference](../4-code/node-reference.md#conditioning)

### Related settings from the same work

- **Linear frequency spacing over a narrow low range** (`hzLinearFactor` = 1, `maxHz`
  ≈ 1200) resolves a bass line's harmonics into separate spikes; log spacing over the full
  band smears them into one mound.
- **Welch averaging** (`averageCount`) — "a single FFT is a high-variance estimate and its
  floor looks like grass".
- **Auto-gain referenced to each band's slow-moving average, not its recent peak** — a peak
  reference "pins every band at its own maximum and flattens the dynamics completely, which
  measured as a punch ratio of exactly 1.0".
- **Symmetric smoothing**, because an asymmetric envelope "makes every bar fall at an
  identical rate regardless of the music", which reads as mechanical.

Two measurement lessons are recorded in the commits and still apply:
- Percentile-to-percentile comparison between two videos is meaningless; compare at the
  same timestamps.
- `-ss` **before** `-i` does not land on the requested frame in these exports. Frame grabs
  must decode sequentially.

## Revisiting it

The defaults encode a specific look and are fair game to retune — `examples/avee-bass.mvproj`
and `avee-linear.mvproj` carry the matched configurations.

What should **not** be casually changed is the availability of linear mode or the black
level: both were added because no setting of the dB path could produce the measured target,
and the record above is the evidence.

Only `bandNormalizationFlattensWithoutCrushingDynamics` pins any of this. The rest was
verified by measurement against a reference video that is not in the repository.
