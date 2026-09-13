# 3 · The document model

**Sources:** `src/app/Node.*`, `src/app/NodePorts.*`, `src/app/NodeGraph.*`, `src/app/RenderList.*`, `src/app/Project.*`, `src/app/Session.*`

The `mvapp` library: what a project *is*, in memory and on disk. Headless — no Quick, no
Multimedia — so all of it is testable without a window.

---

## Components

```mermaid
graph TB
    ports["<b>NodePorts</b><br/>one static table:<br/>which type has which ports,<br/>what each input accepts"]
    node["<b>Node</b> + 10 subclasses<br/>properties, and one input<br/>SLOT per port"]
    graph["<b>NodeGraph</b><br/>the document: an UNORDERED set<br/>of nodes. Validation, cascade."]
    rl["<b>RenderList</b><br/>derived draw order"]
    proj["<b>Project</b><br/>.mvproj save/load<br/>via QMetaObject walk"]
    sess["<b>Session</b><br/>session.json<br/>last project, track, volume"]

    node -->|spec| ports
    graph -->|owns| node
    graph -->|owns| rl
    graph -->|canLink reads| ports
    rl -->|flatten reads| ports
    proj -->|resetNodes| graph
    rl -.->|topologyChanged only| graph

    style graph fill:#9184d9,stroke:#6b5fb8,color:#ffffff
    style ports fill:#9184d9,stroke:#6b5fb8,color:#ffffff
```

---

## The central claim

> **Layering is a consequence of topology.**

There is no layer index anywhere. `NodeGraph` holds an *unordered* `QVector<Node*>`:

> "The list is the set of nodes; it carries no ordering meaning at all — layering is a
> consequence of topology now, and RenderList is what derives it."

Every visual node has at most one visual input, so the subgraph reaching the **Output** is
a chain; walking it backwards produces the draw order. A **Merge** branches it into a
tree, and its two subtrees land one after another because port order is compositing
order.

This replaced a flat layer stack in `3281a95`. The argument that forced it was one
property: `BarLayer::providerId`, an explicit reference from a bar layer to the provider
feeding it. It had no UI, it was 0 in every project file, and it only ever resolved
correctly because the loader happened to renumber ids from 1 in array order. **A stack
cannot express a relationship.**
→ [ADR-0001](../decisions/0001-graph-replaces-layer-stack.md)

---

## Four structural choices

### 1 · An input is a slot, so illegal states are unrepresentable

`Node::m_inputs` is one pointer per port, writable only by `NodeGraph`. There is no
"one input, one source" rule to enforce — there is nowhere to put a second source. The
deletion cascade is one pass nulling matching pointers; the flatten is a pointer walk
with no lookups.

On disk it becomes a flat edge list keyed by port **name**, which is readable, diffable,
and survives the port table being reordered.

### 2 · The port table is data, in C++

One static table drives link validation, cycle detection, the flatten and the deletion
cascade — and it lives in C++ rather than QML precisely because all of those must run
headless in the tests. `NodeTypes.qml` carries icon, title and blurb only.
→ [ADR-0003](../decisions/0003-port-table-in-cpp.md) ·
full grid in [4 · graph-rules](../4-code/graph-rules.md#the-grid)

### 3 · An audio edge selects a *source*; it does not carry numbers

The one thing about the graph that is not guessable from its shape. A `SeparatorNode`
needs PCM to run an FFT over, not its input's band values — so its audio input names
**which** audio to analyse, and `audioRoot()` walks that chain back to the `AudioNode`
holding the samples.

Two consequences:
- **Chained separators do not cascade.** Two in series are two independent analyses of the
  same PCM. Allowed — forbidding it would be a special case in an otherwise uniform type
  system — but not a two-stage filter.
- **The `AudioNode` is itself just an analyser**, with one band over the full range. So
  "one bar = overall volume" falls out of the type system rather than needing a node of
  its own.

→ [ADR-0002](../decisions/0002-audio-edge-selects-a-source.md)

### 4 · Properties are `MEMBER`, so serialization is free

`Q_PROPERTY(… MEMBER … NOTIFY changed)` gives QML two-way binding *and* lets `Project`
serialize by walking the `QMetaObject`. Adding a property costs no serialization code and
no panel plumbing. All properties share one `changed()` signal — the slight
over-invalidation is far cheaper than a signal per property.
→ [ADR-0020](../decisions/0020-member-properties-and-meta-walk.md)

---

## The two signals

The single most load-bearing line of design in `NodeGraph`:

| Signal | Fires on | Listener |
|---|---|---|
| `topologyChanged` | nodes or edges added/removed | **`RenderList::rebuild`, and nothing else** |
| `graphChanged` | anything, including a property edit or a drag | `Project` → dirty flag |

Dragging a node writes `graphX` at 60 Hz. A render list that re-flattened on that would
`beginResetModel` the canvas model every frame of every drag.

> "This one line of signal design decides whether the editor feels good or broken."

→ [ADR-0006](../decisions/0006-topology-vs-graph-changed.md) · pinned by
`renderListIgnoresPropertyChanges`

---

## Identity

Node ids are assigned monotonically and **never reused within a session**. This is not
cosmetic: `SpectrumWorker` keys its analyzers by node id, so reuse would let a deleted
node's smoothing envelope be inherited by a new node — "which looks like random flicker
rather than like a bug".

A document load restarts the counter from the loaded maximum, which is why
`Project::loaded` also resets the analyzers.
→ [ADR-0007](../decisions/0007-node-ids-are-never-reused.md)

---

## Cycles — the genuinely new failure mode

Nothing in a stack could be cyclic. Three independent guards exist, and none is redundant:

| Guard | Catches |
|---|---|
| `canLink` → `reachesUpstream` | building one through the editor |
| `flatten`'s `onPath` path-set | a **hand-edited or corrupt file**, which never goes through `canLink` |
| `audioRoot`'s `seen` set | a separator loop, which would otherwise never return |

All input ports are walked, audio included — "separator → separator → separator loops just
as readily as a visual chain does".
→ [ADR-0008](../decisions/0008-two-independent-cycle-guards.md)

---

## Loading is defensive

A slightly wrong file should **open and be fixable**, not refuse. Duplicate essentials are
dropped, missing ones synthesized, unknown node types skipped, unknown property keys
ignored, and an invalid edge drops *only that edge*. The only hard rejections are a JSON
parse failure and a wrong format version.
→ [ADR-0022](../decisions/0022-defensive-load.md) ·
[4 · project-format](../4-code/project-format.md)

Because loaded edges are applied **through `link()`**, a cyclic file can never produce a
cyclic in-memory graph.

---

## Two files, two jobs

`.mvproj` is a portable **template** and stores no audio path; `session.json` remembers
the track, the last project and the volume for *this machine*. That split is what keeps a
project shareable.
→ [ADR-0017](../decisions/0017-no-audio-path-in-project.md)

---

## Where the traps are

- **`NodeTypeSpec::visual` must agree with the class hierarchy.** `isVisual()` reads the
  flag; `flatten` branches on `qobject_cast<VisualNode*>`. Nothing enforces agreement.
- **A default-constructed `NodeGraph` has no Output** — a state no public path produces.
  Both test files carry a `minimalGraph()` helper.
- **`link()` returns `true` for an edge that already exists**, without emitting. Callers
  cannot distinguish "created" from "already there".
- **Nothing validates property values.** Enum strings fall back silently; numeric ranges
  are inspector-only. A loaded file bypasses both.
- **`autoLayout()`'s grid constants are duplicated in `tools/migrate_v2_to_v3.py`** with no
  shared source.

---

## See also

- [4 · graph-rules](../4-code/graph-rules.md) — the port grid, `canLink`, the cascade
- [4 · render-list](../4-code/render-list.md) — the flatten, with a worked example
- [4 · node-reference](../4-code/node-reference.md) — every type, every property
- [4 · project-format](../4-code/project-format.md) — the on-disk shape
