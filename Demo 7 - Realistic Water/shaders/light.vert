#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 projection;
    vec3 cameraPosition;
    vec3 viewDirection;
    float nearPlane;
    float farPlane;
    float time;
    float gamma;
    float exposure;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in float inStrength;

layout(location = 0) out vec3 fragmentPosition;
layout(location = 1) out vec3 fragmentColor;
layout(location = 2) out float fragmentStrength;

void main() {
    gl_Position = ubo.projection * ubo.view * vec4(inPosition, 1.0);
    fragmentPosition = inPosition;
    fragmentColor = inColor;
    fragmentStrength = inStrength;
}
