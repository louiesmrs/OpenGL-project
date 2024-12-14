#version 330 core


in vec3 color;
in vec3 normal;
in vec3 fragPos;


uniform vec3 u_viewPos;
uniform vec3 ambient;
uniform vec3 diffuse;
uniform vec3 specular;
uniform vec3 direction;


out vec3 finalColor;

vec3 calculateLighting(vec3 Normal, vec3 FragPos) {
    // Ambient lighting
    vec3 ambientL = ambient;
    
    // Diffuse lighting
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(-direction);
    float diff = max(dot(lightDir, norm), 0.0);
    vec3 diffuseL = diffuse * diff;

    // Specular lighting
    float specularStrength = 0.5;
    vec3 viewDir = normalize(u_viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, Normal);

    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16);
    vec3 specularL = specular * spec;
    
    return (ambientL + diffuseL + specularL);
}

void main()
{
	finalColor = color * calculateLighting(normal, fragPos);
	
}
