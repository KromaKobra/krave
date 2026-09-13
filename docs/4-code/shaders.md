# 4 · Shaders

**Sources:** `src/app/shaders/*.frag`, `src/app/qml/layers/LayerFrame.qml`, `src/app/qml/layers/BlurPass.qml`, `src/app/qml/layers/BlurChain.qml`, `src/app/CMakeLists.txt`

Four fragment shaders. All are `#version 440`, all use the standard `ShaderEffect`
contract (`qt_TexCoord0`, `qt_Matrix`, `qt_Opacity`, `sampler2D source`), and all are
compiled to `.qsb` by `qt_add_shaders` with `PREFIX "/qt/qml/krave"` — deliberately
matching the QML module's resource prefix so they sit beside the QML that references
them.

| Shader | Used as | By |
|---|---|---|
| `blend_add.frag` | `layer.effect` | `LayerFrame`, when `blendMode == "add"` |
| `blend_screen.frag` | `layer.effect` | `LayerFrame`, when `blendMode == "screen"` |
| `blur_separable.frag` | `layer.effect` | `BlurPass`, run **twice** per blur node (V then H) |
| `chroma_split.frag` | `layer.effect` | `BlurChain`, once per blur node |

---

## The premultiplied-alpha trick

Qt Quick has **no per-item blend mode**. Both blend shaders work by exploiting the fact
that the compositor does premultiplied source-over:

```
dst = src.rgb + (1 - src.a) * dst
```

So the alpha channel of the shader's output is not opacity here — it is a *lever on the
compositor*. → [ADR-0012](../decisions/0012-blend-via-premultiplied-alpha.md)

### `blend_add.frag`

```glsl
vec4 c = texture(source, qt_TexCoord0) * qt_Opacity;
fragColor = vec4(c.rgb, 0.0);
```

Forcing `src.a = 0` collapses the compositor equation to `dst = src + dst`. **Exactly
additive**, with no pipeline state change and no custom material.

### `blend_screen.frag`

```glsl
vec4 c = texture(source, qt_TexCoord0) * qt_Opacity;
float luma = max(max(c.r, c.g), c.b);
fragColor = vec4(c.rgb, luma);
```

Alpha becomes the channel maximum, giving `dst = src.rgb + (1 - max(src.rgb)) * dst`:
bright source pixels dominate while dark ones leave the backdrop alone.

This is an **approximation** and the file says so — true Screen is `1-(1-a)(1-b)`, which
needs the destination as an input. That is also why `multiply` is not offered at all.

> `LayerFrame` enables `layer.enabled` only for `add` and `screen`, so a `normal` layer
> costs no offscreen buffer. The blend group spans the **whole canvas**, not the layer
> box — a `layer.enabled` texture clips to its item, which would decapitate anything
> drawn outside the box.

---

## `blur_separable.frag`

The workhorse. One pass along one axis; `BlurPass` runs it twice (vertical then
horizontal) per enabled blur node.

### Uniforms

| Name | Type | Meaning |
|---|---|---|
| `gaussPx` | `float` | Gaussian radius **in pixels** (QML converts from the canvas-height fraction) |
| `chromaPx` | `float` | Chroma-blur radius in pixels |
| `chromaOn` | `float` | 0/1 flag |
| `dir` | `vec2` | `(0,1)` or `(1,0)` — the axis for this pass |
| `texSize` | `vec2` | Texture dimensions, for the pixel step and the aspect correction |
| `maskCentre` | `vec2` | Circle centre, normalized |
| `maskRadius` | `float` | Fraction of **height** |
| `maskSoft` | `float` | Fraction of the radius |
| `maskInvert` | `float` | 0/1 |
| `maskOn` | `float` | 0/1 |

### `maskAmount(uv)`

Returns `1.0` when the mask is off. Otherwise:

```glsl
float aspect = texSize.x / max(texSize.y, 1.0);
vec2  d      = vec2((uv.x - maskCentre.x) * aspect, uv.y - maskCentre.y);
float soft   = max(maskSoft * maskRadius, 1e-4);
float m      = 1.0 - smoothstep(maskRadius - soft, maskRadius + soft, length(d));
// inverted: m = 1 - m
```

Scaling x by the aspect ratio is what keeps the circle **circular at any canvas aspect**,
and it is why `maskRadius` is a fraction of height rather than width. A softness of 1
spreads the transition over `0 … 2r`.

### `blurAxis(uv, texStep, radiusPx)`

```glsl
const int kTaps = 12;
float sigma = radiusPx * 0.5;
vec4  sum   = texture(source, uv);
float total = 1.0;
for (int i = 1; i <= kTaps; ++i) {
    float d = radiusPx * float(i) / float(kTaps);
    float w = exp(-0.5 * d * d / (sigma * sigma));
    // symmetric pair at ±(texStep * d), weighted w
}
// normalised by the accumulated total
```

- Early-outs below 0.5 px.
- **12 tap pairs** because the last one weighs `exp(-2)` of the centre — beyond that the
  contribution is invisible.
- Blurring **premultiplied** RGBA directly is valid: premultiplication and a weighted sum
  are both linear and commute.

### Chroma blur, in the same pass

When `chromaOn`, a **second** tap loop runs at `chromaPx × mask` over the same texture.
Both results are un-premultiplied; **luma is taken from the sharp result**
(`dot(rgb, vec3(0.299, 0.587, 0.114))`) and **chroma from the wide one**, via BT.601:

```glsl
chromaOf(rgb) = vec2(dot(rgb, vec3(-0.168736, -0.331264,  0.5)),
                     dot(rgb, vec3( 0.5,      -0.418688, -0.081312)));
```

then recombined and re-premultiplied by the sharp alpha, so the silhouette does not grow.

Doing both in one pass keeps the common Gaussian-only case at **two** offscreen buffers
instead of four.

### The documented caveat

A radius that varies per fragment is **not exactly separable**. The error is a slight
asymmetry across the mask gradient, judged invisible at the radii offered; the
alternative is an O(r²) single-pass kernel.

---

## `chroma_split.frag`

Radial RGB separation — chromatic aberration.

### Uniforms

`splitPx`, `texSize`, `maskCentre`, `maskRadius`, `maskSoft`, `maskInvert`, `maskOn`.

`maskAmount` is **byte-identical** to the blur's, deliberately, so the two passes agree on
the mask edge to the last fragment.

### The maths

```glsl
float amount = splitPx * maskAmount(uv);       // early-out under 0.01
vec2  origin = maskOn > 0.5 ? maskCentre : vec2(0.5);
vec2  fromCentre = (uv - origin) * texSize;    // PIXEL space
vec2  offset = normalize(fromCentre) * amount / texSize;

fragColor.r = texture(source, uv + offset).r;
fragColor.g = texture(source, uv).g;
fragColor.b = texture(source, uv - offset).b;
fragColor.a = max(centre.a, outer.a, inner.a);
```

Computing the direction in **pixel** space means no aspect correction is needed and the
offset is the same pixel count in every direction.

The `max` on alpha stops shifted channels being clipped against a silhouette that no
longer covers them. And because all three samples are premultiplied, taking one channel
from each takes *colour × alpha* — so the fringe fades out with the layer's own edge
rather than ending abruptly.

---

## How the passes nest

`BlurChain` wraps one level per enclosing blur node, **outermost first**:

```
blurV   (layer.effect = BlurPass dir=(0,1))
└ blurH (layer.effect = BlurPass dir=(1,0))
  └ split (layer.effect = chroma_split)
    └ next blur level, or the content hole
```

**Nesting rather than sequencing**, because `layer.enabled` renders a subtree to one
texture for one `ShaderEffect` — a second pass must be a second wrapping `Item`.

The chroma split sits **inside** the blur so its fringe is softened along with everything
else. Each `layer.enabled` is individually gated, so a disabled effect costs no offscreen
buffer at all, and an all-off blur node is free.

All sizes come from the chain's own height, never from the layer box — see
[3 · rendering](../3-components/rendering.md).

---

## Adding a shader

See [guides/add-a-shader-effect](../guides/add-a-shader-effect.md). The short version:
add the `.frag` to the `qt_add_shaders` `FILES` list, keep `PREFIX "/qt/qml/krave"`,
reference it as `qrc:/qt/qml/krave/shaders/<name>.frag.qsb`, and bind uniforms by giving
the `ShaderEffect` QML properties with **exactly the uniform names**.

---

## See also

- [3 · rendering](../3-components/rendering.md) — where these sit in the layer stack
- [node-reference § Blur](node-reference.md#blurnode--blur) — the properties that drive them
- [ADR-0012](../decisions/0012-blend-via-premultiplied-alpha.md) · [ADR-0013](../decisions/0013-transform-is-identity-blur-is-approximation.md)
