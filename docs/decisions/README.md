# Architecture decision records

Twenty-two decisions that shape Krave. Each record states the context, the decision, its
consequences, what would have to change to revisit it — and **the test that pins it**.

Most were recovered from commit messages, several of which carry measured before/after
tables against a reference video. That reasoning was previously invisible unless you knew
to run `git log`.

---

## Why each ADR names a test

The test suite is 59 cases whose **names are invariants**, not descriptions. So a decision
here is not just prose: break it and `ctest` fails, and the failing name leads straight
back to the record explaining why the rule exists.

**Three decisions have no pinning test.** That is recorded honestly rather than hidden,
and it is the natural backlog:

| Unpinned | What a test would have to do |
|---|---|
| [ADR-0011](0011-fixed-step-animation-driver.md) — fixed-step animation driver | Render the same project at 30 and 60 fps and compare mean luma at equal timestamps |
| [ADR-0012](0012-blend-via-premultiplied-alpha.md) — blend via premultiplied alpha | Assert composited output for `add` and `screen` against known values |
| [ADR-0019](0019-normalized-geometry.md) — normalized geometry | Render one project at two resolutions and compare normalized layer boxes |

All three live in the executable's untested half — see
[Known discrepancies](../README.md#known-discrepancies).

---

## The graph

| # | Decision | Pinned by |
|---|---|---|
| [0001](0001-graph-replaces-layer-stack.md) | The graph replaces the layer stack; layering is a consequence of topology | `renderListFollowsTheVisualChain` |
| [0002](0002-audio-edge-selects-a-source.md) | An audio edge selects a source; it does not carry numbers | `chainedSeparatorsBothResolveToTheSameAudioRoot` |
| [0003](0003-port-table-in-cpp.md) | The port table lives in C++, not QML | `portTableCoversEveryNodeType` |
| [0004](0004-connections-keyed-by-port-name.md) | Connections are serialized by port name, never by index | `connectionsRoundTripByPortName` |
| [0005](0005-accepts-list-on-inputs.md) | Inputs declare what they `accept`, so compatibility needs no special case | `maskInputsAcceptVisualOutputs` |
| [0006](0006-topology-vs-graph-changed.md) | `topologyChanged` is separate from `graphChanged` | `renderListIgnoresPropertyChanges` |
| [0007](0007-node-ids-are-never-reused.md) | Node ids are never reused within a session | `nodeIdsAreNotReusedAfterDeletion` |
| [0008](0008-two-independent-cycle-guards.md) | Cycles are rejected at link time **and** guarded again in the flatten | `flattenSurvivesACycleBuiltByHand` |
| [0009](0009-required-marks-data-dependencies.md) | `required` marks data dependencies only | `onlyDataDependenciesAreRequired` |
| [0021](0021-port-index-is-compositing-order.md) | Port index *is* compositing order | `mergeDrawsItsBackgroundArmFirst` |

## Rendering

| # | Decision | Pinned by |
|---|---|---|
| [0011](0011-fixed-step-animation-driver.md) | Export installs a fixed-step animation driver | *none* |
| [0012](0012-blend-via-premultiplied-alpha.md) | Blend modes exploit premultiplied source-over; `multiply` is not offered | *none* |
| [0013](0013-transform-is-identity-blur-is-approximation.md) | Transform is per-layer by identity; blur is per-layer by approximation | `blursAccumulateOutermostFirst` |
| [0019](0019-normalized-geometry.md) | All geometry is normalized 0..1 | *none* |

## Audio and DSP

| # | Decision | Pinned by |
|---|---|---|
| [0010](0010-smoothing-is-a-time-constant.md) | Smoothing is a time constant, never a per-frame factor | `analyzerSmoothingIsFrameRateIndependent` |
| [0014](0014-zero-pad-the-analysis-window.md) | Zero-pad the analysis window rather than round it to a power of two | `windowLengthSetsResolutionContinuously` |
| [0015](0015-linear-amplitude-and-expansion.md) | Linear amplitude, a black level, and expansion rather than compression | `bandNormalizationFlattensWithoutCrushingDynamics` |
| [0016](0016-unresolvable-bands-are-flagged.md) | Bands that collapse onto one bin are flagged, not drawn | `unresolvableLowBandsAreFlagged` |

## Persistence and structure

| # | Decision | Pinned by |
|---|---|---|
| [0017](0017-no-audio-path-in-project.md) | A `.mvproj` stores no audio path; `session.json` does | `projectFileContainsNoAudioPath` |
| [0018](0018-no-in-app-migration.md) | No in-app migration; a one-shot script instead | `rejectsNewerFormatVersion` |
| [0020](0020-member-properties-and-meta-walk.md) | Properties are `MEMBER`, so serialization walks the meta-object | `roundTripPreservesEveryProperty` |
| [0022](0022-defensive-load.md) | A slightly wrong file opens and is fixable, rather than being refused | `loadingAFileWithoutAnOutputSynthesizesOne` |

---

## Writing a new one

Add a record when a decision **spans files** — when the reason for a choice lives in the
relationship between components rather than inside one class. Class-local rationale
belongs in the header comment, where an editor will actually see it.

Copy the shape of any existing record: Status · Source · Pinned by, then Context,
Decision, Consequences, Revisiting it. If there is no test, say so and describe what one
would have to assert.
