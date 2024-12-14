#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>


#include <tiny_gltf.h>

#include <render/shader.h>

#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#define _USE_MATH_DEFINES
#include <math.h>
#include "camera.h"
#include "skybox.h"
#include "entity.h"
#include "terrain.h"





static GLFWwindow *window;
static int windowWidth = 1024;
static int windowHeight = 768;

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);
static void mouse_callback(GLFWwindow *window, double xpos, double ypos);

int xMapChunks = 10;
int yMapChunks = 10;
int chunkWidth = 127;
int chunkHeight = 127;
float originX = (chunkWidth  * xMapChunks) / 2 - chunkWidth / 2;
float originY = (chunkHeight * yMapChunks) / 2 - chunkHeight / 2;

// Camera
static glm::vec3 eye_center(originX, 20.0f, originY);
static glm::vec3 lookat(0.0f, 0.0f, 0.0f);
static glm::vec3 up(0.0f, 1.0f, 0.0f);
static float FoV = 45.0f;
static float zNear = 0.1f;
static float zFar = 250.0f; 

Camera camera(eye_center);
float deltaTime = 0.0f;
float lastX = (float)windowWidth / 2.0;
float lastY = (float)windowHeight / 2.0;
bool firstMouse = true;

// Lighting  
static glm::vec3 lightIntensity(5e6f, 5e6f, 5e6f);
static glm::vec3 lightPosition(originX+50.0f, 500.0f, originY+100.0f);

// Animation 
static bool playAnimation = true;
static float playbackSpeed = 2.0f;

 
int main(void)
{
	// Initialise GLFW
	if (!glfwInit())
	{
		std::cerr << "Failed to initialize GLFW." << std::endl;
		return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // For MacOS
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow(windowWidth, windowHeight, "Lab 4", NULL, NULL);
	if (window == NULL)
	{
		std::cerr << "Failed to open a GLFW window." << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, mouse_callback);

	// Load OpenGL functions, gladLoadGL returns the loaded version, 0 on error.
	int version = gladLoadGL(glfwGetProcAddress);
	if (version == 0)
	{
		std::cerr << "Failed to initialize OpenGL context." << std::endl;
		return -1;
	}

	// Background
	glClearColor(0.2f, 0.2f, 0.25f, 0.0f);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	// Our 3D character
	glm::mat4 modelMatrix = glm::mat4();
	modelMatrix = glm::translate(modelMatrix, glm::vec3(originX,10.0f,originY));
	modelMatrix = glm::scale(modelMatrix, glm::vec3(0.1f,0.1f,0.1f));
	
	// Entity tree("../src/model/fir.gltf", "../src/shader/bot.vert", "../src/shader/bot.frag", modelMatrix, false);
	// modelMatrix = glm::rotate(modelMatrix, glm::radians(90.0f), glm::vec3(1.0f,0.0f,0.0f));
	// Entity bot2("../src/model/flop/gas.gltf", "../src/shader/bot.vert", "../src/shader/bot.frag", modelMatrix, true);
	Skybox skybox;
	std::vector<std::string> faces = {
		"../src/texture/day/right.bmp",
		"../src/texture/day/left.bmp",
		"../src/texture/day/top.bmp",
		"../src/texture/day/bottom.bmp",
		"../src/texture/day/front.bmp",
		"../src/texture/day/back.bmp"
	};
	skybox.initialize(faces, "../src/shader/skybox.vert", "../src/shader/skybox.frag");
   
	Terrain mountains(xMapChunks, yMapChunks, chunkWidth, chunkHeight, originX, originY);
	// Time and frame rate tracking
	static double lastTime = glfwGetTime();
	float time = 0.0f;			// Animation time 
	float fTime = 0.0f;			// Time for measuring fps
	unsigned long frames = 0;
	glm::mat4 viewMatrix, projectionMatrix;
	// Main loop
	do
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Update states for animation
        double currentTime = glfwGetTime();
        deltaTime = float(currentTime - lastTime);
		lastTime = currentTime;

		if (playAnimation) {
			time += deltaTime * playbackSpeed;
			//bot.update(time);
			//bot2.update(time);
		}

		// Rendering
		
		projectionMatrix = glm::perspective(glm::radians(camera.Zoom), (float)windowWidth / windowHeight, zNear, zFar);
		
		
		viewMatrix = camera.GetViewMatrix();
		glm::mat4 vp = projectionMatrix * viewMatrix;
		
		viewMatrix = glm::mat4(glm::mat3(camera.GetViewMatrix()));
		glm::mat4 vpSkybox = projectionMatrix * viewMatrix;
		
		// bot2.render(vp, camera.Position, glm::vec3(0.2,0.2,0.2),glm::vec3(0.3,0.3,0.3),glm::vec3(1.0,1.0,1.0),glm::vec3(-0.2f,-1.0f,-0.3f));
		// tree.render(vp, camera.Position, glm::vec3(0.2,0.2,0.2),glm::vec3(0.3,0.3,0.3),glm::vec3(1.0,1.0,1.0),glm::vec3(-0.2f,-1.0f,-0.3f));
		glDisable(GL_CULL_FACE);
		mountains.render(vp, camera.Position, glm::vec3(0.2,0.2,0.2),glm::vec3(0.3,0.3,0.3),glm::vec3(1.0,1.0,1.0),glm::vec3(-0.2f,-1.0f,-0.3f));
		glEnable(GL_CULL_FACE);
	
		
		
		skybox.render(vpSkybox);
		
		
		
		
		

		// FPS tracking 
		// Count number of frames over a few seconds and take average
		frames++;
		fTime += deltaTime;
		if (fTime > 2.0f) {		
			float fps = frames / fTime;
			frames = 0;
			fTime = 0;
			
			std::stringstream stream;
			stream << std::fixed << std::setprecision(2) << "Lab 4 | Frames per second (FPS): " << fps;
			glfwSetWindowTitle(window, stream.str().c_str());
		}

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the window was closed
	while (!glfwWindowShouldClose(window));

	// Clean up
	//bot2.cleanup();

	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
	// if (key == GLFW_KEY_UP && action == GLFW_PRESS)
	// {
	// 	playbackSpeed += 1.0f;
	// 	if (playbackSpeed > 10.0f) 
	// 		playbackSpeed = 10.0f;
	// }

	// if (key == GLFW_KEY_DOWN && action == GLFW_PRESS)
	// {
	// 	playbackSpeed -= 1.0f;
	// 	if (playbackSpeed < 1.0f) {
	// 		playbackSpeed = 1.0f;
	// 	}
	// }

	// if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
	// 	playAnimation = !playAnimation;
	// }

	std::cout << "pos: " << glm::to_string(camera.Position) << std::endl;
	if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
        camera.resetCamera(eye_center);
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.ProcessKeyboard(UP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        camera.ProcessKeyboard(DOWN, deltaTime);

	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
	std::cout << "pos: " << glm::to_string(camera.Position) << std::endl;
}


static void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}
