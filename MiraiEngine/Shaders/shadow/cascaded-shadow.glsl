#ifndef CASCADE_SHADOW_GLSL
#define CASCADE_SHADOW_GLSL

#extension GL_ARB_shader_draw_parameters : enable

#include "../utils/shadow.glsl"

layout(set = 0, binding = 0) uniform CascadeInfoUniform {
    CascadeInfo cascade_info;
};

layout(std430, set = 0, binding = 1) readonly buffer VertexData {
    uint vertices[];
};

#include "../utils/vertexdata.glsl"
layout(std430, set = 0, binding = 2) readonly buffer DrawDataBinding {
    DrawData draw_datas[];
};

layout(std430, set = 0, binding = 3) readonly buffer Transform {
    mat4 transforms[];
};

layout(push_constant) uniform PushConstants {
    uint cascade_index;
    uint padding[3];
};

#ifdef ALPHA_MODE_MASK
layout(location = 0) out VS_OUT {
    vec2 uv;
    flat uint mat_id;
}
vs_out;
#endif

void main() {
    DrawData draw_data = draw_datas[gl_DrawID];
    uint vertex_address = draw_data.vertex_offset + gl_VertexIndex * draw_data.vertex_stride;
    mat4 M = transforms[draw_data.transform_index];
    gl_Position = cascade_info.VP[cascade_index] * M * vec4(unpack_position(vertex_address), 1.0f);

#ifdef ALPHA_MODE_MASK
    vs_out.uv = unpack_uv(vertex_address);
    vs_out.mat_id = draw_data.material_index;
#endif
}

#endif