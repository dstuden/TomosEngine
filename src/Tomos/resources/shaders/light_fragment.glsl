#version 430 core

#define MAX_LIGHTS 128
#define PI 3.14159265359

in vec2 vTexCoord;
out vec4 FragColor;

// G-buffer
uniform sampler2D uGBase;
uniform sampler2D uGNormal;
uniform sampler2D uGMtlRgh;
uniform sampler2D uGEmission;
uniform sampler2D uDepth;

// Camera
uniform vec3 uCameraPos;
uniform mat4 uInverseViewProj;
uniform float uNear;
uniform float uFar;

struct Light {
    vec3 position;
    float pad0;  // 4 bytes padding
    vec3 direction;
    float pad0;  // 4 bytes padding
    vec3 color;

    int type;
    float intensity;
    float maxRange;
    float innerCone;
    float outerCone;
};

// Lights as SSBO
layout (std430, binding = 0) buffer Lights {
    Light lights[MAX_LIGHTS];
    int lightCount;
};

// G-buffer tex coord
vec2 getScreenCoord(vec2 fragCoord) {
    vec2 res = textureSize(uGBase, 0);
    return fragCoord / res;
}

// Reconstruct world-space position from screen coord + depth
vec3 reconstructPosition(vec2 screenCoord, float depth) {
    float z = depth * 2.0 - 1.0;
    vec4 clip = vec4(screenCoord * 2.0 - 1.0, z, 1.0);
    vec4 world = uInverseViewProj * clip;
    return world.xyz / world.w;
}

// Fresnel-Schlick
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// GGX normal distribution
float distributionGGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// Geometry function (Schlick-GGX)
float geometrySchlickGGX(float NdotV, float roughness) {
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

// Smith geometry
float geometrySmith(float NdotV, float NdotL, float roughness) {
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

// Cook-Torrance BRDF
vec3 brdf(
    vec3 N, vec3 V, vec3 L,
    vec3 baseColor, float metallic, float roughness
) {
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    vec3 F0 = mix(vec3(0.04), baseColor, metallic);
    vec3 F = fresnelSchlick(VdotH, F0);
    float D = distributionGGX(NdotH, roughness);
    float G = geometrySmith(NdotV, NdotL, roughness);

    vec3 numerator = D * G * F;
    float denom = 4.0 * NdotV * NdotL + 0.001;
    vec3 spec = numerator / denom;

    vec3 kd = vec3(1.0) - F;
    kd *= 1.0 - metallic;

    return (kd * baseColor / PI + spec) * NdotL;
}

void main() {
    vec2 screenUV = getScreenCoord(gl_FragCoord.xy);
    float depth = texture(uDepth, screenUV).r;

    if (depth == 1.0) discard;// background pixel

    vec3 pos = reconstructPosition(screenUV, depth);
    vec3 baseColor = texture(uGBase, screenUV).rgb;
    vec3 normal = normalize(texture(uGNormal, screenUV).rgb * 2.0 - 1.0);
    vec2 metRgh = texture(uGMtlRgh, screenUV).rg;
    float metallic = metRgh.r;
    float roughness = metRgh.g;
    vec3 emission = texture(uGEmission, screenUV).rgb;
    vec3 viewDir = normalize(uCameraPos - pos);

    vec3 result = vec3(0.0);

    for (int i = 0; i < lightCount; ++i) {
        if (lights[i].intensity < 0.001) continue;

        vec3 lightDir;
        float attenuation = 1.0;
        vec3 delta;

        // Modified condition: Only type 0 (Directional) uses lights[i].direction directly.
        // Point (type 1) and Spot (type 2) lights will fall into the 'else' block.
        if (lights[i].type == 0) { // Directional Light
                                   lightDir = normalize(-lights[i].direction);
        } else { // Handles Point (type 1) and Spot (type 2) lights
                 delta = lights[i].position - pos;
                 float dist = length(delta);
                 lightDir = normalize(delta);
                 if (dist > lights[i].maxRange) continue;
                 attenuation = 1.0 / (dist * dist + 0.0001);

                 if (lights[i].type == 2) { // Spot Light cone falloff
                                            float coneInner = cos(lights[i].innerCone);
                                            float coneOuter = cos(lights[i].outerCone);
                                            float angle = dot(lightDir, normalize(-lights[i].direction));
                                            float cone = clamp((angle - coneOuter) / (coneInner - coneOuter), 0.0, 1.0);
                                            attenuation *= cone;
                 }
        }

        vec3 radiance = lights[i].color * lights[i].intensity * attenuation;
        result += brdf(normal, viewDir, lightDir, baseColor, metallic, roughness) * radiance;
    }

    // Basic ambient + emissive
    result += baseColor * 0.2 + 5.0 * emission;

    FragColor = vec4(result, 1.0);
}