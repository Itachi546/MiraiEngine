#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_draw_parameters : enable

#include "utils/per-frame-data.glsl"

layout(set = 0, binding = 0) uniform PerFrameDataBinding {
    PerFrameData per_frame_data;
};

layout(set = 1, binding = 0) readonly buffer TransformBinding {
    mat4 transforms[];
};

layout(set = 2, binding = 0) readonly buffer VertexBinding {
    uint vertices[];
};

#include "utils/vertexdata.glsl"
layout(set = 3, binding = 0) readonly buffer DrawDataBinding {
    DrawData draw_datas[];
};

void main() {
    DrawData draw_data = draw_datas[gl_DrawID];

    uint vertex_address = draw_data.vertex_offset + gl_VertexIndex * draw_data.vertex_stride;
    vec3 position = unpack_position(vertex_address);

    mat4 M = transforms[draw_data.transform_index];
    gl_Position = per_frame_data.VP * M * vec4(position, 1.0f);
}