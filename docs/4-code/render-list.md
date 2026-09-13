# 4 · The render list — how draw order is derived

**Sources:** `src/app/RenderList.h`, `src/app/RenderList.cpp`, `src/app/qml/VisualizerCanvas.qml`, `src/app/qml/layers/LayerFrame.qml`, `tests/tst_graph.cpp`

There is no layer index anywhere in Krave. **Layering is a consequence of topology**, and
this file is the derivation.

---

## The claim

Every visual node has at most **one** visual input, so the subgraph reaching the
**Output** node is a chain. Walking that input backwards depth-first produces exactly the
ordered array `VisualizerCanvas`'s `Repeater` consumes, and `z: index` then means
"downstream draws on top".

A **Merge** does not break that — its two subtrees simply land one after the other,
background arm first, because port order is compositing order. A **Transform** does not
break it either, and that is the less obvious half: because it is affine and pivoted in
canvas coordinates, transforming each member of a group individually is the same picture
as transforming the group, so the list stays **flat** and each entry just records which
transforms enclose it.

→ [ADR-0001](../decisions/0001-graph-replaces-layer-stack.md) ·
[ADR-0013](../decisions/0013-transform-is-identity-blur-is-approximation.md)

---

## `Entry`

One draw call:

```cpp
struct Entry {
    VisualNode *node = nullptr;
    QVector<Node *> transforms;   // enclosing TransformNodes, OUTERMOST FIRST
    QVector<Node *> effects;      // enclosing BlurNodes,      OUTERMOST FIRST
};
```

Outermost first is the order the matrices compose in and the order the shader passes
nest in.

`operator==` compares **all three** members. That is deliberate: re-plugging a transform
or a blur must rebuild the list even though the set of nodes drawn has not changed.
Without it, dropping a blur out of the chain would leave the canvas blurred.

Two separate stacks rather than one mixed list, "because the two are consumed by
different parts of `LayerFrame` and nothing about their relative order is meaningful".

---

## `flatten` — the algorithm

```cpp
static void flatten(Node *n, QVector<Entry> &out, QSet<const Node*> &onPath,
                    QVector<Node*> &stack, QVector<Node*> &effects);
```

`RenderList.cpp:56-98`, step by step:

1. **`n == nullptr`** → return. An unconnected input is simply nothing to draw.
2. **`onPath.contains(n)`** → `qWarning("cycle in the visual chain at node %d")`, return.
   Contributes nothing; does not crash.
3. **`out.size() >= kMaxDrawCalls` (256)** → return. Pathological-graph guard.
4. `onPath.insert(n)`.
5. **If `n` is a `TransformNode`** → push onto `stack` — *before* descending, so
   everything upstream inherits it.
6. **If `n` is a `BlurNode`** → push onto `effects`, same shape.
7. **For each input port in index order**, if `ports[port].stacked`, recurse into
   `n->input(port)`.
8. Pop whichever stack was pushed.
9. **If `n` is a `VisualNode`** → `out.append(Entry{n, stack, effects})`, **by copy**, and
   *after* the pops — so a node does not record itself.
10. `onPath.remove(n)`.

Because the append happens **after** recursion, children land in `out` before the node
that consumes them: **index 0 is the bottom of the stack.** Because ports are walked in
index order, `merge.background` (index 0) lands before `merge.foreground` (index 1).

### The four properties worth knowing

| Property | Consequence |
|---|---|
| **Only `stacked` inputs are followed** | Audio ports are skipped (they are unstacked). This is the line that will stop a future mask from being drawn as a layer *as well as* sampled as a matte. |
| **Port order is compositing order** | Input 0 ends up furthest back. |
| **A shared upstream subgraph is drawn TWICE, on purpose** | `onPath` is a **path set**, not a global visited set — it breaks cycles without deduplicating legitimate reuse, and the two paths can carry different transforms. |
| **Only `VisualNode`s are appended** | Merge, Transform, Blur and the Output itself organise the list without ever appearing in it. |

### Why the cycle guard is not redundant

`NodeGraph::canLink` already refuses cycles. But a hand-edited or corrupt file never goes
through `canLink`, and unguarded recursion on A→B→A is a **hard crash** rather than a bad
picture. → [ADR-0008](../decisions/0008-two-independent-cycle-guards.md)

---

## `rebuild` and the two guards

`RenderList.cpp:109-136`:

1. Fresh `next`, `onPath`, `stack`, `effects`.
2. Seed from **`output->input(0)`**, not from the Output itself — "it is not a
   `VisualNode`, so it would not be appended anyway, but starting inside the chain makes
   that independent of the class hierarchy".
3. **`if (next == m_entries) return;`** — adding an unrelated node off to the side changes
   the topology but not the draw order, and must not churn the canvas.
4. Otherwise `beginResetModel` / assign / `endResetModel`. "A chain reorder is not
   expressible as a row move, and topology changes are user-paced, so a full reset is both
   correct and cheap enough."

**`rebuild` is connected to `NodeGraph::topologyChanged` and nothing else.** A property
edit or a 60 Hz drag must never reach it.
→ [ADR-0006](../decisions/0006-topology-vs-graph-changed.md)

---

## Why a model rather than a list property

From `RenderList.h:40-45`:

> "A QAbstractListModel rather than a QVariantList property: a list property would make
> the Repeater destroy and recreate every delegate on any topology change, and would
> force the delegate off the `layerObject` role onto `modelData`."

| Role | roleName | Value |
|---|---|---|
| `LayerRole` | **`layerObject`** | the `VisualNode*` |
| `TransformsRole` | `layerTransforms` | `QVariantList` of enclosing transforms |
| `EffectsRole` | `layerEffects` | `QVariantList` of enclosing blurs |

> The role is `layerObject`, **never `layer`** — `QQuickItem::layer` is `FINAL` and a
> delegate cannot shadow it. This is a hard error, not a style choice. See
> [qt-quirks](qt-quirks.md).

`nodes()` is a projection returning just the nodes in order — most callers, and every
ordering test, care about what is drawn rather than what encloses it.

---

## A worked example

`examples/SyrexAttemptv3-lightweight.mvproj` — 12 nodes, 9 edges:

```
ids present: 1 2 3 4 5 6 7 _ 9 10 11 12 13      ← id 8 is absent
             ^ output       ^ (deleted; never reissued)

edges: 2→3.audio     3→5.audio
       4→13.background   5→13.foreground   13→1.visual
       7→5.visual
       7→6.foreground    9→10.foreground   5→11.visual
```

The flatten starts at `output.input(0)` = node **13** (a Merge):

| Step | Node | Action |
|---|---|---|
| 1 | 13 `merge` | port 0 `background` → node 4 |
| 2 | 4 `background` | no inputs → **append Background(4)** |
| 3 | 13 | port 1 `foreground` → node 5 |
| 4 | 5 `spectrum` | port 0 `visual` → node 7 |
| 5 | 7 `text` | visual input empty → **append Text(7)** |
| 6 | 5 | (audio port skipped — unstacked) → **append Spectrum(5)** |

**Draw order: `[Background(4), Text(7), Spectrum(5)]`** — three draw calls out of twelve
nodes.

Nodes **6, 9, 10, 11 and 12 are orphans**: reachable in the document but not from the
Output, so they are not drawn at all. Node 12 has no edges whatsoever. Note that blur 11
*consumes* spectrum 5 but nothing consumes 11, so 11 never encloses anything on a path to
the Output — spectrum 5's `effects` stack is empty.

This one file demonstrates three things at once: **the node list is a set, not a stack**;
**orphans cost nothing**; and **ids are never reused** (id 8 is gone for good).
→ [ADR-0007](../decisions/0007-node-ids-are-never-reused.md)

---

## What QML does with it

`CanvasStage.qml:104` passes `nodeGraph.renderList` into `VisualizerCanvas.layers`. The
`Repeater` sets `z: index` so index 0 is furthest back, and a `Loader` switches on
`layerObject.typeId` to one of four components — `spectrum`, `background`, `text`,
`image` — with `default: return null`, because only visual nodes reach a render list at
all.

`LayerFrame` folds `layerTransforms` into one `Matrix4x4` (outermost first) and feeds
`layerEffects` to a recursive `BlurChain`. See
[3 · rendering](../3-components/rendering.md).

---

## Tests that pin this

| Test | Protects |
|---|---|
| `renderListFollowsTheVisualChain` | order comes from edges, not array position (the fixture is built in a non-draw order) |
| `renderListSkipsUnreachableNodes` | orphans stay out; unplugging Output empties the list |
| `renderListDrawsASharedSubgraphTwice` | a diamond yields two entries of the same node |
| `renderListIgnoresPropertyChanges` | four property writes → 0 `topologyChanged`, 0 `modelReset` |
| `mergeDrawsItsBackgroundArmFirst` | swapping the two edges flips the order |
| `transformsAccumulateOutermostFirst` | nesting order; a sibling arm's stack stays empty |
| `blursAccumulateOutermostFirst` | nesting order, **and** exactly one `modelReset` on a rewire that changes no drawn node |
| `organizingNodesNeverAppearInTheRenderList` | merge/transform/blur/output are not `VisualNode`s; stacks follow the recursion, not the immediate parent |
| `flattenSurvivesACycleBuiltByHand` | the guard, and that the walk restores its scratch state |

---

## See also

- [graph-rules](graph-rules.md) — `stacked`, port order, the cycle guards
- [3 · rendering](../3-components/rendering.md) — what happens to an `Entry` on screen
- [ADR-0013](../decisions/0013-transform-is-identity-blur-is-approximation.md) — the one place "enclosing stack" ≠ "enclosing the group"
