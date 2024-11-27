#version 330 core

// Input
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexUV;
layout(location = 3) in vec4 a_joint;
layout(location = 4) in vec4 a_weight;

// Output data, to be interpolated for each fragment
out vec3 worldPosition;
out vec3 worldNormal;
out vec2 uv;

uniform mat4 MVP;
uniform mat4 jointMat[65];

void main() {
    // Transform vertex
    mat4 skinMat =
        a_weight.x * jointMat[int(a_joint.x)] +
        a_weight.y * jointMat[int(a_joint.y)] +
        a_weight.z * jointMat[int(a_joint.z)] +
        a_weight.w * jointMat[int(a_joint.w)];
    gl_Position =  MVP * skinMat * vec4(vertexPosition, 1.0);

    // World-space geometry 
    worldPosition = vertexPosition;
    worldNormal = normalize(mat3(transpose(skinMat)) * vertexNormal);
    uv = vertexUV;
}
