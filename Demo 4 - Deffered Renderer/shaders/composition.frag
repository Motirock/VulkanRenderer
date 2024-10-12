#version 450

layout(binding = 0) uniform sampler2D colorSampler;

layout(location = 0) out vec4 outColor;

layout(location = 1) in vec2 fragmentTextureCoordinates;

void main() {
    outColor = texture(colorSampler, fragmentTextureCoordinates);
    //outColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
}