#extension GL_ARB_shader_draw_parameters : enable

#include "../utils/per-frame-data.glsl"

layout(set = 0, binding = 0) uniform PerFrameDataBinding {
    PerFrameData per_frame_data;
};

layout(std430, set = 0, binding = 1) readonly buffer TransformBinding {
    mat4 transforms[];
};

layout(std430, set = 0, binding = 2) readonly buffer VertexBinding {
    uint vertices[];
};

#include "../utils/vertexdata.glsl"
layout(std430, set = 0, binding = 3) readonly buffer DrawDataBinding {
    DrawData draw_datas[];
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
    vec3 position = unpack_position(vertex_address);

    mat4 M = transforms[draw_data.transform_index];
    gl_Position = per_frame_data.VP * M * vec4(position, 1.0f);

#ifdef ALPHA_MODE_MASK
    vs_out.uv = unpack_uv(vertex_address);
    vs_out.mat_id = draw_data.material_index;
#endif
}