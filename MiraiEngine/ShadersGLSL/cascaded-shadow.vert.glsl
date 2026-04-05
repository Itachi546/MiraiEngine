#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_draw_parameters : enable

#include "utils/shadow.glsl"

layout(set = 0, binding = 0) uniform CascadeInfoUniform {
    CascadeInfo cascade_info;
};

layout(set = 2, binding = 0) readonly buffer VertexData {
    uint vertices[];
};

#include "utils/vertexdata.glsl"
layout(set = 1, binding = 0) readonly buffer DrawDataBinding {
    DrawData draw_datas[];
};

layout(set = 3, binding = 0) readonly buffer Transform {
    mat4 transforms[];
};

layout(push_constant) uniform PushConstants {
    uint cascade_index;
    uint padding[3];
};

void main() {
    DrawData draw_data = draw_datas[gl_DrawID];
    uint vertex_address = draw_data.vertex_offset + gl_VertexIndex * draw_data.vertex_stride;
    mat4 M = transforms[draw_data.transform_index];
    gl_Position = cascade_info.VP[cascade_index] * M * vec4(unpack_position(vertex_address), 1.0f);
    // gl_Position.z = max(gl_Position.z, -1.0f);
}