#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_draw_parameters : enable

#include "utils/vertexdata.glsl"
#include "utils/shadow.glsl"

layout(location = 0) out VS_OUT {
    vec2 uv;
    flat uint mat_id;
}
vs_out;

layout(set = 0, binding = 0) uniform CascadeInfoUniform {
    CascadeInfo cascade_info;
};

layout(set = 2, binding = 0) readonly buffer VertexData {
    Vertex vertices[];
};

layout(set = 3, binding = 0) readonly buffer Transform {
    mat4 transforms[];
};

layout(set = 4, binding = 0) readonly buffer DrawDataBinding {
    DrawData draw_datas[];
};

layout(push_constant) uniform PushConstants {
    uint cascade_index;
    uint padding[3];
};

void main() {
    DrawData draw_data = draw_datas[gl_DrawID];
    Vertex vertex = vertices[gl_VertexIndex];
    mat4 M = transforms[draw_data.transform_index];

    vs_out.uv = vec2(vertex.tu, vertex.tv);
    vs_out.mat_id = draw_data.material_index;
    gl_Position = cascade_info.VP[cascade_index] * M * vec4(vertex.px, vertex.py, vertex.pz, 1.0f);
    // gl_Position.z = max(gl_Position.z, -1.0f);
}