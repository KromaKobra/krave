# ADR-0009 — `required` marks data dependencies only

**Status:** Accepted · **Source:** commit `a0ddfe4`, `src/app/NodePorts.h:33-41` · **Pinned by:** `onlyDataDependenciesAreRequired`

## Context

The editor rings an unconnected input in warning amber, so a user can see that a node is
not going to work. The question is which inputs deserve that ring.

Marking *every* unconnected input would be the simple rule.

## Decision

`PortSpec::required` is true only for a **data dependency** — an input without which the
node cannot function at all:

| Type | Required | Not required |
|---|---|---|
| `output` | `visual` | — |
| `separator` | `audio` | — |
| `spectrum` | `audio` | `visual` |
| `text`, `transform`, `blur` | — | `visual` |
| `merge` | — | `background`, `foreground` |

## Consequences

> "True for a data dependency — a separator with no audio has nothing to analyse, a
> spectrum with no audio has no bands to draw, the Output with no visual input renders
> nothing at all. FALSE for a compositing input: a spectrum with nothing behind it is
> simply the bottom of the chain, which is completely normal and must not be flagged as a
> mistake."

And the reason it matters:

> "The editor draws a warning ring on an unconnected required input, so this is the
> difference between a useful hint and a graph covered in false alarms."

A warning that appears on nearly every graph is not a warning — it is decoration, and it
teaches people to ignore the one case that mattered.

The rule also happens to align with a structural fact: **every audio port is `required`
and `stacked: false`**, while every visual input except the Output's is optional and
stacked. `portTableInvariantsHold` asserts the audio half directly.

`PortDot` only draws the `starved` ring when **not** currently linking, so the warning does
not compete with the drag-to-connect highlighting.

The same commit fixed the other half of the same bug: `BarLayerView` clamped its bar count
to at least 1, which turned an unconnected spectrum into one bar filling the whole layer —
"a worse answer than the old silent fallback was". `barCount` stays clamped for the
geometry maths that divides by it; a separate `hasBands` now decides whether anything is
drawn at all.

## Revisiting it

`onlyDataDependenciesAreRequired` spells the expected lists out explicitly rather than
deriving them, so changing one is a deliberate edit to the test.

When adding a port, ask: *is the node useless without this, or is it merely at the bottom
of a chain?* Only the first is `required`.
