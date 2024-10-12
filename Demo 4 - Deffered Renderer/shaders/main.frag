#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragmentPosition;
layout(location = 1) in vec3 fragmentColor;
layout(location = 2) in vec2 fragmentTextureCoordinates;
layout(location = 3) in vec3 fragmentNormal;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(texSampler, fragmentTextureCoordinates);
    outColor = vec4(fragmentNormal, 1.0);
}
