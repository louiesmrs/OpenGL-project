#version 330 core

// Input
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexUV;
layout(location = 3) in vec4 a_joint;
layout(location = 4) in vec4 a_weight;



out vec2 uv;
out vec3 fragPos;
out vec3 normal;

uniform mat4 u_model;
uniform mat4 MVP;
uniform mat4 jointMat[65];
uniform int isSkinning;

void main() {
    // Transform vertex
    mat4 skinMat = mat4(1.0);
    if(isSkinning > 0) {
        skinMat = 
        a_weight.x * jointMat[int(a_joint.x)] +
        a_weight.y * jointMat[int(a_joint.y)] +
        a_weight.z * jointMat[int(a_joint.z)] +
        a_weight.w * jointMat[int(a_joint.w)];
    }
    gl_Position =  MVP * u_model * skinMat * vec4(vertexPosition, 1.0);

    // World-space geometry 
    fragPos = vec3(u_model * vec4(vertexPosition, 1.0));
    normal = normalize(mat3(transpose(skinMat)) * vertexNormal);
    uv = vertexUV;
}
