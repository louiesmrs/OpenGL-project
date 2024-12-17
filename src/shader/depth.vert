#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 3) in vec3 aOffset;

uniform mat4 lightSpaceMatrix;
uniform mat4 u_model;

void main()
{
    gl_Position = lightSpaceMatrix * u_model * vec4(aPos+aOffset, 1.0);
}