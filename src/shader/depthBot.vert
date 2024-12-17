#version 330 core
layout (location = 0) in vec3 aPos;
layout(location = 3) in vec4 a_joint;
layout(location = 4) in vec4 a_weight;

uniform mat4 lightSpaceMatrix;
uniform bool isSkinning;
uniform mat4 u_model;
uniform mat4 jointMat[65];

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
    gl_Position = lightSpaceMatrix * u_model * skinMat * vec4(aPos, 1.0);
}