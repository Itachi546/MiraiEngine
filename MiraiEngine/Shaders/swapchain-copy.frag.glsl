#version 450
#extension GL_GOOGLE_include_directive : enable

layout(set = 0, binding = 0) uniform sampler2D u_texture;

layout(push_constant) uniform PushConstants {
    vec2 resolution;
    float u_enable_aa;
    float _unused;
};

#include "utils/fxaa.glsl"
#include "utils/color.glsl"

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec2 uv;

void main() {
    vec4 col;
    if (u_enable_aa > 0.5f)
        col = fxaa(u_texture, gl_FragCoord.xy, resolution);
    else
        col = texture(u_texture, uv);

#if 1 
    // float exposure = 1.0f;
    //  col.rgb = vec3(1.0) - exp(-col.rgb * exposure);
    col.rgb = Filmic(col.rgb);
    // col.rgb = pow(col.rgb, vec3(0.4545));
    fragColor = col;
#else
    fragColor = col.rrra;
#endif
}