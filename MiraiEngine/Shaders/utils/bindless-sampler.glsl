#ifndef SAMPLER_GLSL
#define SAMPLER_GLSL

#define SAMPLER_LINEAR_REPEAT  0
#define SAMPLER_LINEAR_CLAMP 1
#define SAMPLER_LINEAR_REPEAT_ANISO16 2

#define SAMPLER_POINT_REPEAT 3
#define SAMPLER_POINT_CLAMP 4
#define SAMPLER_POINT_REPEAT_ANISO16 5

layout(set = 1, binding = 0) uniform sampler u_samplers[];
#endif