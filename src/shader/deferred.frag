#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D shadowMap;
uniform sampler2D lightSpaceCoord;

uniform vec3 direction;
uniform vec3 u_viewPos;
uniform mat4 lightSpaceMatrix;

float attenuation_constant = 1.0;
float attenuation_linear = 0.09;
float attenuation_quadratic = 0.032;

const float NB_STEPS = 30;
const float G_SCATTERING = 0.858;

#define M_PI 3.1415926535897932384626433832795

void main()
{
    vec3 fragmentPosition = texture(gPosition, TexCoords).xyz * 2 - 1;
    vec3 normal = texture(gNormal, TexCoords).rgb * 2 - 1;
    vec4 albedoColor = texture(gAlbedoSpec, TexCoords);
    float specular = texture(gAlbedoSpec, TexCoords).a;
    vec3 ambientL = albedoColor.rgb * 0.1;
    // Diffuse lighting
    vec3 lightDir = normalize(-direction);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuseL = vec3(albedoColor) * diff;

    // Specular lighting
    float specularStrength = 0.5;
    vec3 viewDir = normalize(u_viewPos - fragmentPosition);
    vec3 reflectDir = reflect(-lightDir, normal);

    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16);
    vec3 specularL = vec3(1.0f)  * spec * specular;
    vec3 lighting = (ambientL + (1.0f) * (diffuseL + specularL));
	vec4 v = vec4(lighting,1.0);
	v = v / (1.0 + v);

	// Gamma correction
	FragColor = pow(v, vec4(1.0 / 2.2));
    // vec3 viewDirection = normalize(u_viewPos - fragmentPosition);

    // vec3 startPosition = u_viewPos;
    // vec3 endRayPosition = fragmentPosition;

    // vec3 rayVector = endRayPosition.xyz- startPosition;

    // float rayLength = length(rayVector);
    // vec3 rayDirection = normalize(rayVector);

    // float stepLength = rayLength / NB_STEPS;

    // vec3 raymarchingStep = rayDirection * stepLength;

    // vec3 currentPosition = startPosition;

    // vec4 accumFog = vec4(0.0);

    // for (int i = 0; i < NB_STEPS; i++)
    // {
    //     // basically perform shadow mapping
    //     vec4 worldInLightSpace = lightSpaceMatrix * vec4(currentPosition, 1.0f);
    //     worldInLightSpace /= worldInLightSpace.w;

    //     vec2 lightSpaceTextureCoord1 = (worldInLightSpace.xy * 0.5) + 0.5; // [-1..1] -> [0..1]
    //     float shadowMapValue1 = texture(shadowMap, lightSpaceTextureCoord1.xy).r;

    //     if (shadowMapValue1 > worldInLightSpace.z)
    //     {
    //         // Mie scaterring approximated with Henyey-Greenstein phase function
    //         float lightDotView = dot(normalize(rayDirection), normalize(-direction));

    //         float scattering = 1.0 - G_SCATTERING * G_SCATTERING;
    //         scattering /= (4.0f * M_PI * pow(1.0f + G_SCATTERING * G_SCATTERING - (2.0f * G_SCATTERING) * lightDotView, 1.5f));

    //         accumFog += scattering;
    //     }

    //     currentPosition += raymarchingStep;
    // }

    // accumFog /= NB_STEPS;

    // // fade rays away
    // // accumFog *= currentPosition / NB_STEPS);

    // // lighting *= accumFog;

    // vec4 fragmentPositionInLightSpace1 = lightSpaceMatrix * vec4(fragmentPosition, 1.0);
    // fragmentPositionInLightSpace1 /= fragmentPositionInLightSpace1.w;

    // vec2 shadowMapCoord1 = fragmentPositionInLightSpace1.xy * 0.5 + 0.5;

    // // vec3 fragmentPositionInLightSpace2 = texture(lightSpaceCoord, fsIn.textureCoord).xyz;

    // // vec2 shadowMapCoord2 = fragmentPositionInLightSpace2.xy;

    // float thisDepth = fragmentPositionInLightSpace1.z;
    // float occluderDepth = texture(shadowMap, shadowMapCoord1).r;

    // float shadowFactor = 0.0;

    // if (thisDepth > 0.0 && occluderDepth < 1.0 && thisDepth < occluderDepth)
    // {
    //     shadowFactor = 1.0;
    // }

    // FragColor = ((albedoColor * shadowFactor * 0.3) + (accumFog * 0.7));
}