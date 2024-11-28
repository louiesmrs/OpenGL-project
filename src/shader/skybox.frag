#version 330 core


in vec3 TexCoords;

out vec3 finalColor;

uniform samplerCube skybox;



void main()
{    
    finalColor = texture(skybox, TexCoords).rgb;
}