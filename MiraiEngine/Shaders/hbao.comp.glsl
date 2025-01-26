#version 460
layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform samplerCube u_depth_texture;
layout(set = 0, binding = 1, rgba16f) uniform imageCube u_ssao_texture;

layout(push_constant) uniform HBAOPushConstants {
    float width;
    float height;
    float radius;
    float num_step;
    float step_size;
    float direction_step;
};

void main() {
    ivec3 id = ivec3(gl_GlobalInvocationID.xyz);
    if (id.x >= width || id.y >= height)
        return;
}