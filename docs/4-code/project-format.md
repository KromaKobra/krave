# 4 · File formats — `.mvproj` and `session.json`

**Sources:** `src/app/Project.h`, `src/app/Project.cpp`, `src/app/Session.h`, `src/app/Session.cpp`, `tools/migrate_v2_to_v3.py`, `examples/*.mvproj`

Two files, deliberately holding different kinds of state.

---

## Why there are two

| File | Holds | Portable |
|---|---|---|
| **`.mvproj`** | The scene — nodes, properties, graph positions, edges, canvas settings | **Yes.** It stores **no audio path** |
| **`session.json`** | Last project, last track, playback volume | No — it is about *this machine* |

A project is a reusable **template**; the user re-links a track after loading one. That
split is what lets you hand someone a project file without also handing them your music
library. → [ADR-0017](../decisions/0017-no-audio-path-in-project.md)

---

## `.mvproj` — format version 3

Written with `QJsonDocument::Indented` through a **`QSaveFile`**, so a failed write
cannot destroy the previous file.

```jsonc
{
  "format":  "music-visualizer-project",   // constant; written but NEVER validated on load
  "version": 3,                            // == Project::kFormatVersion

  "canvas": {
    "width":  1920,                        // default on read
    "height": 1080,
    "color":  "#ff0b0b12"                  // #AARRGGBB
  },

  "nodes": [
    {
      "id":   5,                           // nodeId — a SIBLING of props, not inside it
      "type": "spectrum",                  // typeId — likewise
      "props": {
        "name":   "Spectrum",
        "graphX": 264.0,
        "graphY": 340.0,
        "colorA": "#ff4cc9f0",             // QColor as #AARRGGBB
        "arc":    0.15999999999999992      // raw double; no rounding on save
        // ...every writable, stored Q_PROPERTY the class declares
      }
    }
  ],

  "connections": [
    { "from": 3, "to": 5, "port": "audio" }   // port NAME, never an index
  ]
}
```

### The `id` / `type` trick

Both sit **beside** `props`, not inside it. That is not cosmetic: it keeps `nodeId` and
`typeId` **non-writable**, which is exactly what keeps them out of the meta-object walk
that produces `props`.

### Serialization is a `QMetaObject` walk

`Project.cpp:21-44`:

```cpp
for (int i = Node::staticMetaObject.propertyOffset(); i < meta->propertyCount(); ++i) {
    const QMetaProperty prop = meta->property(i);
    if (!prop.isWritable() || !prop.isStored()) continue;
    // QColor → name(QColor::HexArgb); everything else → QJsonValue::fromVariant
}
```

Starting at `Node`'s property offset skips `QObject::objectName`; running to the concrete
class's count means subclass properties come along for free.

**A new `Q_PROPERTY(… MEMBER … NOTIFY changed)` on a node persists with zero extra
code.** Conversely, a property declared *without* `MEMBER`, or marked not-stored, will
silently reset to its default on every load — which is what
`tst_project::blurNodesRoundTripEverySetting` exists to catch.
→ [ADR-0020](../decisions/0020-member-properties-and-meta-walk.md)

Loading is the mirror: `createNode(type, id)` then a blanket `setProperty(key, value)`
over every key in `props`. `setProperty` converts through the property system, so
`"#ff4488"` lands correctly in a `QColor`. **Unknown keys are silently dropped by Qt.**

### Edges

A flat list keyed by port **name** rather than a nested `inputs` object per node —
"readable, diffable, and survives the port table being reordered".
→ [ADR-0004](../decisions/0004-connections-keyed-by-port-name.md)

---

## What is deliberately not saved

| Not saved | Why |
|---|---|
| **The audio / track path** | Projects are reusable templates. Pinned by `projectFileContainsNoAudioPath`, which also asserts no `.mp3/.wav/.flac/.m4a/.aac/.ogg` substring and no absolute path appears anywhere in the file |
| `typeId` inside `props` | CONSTANT ⇒ not writable ⇒ skipped; emitted as the `type` sibling |
| `nodeId` inside `props` | read-only ⇒ skipped; emitted as the `id` sibling |
| `bands`, `barCount`, `inactiveEdgeBars`, `level`, `bass`, `mid`, `treble` | Read-only live data |
| `objectName` | Below `Node`'s property offset |
| **The draw order** | Derived from topology by `RenderList`. Nothing persists it |
| Selection, dirty flag, window/editor state | Not modelled |
| Playback position, volume | `session.json`'s job |

The absence of a `"layers"` key is asserted too — "a file still claiming one would be a
half-done migration".

---

## Version gating

```
version = root["version"].toInt()        // a missing key reads as 0

version > 3  → "This project was made with a newer version of the app
                (format N, this build understands 3)."
version < 3  → "…is an older project format (N); this build reads format 3 only.
                Convert it with tools/migrate_v2_to_v3.py, which writes a new file
                and leaves the original alone."
```

**There is no in-app migration at all**, and the error text names the way out — "or the
user is simply stuck". v3 replaced the ordered layer stack with a node graph: `layers`
became `nodes` plus an explicit `connections` edge list.
→ [ADR-0018](../decisions/0018-no-in-app-migration.md)

Note `"format"` is written but never checked on load; only `version` gates.

### Load sequence

1. No graph attached → error.
2. Open file → error naming it.
3. Parse; a `QJsonParseError` or a non-object root → error naming the parse failure.
4. Version gates (above) — **the only hard rejections**.
5. Canvas fields, each with a default.
6. **Pass 1** — build every node with the id from the file. Unknown types return
   `nullptr` and are skipped.
7. **Pass 2** — build the raw edge list.
8. `NodeGraph::resetNodes(nodes, edges)`, which validates every edge through `canLink`
   and synthesizes missing essentials. → [ADR-0022](../decisions/0022-defensive-load.md)
9. `setFilePath`, emit `canvasChanged` and `loaded`, then **`setModified(false)`** —
   `resetNodes` marked the document dirty, but a freshly loaded file is not.

`Project` listens to `NodeGraph::graphChanged`, not `topologyChanged`: "a property edit
and a moved node are both edits, and both should mark the document dirty".

---

## `Project`'s own properties

| Property | Default | Notes |
|---|---|---|
| `filePath` | `""` | read-only |
| `displayName` | `"Untitled"` | `QFileInfo::completeBaseName()` when a path exists |
| `modified` | `false` | drives the unsaved dot in the top bar |
| `lastError` | `""` | |
| `canvasWidth` | `1920` | |
| `canvasHeight` | `1080` | |
| `canvasColor` | `#0b0b12` | |

`newProject()` resets the canvas, calls `NodeGraph::loadDefaults()`, clears the path and
error, and marks unmodified. `saveCurrent()` returns false when there is no path yet.

---

## `session.json`

Format version **1**, under the platform app-config directory
(`~/.config/kroma/Krave/session.json` on Linux).

```json
{ "version": 1, "lastProject": "/path/to/x.mvproj",
  "lastAudio": "/path/to/track.mp3", "volume": 0.8 }
```

- Written through a **`QSaveFile`** after `mkpath` (QSaveFile will not create the
  directory), so "a failed write leaves the previous session intact rather than
  truncating it".
- Writes are **debounced 500 ms**; the destructor and `aboutToQuit` both `flush()`.
- `load()` silently falls back to defaults on an empty path, an unopenable file, a
  non-object root, or a version mismatch. **Loading is not an edit** — it clears the
  dirty flag at the end, so a load cannot trigger a write-back.
- `Session` is a pure C++ collaborator and is **never exposed to QML**.

### The startup ordering discipline

`main.cpp:57-105` is order-sensitive in three ways, each commented in place:

1. `Session` is constructed **after** `setApplicationName`/`setOrganizationName`, because
   its default path argument is evaluated at the call site and resolves the config dir
   from those names.
2. The restored values are **snapshotted before anything emits**, because
   `newProject()` clears the project path and the save hooks would write that empty value
   straight over what we came to restore.
3. **Only then** are the three save hooks connected.

A remembered file that has since moved is not loaded and not a modal error — it becomes a
line in the dismissable `NoticeBar` under the toolbar. "The existence check keeps a
deleted file from reaching the decoder and raising the modal error dialog on every single
launch."

---

## Migration: `tools/migrate_v2_to_v3.py`

Self-described as throwaway — "DELETE THIS FILE once your projects are converted."

Writes `<name>-v3.mvproj` **alongside** the input rather than over it, because "saving
over the original under the new build would leave you with a file the old build cannot
open and no way back". `--in-place` overrides.

What it does:

- `layers` → `nodes` (each with a stable `id`) + `connections`.
- Array order **was** the z-stack, index 0 at the bottom — so index 0 is the **head** of
  the visual chain and **no reversal is needed**.
- old `"audio"` provider → `"separator"`; a new essential `"audio"` node holding the PCM
  feeds every separator.
- a new essential `"output"` terminates the visual chain. Ids 1 and 2 are reserved for
  output and audio "so they are the same in every converted file".
- `"bars"` → `"spectrum"`, and its hidden `providerId` becomes a visible edge.
- Placement properties are stripped from separators — "they never did anything".
- `orientation: "mirrored"` → `linear` + `mirrorBars`.
- **`"particles"` layers are dropped, loudly.** Recover from git if needed.
- Background **images** are not auto-converted — dropped with a warning telling you to add
  an Image node, because "an Image node placed over this background is a second node with
  its own placement — a judgement call rather than a mechanical rewrite".

> **The `providerId` coincidence.** A v2 file's implicit layer id for array element *i*
> was *i+1*, because `resetLayers` numbered from 1 in array order. That coincidence "is
> exactly what made `providerId` look like it worked", and it is what lets the script
> resolve it. → [ADR-0001](../decisions/0001-graph-replaces-layer-stack.md)

> The script duplicates `NodeGraph::autoLayout()`'s grid constants (`40 / 224 / 150`)
> with no shared source. If they drift, a converted file jumps the first time anyone
> presses Arrange.

---

## Tests that pin this

| Test | Protects |
|---|---|
| `roundTripPreservesEveryProperty` | 26 properties pushed off default, compared back **via a meta-walk** so later additions are covered automatically |
| `projectFileContainsNoAudioPath` | no track path, no audio extension, no absolute path, no `layers` key |
| `blurNodesRoundTripEverySetting` | all 12 blur properties — pins that the meta-walk persists new properties for free |
| `transformNodesRoundTripTheirPlacement` | all 7, likewise via meta-walk |
| `imageNodesRoundTripTheirSourceAndCrop` | a `file://` URL **with a space**; four distinct crop values so a transposed pair cannot pass |
| `connectionsRoundTripByPortName` | on-disk `port` values are names, never indices |
| `aMergesTwoEdgesSurviveAsDistinctPorts` | two edges into one node keep their own ports and compositing order |
| `nodeIdsAreStableAcrossSaveAndLoad` | a deletion first, so surviving ids are *not* 1..n in array order |
| `graphPositionsRoundTrip` | `autoLayout` actually moves something, then positions survive |
| `rejectsNewerFormatVersion` | forward-version refusal |
| `loadingAFileWithoutAnOutputSynthesizesOne` | synthesized ids do not steal one the file already used |
| `loadingAFileWithAnInvalidConnectionDropsOnlyThatEdge` | exactly three warnings, one good edge survives |
| `everyExampleProjectLoads` | every bundled `.mvproj` opens with a non-empty render list — the cheapest guard on the migration script's output |
| `tst_session::*` | round-trip, missing/malformed/unknown-version fallbacks, `loadIsNotAnEdit`, volume clamping |

---

## See also

- [node-reference](node-reference.md) — what ends up in `props`
- [graph-rules § Loading](graph-rules.md#loading-defensive-at-every-level)
- [guides/add-a-node-property](../guides/add-a-node-property.md)
