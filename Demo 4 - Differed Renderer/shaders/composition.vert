#version 450 core

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
    vec3 cameraPosition;
    vec3 viewDirection;
    float nearPlane;
    float farPlane;
    float time;
} ubo;

layout(location = 0) in vec3 inPosition;

layout(location = 1) out vec2 fragmentTextureCoordinates;

void main() {
    //Calculate the position of the vertex in clip space
    gl_Position = vec4(inPosition.xy, 0.0, 1.0);
    fragmentTextureCoordinates = inPosition.xy * 0.5 + 0.5;
}  