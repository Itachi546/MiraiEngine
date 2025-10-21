#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 vNormal;

void main() {
    vec3 n = normalize(vNormal);
    vec3 wi = normalize(vec3(1.0));
    float d = dot(wi, n) * 0.5 + 0.5;
    vec3 albedo = vec3(0.988, 0.772, 0.588);
    vec3 shading = (d / 3.14159) * albedo;
    outColor = vec4(shading, 1.0f);
}