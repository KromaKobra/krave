# ADR-0018 — No in-app migration; a one-shot script instead

**Status:** Accepted · **Source:** `src/app/Project.h:36-43`, `tools/migrate_v2_to_v3.py` · **Pinned by:** `rejectsNewerFormatVersion`, `everyExampleProjectLoads`

## Context

v3 replaced the ordered layer stack with a node graph
(→ [ADR-0001](0001-graph-replaces-layer-stack.md)). `layers` became `nodes` plus an
explicit `connections` edge list — a structural change, not a field rename.

The app already carried `migrateFromV1` and `migrateLegacyValues` from an earlier bump.
Adding `migrateFromV2` would mean carrying all three forever.

## Decision

`load()` **rejects** anything that is not exactly format 3, and the error message names the
conversion script. `tools/migrate_v2_to_v3.py` does the job once, offline.

## Consequences

> "Keeping an in-app migration would have meant carrying migrateFromV1,
> migrateLegacyValues and a new migrateFromV2 forever, and pinning the typeId strings they
> read; tools/migrate_v2_to_v3.py does it once instead."

That last clause is the sharp one: in-app migration code **freezes the old type strings
into the current build** permanently. Every future rename has to keep the old names alive
for the migration path.

The script inherits that burden instead, and is explicitly disposable — "DELETE THIS FILE
once your projects are converted".

The error text must name the way out, "or the user is simply stuck":

```
…is an older project format (N); this build reads format 3 only.
Convert it with tools/migrate_v2_to_v3.py, which writes a new file
and leaves the original alone.
```

The script writes `<name>-v3.mvproj` **alongside** the input rather than over it, because
"saving over the original under the new build would leave you with a file the old build
cannot open and no way back".

A missing `"version"` key reads as 0, so it is rejected with the same migration message.
`"format"` is written but never validated.

### What the script has to know

Because it does structurally what the app no longer can, it duplicates knowledge:
- The v2 `providerId` → edge resolution, which works **only** because a v2 file's implicit
  layer id for array element *i* was *i+1*. "That coincidence is exactly what made
  providerId look like it worked."
- `NodeGraph::autoLayout()`'s grid constants, with **no shared source**. If they drift, a
  converted file jumps the first time anyone presses Arrange.
- Which properties were visual-only and must be stripped from separators.

It drops particle layers loudly, and refuses to guess at background images — "a judgement
call rather than a mechanical rewrite".

`everyExampleProjectLoads` is the cheap guard on all of it: every bundled `.mvproj` must
open with a non-empty render list.

## Revisiting it

The pattern generalises. For the next format break: bump `kFormatVersion`, write a new
one-shot script, keep the rejection message pointing at it, and let
`everyExampleProjectLoads` verify the converted examples.

In-app migration only becomes worth it if users routinely open old files — which, for a
tool whose projects are personal presets, they do not.
