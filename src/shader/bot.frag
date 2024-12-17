#version 330 core


in vec2 uv;
in vec3 normal;
in vec3 fragPos;
in vec4 lightSpaceView;


uniform vec3 u_viewPos;
uniform vec3 ambient;
uniform vec3 diffuse;
uniform vec3 specular;
uniform vec3 direction;
uniform sampler2D tex;
uniform vec4 baseColorFactor;
uniform bool isTexture;
uniform sampler2D shadowMap;

out vec4 finalColor;

float ShadowCalculation(vec4 fragPosLightSpace)
{
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadowMap, projCoords.xy).r; 
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // calculate bias (based on depth map resolution and slope)
    vec3 normal = normalize(normal);
    vec3 lightDir = normalize(-direction);
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    // check whether current frag pos is in shadow
    // float shadow = currentDepth - bias > closestDepth  ? 1.0 : 0.0;
    // PCF
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth  ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    
    // keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
    if(projCoords.z > 1.0)
        shadow = 0.0;
        
    return shadow;
}

void main()
{
    vec4 color;
    if(isTexture) {
        color = vec4(texture(tex, uv).rgb, 1.0);
    } else {
        color = baseColorFactor;
    }
	vec3 ambientL = ambient;
    
    // Diffuse lighting
    vec3 norm = normalize(normal);
    vec3 lightDir = normalize(-direction);
    float diff = max(dot(lightDir, norm), 0.0);
    vec3 diffuseL = diffuse * diff;

    // Specular lighting
    float specularStrength = 0.5;
    vec3 viewDir = normalize(u_viewPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, normal);

    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16);
    vec3 specularL = specular * spec;
	float shadow = ShadowCalculation(lightSpaceView);
	vec3 lighting = (ambientL + (1.0 - shadow) * (diffuseL + specularL));
	vec4 v = color * vec4(lighting, 1.0);
	v = v / (1.0 + v);

	// Gamma correction
	finalColor = pow(v, vec4(1.0 / 2.2));
}

	