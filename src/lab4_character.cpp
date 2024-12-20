#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>


#include <tiny_gltf.h>
#include <stb/stb_image_write.h>

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
#include "constants.h"





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
int chunk_render_distance = 3;
// Camera
static glm::vec3 eye_center(originX-30.0f, 20.0f, originY-30.0f);
static glm::vec3 lookat(0.0f, 0.0f, 0.0f);
static glm::vec3 up(0.0f, 1.0f, 0.0f);
static float FoV = 45.0f;
static float zNear = 0.1f;
static float zFar = (float)chunkWidth * (chunk_render_distance); 

// Shadow mapping
static glm::vec3 lightUp(0.0, 1.0, 0.0);
static int shadowMapWidth;
static int shadowMapHeight;

// TODO: set these parameters 
static float depthNear = 0.1f;
static float depthFar = 2000.0f; 

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

static bool saveDepth = false;

static void saveDepthTexture(GLuint fbo, std::string filename) {
    int width = shadowMapWidth;
    int height = shadowMapHeight;
	if (shadowMapWidth == 0 || shadowMapHeight == 0) {
		width = windowWidth;
		height = windowHeight;
	}
    int channels = 3; 
    
    std::vector<float> depth(width * height);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glReadBuffer(GL_DEPTH_COMPONENT);
    glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::vector<unsigned char> img(width * height * 3);
    for (int i = 0; i < width * height; ++i) img[3*i] = img[3*i+1] = img[3*i+2] = depth[i] * 255;

    stbi_write_png(filename.c_str(), width, height, channels, img.data(), width * channels);
}

 
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
	// Prepare shadow map size for shadow mapping. Usually this is the size of the window itself, but on some platforms like Mac this can be 2x the size of the window. Use glfwGetFramebufferSize to get the shadow map size properly. 
    glfwGetFramebufferSize(window, &shadowMapWidth, &shadowMapHeight);
	std::cout << "shadow width " << shadowMapWidth << std::endl;
	std::cout << "shadow height " << shadowMapHeight << std::endl;
	
	GLuint depthMap;
	GLuint depthMapFBO;
	
	glGenFramebuffers(1, &depthMapFBO);
	// create depth texture
	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, shadowMapWidth, shadowMapHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	// attach depth texture as FBO's depth buffer
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glm::vec3 sunPos = glm::vec3(originX+chunkWidth * xMapChunks, 0, originY) - glm::vec3(-0.2f,-1.0f,-0.3f) * 200.0f;
	std::cout << "sun pos " << glm::to_string(sunPos) << std::endl;
	// Background
	glClearColor(0.2f, 0.2f, 0.25f, 0.0f);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	// Our 3D character
	glm::mat4 modelMatrix = glm::mat4();
	modelMatrix = glm::translate(modelMatrix, glm::vec3(originX-40.0f,5.0f,originY-30.0f));
	modelMatrix = glm::scale(modelMatrix, glm::vec3(0.05f,0.05f,0.05f));
	
	//Entity bot2("../src/model/bot/bot.gltf", "../src/shader/bot.vert", "../src/shader/bot.frag", modelMatrix, true);
	//Entity tree("../src/model/oak/oak.gltf", "../src/shader/bot.vert", "../src/shader/bot.frag", modelMatrix, false);
	modelMatrix = glm::rotate(modelMatrix, glm::radians(90.0f), glm::vec3(1.0f,0.0f,0.0f));
	Entity bot1("../src/model/flop/gas.gltf", "../src/shader/bot.vert", "../src/shader/bot.frag", modelMatrix, true);
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
	glm::mat4 viewMatrix, projectionMatrix, lightViewMatrix, vp;
	projectionMatrix = glm::ortho(-1000.0f, 1000.0f, -1000.0f, 1000.0f, depthNear, depthFar);
	std::cout << "ortho " << glm::to_string(projectionMatrix) << std::endl;
	lightViewMatrix = glm::lookAt(sunPos, glm::vec3(0.0f,0.0f,0.0f), lightUp);
	std::cout << "lookat " << glm::to_string(lightViewMatrix) << std::endl;
	lightViewMatrix = projectionMatrix * lightViewMatrix;
	std::cout << "light mat " << glm::to_string(lightViewMatrix) << std::endl;
	Light light = {
		glm::vec3(0.2,0.2,0.2),
		glm::vec3(0.3,0.3,0.3),
		glm::vec3(1.0,1.0,1.0),
		glm::vec3(-0.2f,-1.0f,-0.3f),
	};
	
	Shadow shadow = {
		lightViewMatrix,
		depthMap,
	};
	GLuint botDepthID = LoadShadersFromFile("../src/shader/depthBot.vert", "../src/shader/depth.frag");
	GLuint terrainDepthID = LoadShadersFromFile("../src/shader/depth.vert", "../src/shader/depth.frag");
	GLuint treeDepthID = LoadShadersFromFile("../src/shader/depthTree.vert", "../src/shader/depth.frag");
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
			bot1.update(time);
			//bot2.update(time);
		}

		// Rendering
		
		
		glViewport(0,0,shadowMapWidth,shadowMapHeight);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glGetError();
        glClear(GL_DEPTH_BUFFER_BIT);
		glDisable(GL_CULL_FACE);
		mountains.render(camera.Position, terrainDepthID, treeDepthID, lightViewMatrix);
		glEnable(GL_CULL_FACE);
		bot1.render(botDepthID, lightViewMatrix);
		//bot2.render(botDepthID, lightViewMatrix);
		//tree.render(vp, camera.Position, glm::vec3(0.2,0.2,0.2),glm::vec3(0.3,0.3,0.3),glm::vec3(1.0,1.0,1.0),glm::vec3(-0.2f,-1.0f,-0.3f));
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0,0,windowWidth*4,windowHeight*4);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


		
		projectionMatrix = glm::perspective(glm::radians(camera.Zoom), (float)windowWidth / windowHeight, zNear, zFar);
		
		
		viewMatrix = camera.GetViewMatrix();
		vp = projectionMatrix * viewMatrix;
		
		viewMatrix = glm::mat4(glm::mat3(camera.GetViewMatrix()));
		glm::mat4 vpSkybox = projectionMatrix * viewMatrix;
		
		//bot2.render(vp, camera.Position, shadow, light);
		bot1.render(vp, camera.Position, shadow, light);
		//tree.render(vp, camera.Position, glm::vec3(0.2,0.2,0.2),glm::vec3(0.3,0.3,0.3),glm::vec3(1.0,1.0,1.0),glm::vec3(-0.2f,-1.0f,-0.3f));
		glDisable(GL_CULL_FACE);
		mountains.render(vp, camera.Position, shadow, light);
		glEnable(GL_CULL_FACE);
	
		
		
		skybox.render(vpSkybox);
		
		
		if (saveDepth) {
            std::string filename = "depth_camera.png";
            saveDepthTexture(0, filename);
            std::cout << "Depth texture saved to " << filename << std::endl;
            saveDepth = false;
        }
		
		

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
	if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
        saveDepth = true;
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
