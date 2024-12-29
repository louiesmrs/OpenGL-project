#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 vertexUV;
layout (location = 2) in vec3 vertexColor;
layout (location = 3) in mat4 aInstanceMatrix;


out vec2 uv;
out vec3 color;


uniform vec3 u_viewPos;
uniform mat4 MVP;




void main() {
    uv = vertexUV;
    color = vertexColor;
    gl_Position = MVP * aInstanceMatrix * vec4(aPos,1.0);
}