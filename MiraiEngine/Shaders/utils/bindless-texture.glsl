#ifndef BINDLESS_GLSL
#define BINDLESS_GLSL

#extension GL_EXT_nonuniform_qualifier : enable

const uint K_INVALID_TEXTURE = 0xffffffff;

layout(set = 2, binding = 10) uniform texture2D u_bindless_texture[];
layout(set = 2, binding = 10) uniform samplerCube u_bindless_texture_cube[];

#define sample_texture(index, sampler, uv) texture(sampler2D(u_bindless_texture[nonuniformEXT(index)], sampler), uv)
#define sample_texture_lod(index, sampler, uv, lod) textureLod(sampler2D(u_bindless_texture[nonuniformEXT(index)], sampler), uv, lod)

#define sample_texture_cube(index, sampler, uv) texture(sampler2D(u_bindless_texture_cube[nonuniformEXT(index)], sampler), uv)
#define sample_texture_cube_lod(index, sampler, uv, lod) textureLod(sampler2D(u_bindless_texture_cube[nonuniformEXT(index)], sampler), uv, lod)

#endif