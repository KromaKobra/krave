# ADR-0022 — A slightly wrong file opens and is fixable, rather than being refused

**Status:** Accepted · **Source:** `src/app/NodeGraph.h:124-126`, `NodeGraph.cpp:459-523` · **Pinned by:** `loadingAFileWithoutAnOutputSynthesizesOne`, `loadingAFileWithAnInvalidConnectionDropsOnlyThatEdge`, `everyExampleProjectLoads`

## Context

Project files are hand-editable JSON, converted by an external script, and carry a graph
with structural requirements — exactly one Output, exactly one Audio node, edges that must
type-check.

Plenty can be slightly wrong. The question is whether a slightly wrong file should refuse
to open.

## Decision

Repair what can be repaired, drop what cannot, warn about everything, and refuse only when
the file cannot be read at all.

| Situation | Result |
|---|---|
| A second Output or Audio node | `qWarning`, the extra is deleted |
| No Output / no Audio | `qWarning`, one is **synthesized** |
| Unknown node type | silently skipped |
| Unknown property key | silently ignored by `setProperty` |
| **Invalid edge** | `qWarning` naming it; **that edge only** is dropped |
| Unknown port-kind string | falls back to `Visual` |
| JSON parse failure | **rejected** |
| Wrong format version | **rejected**, with the migration script named |

## Consequences

> "Defensive on purpose: a missing Output or Audio is synthesized and a duplicate Output is
> dropped, because a project file that is slightly wrong should open and be fixable rather
> than refuse."

and on edges specifically:

> "Dropping beats refusing the whole file — a corrupt edge should not lose a project."

The user gets their scene back and can rewire one connection, instead of a dialog and
nothing.

The same instinct appears further down: `portKindFromName` falls back to `Visual` for an
unknown string, so "a hand-edited file should lose one edge at most, not fail to load".

### The important side effect

Loaded edges are applied **through `link()`**, not by writing slots directly — so they go
through the full `canLink` validation, including cycle rejection. **A cyclic file can
therefore never produce a cyclic in-memory graph.**

That is why `flattenSurvivesACycleBuiltByHand` has to *simulate* re-entrancy rather than
load a bad file — and why the flatten still needs its own guard anyway, for whatever path
might one day bypass this one.
→ [ADR-0008](0008-two-independent-cycle-guards.md)

### Ordering detail

`resetNodes` calls `m_renderList->rebuild()` **explicitly, before emitting its signals**, so
anything reacting to `topologyChanged` or `selectionChanged` already sees the correct draw
list. The rebuild that `topologyChanged` then triggers is a no-op thanks to the equality
check.

And `Project::load` calls `setModified(false)` at the very end: `resetNodes` marked the
document dirty along the way, but a freshly loaded file is not an edit.

`loadingAFileWithoutAnOutputSynthesizesOne` additionally checks that synthesized ids **do
not steal an id the file already used** — a subtle way the repair could corrupt what it
was fixing.

## Revisiting it

The line to hold is between *repairable* and *unreadable*. Parse failures and version
mismatches are refused because there is nothing to recover; everything structural is
recovered.

The thing to preserve when adding new load-time validation is granularity: reject the
smallest unit that is actually broken — an edge, a node — never the document.
