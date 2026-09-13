# ADR-0021 — Port index *is* compositing order

**Status:** Accepted · **Source:** `src/app/NodePorts.cpp:9-13` · **Pinned by:** `mergeDrawsItsBackgroundArmFirst`, `aMergesTwoEdgesSurviveAsDistinctPorts`

## Context

`MergeNode` stacks two visual subgraphs. Something has to say which one is behind.

The options were a property on the node (`swapped`, or an explicit order field), or making
the ports themselves carry the meaning.

## Decision

**Port order is compositing order.** Input 0 is the bottom of the stack. `Merge` declares
`background` at index 0 and `foreground` at index 1, and `RenderList::flatten` walks inputs
in index order.

## Consequences

The comment ties three behaviours together at once:

> "Port ORDER is layout order: inputs run left to right along a node's top edge. It is
> also compositing order — port 0 is the bottom of the stack — which is why RenderList
> walks the inputs in index order rather than by name, and why Merge's `background` port
> must come first."

So the same fact drives the visual layout of the node card, the flatten's traversal order,
and the compositing result. The user sees the answer: the left port is the back one.

**`MergeNode` has zero properties and no code on the class at all.**

> "The compositing is done entirely by port ORDER, which RenderList::flatten already walks
> — hence no code here at all."

It also has no inspector, because there is nothing to inspect.

Because the flatten appends a node *after* recursing into its inputs, children land in the
list before their consumer — so index 0 in the draw list is the bottom of the stack, which
is exactly what `z: index` in `VisualizerCanvas` needs.

`mergeDrawsItsBackgroundArmFirst` asserts both directions: `background` draws first, **and
swapping the two edges flips the order** — "which is what makes the two ports meaningfully
different rather than just differently named". Its comment notes this is "the whole of
Merge's behaviour".

`aMergesTwoEdgesSurviveAsDistinctPorts` covers the on-disk half: two edges into one node
are written under their own port **names**, and reload preserves which arm is which.
Collapsing them, or writing an index, would silently lose the compositing order.
→ [ADR-0004](0004-connections-keyed-by-port-name.md)

### What this constrains

Reordering the inputs of any multi-input type is a **behavioural** change, not a cosmetic
one. It is safe on disk — edges are keyed by name — but it changes what is drawn on top.

## Revisiting it

An explicit order property would decouple the two, at the cost of a control that duplicates
information the graph already shows. The current design means there is nothing to get out
of sync between "what the card looks like" and "what happens".

If a node ever needs many stacked inputs, port order scales fine; it is the same rule.
