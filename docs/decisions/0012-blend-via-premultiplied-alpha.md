# ADR-0012 — Blend modes exploit premultiplied source-over; `multiply` is not offered

**Status:** Accepted · **Source:** commit `e8749d7`, `src/app/shaders/blend_add.frag`, `blend_screen.frag` · **Pinned by:** **none** — see below

## Context

Layers need `add` and `screen` compositing. **Qt Quick has no per-item blend mode**, and
adding one properly means a custom `QSGMaterial` with its own pipeline state — a large
amount of scene-graph code for two modes.

## Decision

Exploit the fact that the compositor already does premultiplied source-over:

```
dst = src.rgb + (1 - src.a) * dst
```

The output alpha is not opacity here — it is a **lever on that equation**. A `ShaderEffect`
attached as `layer.effect` sets it deliberately:

| Mode | Shader output | Result |
|---|---|---|
| `add` | `vec4(rgb, 0.0)` | `dst = src + dst` — **exactly** additive |
| `screen` | `vec4(rgb, max(r,g,b))` | `dst = src + (1 - max(src)) · dst` — approximate Screen |

`multiply` is **not offered at all**.

## Consequences

No custom material, no pipeline state change, no scene-graph code — two nine-line shaders.

`LayerFrame` sets `layer.enabled` **only** for `add` and `screen`, so a `normal` layer
costs no offscreen buffer.

**Screen is an approximation and the file says so.** True Screen is `1-(1-a)(1-b)`, which
needs the destination as a shader input. What this gives instead is "bright source pixels
dominate while dark ones leave the backdrop alone, which is the visual character of
Screen".

**`multiply` is refused for the same reason, and cannot be approximated the same way.**
Multiply fundamentally needs `dst`; there is no choice of `src.a` that produces it. Offering
a wrong `multiply` would be worse than not offering one.

A related constraint sits next to this in `LayerFrame`: the blend `group` must span the
**whole canvas**, not the layer box, because a `layer.enabled` texture clips to its item.
A box-sized group cuts off anything drawn outside the box.

And the blend texture is captured **before** the transform matrix — the matrix goes on the
root item, not on `group` — because a rotated layer must blend with what is beneath it,
not blend a rotated texture.

## Revisiting it

`multiply`, and any other destination-reading mode, needs the scene graph to expose the
destination to a `ShaderEffect`. It does not. The alternatives are a custom `QSGMaterial`,
or rendering the whole group to an offscreen target and compositing manually — the same
cost that [ADR-0013](0013-transform-is-identity-blur-is-approximation.md) identifies for
group opacity and group blend.

**There is no test.** One would assert composited output for `add` and `screen` against
known values, which needs an offscreen render harness — the same missing piece as
[ADR-0011](0011-fixed-step-animation-driver.md).
