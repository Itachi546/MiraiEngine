#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 hit_color;
void main() {
    hit_color = vec3(0.1, 0.1, 0.3);
}