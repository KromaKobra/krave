// Screen-style blend for a layer texture.
//
// Same premultiplied source-over trick as blend_add.frag, but instead of zeroing
// alpha entirely it keeps the channel maximum. The composite becomes
// dst = src.rgb + (1 - max(src.rgb)) * dst: bright source pixels dominate while
// dark ones leave the backdrop alone, which is the visual character of Screen.
// It is not the exact 1-(1-a)(1-b) formula -- that needs the destination as an
// input, which the scene graph does not expose to a ShaderEffect.
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
    float luma = max(max(c.r, c.g), c.b);
    fragColor = vec4(c.rgb, luma);
}
