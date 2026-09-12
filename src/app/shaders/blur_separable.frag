// One axis of a separable blur, doing the luma blur and the chroma-only blur in
// the SAME pass.
//
// Two tap loops run over the same texture at different radii: the first at the
// Gaussian radius, the second at the chroma radius. When chroma blur is on, the
// second loop's Cb/Cr replaces the first result's Cb/Cr while the first result's
// luma is kept -- so edges stay crisp and the colour bleeds across them. Doing
// both here is what keeps the common case (Gaussian only) at two offscreen
// buffers rather than four.
//
// RADII ARE FRACTIONS OF CANVAS HEIGHT, converted to pixels by the QML before
// they arrive. A radius stored in raw pixels would look right in the 640x360
// preview and wrong in the 4K export, which is the one failure this codebase is
// built to prevent.
//
// THE MASK MAKES THE RADIUS VARY PER FRAGMENT, which a separable blur cannot do
// exactly: the true 2D kernel at a point where the radius is changing is not the
// product of two 1D kernels, because the horizontal pass has already mixed in
// neighbours that wanted a different radius. The error is a slight asymmetry
// across the mask's gradient, and at the radii this node offers it is not
// visible. The alternative is a single-pass 2D kernel, which costs O(r^2) taps.
#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    // Radii in PIXELS: amount * canvasHeight, done in QML.
    float gaussPx;
    float chromaPx;
    float chromaOn;
    // vec2(1,0) for the horizontal pass, vec2(0,1) for the vertical one.
    vec2 dir;
    // The source item's size in pixels. Both the pixel-to-uv step and the
    // circle's aspect correction come from this, so there is one source of
    // truth for the resolution.
    vec2 texSize;
    vec2 maskCentre;
    float maskRadius;
    float maskSoft;
    float maskInvert;
    float maskOn;
};

layout(binding = 1) uniform sampler2D source;

// Samples per side. The weight at the last tap is exp(-2) of the centre, so the
// kernel is cut where it has little left to contribute.
const int kTaps = 12;

/// How much effect this fragment gets: 1 inside the circle, 0 outside, a
/// smoothstep across the soft edge -- or the reverse when inverted. Distances
/// are measured with x scaled by the aspect ratio so the circle is a circle and
/// not an ellipse, which makes maskRadius a fraction of canvas HEIGHT like every
/// other radius here.
float maskAmount(vec2 uv)
{
    if (maskOn < 0.5)
        return 1.0;

    float aspect = texSize.x / max(texSize.y, 1.0);
    vec2 d = vec2((uv.x - maskCentre.x) * aspect, uv.y - maskCentre.y);
    // A soft edge of 1 spreads the transition over 0..2r, which is the wide
    // falloff the node is for. Never exactly 0, or smoothstep divides by zero.
    float soft = max(maskSoft * maskRadius, 1e-4);
    float m = 1.0 - smoothstep(maskRadius - soft, maskRadius + soft, length(d));
    return maskInvert > 0.5 ? 1.0 - m : m;
}

/// A Gaussian tap loop along `texStep` (one pixel of travel, in uv) out to
/// `radiusPx`. Blurring PREMULTIPLIED rgba is fine as-is: premultiplication is a
/// linear operation and so is a weighted sum, so they commute.
vec4 blurAxis(vec2 uv, vec2 texStep, float radiusPx)
{
    if (radiusPx < 0.5)
        return texture(source, uv);

    float sigma = radiusPx * 0.5;
    vec4 sum = texture(source, uv);
    float total = 1.0;

    for (int i = 1; i <= kTaps; ++i) {
        float d = radiusPx * float(i) / float(kTaps);
        float w = exp(-0.5 * d * d / (sigma * sigma));
        vec2 o = texStep * d;
        sum += (texture(source, uv + o) + texture(source, uv - o)) * w;
        total += 2.0 * w;
    }
    return sum / total;
}

/// Premultiplied rgba -> straight rgb. Chroma is a ratio between the channels,
/// so it is meaningless until the alpha is divided back out.
vec3 unpremultiply(vec4 c)
{
    return c.a > 0.0001 ? c.rgb / c.a : vec3(0.0);
}

// BT.601. Only the two chroma axes are needed, and only their round trip has to
// be exact -- which it is, these being inverses.
vec2 chromaOf(vec3 rgb)
{
    return vec2(dot(rgb, vec3(-0.168736, -0.331264, 0.5)),
                dot(rgb, vec3(0.5, -0.418688, -0.081312)));
}

vec3 fromLumaChroma(float y, vec2 c)
{
    return vec3(y + 1.402 * c.y,
                y - 0.344136 * c.x - 0.714136 * c.y,
                y + 1.772 * c.x);
}

void main()
{
    vec2 uv = qt_TexCoord0;
    float m = maskAmount(uv);
    vec2 texStep = dir / texSize;

    vec4 base = blurAxis(uv, texStep, gaussPx * m);

    if (chromaOn > 0.5) {
        vec4 wide = blurAxis(uv, texStep, chromaPx * m);
        vec3 baseRgb = unpremultiply(base);
        vec3 wideRgb = unpremultiply(wide);
        float y = dot(baseRgb, vec3(0.299, 0.587, 0.114));
        // Luma from the sharp result, chroma from the wide one; the alpha is
        // the sharp result's too, so the silhouette does not grow.
        base = vec4(clamp(fromLumaChroma(y, chromaOf(wideRgb)), 0.0, 1.0) * base.a,
                    base.a);
    }

    fragColor = base * qt_Opacity;
}
