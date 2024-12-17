

#ifndef ENTITY_H
#define ENTITY_H

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <iostream>
#include <vector>
#include <tiny_gltf.h>
#include <glm/gtc/type_ptr.hpp>
#include <render/shader.h>
#include "constants.h"


#define BUFFER_OFFSET(i) ((char *)NULL + (i))

struct SkinObject {
		// Transforms the geometry into the space of the respective joint
		std::vector<glm::mat4> inverseBindMatrices;  

		// Transforms the geometry following the movement of the joints
		std::vector<glm::mat4> globalJointTransforms;

		// Combined transforms
		std::vector<glm::mat4> jointMatrices;
	};

struct PrimitiveObject {
    GLuint vao;
    std::map<int, GLuint> vbos;
};


struct SamplerObject {
    std::vector<float> input;
    std::vector<glm::vec4> output;
    int interpolation;
};

struct ChannelObject {
    int sampler;
    std::string targetPath;
    int targetNode;
}; 
struct AnimationObject {
    std::vector<SamplerObject> samplers;	// Animation data
};




class Entity
{
public:
    //Shader variable IDs
	GLuint mvpMatrixID;
	GLuint jointMatricesID;
	GLuint ambientID;
    GLuint diffuseID;
    GLuint specularID;
    GLuint directionID;
    GLuint viewPosID;
	GLuint programID;
	GLuint isSkinningID;
    GLuint instanceMatricesID;
    GLuint modelID;
    GLuint isTextureID;
    GLuint baseColorFactorID;
    GLuint textureID;
    GLuint textureSamplerID;
	GLuint depthSamplerID;
    GLuint lightSpaceMatrixID;

	tinygltf::Model model;

	glm::mat4 transform;
	bool isSkinning;
    bool isTexture = false;
    bool isInstancing;
    int instances;
    std::vector<glm::mat4> instanceMatrices;

	// Each VAO corresponds to each mesh primitive in the GLTF model
	
	std::vector<PrimitiveObject> primitiveObjects;

	// Skinning 
	
	std::vector<SkinObject> skinObjects;

	// Animation 

	std::vector<AnimationObject> animationObjects;

    Entity(){}

    Entity(const char * modelPath, const char * vertPath, const char * fragPath, const glm::mat4& transform = glm::mat4(1.0),
        bool isSkinning = false, int instances = 1, std::vector<glm::mat4> instanceMatrices = {}) 
        : transform(transform), isSkinning(isSkinning), instances(instances), instanceMatrices(instanceMatrices) {
		// Modify your path if needed
		if (!loadModel(model, modelPath)) {
			return;
		}

		primitiveObjects = bindModel(model);

		// Prepare joint matrices
		skinObjects = prepareSkinning(model);

		// Prepare animation data 
		animationObjects = prepareAnimation(model);

		// Create and compile our GLSL program from the shaders
		programID = LoadShadersFromFile(vertPath, fragPath);
		if (programID == 0)
		{
			std::cerr << "Failed to load shaders." << std::endl;
		}

		// Get a handle for GLSL variables
		mvpMatrixID = glGetUniformLocation(programID, "MVP");
		ambientID = glGetUniformLocation(programID, "ambient");
		diffuseID = glGetUniformLocation(programID, "diffuse");
        specularID = glGetUniformLocation(programID, "specular");
		directionID = glGetUniformLocation(programID, "direction");
		viewPosID = glGetUniformLocation(programID, "u_viewPos");
        modelID = glGetUniformLocation(programID, "u_model");
        isTextureID = glGetUniformLocation(programID, "isTexture");
        baseColorFactorID = glGetUniformLocation(programID, "baseColorFactor");

		// Get a handle for joint matrices
		jointMatricesID = glGetUniformLocation(programID, "jointMat");
		isSkinningID = glGetUniformLocation(programID, "isSkinning");
        depthSamplerID = glGetUniformLocation(programID, "shadowMap");
		lightSpaceMatrixID = glGetUniformLocation(programID, "lightSpaceMatrix");
	}
    void update(float time);
    void render(glm::mat4 cameraMatrix, glm::vec3 cameraPosition, Shadow shadow, Light light);
    void render(GLuint depthID, glm::mat4 vp); 
    void cleanup();
	std::vector<SkinObject> prepareSkinning(const tinygltf::Model &model);
	std::vector<AnimationObject> prepareAnimation(const tinygltf::Model &model);
	bool loadModel(tinygltf::Model &model, const char *filename);
	std::vector<PrimitiveObject> bindModel(tinygltf::Model &model);
    void bindMesh(std::vector<PrimitiveObject> &primitiveObjects,
				tinygltf::Model &model, tinygltf::Mesh &mesh);
    void drawMesh(const std::vector<PrimitiveObject> &primitiveObjects,
				tinygltf::Model &model, tinygltf::Mesh &mesh);
    void drawModelNodes(const std::vector<PrimitiveObject>& primitiveObjects,
						tinygltf::Model &model, tinygltf::Node &node);
    void drawModel(const std::vector<PrimitiveObject>& primitiveObjects,
				tinygltf::Model &model);      
    void bindModelNodes(std::vector<PrimitiveObject> &primitiveObjects, 
						tinygltf::Model &model,
						tinygltf::Node &node);            
};

#endif;