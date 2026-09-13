# ADR-0013 — Transform is per-layer by identity; blur is per-layer by approximation

**Status:** Accepted · **Source:** `src/app/Node.h:560-574` and `:617-625`, `src/app/RenderList.h:22-38` · **Pinned by:** `transformsAccumulateOutermostFirst`, `blursAccumulateOutermostFirst`, `renderListDrawsASharedSubgraphTwice`

## Context

`TransformNode` and `BlurNode` both **enclose a whole upstream subgraph** and apply
something to it as a unit. The natural implementation is an offscreen render target per
group: render the group to a texture, then transform or blur the texture.

That is expensive, and it would break the flat draw list that makes
`VisualizerCanvas` simple.

## Decision

Keep the draw list **flat**. Each `RenderList::Entry` carries the stack of enclosing
transform nodes and the stack of enclosing blur nodes, outermost first, and each layer
applies them individually.

**Use the same mechanism for both — but be explicit that the justification only holds for
one of them.**

## Consequences

### Transform: an identity

> "A visual edge carries no pixels: compositing is Qt Quick's scene graph drawing a flat,
> z-ordered list of items that all share the canvas coordinate space. Every transform here
> is affine and expressed about a canvas-relative pivot, and applying one accumulated
> affine transform to each member of a group is mathematically identical to applying it
> once to the group."

Transforming each member **equals** transforming the group. The flat list is not an
approximation; it is a restatement. That is also why the pivot is a fraction of the
**canvas**, not of the group's bounding box — no node knows the latter.

### Blur: an approximation, stated as one

> "An affine map distributes over a group, a convolution does not. Two overlapping upstream
> layers therefore give `blur(A) over blur(B)` rather than `blur(A over B)`."

Where it shows:
- **Overlapping upstream layers** keep crisp edges *against each other*, because each was
  blurred before they met.
- **A flat Background upstream stays sharp**, since blurring a solid colour returns the
  same solid colour.

Neither is wrong-looking on its own. The workaround is stated in the header: to smear two
layers together, **blur each arm before the Merge**, not once after it.

### What this rules out

Group **opacity** and group **blend** are named twice as the cases that would need a real
render target — two overlapping half-transparent layers composited separately do not look
like the pair composited then faded. That is why `TransformNode` has neither, and why
adding them later means a render target rather than another property.

Blur is a third such case, and it "buys its way out of one by accepting that difference".

`RenderList.h:36-38` flags the trap directly:

> "That asymmetry is deliberate and is documented on BlurNode; **it is the one place where
> reading 'enclosing stack' as 'identical to enclosing the group' is wrong.**"

### Mechanics

Both stacks are **outermost first**, because that is the order matrices compose in and the
order shader passes nest in. They are two separate stacks rather than one mixed list,
since they are consumed by different parts of `LayerFrame` and their relative order is
meaningless.

`Entry::operator==` compares both stacks, so re-plugging a transform or blur rebuilds the
list even when the drawn node set is unchanged.
→ [ADR-0006](0006-topology-vs-graph-changed.md)

## Revisiting it

An offscreen render target per group would make blur exact **and** unlock group opacity and
group blend in one move. The cost is a texture per group per frame, and `RenderList` would
stop producing a flat list — `VisualizerCanvas` would need to consume a tree.

That is the single largest architectural change available in the rendering path, and this
record is where to start reading before attempting it.
