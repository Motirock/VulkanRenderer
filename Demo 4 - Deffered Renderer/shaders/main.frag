#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragmentPosition;
layout(location = 1) in vec3 fragmentColor;
layout(location = 2) in vec2 fragmentTextureCoordinates;
layout(location = 3) in vec3 fragmentNormal;

layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gColor;
layout(location = 2) out vec4 gNormal;

void main() {
    gPosition = vec4(fragmentPosition, 1.0);
    if (texture(texSampler, fragmentTextureCoordinates).w <= 0.01f)
        discard;
    gColor = vec4(texture(texSampler, fragmentTextureCoordinates).xyz, 1.0f);
    gNormal = vec4(fragmentNormal, 1.0);
}
