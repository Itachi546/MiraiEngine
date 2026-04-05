#version 460
#extension GL_GOOGLE_include_directive : enable

#include "utils/per-frame-data.glsl"
#include "utils/bindless.glsl"

layout(location = 0) out vec4 fragColor;

layout(location = 0) in FS_IN {
    vec2 uv;
    flat uint mat_id;
} fs_in;

layout(set = 0, binding = 0) uniform PerFrameBinding {
    PerFrameData per_frame_data;
};

struct UnlitMaterial {
    vec4 color;

    uint texture_id;
    uint flags;
    uint padding[2];
};

layout(set = 3, binding = 1) readonly buffer Materials {
    UnlitMaterial materials[];
};


void main() {
   UnlitMaterial material = materials[fs_in.mat_id];
   vec4 col = material.color;

    if (material.texture_id != K_INVALID_TEXTURE)
        col *= sample_texture(material.texture_id, fs_in.uv);
    if(col.a < 0.5) 
         discard;

   fragColor = vec4(col.rgb, 1.0f);
}

