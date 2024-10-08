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

layout(location = 0) out vec4 fragmentColor;
layout(location = 1) out vec3 fragmentNormal;
layout(location = 2) out float fragmentHeight;

const float IDK[8] = {
    1.0f, 1.0f, 0.5f, 0.5f, 0.25f, 0.25f, 0.25f, 0.25f
};

const float a[8*2] = {
    0.527f, 0.543f,
    0.809f, 0.256f,
    0.517f, 0.383f,
    0.368f, 0.466f,
    0.646f, 0.208f,
    0.307f, 0.963f,
    0.915f, 0.446f,
    0.580f, 0.497f,
};

const float w[8*2] = {
    0.706f,   0.401f,
    -0.196f,  0.083f,
    -0.286f, -0.389f,
    0.893f,   0.567f,
    0.902f,  -0.427f,
    0.073f,  -0.337f,
    0.074f,  -0.780f,
    -0.079f,  0.411f,
};

const float p[8*2] = {
     0.415f, -0.056f,
     0.306f, -0.575f,
     0.302f, -0.781f,
    -0.490f,  0.803f,
     0.201f, -0.919f,
    -0.560f, -0.624f,
     0.490f,  0.090f,
    -0.109f,  0.264f,
};

float maxAmplitude = 10.0f;

vec4 getAmplitudeAndNormal(in float x, in float y, in float time) {
    int octaves = 8;

    float amplitude = 0.0f;

    float slopeX = 0.0f;
    float slopeY = 0.0f;

    for(int i = 0; i < octaves; i++) {
        amplitude += IDK[i]*a[i*2]/(i+1)   * sin(w[i*2]*x   + p[i*2]*time);
        amplitude += IDK[i]*a[i*2+1]/(i+1) * sin(w[i*2+1]*y + p[i*2+1]*time+1.57f);

        slopeX +=     IDK[i]*a[i*2]/(i+1)   * cos(w[i*2]*x   + p[i*2]*time);
        slopeY +=     IDK[i]*a[i*2+1]/(i+1) * cos(w[i*2+1]*y + p[i*2+1]*time+1.57f);
    }
    amplitude *= 0.33333f*0.5f*maxAmplitude;

    return vec4(amplitude, cross(vec3(slopeX, 0.0f, 1.0f), (vec3(0.0f, slopeY, 1.0f))));
    //return vec4(amplitude, vec3(-slopeX, 0.0f, 1.0f));
}


vec3 lightDirection = -vec3(-0.577);

void main() {
    vec4 amplitudeAndNormal = getAmplitudeAndNormal(inPosition.x, inPosition.y, ubo.time*5.0f);

    //Get amplitude
    float amplitude = amplitudeAndNormal.x;

    vec3 normal = amplitudeAndNormal.yzw;
    
    //Calculate the position of the vertex in clip space
    gl_Position = ubo.projection * ubo.view * vec4(inPosition.xy, inPosition.z+amplitude, 1.0);
    
    fragmentNormal = normal;

    fragmentHeight = (amplitude/maxAmplitude)/2.0f+0.5f;

    //fragmentColor = vec4(0.05f, 0.5f, 0.80f, 0.8f)+max(dot(lightDirection, normalize(normal)), 0.0f)*vec4(0.95f, 0.5f, 0.20f, 0.20f);
    //fragmentColor = vec4(vec2(0.0f+1.0f*max(dot(lightDirection, normalize(normal)), 0.0f)), 1.0f, 0.6f+max(0.4f*dot(lightDirection, normalize(normal)), 0.0f));
    //fragmentColor = vec4(vec3(0.5f+amplitude/maxAmplitude), 1.0f);
}  