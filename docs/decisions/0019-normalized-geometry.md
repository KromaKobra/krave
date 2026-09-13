# ADR-0019 — All geometry is normalized 0..1

**Status:** Accepted · **Source:** `src/app/Node.h:133-134`, `:627-629` · **Pinned by:** **none** — see below

## Context

The preview canvas is a few hundred pixels tall. The export is commonly 1080p and may be
4K. If a layer's position or a blur radius were stored in pixels, the same project would
look different at every size — and what the user approved in the preview would not be what
they exported.

## Decision

**Every geometric quantity is a fraction, never a pixel count.**

| Quantity | Normalized to |
|---|---|
| `posX`, `posY` | canvas width / height (a **centre**, not a corner) |
| `sizeW`, `sizeH` | canvas width / height |
| `fontScale` | canvas **height** |
| Transform `offsetX/Y` | canvas width / height |
| Transform `pivotX/Y` | the **canvas**, not the group's bounding box |
| Blur `gaussianAmount`, `chromaBlurAmount`, `chromaSplitAmount` | canvas **height** |
| Blur `maskX/Y`, `maskRadius` | canvas; radius to **height** |
| Blur `maskSoftness` | the **radius** |
| `minHeight`, `maxHeight` | the layer box |
| Image `cropX/Y/W/H` | the **source image** |

The two exceptions are deliberate and both are genuinely pixel quantities:
`cornerRadius` (pixels) and `graphX`/`graphY` (graph-editor space, not canvas space).

## Consequences

> "Geometry is normalized to the canvas (0..1) so a project renders the same at 1280x720
> preview and 3840x2160 export."

and for the blur node:

> "Every amount is a fraction of the CANVAS HEIGHT, never a pixel count, for the same
> reason every other geometry property here is normalized: a project has to look identical
> at 640x360 preview and at 4K export."

The conversion happens in exactly one place — `LayerFrame`, which multiplies by
`frame.width`/`frame.height`. Since `LayerFrame`'s root always spans the canvas (forced by
`Loader` clobbering the loaded item's size), `frame.width` reliably *means* the canvas.

**Height, not width, is the reference for scalar quantities** — font size, blur radius, mask
radius. That keeps them stable across aspect-ratio changes. The mask circle's aspect
correction is done in the shader for the same reason, so the circle stays circular at any
aspect.

The **crop rectangle is normalized to the source image**, not the canvas — a different
reference for a different reason:

> "…so the node needs to know nothing about the file's dimensions and a project survives
> the image being replaced with one of a different size."

## Revisiting it

Nothing to revisit; this is the property that makes preview/export parity meaningful at
all. The risk is not reversal but **erosion** — a new property added in pixels because it
was easier at the time.

When adding any geometric property, the question is: *what is this a fraction of?* If the
answer is "pixels", it is wrong.

**There is no test.** One would render a project at two resolutions and compare the
resulting layer boxes as fractions. Like [ADR-0011](0011-fixed-step-animation-driver.md)
and [ADR-0012](0012-blend-via-premultiplied-alpha.md), it needs an offscreen render
harness that does not currently exist.
