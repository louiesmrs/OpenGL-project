#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec3 aOffset;


out vec3 fragPos;
out vec3 vertexNorm;
out vec4 lightSpaceView;
out vec2 uv;

uniform mat4 u_model;
uniform mat4 MVP;
uniform mat4 lightSpaceMatrix;


void main() {
    fragPos = vec3(u_model * vec4(aPos + aOffset, 1.0));
    vertexNorm = transpose(inverse(mat3(u_model))) * aNormal;
    uv = aUV;
    lightSpaceView = lightSpaceMatrix * vec4(fragPos, 1);
    gl_Position = MVP * u_model * vec4(aPos+aOffset , 1.0);
}