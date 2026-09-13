# ADR-0001 — The graph replaces the layer stack

**Status:** Accepted · **Source:** commit `3281a95` · **Pinned by:** `renderListFollowsTheVisualChain`, `renderListSkipsUnreachableNodes`, `organizingNodesNeverAppearInTheRenderList`

## Context

The scene used to be an ordered array of layers, with z-order given by array index. That
worked until one property needed to exist: **`BarLayer::providerId`**, an explicit
reference from a bar layer to the audio provider feeding it.

It had no UI. It was `0` in every project file. And it only ever resolved correctly
because `resetLayers` happened to renumber ids from 1 in array order — a coincidence, not
a mechanism.

> "A stack cannot express a relationship, so that one bolted-on edge was unstable,
> un-editable and invisible."

## Decision

Replace the array with a directed graph of typed nodes. **Layering becomes a consequence
of topology**: every visual node has at most one visual input, so the subgraph reaching
the **Output** node is a chain, and walking that input backwards produces the draw order.

## Consequences

**The flatten is what made this cheap.** Walking backwards from Output produces exactly
the ordered array `VisualizerCanvas`'s `Repeater` already consumed — two lines of that
file changed. `LayerFrame`, the blend shaders, normalized geometry and the export path
were untouched.

Things that became possible, or fell out for free:
- A provider can feed **many** consumers, and the relationship is visible on screen.
- Orphan subgraphs are legal and cost nothing — they simply are not reachable from Output.
- A shared subgraph can be drawn twice, along two paths carrying different transforms.
- `MergeNode` branches the chain into a tree without any new ordering concept.

Things that became new problems, each with its own record:
- **Cycles** — impossible in a stack. → [ADR-0008](0008-two-independent-cycle-guards.md)
- **Id stability** — ids must never be reused. → [ADR-0007](0007-node-ids-are-never-reused.md)
- **Signal granularity** — a 60 Hz drag must not re-flatten. → [ADR-0006](0006-topology-vs-graph-changed.md)

Deliberate behaviour changes in the same commit:
- An unconnected spectrum now draws **nothing** instead of falling back to the first
  provider. "Honest beats magical."
- Placement and compositing moved off the base class onto `VisualNode` — an audio provider
  used to serialize a meaningless `posX` and show seven dead controls.
- The **particle layer** and the background's **still image** were removed. Both are
  recoverable from git; the still image's role now belongs to an Image node.

The file format went to v3, and nothing older loads.
→ [ADR-0018](0018-no-in-app-migration.md)

## Revisiting it

Going back to a stack would mean giving up every relationship the graph expresses — the
`providerId` problem returns immediately, and Merge, Transform and Blur all lose their
meaning. There is no partial retreat.

The one thing worth watching is that `RenderList` still hands QML a **flat, ordered
list**. That is what keeps the canvas simple, and it survives only because transform
distributes over a group. Any effect that does not distribute is the boundary.
→ [ADR-0013](0013-transform-is-identity-blur-is-approximation.md)
