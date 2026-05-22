#version 460

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#extension GL_GOOGLE_include_directive : enable

layout(push_constant) uniform PushConstants{
    vec3 probe_step;
    uint probe_count;

    ivec3 probe_counts;
    uint irradiance_oct_resolution;

    vec3 probe_start_position;
    uint depth_oct_resolution;
};

#include "ddgi.glsl"

void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);
}