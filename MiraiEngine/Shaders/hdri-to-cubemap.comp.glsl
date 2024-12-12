#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D u_hdri;
layout(set = 0, binding = 1) writeonly uniform imageCube u_cubemap;

layout(push_constant) uniform PushConstants {
    vec2 image_dims;
};

// Cubemap helper function for GLSL
vec3 uv_to_xyz(ivec3 cubeCoord, vec2 cubemapSize) {
    vec2 texCoord = vec2(cubeCoord.xy) / cubemapSize;
    texCoord = texCoord * 2.0f - 1.0f;
    switch (cubeCoord.z) {
    case 0: return vec3(1.0f, -texCoord.yx);             // +X
    case 1: return vec3(-1.0f, -texCoord.y, texCoord.x); // -X
    case 2: return vec3(texCoord.x, 1.0f, texCoord.y);   // +Y
    case 3: return vec3(texCoord.x, -1.0f, -texCoord.y); // -Y
    case 4: return vec3(texCoord.x, -texCoord.y, 1.0f);  // +Z
    case 5: return vec3(-texCoord.xy, -1.0f);            // -Z
    }
    return vec3(0.0);
}

// x = rsin0 * rcosp
// y = rsin0 * rsinp
// z = rcos0
#define PI 3.1415926535897
#define PI_2 1.57079632679
vec2 project_on_sphere(vec3 direction) {
    vec2 uv = vec2(atan(direction.z, direction.x), asin(direction.y));
    uv *= vec2(0.1591, 0.3183);
    uv += 0.5f;
    return uv;
}

void main() {
    ivec3 id = ivec3(gl_GlobalInvocationID.xyz);
    vec3 direction = uv_to_xyz(id, image_dims);
    vec2 uv = project_on_sphere(direction);
    vec3 color = texture(u_hdri, uv).rgb;
    imageStore(u_cubemap, id, vec4(color, 1.0f));
}