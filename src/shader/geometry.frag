#version 330 core


in vec3 fragmentPosition;
in vec3 normal;
in vec2 textureCoord;
in vec3 lightSpaceCoord;


layout (location = 0) out vec3 fsPosition;
layout (location = 1) out vec3 fsNormal;
layout (location = 2) out vec4 fsAlbedo;
layout (location = 3) out vec3 fsLightSpaceCoord;

uniform sampler2D diffuseTexture;
uniform sampler2D normalMapTexture;

void main()
{
    fsPosition = fragmentPosition * 0.5 + 0.5;

    fsNormal = texture(normalMapTexture, textureCoord).rgb;

    if (length(fsNormal) == 0.0)
    {
        fsNormal = normal;
    }

    fsNormal = normalize(fsNormal.xyz) * 0.5 + 0.5;

    fsAlbedo = texture(diffuseTexture, textureCoord);

    fsLightSpaceCoord = lightSpaceCoord.xyz * 0.5 + 0.5;
}