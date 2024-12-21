#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 vertexUV;
layout (location = 3) in mat4 aInstanceMatrix;

out vec3 fragmentPosition;
out vec3 normal;
out vec2 textureCoord;
out vec3 lightSpaceCoord;

uniform mat4 u_model;
uniform mat4 MVP;
uniform mat4 lightSpaceMatrix;

void main()
{
    
    vec4 position = u_model * aInstanceMatrix * vec4(vertexPosition, 1.0);
    fragmentPosition = position.xyz / position.w;
    normal = transpose(inverse(mat3(u_model * aInstanceMatrix))) * vertexNormal;
    textureCoord = vertexUV;

    vec4 lightSpacePosition = lightSpaceMatrix * position;
    lightSpaceCoord = lightSpacePosition.xyz / lightSpacePosition.w;

    gl_Position = MVP * position;
}