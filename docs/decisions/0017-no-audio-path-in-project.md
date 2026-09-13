# ADR-0017 — A `.mvproj` stores no audio path; `session.json` does

**Status:** Accepted · **Source:** `src/app/Project.h:20-21`, `src/app/Session.h:9-16` · **Pinned by:** `projectFileContainsNoAudioPath`, `tst_session::roundTrip`, `tst_session::loadIsNotAnEdit`

## Context

When you save a project you have a track loaded. The obvious thing is to save its path so
reopening the project reopens the track.

But there are two different kinds of state tangled together: **what the scene is**, and
**what this machine was last doing**.

## Decision

Split them across two files.

| File | Holds | Portable |
|---|---|---|
| `.mvproj` | Nodes, properties, graph positions, edges, canvas settings | **Yes** — no audio path, no absolute paths |
| `session.json` | Last project, last track, playback volume | No — it is about this machine |

`session.json` lives under the platform app-config directory
(`~/.config/kroma/Krave/` on Linux), not beside the project.

## Consequences

> "The project file intentionally stores NO audio path. Projects are meant to be reusable
> templates; the user re-links a track after loading one."

And from `Session.h`: a `.mvproj` is a portable template, while "the track, and where the
volume was left, belong to this machine's session rather than to the document".

So you can hand someone a project file without also handing them your music library, and
a project works as a **preset** — apply it to any track.

The user still gets the convenience: the editor reopens the last project, the last track
and the volume on launch. That just comes from the other file.

`projectFileContainsNoAudioPath` asserts this quite aggressively — no `trackPath` key, no
`audioPath` key, **no `.mp3/.wav/.flac/.m4a/.aac/.ogg` substring anywhere in the file, and
no absolute path at all**. A leak would otherwise be easy to miss.

### The consequences for startup

A remembered file that has since moved must not become a modal error on every launch. It
is checked for existence, skipped, and reported in a dismissable `NoticeBar` strip under
the toolbar:

> "The existence check keeps a deleted file from reaching the decoder and raising the modal
> error dialog on every single launch; a missing file is a quiet notice instead."

And `main.cpp`'s startup is order-sensitive because of this split: the restored values must
be **snapshotted before anything emits**, because `newProject()` clears the project path
and the save hooks — "which is exactly why they are not connected yet" — would write that
empty value straight over what we came to restore.

`Session` writes through a `QSaveFile` after `mkpath`, debounced 500 ms, and flushes on
`aboutToQuit`. Loading is explicitly **not** an edit, so a load cannot trigger a write-back.

## Revisiting it

An optional, clearly-marked "remember this track with the project" flag would be
compatible with the intent — the objection is to storing it *unconditionally and
invisibly*, which is what makes a project unshareable without anyone noticing.

If that is ever added, `projectFileContainsNoAudioPath` must be updated deliberately, and
the default must stay off.
