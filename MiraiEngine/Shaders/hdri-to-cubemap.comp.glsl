#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D u_hdri;
layout(set = 0, binding = 1) writeonly uniform imageCube u_cubemap;

layout(push_constant) uniform PushConstants {
    vec2 cubemap_size;
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

const vec2 inv_atan = vec2(0.1591, 0.3183);
vec2 spherical_coord(vec3 p) {
    vec2 uv = vec2(atan(p.z, p.x), asin(p.y));
    uv *= inv_atan;
    uv += 0.5f;
    return uv;
}

void main() {
    ivec3 cube_coord = ivec3(gl_GlobalInvocationID.xyz);
    vec3 p = normalize(uv_to_xyz(cube_coord, cubemap_size));
    vec3 color = texture(u_hdri, spherical_coord(p)).rgb;
    imageStore(u_cubemap, cube_coord, vec4(color, 1.0f));
}