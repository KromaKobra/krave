// Chromatic aberration: red and blue pulled apart along the radius.
//
// R is sampled a little way out from the centre and B a little way in, with G
// left where it is. Because the direction is radial rather than fixed, the
// fringing reads as a lens artefact -- growing outward from the centre -- rather
// than as a flat colour offset.
//
// The centre is the blur mask's centre when the mask is on, so the sharp region
// and the fringe share an origin; the canvas centre otherwise.
//
// SEPARATION IS A FRACTION OF CANVAS HEIGHT, turned into pixels by the QML, for
// the same reason the blur radii are: the exported 4K frame has to match the
// preview.
#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    // Half the distance between the red and blue samples, in PIXELS.
    float splitPx;
    // The source item's size in pixels: the pixel-to-uv step and the circle's
    // aspect correction both come from it.
    vec2 texSize;
    vec2 maskCentre;
    float maskRadius;
    float maskSoft;
    float maskInvert;
    float maskOn;
};

layout(binding = 1) uniform sampler2D source;

/// Identical to blur_separable.frag's, deliberately: the two passes must agree
/// on the mask to the last fragment, or the edge of the sharp region would show
/// up twice in slightly different places.
float maskAmount(vec2 uv)
{
    if (maskOn < 0.5)
        return 1.0;

    float aspect = texSize.x / max(texSize.y, 1.0);
    vec2 d = vec2((uv.x - maskCentre.x) * aspect, uv.y - maskCentre.y);
    float soft = max(maskSoft * maskRadius, 1e-4);
    float m = 1.0 - smoothstep(maskRadius - soft, maskRadius + soft, length(d));
    return maskInvert > 0.5 ? 1.0 - m : m;
}

void main()
{
    vec2 uv = qt_TexCoord0;
    vec4 centre = texture(source, uv);

    float amount = splitPx * maskAmount(uv);
    if (amount < 0.01) {
        fragColor = centre * qt_Opacity;
        return;
    }

    // Radial direction worked out in PIXELS, where the axes are the same scale,
    // so no aspect correction is needed and the offset is the same number of
    // pixels in every direction.
    vec2 origin = maskOn > 0.5 ? maskCentre : vec2(0.5);
    vec2 fromCentre = (uv - origin) * texSize;
    float dist = length(fromCentre);
    // Exactly at the centre there is no radial direction; the separation is zero
    // there anyway, so any direction does.
    vec2 dir = dist > 0.0001 ? fromCentre / dist : vec2(0.0);
    vec2 offset = dir * amount / texSize;

    vec4 outer = texture(source, uv + offset);
    vec4 inner = texture(source, uv - offset);

    // All three samples are premultiplied, so taking one channel from each is
    // taking colour*alpha from each -- which is what makes the fringe fade out
    // with the layer's own edge instead of smearing opaque colour past it. The
    // alpha is the widest of the three, so the shifted channels are not clipped
    // against a silhouette that no longer covers them.
    fragColor = vec4(outer.r, centre.g, inner.b,
                     max(centre.a, max(outer.a, inner.a))) * qt_Opacity;
}
