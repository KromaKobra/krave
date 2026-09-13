# Guide · Add a shader effect

**Derived from** `c5efbd9`, which added `blur_separable.frag` and `chroma_split.frag`
together with the blur node.

Read [4 · shaders](../4-code/shaders.md) first.

---

## 1 · Write the fragment shader

`src/app/shaders/myeffect.frag`. The `ShaderEffect` contract is fixed:

```glsl
#version 440

layout(location = 0) in  vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4  qt_Matrix;
    float qt_Opacity;
    // your uniforms follow, in declaration order
    float amount;
    vec2  texSize;
};

layout(binding = 1) uniform sampler2D source;

void main()
{
    vec4 c = texture(source, qt_TexCoord0);
    fragColor = /* … */;
}
```

### Things that will bite

- **Textures are premultiplied.** A weighted sum of premultiplied samples is valid (both
  operations are linear and commute), but anything non-linear — a luma/chroma split, a
  channel swap — must `unpremultiply` first and re-premultiply after. `blur_separable.frag`
  does exactly this for its chroma path.
- **Aspect correction.** If you measure a distance in UV space, scale x by
  `texSize.x / texSize.y` or your circle becomes an ellipse. If you work in **pixel** space
  (`(uv - origin) * texSize`), no correction is needed — that is why `chroma_split.frag`
  does it that way.
- **Sizes arrive in pixels.** QML converts from the canvas-height fraction before setting
  the uniform, so the effect matches at preview size and at 4K.
  → [ADR-0019](../decisions/0019-normalized-geometry.md)
- **Early-out on a no-op.** Every existing shader returns immediately below a threshold
  (0.5 px for the blur, 0.01 for the split). Do the same.
- **If your effect pairs with an existing mask**, copy `maskAmount()` **verbatim** — the
  blur and the split share a byte-identical copy deliberately, "so the two passes agree on
  the mask edge to the last fragment".

---

## 2 · Register it — `src/app/CMakeLists.txt`

```cmake
qt_add_shaders(krave "layer_shaders"
    PREFIX "/qt/qml/krave"
    FILES   shaders/blend_add.frag
            ...
            shaders/myeffect.frag          # ← add here
    OUTPUTS shaders/*.frag.qsb)
```

**`PREFIX` must stay `/qt/qml/krave`** — it deliberately matches the QML module's resource
prefix so the compiled `.qsb` sits beside the QML that references it.

---

## 3 · Bind it from QML

```qml
ShaderEffect {
    property real amount: pass.amountPx
    property var  texSize: Qt.size(width, height)
    fragmentShader: "qrc:/qt/qml/krave/shaders/myeffect.frag.qsb"
}
```

**Uniform binding is by name.** A QML property whose name exactly matches a uniform is
bound automatically; a typo silently binds nothing and the uniform reads as zero. There is
no warning.

---

## 4 · Attach it to a layer

Effects are applied as `layer.effect` on a wrapping `Item`, gated by `layer.enabled`:

```qml
Item {
    layer.enabled: root.effectOn       // gate it — a disabled effect must cost nothing
    layer.effect: myEffectComponent
}
```

### Nesting, not sequencing

`layer.enabled` renders a subtree to **one** texture for **one** `ShaderEffect`. A second
pass must be a **second wrapping `Item`**, not a second effect on the same one. That is why
`BlurChain` looks the way it does:

```
blurV    (layer.effect = BlurPass dir=(0,1))
└ blurH  (layer.effect = BlurPass dir=(1,0))
  └ split (layer.effect = chroma_split)
    └ content
```

Gate each level independently, so a disabled effect costs no offscreen buffer at all.

### Where to put it

| Situation | Where |
|---|---|
| Per-layer, driven by a node property | A new level in `BlurChain.qml`, or its own chain component |
| A blend mode | `LayerFrame.qml`'s `layer.effect` on `group` — but see [ADR-0012](../decisions/0012-blend-via-premultiplied-alpha.md); destination-reading modes are not possible |
| A whole-group effect | **Not currently possible.** Needs an offscreen render target per group → [ADR-0013](../decisions/0013-transform-is-identity-blur-is-approximation.md) |

Two `LayerFrame` constraints that apply to any new pass:

- **The group must span the canvas, not the layer box.** A `layer.enabled` texture clips
  to its item.
- Sizes come from the chain's own height, never from the layer box.

---

## 5 · Expose it

A shader that no node drives is dead code. Usually it belongs to a node's properties —
either a new node ([add-a-node-type](add-a-node-type.md)) or new properties on an existing
one ([add-a-node-property](add-a-node-property.md)).

Remember to add any new `.qml` to `app_qml_files`, and every new property to the
round-trip test.

---

## Verify

```bash
./scripts/build.sh --run
```

Check all four of these, because each has caught a real bug:

1. The effect **off** costs nothing — no visual change, no offscreen buffer.
2. It looks the same at **Fit** and at **100%** zoom (normalized sizes).
3. It looks the same in an **export** at a different resolution.
4. **Two of the node stacked** nest rather than conflict.

> `console.log` prints nothing in this Qt build. For shader debugging, output a channel
> directly to `fragColor` and look at it.

---

## See also

- [4 · shaders](../4-code/shaders.md) — the four existing shaders in detail
- [3 · rendering](../3-components/rendering.md) — where passes sit in the layer stack
