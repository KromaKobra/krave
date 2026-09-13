# 4 · Graph rules — ports, linking, cycles, deletion

**Sources:** `src/app/NodePorts.h`, `src/app/NodePorts.cpp`, `src/app/NodeGraph.h`, `src/app/NodeGraph.cpp`, `src/app/Node.cpp`, `tests/tst_graph.cpp`

Everything that decides what may connect to what, and what happens when the graph
changes.

---

## The port table

One static table in C++ describes every node type. **It lives in C++ rather than QML**
because flattening, cycle detection, the deletion cascade and link validation all read
it, and all of them must run headless in the tests with no QML engine anywhere.
`NodeTypes.qml` keeps presentation only — icon, title, blurb.
→ [ADR-0003](../decisions/0003-port-table-in-cpp.md)

### The grid

`NodePorts.cpp:49-131`. Input ports are listed **in index order, and index order is
meaningful** — see [compositing order](#port-index-is-compositing-order) below.

| Type | Inputs (index → name · kind · accepts · required · stacked) | Output | essential | visual |
|---|---|---|---|---|
| `output` | 0 → `visual` · Visual · {Visual} · **required** · stacked | **none** | **yes** | no |
| `audio` | *(none)* | **Audio** | **yes** | no |
| `separator` | 0 → `audio` · Audio · {Audio} · **required** · *not* stacked | **Audio** | no | no |
| `background` | *(none)* | Visual | no | **yes** |
| `spectrum` | 0 → `visual` · Visual · {Visual} · optional · stacked<br>1 → `audio` · Audio · {Audio} · **required** · *not* stacked | Visual | no | **yes** |
| `text` | 0 → `visual` · Visual · {Visual} · optional · stacked | Visual | no | **yes** |
| `image` | *(none)* | Visual | no | **yes** |
| `merge` | 0 → `background` · Visual · {Visual} · optional · stacked<br>1 → `foreground` · Visual · {Visual} · optional · stacked | Visual | no | no |
| `transform` | 0 → `visual` · Visual · {Visual} · optional · stacked | Visual | no | no |
| `blur` | 0 → `visual` · Visual · {Visual} · optional · stacked | Visual | no | no |

`ImageNode` deliberately has **no visual input** — "stacking one thing over another is
what Merge is for, and giving every visual type a pass-through input is what made the
graph a chain in the first place".

### The four `PortSpec` flags

| Field | Meaning | Get it wrong and… |
|---|---|---|
| `name` | **Stable identifier.** This string is what is serialized as a connection's `port` field. | Renaming it silently breaks every existing project file. → [ADR-0004](../decisions/0004-connections-keyed-by-port-name.md) |
| `accepts` | The output kinds legal on this input. A Mask input declares `{Visual}`, so "blue accepts purple" needs no special case in the validator, the hit test, or the highlighting. | → [ADR-0005](../decisions/0005-accepts-list-on-inputs.md) |
| `required` | Is the node **useless** without it? True only for data dependencies. | Marking a compositing input required puts a warning on nearly every graph and teaches people to ignore the marker. → [ADR-0009](../decisions/0009-required-marks-data-dependencies.md) |
| `stacked` | Does the subgraph reaching this port join the **draw list**? | A future mask would be silently drawn as a layer *as well as* sampled as a matte. |

Every Audio port is `required` and **not** `stacked`. That pairing is asserted by
`tst_graph::portTableInvariantsHold`.

### `PortKind`

| Kind | Colour | Status |
|---|---|---|
| `Visual` | purple `#b07cff` | in use |
| `Audio` | orange `#e8a05c` | in use |
| `Mask` | blue `#6fb6ef` | **reserved — no port uses it today** |

The colour a port and its edges are drawn in is a pure function of the kind, so **the
kind is the visible type**. `Theme.portColor(kind)` is the one switch on a kind string
in the whole QML tree.

`Mask` is fully specified ahead of its first use — colour, name round-trip, and the
rules that a mask input must accept Visual and must not be stacked.
`tst_graph::maskInputsAcceptVisualOutputs` pins those rules **vacuously today**, by
design: "It starts biting the moment a mask port is added, which is the point at which
getting it wrong would be expensive."

### Consulting the table

`nodeTypeSpec(typeId)` is a **linear scan returning the first match**, with a
`qWarning` and `nullptr` on a miss — hence `portTableCoversEveryNodeType` checks for
duplicate ids, which would shadow silently.

`Node::spec()` falls back to a static default-constructed `NodeTypeSpec` for an unknown
id, "so a missing entry degrades to a port-less node rather than dereferencing null".

> **`NodeTypeSpec::visual` must agree with the class hierarchy.** `Node::isVisual()`
> reads the flag, but `RenderList::flatten` branches on
> `qobject_cast<VisualNode*>`. Nothing mechanically enforces that the two agree.

---

## Edges are slots, not a list

In memory, an input is **one pointer**. `Node::m_inputs` is a `QVector<Node*>` sized
from the port table, and only `NodeGraph` may write it (`friend class NodeGraph`; QML
cannot reach it at all).

From `Node.h:102-109`:

> "In memory an input is a SLOT, so 'one input, one source' is structurally impossible
> to violate — there is nowhere to put a second source, the deletion cascade is one pass
> nulling matching pointers, and the flatten is a pointer walk with no lookups."

**There is no "single input" rule to enforce, because the illegal state is
unrepresentable.** On disk it becomes a flat edge list keyed by port *name* instead,
which is readable, diffable, and survives the port table being reordered.

`m_inputs` is `mutable` and lazily sized, because `spec()` is virtual and cannot be
called from the base constructor.

---

## `canLink` — the six checks, in order

`NodeGraph.cpp:241-270`. QML runs this for the hover preview *and* `link()` re-runs it
on drop, so the two can never disagree.

1. **Self-link** — `fromId == toId` → false.
2. **Existence** — either node missing → false.
3. **Source has an output** — `!fromSpec.hasOutput` → false. This is what stops anything
   being driven from the Output node.
4. **Port exists** — `to->portIndex(toPort) < 0` → false.
5. **Kind compatibility** — `accepts.contains(fromSpec.outputKind)` → else false.
6. **Acyclicity** — `!reachesUpstream(from, to, seen)`.

**Not checked:** whether the target port is already occupied (replacement is
intentional), whether `required` inputs are satisfied, or whether the result is reachable
from the Output. Orphan subgraphs are legal and cost nothing.

### `link` and `unlink`

`link()` returns `true` **without emitting anything** if the edge already exists — "do
not churn the render list". Callers cannot distinguish "created" from "already there".
Dropping onto an occupied input **replaces** silently, "which is what every node editor
does and what the gesture reads as".

> It is called `link`, not `connect`, because a member of that name shadows
> `QObject::connect` inside every other member function of the class — "a remarkably
> annoying way to lose an afternoon".

---

## Cycles: three independent guards, none redundant

Nothing in a layer stack could be cyclic. This is a genuinely new failure mode, and it
is guarded in three separate places because each is reachable without the others.

| Guard | Where | Protects against |
|---|---|---|
| `reachesUpstream` | `NodeGraph.cpp:224-239`, used by `canLink` | Building a cycle through the editor |
| `onPath` path-set | `RenderList::flatten` | A **hand-edited or corrupt file**, which never goes through `canLink`. Unguarded recursion on A→B→A is a hard crash, not a bad picture |
| `seen` set | `AudioSourceNode::audioRoot` | A separator loop — "an unguarded walk over A→B→A never returns" |

(A fourth, `depthOf`'s `onPath`, keeps `autoLayout` terminating.)

`reachesUpstream` is a recursive DFS following **every** input port — audio included,
because "separator → separator → separator loops just as readily as a visual chain
does". The `seen` set is a memoizing visited set, which is correct and cheaper for a
pure reachability question.

Since `resetNodes` applies loaded edges **through `link()`**, a cyclic file can never
produce a cyclic in-memory graph. That is exactly why
`tst_graph::flattenSurvivesACycleBuiltByHand` has to *simulate* the re-entrant state
rather than build one.
→ [ADR-0008](../decisions/0008-two-independent-cycle-guards.md)

---

## The deletion cascade

`NodeGraph.cpp:185-220`, in order:

1. `canRemove` gate — the node exists **and** is not `essential`.
2. **Break every incoming edge first** — a nested sweep over all nodes × all ports,
   nulling any pointer equal to the victim, "so nothing downstream is left holding a
   pointer to a deleted node".
3. `beginRemoveRows` / `removeAt` / `endRemoveRows`.
4. `disconnect(victim, …)` **then** `victim->deleteLater()`. Order matters:
   `deleteLater` so a QML binding still evaluating this node survives the turn;
   disconnect first "or its dying property writes would re-dirty the document".
5. Clear the selection if the victim was selected.
6. Emit `countChanged`, `topologyChanged`, `graphChanged`.

The victim's id is **never recycled** — `m_nextNodeId` is monotone and is reset only by a
document load, which re-derives it from the loaded maximum.
→ [ADR-0007](../decisions/0007-node-ids-are-never-reused.md)

---

## The two signals

This is the most load-bearing line of design in the class.

| Signal | Emitted for | Listener |
|---|---|---|
| `topologyChanged` | nodes or edges added/removed | **`RenderList::rebuild` — and nothing else** |
| `graphChanged` | anything at all, including a property edit or a drag | `Project::setModified(true)` |

From `NodeGraph.h:136-146`:

> "Kept separate from graphChanged because dragging a node writes graphX at 60 Hz, and a
> render list that re-flattened on that would reset the canvas model every frame of every
> drag. **This one line of signal design decides whether the editor feels good or
> broken.**"

`connections` re-emits on `topologyChanged` only, so edge delegates rebuild on a rewire
and never on a drag.
→ [ADR-0006](../decisions/0006-topology-vs-graph-changed.md)

---

## Port index *is* compositing order

`NodePorts.cpp:9-13` explains three behaviours at once:

> "Port ORDER is layout order: inputs run left to right along a node's top edge. It is
> also compositing order — port 0 is the bottom of the stack — which is why RenderList
> walks the inputs in index order rather than by name, and why Merge's `background` port
> must come first."

Swap a Merge's two edges and the draw order flips. That is the whole of Merge's
behaviour; there is no code on the class.
→ [ADR-0021](../decisions/0021-port-index-is-compositing-order.md)

---

## Loading: defensive at every level

`resetNodes` (`NodeGraph.cpp:459-523`) takes ownership of parsed nodes and resolves
edges. Its policy is that **a slightly wrong file should open and be fixable rather than
refuse**:

| Situation | Result |
|---|---|
| A second Output or Audio node | `qWarning`, the extra is deleted |
| No Output / no Audio | `qWarning`, one is **synthesized** |
| Unknown node type | silently skipped (`createNode` returns null) |
| Unknown property key | silently ignored by `setProperty` |
| Invalid edge | `qWarning` naming it, **that edge only** is dropped |
| Unknown port-kind string | falls back to `Visual` |

The only hard rejections are a JSON parse failure and a wrong format version.
→ [ADR-0022](../decisions/0022-defensive-load.md)

`resetNodes` calls `m_renderList->rebuild()` **explicitly before its signals**, so
anything reacting to `topologyChanged` or `selectionChanged` already sees the correct
draw list; the subsequent rebuild is a no-op thanks to the equality check.

### `loadDefaults`

Five nodes with fixed ids — output 1, audio 2, separator 3, background 4, spectrum 5 —
wired `2→3.audio`, `3→5.audio`, `4→5.visual`, `5→1.visual`, then `autoLayout()`. It is a
**working project out of the box**: bars move without the user having to discover that an
analyser is needed first.

> A default-constructed `NodeGraph` has **no Output at all** — a state no public path in
> the app produces. Both test files carry a `minimalGraph()` helper for this reason.

---

## `autoLayout`

Rows by longest-path depth, columns by node id within a row; grid constants `kMargin 40`,
`kColumnStep 224`, `kRowStep 150`. Sorting by id makes it **stable** — pressing Arrange
twice shuffles nothing.

Depth is the **longest** path from any input-less node, not the shortest: "with the
shortest path, a spectrum fed by both a background (depth 0) and a separator (depth 1)
would land level with the separator".

> **These constants are duplicated in `tools/migrate_v2_to_v3.py`** with no shared source.
> If they drift, a converted file jumps the first time anyone presses Arrange.

---

## The QML surface

| `Q_INVOKABLE` | Used by |
|---|---|
| `addNode(typeId, x, y)` | add menu |
| `removeNode` / `canRemove` | `Delete` shortcut |
| `canLink` / `link` / `unlink` | drag-to-connect |
| `nodeById` / `nodeAt(row)` | card lookup, port hit test |
| `portsFor(typeId)` | `NodeCard` port layout |
| `outputKindFor(typeId)` | `PortDot` |
| `creatableTypes()` | add menu model (everything not `essential`) |
| `autoLayout()` | Arrange button |

Model roles: `nodeObject`, `name`, `typeId`, `layerVisible`.

---

## Tests that pin this

| Test | Protects |
|---|---|
| `portTableCoversEveryNodeType` | the exact ten-type set; no duplicate ids; `createNode` refuses unknown types |
| `portTableInvariantsHold` | port names unique per node; `accepts` non-empty; every Audio port required and unstacked |
| `onlyDataDependenciesAreRequired` | the exact required-port lists |
| `maskInputsAcceptVisualOutputs` | the Mask rules, ahead of first use |
| `portKindNamesRoundTrip` | kind ⇄ string, unknown → `Visual` |
| `mismatchedTypesCannotConnect` | `canLink` and `link` agree |
| `cyclesAreRejected` | 2-node, 3-node, audio-only and mixed cycles |
| `aCycleThroughAMergesSecondInputIsRefused` | Merge's second stacked input closes no loop |
| `flattenSurvivesACycleBuiltByHand` | the flatten's independent guard |
| `connectingAnOccupiedInputReplacesTheEdge` | replacement, and that there is nowhere for a second source |
| `deletingANodeBreaksItsConnections` | no dangling pointers; selection cleared |
| `nodeIdsAreNotReusedAfterDeletion` | 20 add/remove cycles never reissue an id |
| `essentialNodesCannotBeCreatedOrRemoved` | output and audio are fixed |
| `defaultProjectHasExactlyOneOutputAndOneAudio` | `loadDefaults` is a working project |
| `oneOutputFeedsManyInputs` | fan-out costs one analysis, not one per consumer |
| `unconnectedSeparatorsAreNotAnalyzed` | `audioRoot` is the liveness predicate |
| `chainedSeparatorsBothResolveToTheSameAudioRoot` | chained separators do not cascade |
| `renderListIgnoresPropertyChanges` | the topology/graph signal split |

---

## See also

- [render-list](render-list.md) — what the flatten does with `stacked` and port order
- [node-reference](node-reference.md) — the properties on each type
- [guides/add-a-node-type](../guides/add-a-node-type.md)
