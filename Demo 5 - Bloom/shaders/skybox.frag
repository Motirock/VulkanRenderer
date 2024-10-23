#version 450

layout(binding = 2) uniform sampler2D skyboxSamplers[6];

layout(location = 0) in vec2 fragmentTextureCoordinates;
layout(location = 1) in flat uint fragmentFaceIndex;

layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gColor;
layout(location = 2) out vec4 gNormal;
layout(location = 3) out vec4 gExtra;

void main() {
    vec4 sampledColor = texture(skyboxSamplers[fragmentFaceIndex], fragmentTextureCoordinates);
    gPosition = vec4(0.0f);
    gColor = vec4(sampledColor.xyz, 1.0f);
    gNormal = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    gExtra = vec4(sampledColor.xyz*2.0f, 0.0f);
}