#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec4 fragmentColor;
layout(location = 1) in vec3 fragmentNormal;
layout(location = 2) in float fragmentHeight;

vec3 lightDirection = vec3(0.1f, 0.5f, -1.0f);

void main() {
    outColor = (0.5f+fragmentHeight/2.0f)*vec4(0.05f, 0.5f, 0.80f, 0.8f)+0.1*sqrt(max(dot(lightDirection, normalize(fragmentNormal)), 0.0f))*vec4(0.95f, 0.5f, 0.20f, 0.20f);
}