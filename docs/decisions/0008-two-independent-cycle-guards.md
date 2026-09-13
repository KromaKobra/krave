# ADR-0008 — Cycles are rejected at link time and guarded again in the flatten

**Status:** Accepted · **Source:** commit `3281a95` · **Pinned by:** `cyclesAreRejected`, `flattenSurvivesACycleBuiltByHand`, `aCycleThroughAMergesSecondInputIsRefused`

## Context

Cycles are the one genuinely new failure mode the graph introduced — **nothing in a layer
stack could be cyclic**. An unguarded recursive walk over A→B→A does not produce a bad
picture; it produces a hard crash.

## Decision

Guard in three independent places, and treat none of them as redundant:

| Guard | Where | Catches |
|---|---|---|
| `reachesUpstream` | `NodeGraph::canLink` | Building a cycle through the editor |
| `onPath` path-set | `RenderList::flatten` | A hand-edited or corrupt file |
| `seen` set | `AudioSourceNode::audioRoot` | A separator loop |

(A fourth, `depthOf`'s `onPath`, keeps `autoLayout` terminating.)

## Consequences

**`canLink` walks every input port, audio included.** From the commit:

> "A separator loop hangs `audioRoot()` as readily as a visual loop hangs the flatten."

**The flatten's guard is not belt-and-braces.** `canLink` is only reached through the
editor. A hand-edited or corrupt file never goes through it:

> "The cycle guard is needed IN ADDITION TO NodeGraph::canLink's, because a hand-edited or
> corrupt file never goes through canLink, and unguarded recursion on A->B->A is a hard
> crash rather than a bad picture."

The flatten's guard is a **path set, not a global visited set** — which is a separate
decision doing double duty. It breaks cycles *without* deduplicating legitimate reuse, so
a diamond still draws its shared subgraph twice, and the two paths can carry different
transforms.

There is also a blunt backstop: `kMaxDrawCalls = 256`, for a corrupt file whose reused
subgraph expands combinatorially.

**Since loaded edges are applied through `link()`**, and `link()` re-runs `canLink`, a
cyclic file can never produce a cyclic in-memory graph. That is precisely why
`flattenSurvivesACycleBuiltByHand` cannot build one — it *simulates* the re-entrant state
by pre-seeding `onPath` with the node, "EXACTLY the state the recursion reaches on the
second visit to a node in a cycle".

That test also asserts the sane path leaves `onPath`, `stack` and `effects` **empty** —
the walk restores its own scratch state.

## Revisiting it

Removing the flatten's guard on the grounds that `canLink` already prevents cycles is the
tempting mistake, and the commit anticipates it:

> "'unreachable today' is a weak thing to stake a crash on."

Any new input port must be walked by `reachesUpstream`, and any new recursive walk over
inputs needs its own guard. Both are cheap; neither is optional.
