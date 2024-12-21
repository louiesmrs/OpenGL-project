#version 330 core

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexUV;
layout(location = 3) in vec4 a_joint;
layout(location = 4) in vec4 a_weight;


out vec3 fragmentPosition;
out vec3 normal;
out vec2 textureCoord;
out vec3 lightSpaceCoord;

uniform mat4 u_model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 jointMat[65];
uniform bool isSkinning;
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
    normal = transpose(inverse(mat3(view * skinMat))) * vertexNormal;
    textureCoord = vertexUV;

    vec4 lightSpacePos = lightSpaceMatrix * position;
    lightSpaceCoord = lightSpacePos.xyz / lightSpacePos.w;

    gl_Position = projection * view * position;
}