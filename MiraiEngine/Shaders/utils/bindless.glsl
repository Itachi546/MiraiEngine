#ifndef BINDLESS_GLSL
#define BINDLESS_GLSL

#extension GL_EXT_nonuniform_qualifier : enable

const uint K_INVALID_TEXTURE = 0xffffffff;

layout(set = 1, binding = 10) uniform sampler2D u_bindless_texture[];
layout(set = 1, binding = 10) uniform samplerCube u_bindless_texture_cube[];

#define sample_texture(index, uv) texture(u_bindless_texture[nonuniformEXT(index)], uv)
#define sample_texture_lod(index, uv, lod) textureLod(u_bindless_texture[nonuniformEXT(index)], uv, lod)

#define sample_texture_cube(index, uv) texture(u_bindless_texture_cube[nonuniformEXT(index)], uv)
#define sample_texture_cube_lod(index, uv, lod) textureLod(u_bindless_texture_cube[nonuniformEXT(index)], uv, lod)

#endif