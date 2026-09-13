# ADR-0006 — `topologyChanged` is separate from `graphChanged`

**Status:** Accepted · **Source:** commit `3281a95`, `src/app/NodeGraph.h:136-146` · **Pinned by:** `renderListIgnoresPropertyChanges`, `blursAccumulateOutermostFirst`

## Context

`NodeGraph` needs to tell the world it changed. Two very different consumers listen:

- `RenderList` must re-derive the draw order — but **only** when the shape of the graph
  changed.
- `Project` must mark the document dirty — for **any** change at all, including moving a
  node.

Dragging a node writes `graphX` at **60 Hz**.

## Decision

Two signals with different meanings:

| Signal | Fires on | Listener |
|---|---|---|
| `topologyChanged` | nodes or edges added/removed | `RenderList::rebuild` — **and nothing else** |
| `graphChanged` | anything, including a property edit or a drag | `Project` → dirty flag |

`connections` re-emits on `topologyChanged` only, so the editor's edge delegates rebuild
on a rewire and never on a drag.

## Consequences

From the header:

> "Kept separate from graphChanged because dragging a node writes graphX at 60 Hz, and a
> render list that re-flattened on that would reset the canvas model every frame of every
> drag. **This one line of signal design decides whether the editor feels good or
> broken.**"

A `beginResetModel` during a drag destroys and recreates every canvas delegate, sixty
times a second. The preview strobes.

There is a second guard behind it: `RenderList::rebuild` compares the new entry list
against the old and returns early if identical. So adding an unrelated node off to the
side changes the topology but does **not** churn the canvas.

And `Entry::operator==` compares the transform and effect stacks as well as the node — so
re-plugging a transform or a blur **does** rebuild, even though the set of nodes drawn is
unchanged. Without that, dropping a blur out of the chain would leave the canvas blurred.

`renderListIgnoresPropertyChanges` asserts both halves: four property writes produce **0**
`topologyChanged` and **≥4** `graphChanged`; then adding an unrelated node produces
exactly **1** `topologyChanged` but **0** model resets.

## Revisiting it

Merging them back is the failure. If anything, the split could go further — a third signal
for "an edge changed but no node was added or removed" would let `RenderList` skip work it
currently does and discards, though the equality check already makes that nearly free.

When adding any new mutation to `NodeGraph`, the question to ask is: *does this change
what is drawn, or only how it looks?* Emit accordingly.
