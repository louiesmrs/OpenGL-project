#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
layout (location = 3) in vec3 aOffset;


out vec3 fragmentPosition;
out vec3 normal;
out vec2 textureCoord;
out vec3 lightSpaceCoord;

uniform mat4 u_model;
uniform mat4 MVP;
uniform mat4 lightSpaceMatrix;

void main()
{
    mat4 skinMat = mat4(1.0);
    
    if(isSkinning) {
        skinMat = 
        a_weight.x * jointMat[int(a_joint.x)] +
        a_weight.y * jointMat[int(a_joint.y)] +
        a_weight.z * jointMat[int(a_joint.z)] +
        a_weight.w * jointMat[int(a_joint.w)];
    }
    vec4 position = u_model * skinMat * vec4(vertexPosition, 1.0);
    fragmentPosition = position.xyz / position.w;
    normal = transpose(inverse(mat3(skinMat))) * vertexNormal;
    textureCoord = vertexUV;

    vec4 lightSpaceCoord = lightSpaceMatrix * position;
    lightSpaceCoord = lightSpaceCoord.xyz / lightSpaceCoord.w;

    gl_Position = MVP * position;
    vec4 position = u_model * vec4(aPos + aOffset, 1.0);
    fragmentPosition = position.xyz / position.w;
    normal = transpose(inverse(mat3(u_model))) * vertexNormal;
    textureCoord = vertexUV;

    vec4 lightSpaceCoord = lightSpaceMatrix * position;
    lightSpaceCoord = lightSpaceCoord.xyz / lightSpaceCoord.w;

    gl_Position = MVP * position;
}