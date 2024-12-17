#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 vertexUV;
layout (location = 3) in mat4 aInstanceMatrix;


out vec2 uv;
out vec3 fragPos;
out vec3 normal;
out vec4 lightSpaceView;


uniform mat4 u_model;
uniform mat4 MVP;
uniform mat4 lightSpaceMatrix;




void main() {
    fragPos = vec3(u_model * aInstanceMatrix * vec4(aPos, 1.0));
    normal = aNormal;
    uv= vertexUV;
    lightSpaceView = lightSpaceMatrix * vec4(aPos, 1);
    gl_Position = MVP * vec4(fragPos,1.0);
}