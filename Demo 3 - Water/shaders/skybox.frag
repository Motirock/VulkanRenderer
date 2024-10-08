#version 450

layout(binding = 2) uniform sampler2D skyboxSamplers[6];

layout(location = 0) in vec2 fragmentTextureCoordinates;
layout(location = 1) in flat uint fragmentFaceIndex;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 sampledColor = texture(skyboxSamplers[fragmentFaceIndex], fragmentTextureCoordinates);
    outColor = sampledColor;
}