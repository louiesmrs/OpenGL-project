#version 330 core

in vec3 worldPosition;
in vec3 worldNormal; 
in vec2 uv;

out vec3 finalColor;

uniform sampler2D tex;
uniform vec3 lightPosition;
uniform vec3 lightIntensity;


void main()
{
	// Lighting
	vec3 color = texture(tex, uv).rgb;
	vec3 lightDir = lightPosition - worldPosition;
	float lightDist = dot(lightDir, lightDir);
	lightDir = normalize(lightDir);
	vec3 v = lightIntensity * clamp(dot(lightDir, worldNormal), 0.0, 1.0) / lightDist * color;

	// Tone mapping
	v = v / (1.0 + v);

	// Gamma correction
	finalColor = pow(v, vec3(1.0 / 2.2));
}
