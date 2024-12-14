#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
layout (location = 3) in vec3 aOffset;


out vec3 color;
out vec3 fragPos;
out vec3 normal;

uniform mat4 u_model;
uniform mat4 MVP;



void main() {
    fragPos = vec3(u_model * vec4(aPos + aOffset, 1.0));
    normal = aNormal;
    color = aColor;
    gl_Position = MVP * u_model * vec4(aPos+aOffset , 1.0);
}