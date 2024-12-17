#version 460

#extension GL_GOOGLE_include_directive : enable
#include "utils/shadow.glsl"

layout(triangles, invocations = NUM_DIRLIGHT_CASCADE) in;
layout(triangle_strip, max_vertices = 3) out;

layout(set = 0, binding = 0) uniform CascadeInfoUniform {
    CascadeInfo cascade_info;
};

void main() {

    for (int i = 0; i < gl_in.length(); ++i) {
        gl_Layer = gl_InvocationID;
        gl_Position = cascade_info.VP[gl_InvocationID] * gl_in[i].gl_Position;
        gl_Position.z = max(gl_Position.z, -1.0f);
        EmitVertex();
    }
    EndPrimitive();
}