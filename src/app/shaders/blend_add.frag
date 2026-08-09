// Additive blend for a layer texture.
//
// Qt Quick has no per-item blend mode, but the scene graph composites with
// premultiplied source-over: dst = src.rgb + (1 - src.a) * dst. Forcing src.a to
// zero while keeping the premultiplied colour collapses that to dst = src + dst,
// which is exactly additive -- no custom material or pipeline state needed.
#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};

layout(binding = 1) uniform sampler2D source;

void main()
{
    vec4 c = texture(source, qt_TexCoord0) * qt_Opacity;
    fragColor = vec4(c.rgb, 0.0);
}
