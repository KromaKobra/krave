# ADR-0007 — Node ids are never reused within a session

**Status:** Accepted · **Source:** commit `3281a95`, `src/app/NodeGraph.h:159-163` · **Pinned by:** `nodeIdsAreNotReusedAfterDeletion`, `nodeIdsAreStableAcrossSaveAndLoad`

## Context

Nodes get integer ids. The obvious implementation reuses the lowest free one, or renumbers
on load — which is what the old layer stack did (`resetLayers` numbered from 1 in array
order).

But ids are not just labels here. `SpectrumWorker` keys its analyzers by node id:

```cpp
std::unordered_map<int, SpectrumAnalyzer> m_analyzers;
```

Each analyzer carries a smoothing envelope and an auto-gain reference — **live state that
evolves over time**.

## Decision

`m_nextNodeId` is monotone, never reset and never reused within a session. A document load
restarts it from the loaded maximum rather than from 1.

## Consequences

> "Reuse would let a deleted node's SpectrumAnalyzer in SpectrumWorker::m_analyzers be
> inherited by a new node, cross-wiring two smoothing envelopes into what looks like
> random flicker."

The failure it prevents is the worst kind: no crash, no warning, just bars that behave
oddly for reasons invisible in the graph.

It also makes the analyzer GC safe. `SpectrumWorker` erases analyzers whose node is no
longer live — "load-bearing now that ids are never reused: without it a deleted node's
envelope would linger forever". Because ids never collide, an analyzer surviving one extra
frame is harmless.

Visible evidence: `examples/SyrexAttemptv3-lightweight.mvproj` contains node ids 1–7 and
9–13. **Id 8 is simply gone**, and the file round-trips that way.

Two related resets are wired in `main.cpp`:
- `Project::loaded` → `SpectrumEngine::reset()`, because a freshly loaded file restarts ids
  from *its own* numbering — the one case where the never-reused invariant does not hold
  across documents. Without it, node 3 of the new project inherits node 3 of the old one's
  state for a frame.
- `AudioEngine::seeked` → `reset()`, for the unrelated reason that a jump is not a
  continuation.

`nodeIdsAreStableAcrossSaveAndLoad` deletes a node **first**, so the surviving ids are not
`1..n` in array order — otherwise a renumbering bug would pass.

## Revisiting it

Reuse only becomes safe if analyzer lifetime stops being keyed by id — for example if each
`AudioSourceNode` owned its analyzer directly rather than the worker holding a map. That
would be a reasonable refactor, and it would dissolve this decision rather than reverse
it.

Until then, treat any change that renumbers ids as a correctness change, not a cosmetic
one.
