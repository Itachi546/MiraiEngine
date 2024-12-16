#version 460
layout(triangles, invocations = 5) in;
layout(triangle_strip, max_vertices = 3) out;

layout(set = 0, binding = 0) uniform CascadeInfo {
    mat4 VP[5];
    vec4 split_distance[2];
};

void main() {

    for (int i = 0; i < gl_in.length(); ++i) {
        gl_Layer = gl_InvocationID;
        gl_Position = VP[gl_InvocationID] * gl_in[i].gl_Position;
        EmitVertex();
    }
    EndPrimitive();
}