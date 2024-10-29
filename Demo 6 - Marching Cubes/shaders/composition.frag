#version 450

const float PI = 3.14159265359;

struct Light {
    vec3 position;
    vec3 color;
    float strength; //!= 0
};

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
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

layout(binding = 1) uniform sampler2D gSamplers[4];

layout(std430, binding = 2) readonly restrict buffer LightBuffer {
    int lightCount;
    Light lights[2000];
} lightBuffer;

layout(location = 0) out vec4 outColor;

layout(location = 1) in vec2 fragmentTextureCoordinates;

vec3 toneMap(inout vec3 color) {
    // //Exposure tone mapping
    // color = vec3(1.0) - exp(-color * ubo.exposure);
    // //Gamma correction 
    // color = pow(color, vec3(1.0 / ubo.gamma));
    return color;
}

float magnitude(in vec3 v) {
    return sqrt(dot(v, v));
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// --------------

void main() {
    vec4 tempPosition = texture(gSamplers[0], fragmentTextureCoordinates);
    vec3 position = tempPosition.xyz;
    vec3 albedo = texture(gSamplers[1], fragmentTextureCoordinates).xyz;

    if (tempPosition.w == 0.0) {
        vec3 hdrColor = albedo + texture(gSamplers[3], fragmentTextureCoordinates).xyz;
        outColor = vec4(toneMap(hdrColor), 1.0);
        return;
    }

    vec4 tempNormal = texture(gSamplers[2], fragmentTextureCoordinates);
    vec3 normal = tempNormal.xyz;
    float roughness = tempNormal.w;

    outColor = vec4(normal, 1.0f);
    return;
    
    float ambient = 0.005f;
    vec3 hdrColor = albedo * ambient; //Hard-coded ambient component
    vec3 viewDirection = normalize(ubo.cameraPosition - position);

    vec3 V = normalize(ubo.cameraPosition-position);
    vec3 N = normal;
    float metallic = 0.0f;
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);


    for(int i = 0; i < lightBuffer.lightCount; i++)  {
        // calculate per-light radiance
        vec3 L = normalize(lightBuffer.lights[i].position - position);
        vec3 H = normalize(ubo.viewDirection + L);
        float distance = length(lightBuffer.lights[i].position - position);
        float attenuation = lightBuffer.lights[i].strength / (distance * distance);
        vec3 radiance = lightBuffer.lights[i].color * attenuation;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, V, L, roughness);      
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
           
        vec3 numerator    = NDF * G * F; 
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
        vec3 specular = numerator / denominator;
        
        // kS is equal to Fresnel
        vec3 kS = F;
        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kD) should equal 1.0 - kS.
        vec3 kD = vec3(1.0) - kS;
        // multiply kD by the inverse metalness such that only non-metals 
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
        kD *= 1.0 - metallic;	  

        // scale light by NdotL
        float NdotL = max(dot(N, L), 0.0);        

        // add to outgoing radiance Lo
        hdrColor += (kD * albedo / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
    }

    // calculate per-light radiance
    vec3 L = normalize(vec3(1.0f));
    vec3 H = normalize(ubo.viewDirection + L);
    vec3 radiance = vec3(0.0f);

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);      
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
    vec3 numerator    = NDF * G * F; 
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    vec3 specular = numerator / denominator;
    
    // kS is equal to Fresnel
    vec3 kS = F;
    // for energy conservation, the diffuse and specular light can't
    // be above 1.0 (unless the surface emits light); to preserve this
    // relationship the diffuse component (kD) should equal 1.0 - kS.
    vec3 kD = vec3(1.0) - kS;
    // multiply kD by the inverse metalness such that only non-metals 
    // have diffuse lighting, or a linear blend if partly metal (pure metals
    // have no diffuse light).
    kD *= 1.0 - metallic;	  

    // scale light by NdotL
    float NdotL = max(dot(N, L), 0.0);        

    // add to outgoing radiance Lo
    hdrColor += (kD * albedo / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again

    hdrColor += texture(gSamplers[3], fragmentTextureCoordinates).xyz;
  
    outColor = vec4(toneMap(hdrColor), 1.0);
}