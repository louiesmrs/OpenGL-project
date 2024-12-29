#version 330 core

in vec3 fragPos;
in vec3 cameraPosition;
in vec2 uv;

uniform sampler2D tex;

out vec4 finalColor;

const float FOG_MIN_DIST = 1024;
const float FOG_MAX_DIST = 2048;
const vec4 FOG_COLOUR = vec4(0.004f, 0.02f, 0.05f, 0.0);

void main()
{
    // Fogging
    float distanceToCamera = length(fragPos - cameraPosition);
    float fogFactor = smoothstep(FOG_MIN_DIST, FOG_MAX_DIST, distanceToCamera);

    // Texture (use instance alpha)
    vec4 texColor = texture(tex, uv);
    //vec4 fragColor = vec4(texColor.rgb, texColor.a * alpha);

    // Discard fragments that are barely visible
    // if (fragColor.a < 0.01) {
    //     discard;
    // }

    finalColor = texture(tex, uv);
}