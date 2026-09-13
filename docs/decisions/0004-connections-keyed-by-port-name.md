# ADR-0004 — Connections are serialized by port name, never by index

**Status:** Accepted · **Source:** commit `2599abb`, `src/app/NodePorts.h:21-24` · **Pinned by:** `connectionsRoundTripByPortName`, `aMergesTwoEdgesSurviveAsDistinctPorts`

## Context

In memory, an input is a slot at a known index — `Node::m_inputs[port]`. Writing that
index to disk would be the obvious encoding, and the smallest.

## Decision

On disk an edge is `{ "from": id, "to": id, "port": "<name>" }`. The `PortSpec::name`
string is a **stable identifier**.

## Consequences

> "STABLE: this string is what gets serialized as the `port` field of a connection, so
> the on-disk edge list survives the port table being reordered."

Concretely:
- Reordering or inserting ports in `NodePorts.cpp` cannot silently reinterpret an existing
  project file. With indices, inserting a port at position 0 would rewire every saved
  project.
- The file is **readable and diffable**. `"port": "foreground"` says what it means;
  `"port": 1` does not.
- Renaming a port name is now a **breaking format change**, and must be treated as one.

The in-memory and on-disk representations therefore differ on purpose, and `Node.h:107-109`
says so:

> "In memory an input is a SLOT … On disk it is a flat edge list keyed by port NAME
> instead, which is readable, diffable, and survives the port table being reordered."

A flat edge list rather than a nested `inputs` object per node, for the same
readability reason.

Note the two properties are independent: **port *order* still matters** for compositing,
it just is not what gets written down.
→ [ADR-0021](0021-port-index-is-compositing-order.md)

## Revisiting it

There is no pressure to. The index encoding saves a handful of bytes in a file that is
already indented JSON, and costs the one property that makes the port table safe to edit.

If a port ever must be renamed, the migration path is the same as any other format break:
bump `kFormatVersion` and extend `tools/migrate_v2_to_v3.py`'s successor.
→ [ADR-0018](0018-no-in-app-migration.md)
