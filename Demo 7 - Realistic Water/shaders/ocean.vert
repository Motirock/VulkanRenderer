#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 projection;
    mat4 reflectView;
    mat4 refractView;
    vec3 cameraPosition;
    vec3 viewDirection;
    float nearPlane;
    float farPlane;
    float time;
    float gamma;
    float exposure;
} ubo;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 fragmentPosition;

void main() {
    gl_Position = ubo.projection * ubo.view * vec4(inPosition, 1.0);
    fragmentPosition = vec4(inPosition, gl_Position.w);
}