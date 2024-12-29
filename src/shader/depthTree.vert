#version 330 core
layout (location = 0) in vec3 aPos;
layout(location = 5) in mat4 aInstanceMatrix;

uniform mat4 lightSpaceMatrix;
uniform mat4 u_model;

void main()
{
    gl_Position = lightSpaceMatrix * u_model * aInstanceMatrix * vec4(aPos, 1.0);
}