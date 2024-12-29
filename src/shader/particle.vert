#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 vertexUV;
layout (location = 3) in mat4 aInstanceMatrix;


out vec2 uv;
out vec3 fragPos;
out vec3 cameraPosition;


uniform vec3 u_viewPos;
uniform mat4 MVP;




void main() {
    vec3 position = vec3(aInstanceMatrix[3]);
	vec3 toCamera = normalize(u_viewPos - aPos);
	vec3 up = vec3(0.0, 1.0, 0.0);
	vec3 right = normalize(cross(up, toCamera));
	up = cross(toCamera, right);
	mat4 rotationMatrix = mat4(1.0f);
	rotationMatrix[0] = vec4(right, 0.0f);
	rotationMatrix[1] = vec4(up, 0.0f);
	rotationMatrix[2] = vec4(toCamera, 0.0f);
    fragPos = vec3(aInstanceMatrix * rotationMatrix * vec4(aPos, 1.0));
    uv = vertexUV;
    cameraPosition = u_viewPos;
    gl_Position = MVP * vec4(fragPos,1.0);
}