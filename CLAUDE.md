# Working on Krave

A desktop music visualizer: C++17 core, Qt Quick/QML front end, ffmpeg for decode and
encode. Scene graphs of typed nodes, previewed live and exported to video.

## Start here

**[`docs/README.md`](docs/README.md)** — the documentation index, organised on the C4 model
(Context → Container → Component → Code) with an "I want to…" table that routes you to the
right level for your task.

Do not read the whole tree. Enter at the scope matching your change:

| Task | Entry point |
|---|---|
| Add a node type | [`docs/guides/add-a-node-type.md`](docs/guides/add-a-node-type.md) |
| Add a property to a node | [`docs/guides/add-a-node-property.md`](docs/guides/add-a-node-property.md) |
| Change the audio analysis | [`docs/4-code/dsp-pipeline.md`](docs/4-code/dsp-pipeline.md) |
| Touch anything in QML | [`docs/4-code/qt-quirks.md`](docs/4-code/qt-quirks.md) **first** |
| Change the save format | [`docs/4-code/project-format.md`](docs/4-code/project-format.md) |
| Understand draw order | [`docs/4-code/render-list.md`](docs/4-code/render-list.md) |
| Know why something is so | [`docs/decisions/`](docs/decisions/README.md) — 22 ADRs |

## Invariants — do not break these

Each is load-bearing, each has a record explaining why, and most fail loudly in `ctest` if
broken. The ones that fail *silently* are marked.

- **Smoothing is a time constant, never a per-frame factor.** Derive the blend factor as
  `1 - exp(-dt/tau)`. → [ADR-0010](docs/decisions/0010-smoothing-is-a-time-constant.md)
- **Never use `mapToItem` in a binding.** It is not binding-reactive: the result is correct
  on first paint and then *silently* never updates. In an event handler it is fine.
  → [qt-quirks](docs/4-code/qt-quirks.md)
- **Never use `drag.target` on a node card.** It destroys the `x: node.graphX` binding —
  the node moves on screen, nothing reaches the model, and it snaps back. *Silent.*
- **`SpectrumAnalyzer::setParams` must distinguish "rebuild buffers" from "a value
  changed".** It is called every frame; rebuilding unconditionally degrades attack/decay
  into a plain gain and stops auto-gain converging. *Silent, with a plausible picture.*
- **Never name a QML property or model role `layer`** — `QQuickItem::layer` is `FINAL`.
  Hence `layerObject` and `layerData`.
- **A `Loader` overwrites its item's `width`/`height`.** `LayerFrame`'s root therefore spans
  the canvas, which is why `frame.width` means the canvas, not the layer.
- **`NodeTypeSpec::visual` must agree with the class hierarchy** — `isVisual()` reads the
  flag, but `flatten` branches on `qobject_cast<VisualNode*>`. Nothing enforces this.
  *Silent.*
- **`topologyChanged` is for structure; `graphChanged` is for edits.** A drag writes
  `graphX` at 60 Hz; routing that to the render list strobes the canvas.
  → [ADR-0006](docs/decisions/0006-topology-vs-graph-changed.md)
- **Node ids are never reused within a session.** Analyzer state is keyed by id.
  → [ADR-0007](docs/decisions/0007-node-ids-are-never-reused.md)
- **All geometry is normalized 0..1**, never pixels — preview and 4K export must agree.
  → [ADR-0019](docs/decisions/0019-normalized-geometry.md)
- **`.mvproj` must never contain an audio path.** It is a portable template; `session.json`
  holds machine state. → [ADR-0017](docs/decisions/0017-no-audio-path-in-project.md)
- **`autoLayout()`'s grid constants are duplicated in `tools/migrate_v2_to_v3.py`.** Keep
  them in step or converted files jump on first Arrange. *Silent.*
- **Every new `.qml` must be added to `app_qml_files` in `src/app/CMakeLists.txt`.** All 49
  are listed explicitly; there is no glob.

## If you change X, update Y

| Changed | Also update |
|---|---|
| `src/app/Node.h` | `docs/4-code/node-reference.md` |
| `src/core/SpectrumAnalyzer.*` | `docs/4-code/dsp-pipeline.md` |
| `src/app/NodePorts.cpp` | `docs/4-code/graph-rules.md` (the port grid) |
| `src/app/RenderList.*` | `docs/4-code/render-list.md` |
| `Project.cpp` on-disk shape | `docs/4-code/project-format.md` **and bump `kFormatVersion`** |
| `src/app/ExportController.cpp` | `docs/4-code/export-pipeline.md` |
| `src/app/shaders/*` | `docs/4-code/shaders.md` |
| any new `.qml` | `src/app/CMakeLists.txt` **and** `docs/4-code/qml-kit.md` |
| a decision spanning files | a new ADR in `docs/decisions/` |

## How the docs are written

**Docs carry only what no single file can carry.** Class-local rationale stays in the
header comment, where an editor will see it — `Node.h` opens with a 30-line essay on why an
audio edge selects a *source*, and that is the right place for it. The docs carry
end-to-end flows, invariants spanning files, decision history, and navigation, and they
**link to `file:line`** rather than restating.

Do not copy header comments into markdown. That creates a second source of truth that rots.

Every page opens with a `**Sources:**` line. If you change those files, check that page.

Each ADR names **the test that pins it**. If you add a decision, name its test — or say
explicitly that none exists.

## Conventions

- Match the surrounding comment density. This codebase comments the *why*, at length, at
  the point of the decision. Follow that.
- `Q_PROPERTY(… MEMBER … NOTIFY changed)` for node properties — serialization and QML
  binding both come free from it.
- Build and test with `./scripts/build.sh --test`.

## Local notes

- **QML `console.log` prints nothing in this Qt build.** Debug via side effects — a visible
  property, a colour change. `qWarning` from C++ does work.
- **No command-line flags work.** `main.cpp` has no `QCommandLineParser`. The `--render`
  mode and `--audio` flag were removed in `5fa7fa8`; the README and
  `.vscode/launch.json` still reference them. See
  [Known discrepancies](docs/README.md#known-discrepancies).
