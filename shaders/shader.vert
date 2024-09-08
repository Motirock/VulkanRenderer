#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 cameraPosition;
} ubo;

layout(location = 0) in uint inID;
layout(location = 1) in vec3 inInstancePosition;
layout(location = 2) in uint inInstanceOrientation;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragColor;

const vec3 positions[24] = {
    //Positive x
    vec3( 0.5, -0.5, -0.5),
    vec3( 0.5,  0.5, -0.5),
    vec3( 0.5, -0.5,  0.5),
    vec3( 0.5,  0.5,  0.5),

    //Negative x
    vec3(-0.5,  0.5, -0.5),
    vec3(-0.5, -0.5, -0.5),
    vec3(-0.5,  0.5,  0.5),
    vec3(-0.5, -0.5,  0.5),

    //Positive y
    vec3( 0.5,  0.5, -0.5),
    vec3(-0.5,  0.5, -0.5),
    vec3( 0.5,  0.5,  0.5),
    vec3(-0.5,  0.5,  0.5),

    //Negative y
    vec3(-0.5, -0.5, -0.5),
    vec3( 0.5, -0.5, -0.5),
    vec3(-0.5, -0.5,  0.5),
    vec3( 0.5, -0.5,  0.5),

    //Positive z
    vec3(-0.5, -0.5,  0.5),
    vec3( 0.5, -0.5,  0.5),
    vec3(-0.5,  0.5,  0.5),
    vec3( 0.5,  0.5,  0.5),

    //Negative z
    vec3( 0.5, -0.5, -0.5),
    vec3(-0.5, -0.5, -0.5),
    vec3( 0.5,  0.5, -0.5),
    vec3(-0.5,  0.5, -0.5),
};

const vec2 textureCoordinates[4] = {
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0)
};

const vec3 colors[4] = {
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(1.0, 1.0, 1.0)
};

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inInstancePosition+positions[inInstanceOrientation*4+inID], 1.0);
    fragTexCoord = textureCoordinates[inID];
    fragColor = colors[inID];
    //fragColor = rotatedPosition;
}