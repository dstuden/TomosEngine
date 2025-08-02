#version 430 core

uniform vec4 uBaseFactor;
uniform vec3 uEmissionFactor;
uniform float uMetalFactor;
uniform float uRoughFactor;
uniform float uNormalScale;
uniform float uAlphaCutoff;

uniform sampler2D uBaseTexture;
uniform sampler2D uMetalRoughTexture;
uniform sampler2D uNormalTexture;

in vec3 vNormal;
in vec3 vTangent;
in float vTangentW;
in vec2 vTexCoord;

layout (location = 0) out vec4 outBase;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec2 outMtlRgh;
layout (location = 3) out vec4 outEmission;

void main()
{
    vec4 baseColor = texture(uBaseTexture, vTexCoord) * uBaseFactor;
    if (baseColor.a < uAlphaCutoff)
    discard;

    vec2 metalRough = texture(uMetalRoughTexture, vTexCoord).zy;

    mat3 TBN;

    vec3 N = normalize(vNormal);
    float tangentLen = length(vTangent);
    if (tangentLen < 1e-4) {
        vec3 up = abs(N.z) < 0.999 ? vec3(0, 0, 1) : vec3(0, 1, 0);
        vec3 T = normalize(cross(up, N));
        vec3 B = cross(N, T) * vTangentW;
        TBN = mat3(T, B, N);
    } else {
        vec3 T = normalize(vTangent - dot(vTangent, N) * N);
        T = normalize(T);
        vec3 B = cross(N, T) * vTangentW;
        TBN = mat3(T, B, N);
    }

    // Sample normal map in tangent space and scale it
    vec3 texNormal = texture(uNormalTexture, vTexCoord).xyz * 2.0 - 1.0;
    vec3 scaledNormal = normalize(vec3(texNormal.xy * uNormalScale, texNormal.z));

    vec3 worldNormal = normalize(TBN * scaledNormal);
    //    vec3 worldNormal = normalize(vNormal);

    float metallic = metalRough.x * uMetalFactor;
    float roughness = metalRough.y * uRoughFactor;

    outBase = baseColor;
    outNormal = vec4((worldNormal * 0.5) + 0.5, 0.0); // encode normal to [0,1]
    outMtlRgh = vec2(metallic, roughness);
    outEmission = vec4(uEmissionFactor, 1.0);
}
