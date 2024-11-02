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

layout(push_constant) uniform PushConstant {
    uint isWater;
    uint isReflect;
} pushConstant;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in float inStrength;

layout(location = 0) out vec3 fragmentPosition;
layout(location = 1) out vec3 fragmentColor;
layout(location = 2) out float fragmentStrength;

void main() {
     if (pushConstant.isWater == 0) {
        gl_Position = ubo.projection * ubo.view * vec4(inPosition, 1.0);
    }
    else {
        if (pushConstant.isReflect == 1) {
            gl_Position = ubo.projection * ubo.reflectView * vec4(inPosition, 1.0);    
            gl_ClipDistance[0] = inPosition.z-5.0f;
        }
        else {
            gl_Position = ubo.projection * ubo.refractView * vec4(inPosition, 1.0);
            gl_ClipDistance[0] = 5.0f-inPosition.z;
        }
    }

    fragmentPosition = inPosition;
    fragmentColor = inColor;
    fragmentStrength = inStrength;
}
