#version 430 core

struct Instance {
    mat4 transform;
};

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec4 aTangent; // xyz = tangent, w = bitangent sign

layout (std430, binding = 0) readonly buffer InstanceBuffer {
    Instance instances[];
};

uniform mat4 uViewProjection;

out vec3 vNormal;
out vec3 vTangent;
out float vTangentW;
out vec2 vTexCoord;

void main()
{
    mat4 model = instances[gl_InstanceID].transform;
    vec4 worldPos = model * vec4(aPosition, 1.0);

    gl_Position = uViewProjection * worldPos;

    // Normal matrix (transpose of inverse of model)
    mat3 normalMatrix = transpose(inverse(mat3(model)));

    vNormal = normalize(normalMatrix * aNormal);
    vTangent = normalize(normalMatrix * aTangent.xyz);
    vTangentW = aTangent.w;

    vTexCoord = aTexCoord;
}
