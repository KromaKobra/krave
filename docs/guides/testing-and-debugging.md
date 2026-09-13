# Guide · Testing and debugging

**Sources:** `tests/CMakeLists.txt`, `tests/tst_*.cpp`

---

## Running the tests

```bash
./scripts/build.sh --test
# or, inside the dev shell:
ctest --preset linux-debug
ctest --test-dir build/linux-debug --output-on-failure -R renderList   # one pattern
./build/linux-debug/bin/tst_graph                                      # one binary
./build/linux-debug/bin/tst_graph cyclesAreRejected                    # one case
```

## The four targets

| Target | Links | Platform plugin | Cases |
|---|---|---|---|
| `tst_fft` | `mvcore` | none needed | 13 |
| `tst_graph` | `mvapp` | `offscreen` | 24 |
| `tst_project` | `mvapp` | `offscreen` | 16 |
| `tst_session` | `mvapp` | none — `QTEST_GUILESS_MAIN` | 6 |

`tst_project` and `tst_graph` build a `QGuiApplication` only to own QML engine types, so
they are pinned to `QT_QPA_PLATFORM=offscreen` to survive SSH, CI and the Nix sandbox.
`tst_project` also gets `MV_EXAMPLES_DIR` and smoke-loads every bundled `.mvproj`.

They are kept as two separate binaries "so a broken flatten and a broken round-trip report
separately".

> **`AudioEngine`, `SpectrumEngine` and `ExportController` have no tests at all.** They
> live in the `krave` executable rather than in `mvapp`, so no test target links them. That
> is the largest coverage gap in the project.

---

## The suite as an executable spec

The 59 test names are **invariants, not descriptions**. Reading the list is a fast way to
learn what the system guarantees, and a failing name usually tells you what you broke
without opening the file.

### `tst_fft` — 13 cases

| Test | Invariant |
|---|---|
| `matchesNaiveDft` | the transform is correct |
| `sineLandsInOneBin`, `impulseGivesFlatSpectrum` | basic spectral behaviour |
| `rejectsNonPowerOfTwo` | `Fft` guards its size |
| `bufferReadMonoPadsEdges` | out-of-range frames read as **silence** — what lets a window centre anywhere |
| `bufferPeaksTrackAmplitude` | the waveform envelope |
| `analyzerFindsToneInRightBand` | band mapping |
| **`analyzerSmoothingIsFrameRateIndependent`** | **preview/export parity** — 0.5 s of decay at 30 and 60 fps agree within 0.02 |
| `analyzerDecaysToSilence` | envelopes fall to zero |
| **`analyzerKeepsStateWhenParamsAreResent`** | `setParams` does not clear state every frame — also asserts the envelope is *still falling*, so two equally-broken runs cannot pass |
| `windowLengthSetsResolutionContinuously` | zero-padding, not rounding |
| `unresolvableLowBandsAreFlagged` | `activeFrom` |
| `bandNormalizationFlattensWithoutCrushingDynamics` | auto-gain lifts without pinning |

### `tst_graph` — 24 cases

Port table: `portTableCoversEveryNodeType` (the **exact** ten-type set, spelled out so
adding one is a deliberate edit), `portTableInvariantsHold`,
`onlyDataDependenciesAreRequired`, `maskInputsAcceptVisualOutputs` (vacuous today, by
design), `portKindNamesRoundTrip`.

Graph: `defaultProjectHasExactlyOneOutputAndOneAudio`,
`essentialNodesCannotBeCreatedOrRemoved`, `mismatchedTypesCannotConnect`,
`oneOutputFeedsManyInputs`, `connectingAnOccupiedInputReplacesTheEdge`, `cyclesAreRejected`,
`aCycleThroughAMergesSecondInputIsRefused`, `flattenSurvivesACycleBuiltByHand`,
`deletingANodeBreaksItsConnections`, `nodeIdsAreNotReusedAfterDeletion`,
`unconnectedSeparatorsAreNotAnalyzed`, `chainedSeparatorsBothResolveToTheSameAudioRoot`.

Render list: `renderListIgnoresPropertyChanges` (the drag path),
`renderListSkipsUnreachableNodes`, `renderListDrawsASharedSubgraphTwice`,
`mergeDrawsItsBackgroundArmFirst`, `transformsAccumulateOutermostFirst`,
`blursAccumulateOutermostFirst`, `organizingNodesNeverAppearInTheRenderList`.

### `tst_project` — 16 cases

`roundTripPreservesEveryProperty`, `blurNodesRoundTripEverySetting`,
`transformNodesRoundTripTheirPlacement`, `imageNodesRoundTripTheirSourceAndCrop`,
`connectionsRoundTripByPortName`, `aMergesTwoEdgesSurviveAsDistinctPorts`,
`graphPositionsRoundTrip`, `nodeIdsAreStableAcrossSaveAndLoad`,
`projectFileContainsNoAudioPath`, `rejectsNewerFormatVersion`,
`loadingAFileWithoutAnOutputSynthesizesOne`,
`loadingAFileWithAnInvalidConnectionDropsOnlyThatEdge`, `everyExampleProjectLoads`,
`renderListFollowsTheVisualChain`, `audioSourcesAreDiscovered`, `mirrorSpectrumIsSymmetric`.

### `tst_session` — 6 cases

`roundTrip`, `missingFileLoadsDefaults`, `malformedFileLoadsDefaults`,
`unknownVersionIsIgnored`, `loadIsNotAnEdit`, `volumeIsClamped`.

---

## Writing a test

Patterns worth copying:

- **A default-constructed `NodeGraph` has no Output** — a state no public path produces.
  Both graph test files carry a `minimalGraph()` helper; use it.
- **Compare via a meta-walk**, as the round-trip cases do, so properties added later are
  covered without anyone remembering.
- **Push every value off its default** in a round-trip fixture, or a dropped field passes
  by luck.
- **Build fixtures in a non-obvious order** — `renderListFollowsTheVisualChain` constructs
  its chain in a *non-draw* order "so the list can only be right by following the edges".
- **Delete something first** when testing ids, so the surviving set is not `1..n`.
- **Assert both directions.** `mergeDrawsItsBackgroundArmFirst` swaps the edges and checks
  the order flips; a one-sided assertion would pass on a constant.
- Use `QSignalSpy` for signal-count invariants — that is how the topology/graph split is
  pinned.

If you add a decision that spans files, add an [ADR](../decisions/README.md) and name the
test in it.

---

## Debugging

### QML

> **`console.log`, `console.warn` and friends print nothing in this Qt build.**

Debug through side effects instead: bind a value to a visible `Text`, flip a colour, or
change an opacity. For shaders, write the channel you care about straight to `fragColor`.

`qmllint` is in the dev shell.

### C++

`gdb` and `lldb` are in the dev shell. In VS Code use the **CodeLLDB** configs — they read
`build/<preset>/dev.env` for the Qt environment, which is why a debug session finds the QML
modules without VS Code being launched from the shell. See
[build-and-run](build-and-run.md#vs-code--vscodium).

`qWarning` **does** work, and the codebase uses it for every recoverable load problem —
dropped edges, synthesized essentials, unknown types, cycles found in the flatten. Run from
a terminal when loading a suspect project; the warnings are the diagnostic.

### Analysis and rendering

- **Bars are frozen** → check the spectrum node's audio input actually reaches an
  `AudioNode`. `audioRoot()` returning null is the liveness predicate, and an unconnected
  spectrum draws **nothing** by design.
- **Bars behave oddly after editing** → suspect `setParams` state being cleared. See
  [dsp-pipeline](../4-code/dsp-pipeline.md#the-biggest-trap-setparams-runs-every-frame).
- **A separator control does nothing** → check the amplitude mode. Linear mode ignores
  `bandNormalization`, `gateDb`, `floorDb` and `ceilingDb` entirely.
- **The canvas strobes during a drag** → something reached `topologyChanged` that should
  have gone to `graphChanged`. → [ADR-0006](../decisions/0006-topology-vs-graph-changed.md)
- **An edge does not follow its node** → `mapToItem` in a binding.
  → [4 · qt-quirks](../4-code/qt-quirks.md)
- **A node snaps back after dragging** → `drag.target` destroyed the position binding.
- **A property resets on reload** → it is missing `MEMBER`, or is not stored.
- **A blur does not affect the background** → working as designed; blur is per-layer and a
  solid colour blurs to itself.
  → [ADR-0013](../decisions/0013-transform-is-identity-blur-is-approximation.md)

### Export

No automated coverage exists. Check by hand: export a short clip, confirm frame 0 matches
the preview at the same timestamp, and confirm cancelling mid-render leaves no partial
file.

> A frozen progress bar with no error usually means ffmpeg stalled past the 32 MiB
> write high-water mark — there is no watchdog. Cancel and check ffmpeg's stderr.

---

## See also

- [build-and-run](build-and-run.md)
- [4 · qt-quirks](../4-code/qt-quirks.md) — the traps, before you hit them
- [decisions/](../decisions/README.md) — each record names the test that pins it
