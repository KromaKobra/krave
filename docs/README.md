# Krave documentation

Krave is a desktop music visualizer: import a track, build a frequency-reactive scene
as a **node graph**, preview it live against the audio, and export a video with the
visualizer burned in. C++17 core, Qt Quick / QML front end, ffmpeg for decode and
encode.

This documentation is organised on the [C4 model](https://c4model.com/) — four levels
of zoom. Enter at the level that matches the size of what you are trying to do, and
descend only into the branch you need.

---

## Start here: what are you trying to do?

| I want to… | Read, in order |
|---|---|
| **Understand what this program is** | [1 · Context](1-context/README.md) |
| **Get it building and running** | [guides/build-and-run](guides/build-and-run.md) |
| **Add a new node type** | [guides/add-a-node-type](guides/add-a-node-type.md) → [4 · node-reference](4-code/node-reference.md) → [4 · graph-rules](4-code/graph-rules.md) |
| **Add a property to an existing node** | [guides/add-a-node-property](guides/add-a-node-property.md) |
| **Change how the audio is analysed** | [3 · audio-and-analysis](3-components/audio-and-analysis.md) → [4 · dsp-pipeline](4-code/dsp-pipeline.md) |
| **Make the bars look different** | [4 · node-reference § Spectrum](4-code/node-reference.md#spectrumnode--spectrum) → [3 · rendering](3-components/rendering.md) |
| **Fix something about export** | [3 · export](3-components/export.md) → [4 · export-pipeline](4-code/export-pipeline.md) |
| **Understand why export matches preview** | [1 · Context § the founding idea](1-context/README.md#the-founding-idea) → [ADR-0010](decisions/0010-smoothing-is-a-time-constant.md), [ADR-0011](decisions/0011-fixed-step-animation-driver.md) |
| **Work on the node editor UI** | [3 · editor-ui](3-components/editor-ui.md) → [4 · qml-kit](4-code/qml-kit.md) → [4 · qt-quirks](4-code/qt-quirks.md) |
| **Add or change a visual effect / shader** | [guides/add-a-shader-effect](guides/add-a-shader-effect.md) → [4 · shaders](4-code/shaders.md) |
| **Change the save-file format** | [4 · project-format](4-code/project-format.md) → [ADR-0017](decisions/0017-no-audio-path-in-project.md) |
| **Understand the draw order / layering** | [4 · render-list](4-code/render-list.md) → [ADR-0001](decisions/0001-graph-replaces-layer-stack.md) |
| **Write or run tests** | [guides/testing-and-debugging](guides/testing-and-debugging.md) |
| **Know why something is the way it is** | [decisions/](decisions/README.md) — 22 architecture decision records |
| **Avoid the traps** | [4 · qt-quirks](4-code/qt-quirks.md) — read this before touching QML |

---

## The four levels

### [1 · Context](1-context/README.md)
Krave as a single box in its world. Who uses it, what it talks to (ffmpeg, the
filesystem), and **the founding idea that everything else follows from**. One page.

### [2 · Containers](2-containers/README.md)
The four deployable/linkable units — `mvcore`, `mvapp`, the `krave` executable, the
ffmpeg subprocess — plus the **thread model** and what crosses each boundary.

### [3 · Components](3-components/)
Inside each container. One document per subsystem, each carrying the end-to-end flows
that no single file can show.

| Document | Covers |
|---|---|
| [audio-and-analysis](3-components/audio-and-analysis.md) | decode → buffer → analyzer → `AudioEngine` / `SpectrumEngine` |
| [document-model](3-components/document-model.md) | `Node`, `NodePorts`, `NodeGraph`, `RenderList`, `Project`, `Session` |
| [rendering](3-components/rendering.md) | `VisualizerCanvas` → layer views → blend modes → the blur chain |
| [editor-ui](3-components/editor-ui.md) | panels, inspectors, the graph editor, the control kit |
| [export](3-components/export.md) | offscreen render → ffmpeg, and how it stays identical to preview |

### [4 · Code](4-code/)
The reference tier. Exhaustive, and the place to look up an exact value.

| Document | Covers |
|---|---|
| [node-reference](4-code/node-reference.md) | All 10 node types and every property: type, default, range, what reads it |
| [dsp-pipeline](4-code/dsp-pipeline.md) | The exact order of operations in `SpectrumAnalyzer` |
| [graph-rules](4-code/graph-rules.md) | The port table, `canLink`, cycle detection, the deletion cascade |
| [render-list](4-code/render-list.md) | The flatten algorithm, step by step |
| [project-format](4-code/project-format.md) | `.mvproj` v3 schema, `session.json`, migration |
| [export-pipeline](4-code/export-pipeline.md) | `QQuickRenderControl`, `QRhiTexture`, ffmpeg arguments, backpressure |
| [qml-kit](4-code/qml-kit.md) | The 19 controls and the 4 singletons |
| [shaders](4-code/shaders.md) | The 4 `.frag` files, mathematically |
| [qt-quirks](4-code/qt-quirks.md) | The Qt/QML constraints this codebase is shaped by |

### [Decisions](decisions/README.md)
Twenty-two architecture decision records, mined from the commit history and the source
comments. Each states the context, the decision, its consequences, what would have to
change to revisit it — and **the test that pins it**, where one exists.

### [Guides](guides/)
Task recipes. [add-a-node-type](guides/add-a-node-type.md) is the flagship: every file
you must touch, derived from how the merge, transform and blur nodes were actually
added.

---

## How this documentation is kept honest

Two mechanisms, both deliberate:

**1. Docs carry only what no single file can carry.** Class-local rationale stays in
the header comment, where an editor will actually see it. These documents carry the
cross-cutting things a header cannot — end-to-end flows, invariants spanning files, the
decision history, navigation — and **link to `file:line`** rather than restating.
`Node.h` opens with a 30-line essay on why an audio edge selects a *source*; copying it
here would create a second source of truth that silently rots.

Every page opens with a `**Sources:**` line naming the files it describes. If those
files change, that page is the one to check.

**2. Decisions are pinned by tests.** The suite is 60 named cases, and the names are
invariants rather than descriptions — `analyzerSmoothingIsFrameRateIndependent`,
`nodeIdsAreNotReusedAfterDeletion`, `projectFileContainsNoAudioPath`. Each ADR names
the test enforcing it. Break the decision and `ctest` says so; the failing name leads
back to the record explaining why it exists.

See [CLAUDE.md](../CLAUDE.md) for the contract that keeps this in step with the code.

---

## Known discrepancies

Things the repository currently claims that the code no longer does. Documented here
rather than silently corrected, because fixing them is a code change:

- **No command-line flags work.** `main.cpp` contains no `QCommandLineParser` and never
  inspects `argv`. The headless `--render` mode was deleted in `5fa7fa8` (a rename
  commit — almost certainly collateral damage, no commit message mentions it).
  `.vscode/launch.json:63,137` still define launch configs passing `--render`; they
  silently open the editor instead. That removal also took the export pipeline's only
  regression harness (`--render --cancel-after N`) with it.
- **`Node.h:238` claims `windowMs` is "rounded up to a power of two internally".** It is
  not — `Node.cpp:176-180` and `SpectrumAnalyzer.h:61-66` both say the window is
  zero-padded rather than rounded, specifically so resolution is continuous. The header
  comment is stale. See [ADR-0014](decisions/0014-zero-pad-the-analysis-window.md).
- **`SeparatorNode::lowFreqMagnitude` may not take effect on its own.** It is folded
  into the band gains by `layoutBands()`, but it is not in `setParams`'s relayout
  trigger set (`SpectrumAnalyzer.cpp:52-70`). Looks like a live bug.
- **A stalled ffmpeg wedges an export silently.** Once the 32 MiB write high-water mark
  pauses the frame loop, only `QProcess::bytesWritten` resumes it. There is no
  watchdog, so the symptom is a frozen progress bar with no error.
- **`AudioEngine`, `SpectrumEngine` and `ExportController` have no tests at all** — they
  live in the executable, not in the `mvapp` library the tests link.
