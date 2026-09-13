# ADR-0005 — Inputs declare what they accept

**Status:** Accepted · **Source:** `src/app/NodePorts.h:26-29` · **Pinned by:** `maskInputsAcceptVisualOutputs`, `mismatchedTypesCannotConnect`, `portTableInvariantsHold`

## Context

Ports are typed, and the type is visible — purple carries a rendered image, orange carries
audio, and "the colour a port and its edges are drawn in is a pure function of this, so
the kind IS the visible type".

But type compatibility is not always identity. A **Mask** input is drawn blue, because it
is a different *role*, yet what plugs into it is an ordinary Visual output. Handled
naively, that becomes a special case in the validator, again in the hit test, and again in
the highlighting.

## Decision

Each input carries `accepts` — the list of output **kinds** that may be plugged in. A Mask
input declares `accepts = {Visual}`.

## Consequences

> "A Mask input declares {Visual}, so 'blue accepts purple' needs no special case in the
> validator, in the hit test, or in the highlighting."

All three consumers become the same one-line test:

| Consumer | Code |
|---|---|
| `NodeGraph::canLink` | `accepts.contains(fromSpec.outputKind)` |
| `GraphLayout.hitTestInput` | nearest input whose `accepts` contains the dragged kind |
| `PortDot` highlighting | `port.accepts.indexOf(canvas.linkKind) >= 0` |

The port's own `kind` stays free to mean "what role is this", independent of "what may
connect here" — which is what lets the colour stay honest.

`PortKind::Mask` is fully specified **ahead of its first use**: colour, name round-trip,
and the rules that a mask input must accept Visual and must not be `stacked`.
`maskInputsAcceptVisualOutputs` pins those rules **vacuously today**, on purpose:

> "It starts biting the moment a mask port is added, which is the point at which getting
> it wrong would be expensive."

`portTableInvariantsHold` additionally requires every `accepts` list to be non-empty — an
input accepting nothing could never be connected, which is always a mistake rather than a
design.

## Revisiting it

The mechanism generalises to any future kind with no code change: declare the kind, give
it a colour, and list it in the `accepts` of whatever should take it. There is nothing to
revisit unless compatibility ever becomes conditional on something other than kind — a
port that accepts Visual only from certain node types, say — at which point `accepts`
would need to become a predicate rather than a list.
