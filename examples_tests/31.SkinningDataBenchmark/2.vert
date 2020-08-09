#version 460 core

layout( push_constant, row_major ) uniform Block 
{
    uint normalMatrixArrayOffset;
}pc;

layout(std430, set = 0, binding = 0, row_major) restrict readonly buffer BoneMatrices
{
    mat4 matrix[];
};

layout(location = 0) in vec3 pos;
layout(location = 3) in vec3 normal;
layout(location = 4) in int boneID;

layout(location = 0) out vec3 vNormal;

void main()
{
    gl_Position = matrix[boneID] * vec4(pos, 1.0);
    vNormal = vec3(matrix[pc.normalMatrixArrayOffset + boneID] * vec4(normalize(normal), 0.0));
}