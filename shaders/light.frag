#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragmentPosition;
layout(location = 1) in vec3 fragmentColor;
layout(location = 2) in float fragmentStrength;

layout(location = 0) out vec4 gPosition; //w is for to ignore
layout(location = 1) out vec4 gColor; //w is for transparency
layout(location = 2) out vec4 gNormal; //w is for specular
layout(location = 3) out vec4 gExtra; //bloom (xyz), water? (w)

float magnitude(in vec3 v) {
    return sqrt(dot(v, v));
}

void main() {
    gPosition = vec4(0.0f);
    gColor = vec4(fragmentColor*fragmentStrength, 1.0f);
    gNormal = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    gExtra = vec4(fragmentColor*fragmentStrength, 0.0f);
}
