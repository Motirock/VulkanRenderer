#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
    vec3 cameraPosition;
    vec3 viewDirection;
    float nearPlane;
    float farPlane;
} ubo;

layout(location = 0) in uint inID;
layout(location = 1) in vec3 inInstancePosition;
layout(location = 2) in uint inInstanceOrientation;
layout(location = 3) in uint inInstanceBlockID;
layout(location = 4) in vec2 inInstanceWidthHeight;

layout(location = 0) out vec2 fragmentTextureCoordinates;
layout(location = 1) out vec3 fragmentColor;

const vec3 positions[24] = {
    //Positive x
    vec3( 1.0, 0.0, 0.0),
    vec3( 1.0,  1.0, 0.0),
    vec3( 1.0, 0.0,  1.0),
    vec3( 1.0,  1.0,  1.0),

    //Negative x
    vec3(0.0,  1.0, 0.0),
    vec3(0.0, 0.0, 0.0),
    vec3(0.0,  1.0,  1.0),
    vec3(0.0, 0.0,  1.0),

    //Positive y
    vec3( 1.0,  1.0, 0.0),
    vec3(0.0,  1.0, 0.0),
    vec3( 1.0,  1.0,  1.0),
    vec3(0.0,  1.0,  1.0),

    //Negative y
    vec3(0.0, 0.0, 0.0),
    vec3( 1.0, 0.0, 0.0),
    vec3(0.0, 0.0,  1.0),
    vec3( 1.0, 0.0,  1.0),

    //Positive z
    vec3(0.0, 0.0, 1.0),
    vec3( 1.0, 0.0, 1.0),
    vec3(0.0,  1.0, 1.0),
    vec3( 1.0,  1.0, 1.0),

    //Negative z
    vec3( 1.0, 0.0, 0.0),
    vec3(0.0, 0.0, 0.0),
    vec3( 1.0,  1.0, 0.0),
    vec3(0.0,  1.0, 0.0),
};

const vec2 textureCoordinates[4] = {
    vec2(0.0, 1.0)/8.0f,
    vec2(1.0, 1.0)/8.0f,
    vec2(0.0, 0.0)/8.0f,
    vec2(1.0, 0.0)/8.0f
};

const vec2 textureOffsets[3] = {
    vec2(0.0f, 0.0f)/8.0f,
    vec2(6.0f, 0.0f)/8.0f,
    vec2(3.0f, 0.0f)/8.0f,
};

const vec3 colors[4] = {
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(1.0, 1.0, 1.0)
};

void main() {
    vec3 localPosition = positions[inInstanceOrientation*4+inID];

    //Width and height
    if (inInstanceOrientation == 0 || inInstanceOrientation == 1)
        localPosition *= vec3(1.0f, inInstanceWidthHeight.x, inInstanceWidthHeight.y);
    else if (inInstanceOrientation == 2 || inInstanceOrientation == 3)
        localPosition *= vec3(inInstanceWidthHeight.x, 1.0f, inInstanceWidthHeight.y);
    else
        localPosition *= vec3(inInstanceWidthHeight.x, inInstanceWidthHeight.y, 1.0f);

    //Calculate the position of the vertex in clip space
    gl_Position = ubo.projection * ubo.view * vec4(inInstancePosition+localPosition, 1.0);

    //Determine the texture coordinates of the vertex
    fragmentTextureCoordinates = textureCoordinates[inID];
    fragmentTextureCoordinates += textureOffsets[inInstanceBlockID];
    fragmentColor = colors[inID];
}