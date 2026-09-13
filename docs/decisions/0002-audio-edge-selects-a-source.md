# ADR-0002 — An audio edge selects a source; it does not carry numbers

**Status:** Accepted · **Source:** `src/app/Node.h:28-48` · **Pinned by:** `chainedSeparatorsBothResolveToTheSameAudioRoot`, `unconnectedSeparatorsAreNotAnalyzed`, `oneOutputFeedsManyInputs`

## Context

In a node graph, an edge normally carries data: the upstream node computes something and
the downstream node consumes it. That intuition is wrong for audio here, and the shape of
the graph does not reveal it.

A `SeparatorNode` runs an FFT. An FFT needs **PCM samples** — not the band values its
input happens to be producing.

## Decision

An audio edge names **which audio to analyse**. `AudioSourceNode::audioRoot()` walks the
audio-input chain back to the `AudioNode` that holds the samples, and the analysis runs
against that buffer.

A separator never reads its immediate input's bands.

## Consequences

Two corollaries that will otherwise be guessed wrong:

**Chained separators do not cascade.** Two in series produce two *independent* analyses of
the same PCM, each with its own band count and smoothing. It is allowed — forbidding it
would be a special case in an otherwise uniform type system — but it is not a two-stage
filter, and nobody should expect the second to refine the first.

**The `AudioNode` is itself just an analyser**, with `bandCount 1` over the full range. So
"one bar = overall volume" falls out of the type system rather than needing a node of its
own.

Practical effects:
- `audioRoot()` returning `nullptr` **is** the liveness predicate — it is how
  `SpectrumEngine` knows to skip a source entirely.
- The FFT cost scales with the number of **sources**, not with the number of things
  drawing them. Ten spectrum nodes on one separator cost one analysis, which the engine
  then fans out along the audio edges.
- `audioRoot()` needs its own cycle guard, because a hand-edited file bypasses `canLink`
  and an unguarded walk over A→B→A never returns.
  → [ADR-0008](0008-two-independent-cycle-guards.md)
- Audio ports are declared `stacked: false`, which is what keeps the flatten from
  following them into the draw list.

## Revisiting it

Making audio edges carry band values would turn separators into a real filter chain —
appealing, but it changes what a separator *is*. Every separator would need to decide
whether it is analysing PCM or transforming an existing spectrum, and the `AudioNode`
would stop being uniform with the rest.

If it were done, `audioRoot()` disappears and `SpectrumEngine`'s fan-out becomes a
per-edge evaluation — the FFT-cost-per-source property goes with it.
