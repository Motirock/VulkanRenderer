#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(binding = 2) uniform sampler2D skyboxSamplers[6];

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 sampledColor = texture(texSampler, fragTexCoord);
    outColor = vec4(fragColor, 1.0) * sampledColor;
    //outColor = vec4(fragColor, 1.0);
    //outColor = vec4(1.0f);
}
