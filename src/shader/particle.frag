#version 330 core


in vec2 uv;
in vec3 color;

uniform sampler2D tex;

out vec4 finalColor;


void main()
{
    vec4 v = texture(tex, uv) * vec4(color, 1.0f);
    v = v / (1.0 + v);
    finalColor = pow(v, vec4(1.0 / 2.2));

}