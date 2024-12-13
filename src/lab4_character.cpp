#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

// GLTF model loader
#define TINYGLTF_IMPLEMENTATION
// #define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <render/shader.h>

#include <vector>
#include <iostream>
#include <string>
#define _USE_MATH_DEFINES
#include <math.h>
#include "camera.h"
#include "skybox.h"
#include "lab1_cube.h"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

static GLFWwindow *window;
static int windowWidth = 1024;
static int windowHeight = 768;

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);
static void mouse_callback(GLFWwindow *window, double xpos, double ypos);

// Camera
static glm::vec3 eye_center(-15.0f, 10.0f, 0.0f);
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
static glm::vec3 lightPosition(-275.0f, 500.0f, 800.0f);

// Animation 
static bool playAnimation = true;
static float playbackSpeed = 2.0f;

struct MyBot {
	// Shader variable IDs
	GLuint mvpMatrixID;
	GLuint jointMatricesID;
	GLuint lightPositionID;
	GLuint lightIntensityID;
	GLuint programID;
	GLuint useSkinningID;

	tinygltf::Model model;

	glm::mat4 transform;
	bool use_skinning;

	// Each VAO corresponds to each mesh primitive in the GLTF model
	struct PrimitiveObject {
		GLuint vao;
		std::map<int, GLuint> vbos;
	};
	std::vector<PrimitiveObject> primitiveObjects;

	// Skinning 
	struct SkinObject {
		// Transforms the geometry into the space of the respective joint
		std::vector<glm::mat4> inverseBindMatrices;  

		// Transforms the geometry following the movement of the joints
		std::vector<glm::mat4> globalJointTransforms;

		// Combined transforms
		std::vector<glm::mat4> jointMatrices;
	};
	std::vector<SkinObject> skinObjects;

	// Animation 
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
	std::vector<AnimationObject> animationObjects;

	glm::mat4 getNodeTransform(const tinygltf::Node& node) {
		glm::mat4 transform(1.0f); 

		if (node.matrix.size() == 16) {
			transform = glm::make_mat4(node.matrix.data());
		} else {
			if (node.translation.size() == 3) {
				transform = glm::translate(transform, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
			}
			if (node.rotation.size() == 4) {
				glm::quat q(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
				transform *= glm::mat4_cast(q);
			}
			if (node.scale.size() == 3) {
				transform = glm::scale(transform, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
			}
		}
		return transform;
	}

	void computeLocalNodeTransform(const tinygltf::Model& model, 
		int nodeIndex, 
		std::vector<glm::mat4> &localTransforms)
	{
		tinygltf::Node root = model.nodes[nodeIndex];
		computeLocalNodeTransform(model, root, localTransforms);
	}

	void computeLocalNodeTransform(const tinygltf::Model& model, const tinygltf::Node& node,
	 std::vector<glm::mat4> &localTransforms) {
		localTransforms.push_back(getNodeTransform(node));
		for (size_t i = 0; i < node.children.size(); i++) {
			computeLocalNodeTransform(model, model.nodes[node.children[i]], localTransforms);
  		}
	 }


	void computeGlobalNodeTransform(const tinygltf::Model& model, 
		const std::vector<glm::mat4> &localTransforms,
		int nodeIndex, const glm::mat4& parentTransform, 
		std::vector<glm::mat4> &globalTransforms)
	{
		globalTransforms[nodeIndex] = parentTransform * localTransforms[nodeIndex];
		const tinygltf::Node& node = model.nodes[nodeIndex];
		for (size_t i = 0; i < node.children.size(); ++i) {
			int childIndex = node.children[i];
			computeGlobalNodeTransform(model, localTransforms, childIndex, globalTransforms[nodeIndex], globalTransforms);
		}
	}

	std::vector<SkinObject> prepareSkinning(const tinygltf::Model &model) {
		std::vector<SkinObject> skinObjects;

		// In our Blender exporter, the default number of joints that may influence a vertex is set to 4, just for convenient implementation in shaders.

		for (size_t i = 0; i < model.skins.size(); i++) {
			SkinObject skinObject;

			const tinygltf::Skin &skin = model.skins[i];

			// Read inverseBindMatrices
			const tinygltf::Accessor &accessor = model.accessors[skin.inverseBindMatrices];
			assert(accessor.type == TINYGLTF_TYPE_MAT4);
			const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
			const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
			const float *ptr = reinterpret_cast<const float *>(
				buffer.data.data() + accessor.byteOffset + bufferView.byteOffset);
			
			skinObject.inverseBindMatrices.resize(accessor.count);
			for (size_t j = 0; j < accessor.count; j++) {
				float m[16];
				memcpy(m, ptr + j * 16, 16 * sizeof(float));
				skinObject.inverseBindMatrices[j] = glm::make_mat4(m);
			}

			assert(skin.joints.size() == accessor.count);

			skinObject.globalJointTransforms.resize(skin.joints.size());
			skinObject.jointMatrices.resize(skin.joints.size());

			// ----------------------------------------------
			// Compute joint matrices
			// ----------------------------------------------
			std::vector<glm::mat4> localNodeTransforms(model.nodes.size());
			for (size_t j = 0; j < model.nodes.size(); ++j) {
				localNodeTransforms[j] = getNodeTransform(model.nodes[j]);
			}

			std::vector<glm::mat4> globalNodeTransforms(model.nodes.size(), glm::mat4(1.0f));
			for (size_t j = 0; j < model.scenes[model.defaultScene].nodes.size(); ++j) {
				int rootNodeIndex = model.scenes[model.defaultScene].nodes[j];
				computeGlobalNodeTransform(model, localNodeTransforms, rootNodeIndex, glm::mat4(1.0f), globalNodeTransforms);
			}

			for (size_t j = 0; j < skin.joints.size(); ++j) {
				int jointIndex = skin.joints[j];
				skinObject.globalJointTransforms[j] = globalNodeTransforms[jointIndex];
				skinObject.jointMatrices[j] = globalNodeTransforms[jointIndex] * skinObject.inverseBindMatrices[j];
			}

			skinObjects.push_back(skinObject);
		}
		return skinObjects;
	}




	int findKeyframeIndex(const std::vector<float>& times, float animationTime) 
	{
		int left = 0;
		int right = times.size() - 1;

		while (left <= right) {
			int mid = (left + right) / 2;

			if (mid + 1 < times.size() && times[mid] <= animationTime && animationTime < times[mid + 1]) {
				return mid;
			}
			else if (times[mid] > animationTime) {
				right = mid - 1;
			}
			else { // animationTime >= times[mid + 1]
				left = mid + 1;
			}
		}

		// Target not found
		return times.size() - 2;
	}

	std::vector<AnimationObject> prepareAnimation(const tinygltf::Model &model) 
	{
		std::vector<AnimationObject> animationObjects;
		for (const auto &anim : model.animations) {
			AnimationObject animationObject;
			
			for (const auto &sampler : anim.samplers) {
				SamplerObject samplerObject;

				const tinygltf::Accessor &inputAccessor = model.accessors[sampler.input];
				const tinygltf::BufferView &inputBufferView = model.bufferViews[inputAccessor.bufferView];
				const tinygltf::Buffer &inputBuffer = model.buffers[inputBufferView.buffer];

				assert(inputAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
				assert(inputAccessor.type == TINYGLTF_TYPE_SCALAR);

				// Input (time) values
				samplerObject.input.resize(inputAccessor.count);

				const unsigned char *inputPtr = &inputBuffer.data[inputBufferView.byteOffset + inputAccessor.byteOffset];
				const float *inputBuf = reinterpret_cast<const float*>(inputPtr);

				// Read input (time) values
				int stride = inputAccessor.ByteStride(inputBufferView);
				for (size_t i = 0; i < inputAccessor.count; ++i) {
					samplerObject.input[i] = *reinterpret_cast<const float*>(inputPtr + i * stride);
				}
				
				const tinygltf::Accessor &outputAccessor = model.accessors[sampler.output];
				const tinygltf::BufferView &outputBufferView = model.bufferViews[outputAccessor.bufferView];
				const tinygltf::Buffer &outputBuffer = model.buffers[outputBufferView.buffer];

				assert(outputAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

				const unsigned char *outputPtr = &outputBuffer.data[outputBufferView.byteOffset + outputAccessor.byteOffset];
				const float *outputBuf = reinterpret_cast<const float*>(outputPtr);

				int outputStride = outputAccessor.ByteStride(outputBufferView);
				
				// Output values
				samplerObject.output.resize(outputAccessor.count);
				
				for (size_t i = 0; i < outputAccessor.count; ++i) {

					if (outputAccessor.type == TINYGLTF_TYPE_VEC3) {
						memcpy(&samplerObject.output[i], outputPtr + i * 3 * sizeof(float), 3 * sizeof(float));
					} else if (outputAccessor.type == TINYGLTF_TYPE_VEC4) {
						memcpy(&samplerObject.output[i], outputPtr + i * 4 * sizeof(float), 4 * sizeof(float));
					} else {
						std::cout << "Unsupport accessor type ..." << std::endl;
					}

				}

				animationObject.samplers.push_back(samplerObject);			
			}

			animationObjects.push_back(animationObject);
		}
		return animationObjects;
	}

	void updateAnimation(
		const tinygltf::Model &model, 
		const tinygltf::Animation &anim, 
		const AnimationObject &animationObject, 
		float time,
		std::vector<glm::mat4> &nodeTransforms) 
	{
		// There are many channels so we have to accumulate the transforms 
		for (const auto &channel : anim.channels) {
			
			int targetNodeIndex = channel.target_node;
			const auto &sampler = anim.samplers[channel.sampler];
			
			// Access output (value) data for the channel
			const tinygltf::Accessor &outputAccessor = model.accessors[sampler.output];
			const tinygltf::BufferView &outputBufferView = model.bufferViews[outputAccessor.bufferView];
			const tinygltf::Buffer &outputBuffer = model.buffers[outputBufferView.buffer];

			// Calculate current animation time (wrap if necessary)
			const std::vector<float> &times = animationObject.samplers[channel.sampler].input;
			float animationTime = fmod(time, times.back());
			
			// Find a keyframe for getting animation data 
			int keyframeIndex = findKeyframeIndex(times, animationTime);

			const unsigned char *outputPtr = &outputBuffer.data[outputBufferView.byteOffset + outputAccessor.byteOffset];
			const float *outputBuf = reinterpret_cast<const float*>(outputPtr);

			// Add interpolation for smooth interpolation
			float previousTime = times[keyframeIndex];
			float nextTime = times[keyframeIndex + 1];
			float interpolationValue = (animationTime - previousTime) / (nextTime - previousTime);

			if (channel.target_path == "translation") {
				glm::vec3 previousTranslation, nextTranslation;
				memcpy(&previousTranslation, outputPtr + keyframeIndex * 3 * sizeof(float), 3 * sizeof(float));
				memcpy(&nextTranslation, outputPtr + (keyframeIndex + 1) * 3 * sizeof(float), 3 * sizeof(float));

				glm::vec3 currentTranslation = previousTranslation + interpolationValue * (nextTranslation - previousTranslation);
				nodeTransforms[targetNodeIndex] = glm::translate(nodeTransforms[targetNodeIndex], currentTranslation);
			} else if (channel.target_path == "rotation") {
				glm::quat previousRotation, nextRotation;
				memcpy(&previousRotation, outputPtr + keyframeIndex * 4 * sizeof(float), 4 * sizeof(float));
				memcpy(&nextRotation, outputPtr + (keyframeIndex + 1) * 4 * sizeof(float), 4 * sizeof(float));

				glm::quat currentRotation = glm::slerp(previousRotation, nextRotation, interpolationValue);
				nodeTransforms[targetNodeIndex] *= glm::mat4_cast(currentRotation);
			} else if (channel.target_path == "scale") {
				glm::vec3 previousScale, nextScale;
				memcpy(&previousScale, outputPtr + keyframeIndex * 3 * sizeof(float), 3 * sizeof(float));
				memcpy(&nextScale, outputPtr + (keyframeIndex + 1) * 3 * sizeof(float), 3 * sizeof(float));
				
				glm::vec3 currentScale = previousScale + interpolationValue * (nextScale - previousScale);
				nodeTransforms[targetNodeIndex] = glm::scale(nodeTransforms[targetNodeIndex], currentScale);
			}
		}
	}

	void updateSkinning(const std::vector<glm::mat4> &nodeTransforms, const tinygltf::Skin &skin, SkinObject &skinObject) {
		// -------------------------------------------------
		// TODO: Recompute joint matrices 
		// -------------------------------------------------
		for (size_t j = 0; j < skin.joints.size(); ++j) {
				int jointIndex = skin.joints[j];
				skinObject.globalJointTransforms[j] = nodeTransforms[jointIndex];
				skinObject.jointMatrices[j] = nodeTransforms[jointIndex] * skinObject.inverseBindMatrices[j];
		}
	}

	void update(float time) {
		if (model.animations.size() > 0) {
			for (size_t i = 0; i < model.skins.size(); i++) {
				const tinygltf::Animation &animation = model.animations[i];
				const AnimationObject &animationObject = animationObjects[i];

				const tinygltf::Skin &skin = model.skins[i];
				std::vector<glm::mat4> nodeTransforms(model.nodes.size(), glm::mat4(1.0f));

				updateAnimation(model, animation, animationObject, time, nodeTransforms);

				std::vector<glm::mat4> globalNodeTransforms(model.nodes.size(), glm::mat4(1.0f));
				for (size_t j = 0; j < model.scenes[model.defaultScene].nodes.size(); ++j) {
					int rootNodeIndex = model.scenes[model.defaultScene].nodes[j];
					computeGlobalNodeTransform(model, nodeTransforms, rootNodeIndex, glm::mat4(1.0f), globalNodeTransforms);
				}

				updateSkinning(globalNodeTransforms, skin, skinObjects[i]);
			}
		}
	}

	bool loadModel(tinygltf::Model &model, const char *filename) {
		tinygltf::TinyGLTF loader;
		std::string err;
		std::string warn;

		bool res = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
		if (!warn.empty()) {
			std::cout << "WARN: " << warn << std::endl;
		}

		if (!err.empty()) {
			std::cout << "ERR: " << err << std::endl;
		}

		if (!res)
			std::cout << "Failed to load glTF: " << filename << std::endl;
		else
			std::cout << "Loaded glTF: " << filename << std::endl;

		return res;
	}

	MyBot(const char * modelPath, bool use_skinning = false, 
		const glm::mat4& transform = glm::mat4(1.0)) : transform(transform), use_skinning(use_skinning) {
		// Modify your path if needed
		if (!loadModel(model, modelPath)) {
			return;
		}

		// Prepare buffers for rendering 
		primitiveObjects = bindModel(model);

		// Prepare joint matrices
		skinObjects = prepareSkinning(model);

		// Prepare animation data 
		animationObjects = prepareAnimation(model);

		// Create and compile our GLSL program from the shaders
		programID = LoadShadersFromFile("../src/shader/bot.vert", "../src/shader/bot.frag");
		if (programID == 0)
		{
			std::cerr << "Failed to load shaders." << std::endl;
		}

		// Get a handle for GLSL variables
		mvpMatrixID = glGetUniformLocation(programID, "MVP");
		lightPositionID = glGetUniformLocation(programID, "lightPosition");
		lightIntensityID = glGetUniformLocation(programID, "lightIntensity");

		// Get a handle for joint matrices
		jointMatricesID = glGetUniformLocation(programID, "jointMat");
		useSkinningID = glGetUniformLocation(programID, "useSkinning");
		
	}

	void bindMesh(std::vector<PrimitiveObject> &primitiveObjects,
				tinygltf::Model &model, tinygltf::Mesh &mesh) {

		std::map<int, GLuint> vbos;
		for (size_t i = 0; i < model.bufferViews.size(); ++i) {
			const tinygltf::BufferView &bufferView = model.bufferViews[i];

			int target = bufferView.target;
			
			if (bufferView.target == 0) { 
				// The bufferView with target == 0 in our model refers to 
				// the skinning weights, for 25 joints, each 4x4 matrix (16 floats), totaling to 400 floats or 1600 bytes. 
				// So it is considered safe to skip the warning.
				//std::cout << "WARN: bufferView.target is zero" << std::endl;
				continue;
			}

			const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
			GLuint vbo;
			glGenBuffers(1, &vbo);
			glBindBuffer(target, vbo);
			glBufferData(target, bufferView.byteLength,
						&buffer.data.at(0) + bufferView.byteOffset, GL_STATIC_DRAW);
			
			vbos[i] = vbo;
		}

		// Each mesh can contain several primitives (or parts), each we need to 
		// bind to an OpenGL vertex array object
		for (size_t i = 0; i < mesh.primitives.size(); ++i) {

			tinygltf::Primitive primitive = mesh.primitives[i];
			tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

			GLuint vao;
			glGenVertexArrays(1, &vao);
			glBindVertexArray(vao);

			for (auto &attrib : primitive.attributes) {
				tinygltf::Accessor accessor = model.accessors[attrib.second];
				int byteStride =
					accessor.ByteStride(model.bufferViews[accessor.bufferView]);
				glBindBuffer(GL_ARRAY_BUFFER, vbos[accessor.bufferView]);
				
				int size = 1;
				if (accessor.type != TINYGLTF_TYPE_SCALAR) {
					size = accessor.type;
				}

				int vaa = -1;
				if (attrib.first.compare("POSITION") == 0) vaa = 0;
				if (attrib.first.compare("NORMAL") == 0) vaa = 1;
				if (attrib.first.compare("TEXCOORD_0") == 0) vaa = 2;
				if (attrib.first.compare("JOINTS_0") == 0) vaa = 3;
				if (attrib.first.compare("WEIGHTS_0") == 0) vaa = 4;
				if (vaa > -1) {
					// std::cout << "id: " << vbos[accessor.bufferView] << std::endl;
					// std::cout << "size: " << size << std::endl;
					// std::cout << "vaa: " << vaa << std::endl;
					// std::cout << "" << std::endl;
					glEnableVertexAttribArray(vaa);
					glVertexAttribPointer(vaa, size, accessor.componentType,
										accessor.normalized ? GL_TRUE : GL_FALSE,
										byteStride, BUFFER_OFFSET(accessor.byteOffset));
				} else {
					std::cout << "vaa missing: " << attrib.first << std::endl;
				}
			}

			// Record VAO for later use
			PrimitiveObject primitiveObject;
			primitiveObject.vao = vao;
			primitiveObject.vbos = vbos;
			primitiveObjects.push_back(primitiveObject);

			glBindVertexArray(0);
		}
		for (const auto& material : model.materials) {
			for (const auto& value : material.values) {
				if (value.first == "baseColorTexture") {
					const tinygltf::Texture& tex = model.textures[value.second.TextureIndex()];
					if (tex.source > -1) {
						GLuint texid;
						glGenTextures(1, &texid);

						const tinygltf::Image& image = model.images[tex.source];

						glBindTexture(GL_TEXTURE_2D, texid);
						glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
						glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
						glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

						GLenum format = GL_RGBA;
						if (image.component == 1) {
							format = GL_RED;
						} else if (image.component == 2) {
							format = GL_RG;
						} else if (image.component == 3) {
							format = GL_RGB;
						}

						GLenum type = GL_UNSIGNED_BYTE;
						if (image.bits == 16) {
							type = GL_UNSIGNED_SHORT;
						}

						glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
									format, type, &image.image.at(0));
					}
				}
			}
		}
	}

	void bindModelNodes(std::vector<PrimitiveObject> &primitiveObjects, 
						tinygltf::Model &model,
						tinygltf::Node &node) {
		// Bind buffers for the current mesh at the node
		if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
			bindMesh(primitiveObjects, model, model.meshes[node.mesh]);
		}

		// Recursive into children nodes
		for (size_t i = 0; i < node.children.size(); i++) {
			assert((node.children[i] >= 0) && (node.children[i] < model.nodes.size()));
			bindModelNodes(primitiveObjects, model, model.nodes[node.children[i]]);
		}
	}

	std::vector<PrimitiveObject> bindModel(tinygltf::Model &model) {
		std::vector<PrimitiveObject> primitiveObjects;

		const tinygltf::Scene &scene = model.scenes[model.defaultScene];
		for (size_t i = 0; i < scene.nodes.size(); ++i) {
			assert((scene.nodes[i] >= 0) && (scene.nodes[i] < model.nodes.size()));
			bindModelNodes(primitiveObjects, model, model.nodes[scene.nodes[i]]);
		}

		return primitiveObjects;
	}

	void drawMesh(const std::vector<PrimitiveObject> &primitiveObjects,
				tinygltf::Model &model, tinygltf::Mesh &mesh) {
		
		for (size_t i = 0; i < mesh.primitives.size(); ++i) 
		{
			GLuint vao = primitiveObjects[i].vao;
			std::map<int, GLuint> vbos = primitiveObjects[i].vbos;

			glBindVertexArray(vao);

			tinygltf::Primitive primitive = mesh.primitives[i];
			tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos.at(indexAccessor.bufferView));

			glDrawElements(primitive.mode, indexAccessor.count,
						indexAccessor.componentType,
						BUFFER_OFFSET(indexAccessor.byteOffset));

			glBindVertexArray(0);
		}
	}

	void drawModelNodes(const std::vector<PrimitiveObject>& primitiveObjects,
						tinygltf::Model &model, tinygltf::Node &node) {
		// Draw the mesh at the node, and recursively do so for children nodes
		if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
			drawMesh(primitiveObjects, model, model.meshes[node.mesh]);
		}
		for (size_t i = 0; i < node.children.size(); i++) {
			drawModelNodes(primitiveObjects, model, model.nodes[node.children[i]]);
		}
	}
	void drawModel(const std::vector<PrimitiveObject>& primitiveObjects,
				tinygltf::Model &model) {
		// Draw all nodes
		const tinygltf::Scene &scene = model.scenes[model.defaultScene];
		for (size_t i = 0; i < scene.nodes.size(); ++i) {
			drawModelNodes(primitiveObjects, model, model.nodes[scene.nodes[i]]);
		}
	}

	void render(glm::mat4 cameraMatrix) {
		glUseProgram(programID);
		
		// Set camera
		glm::mat4 mvp = cameraMatrix * transform;
		glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);

		// -----------------------------------------------------------------
		// Set animation data for linear blend skinning in shader
		// -----------------------------------------------------------------
		
		
		if(use_skinning) {
			glUniformMatrix4fv(jointMatricesID, skinObjects[0].jointMatrices.size(), GL_FALSE, glm::value_ptr(skinObjects[0].jointMatrices[0]));
		}
		glUniform1i(useSkinningID, use_skinning);
		// -----------------------------------------------------------------

		// Set light data 
		glUniform3fv(lightPositionID, 1, &lightPosition[0]);
		glUniform3fv(lightIntensityID, 1, &lightIntensity[0]);
		 

		// Draw the GLTF model
		drawModel(primitiveObjects, model);
	}

	void cleanup() {
		glDeleteProgram(programID);
	}
}; 

struct AxisXYZ {
    // A structure for visualizing the global 3D coordinate system 
	
	GLfloat vertex_buffer_data[18] = {
		// X axis
		0.0, 0.0f, 0.0f, 
		100.0f, 0.0f, 0.0f,
		
		// Y axis
		0.0f, 0.0f, 0.0f, 
		0.0f, 100.0f, 0.0f, 
		
		// Z axis
		0.0f, 0.0f, 0.0f, 
		0.0f, 0.0f, 100.0f,
	};

	GLfloat color_buffer_data[18] = {
		// X, red
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,

		// Y, green
		0.0f, 1.0f, 0.0f, 
		0.0f, 1.0f, 0.0f,

		// Z, blue
		0.0f, 0.0f, 1.0f, 
		0.0f, 0.0f, 1.0f,
	};

	// OpenGL buffers
	GLuint vertexArrayID; 
	GLuint vertexBufferID; 
	GLuint colorBufferID;

	// Shader variable IDs
	GLuint mvpMatrixID;
	GLuint programID;

	void initialize() {
		// Create a vertex array object
		glGenVertexArrays(1, &vertexArrayID);
		glBindVertexArray(vertexArrayID);

		// Create a vertex buffer object to store the vertex data		
		glGenBuffers(1, &vertexBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

		// Create a vertex buffer object to store the color data
		glGenBuffers(1, &colorBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(color_buffer_data), color_buffer_data, GL_STATIC_DRAW);

		// Create and compile our GLSL program from the shaders
		programID = LoadShadersFromString(cubeVertexShader, cubeFragmentShader);
		if (programID == 0)
		{
			std::cerr << "Failed to load shaders." << std::endl;
		}

		// Get a handle for our "MVP" uniform
		mvpMatrixID = glGetUniformLocation(programID, "MVP");
	}

	void render(glm::mat4 cameraMatrix) {
		glUseProgram(programID);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glm::mat4 mvp = cameraMatrix;
		glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);

        // Draw the lines
        glDrawArrays(GL_LINES, 0, 6);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
	}

	void cleanup() {
		glDeleteBuffers(1, &vertexBufferID);
		glDeleteBuffers(1, &colorBufferID);
		glDeleteVertexArrays(1, &vertexArrayID);
		glDeleteProgram(programID);
	}
}; 

struct Box {
	glm::vec3 position;			// Position of the box 
	glm::vec3 scale;			// Size of the box in each axis
	glm::float32 rotationRadian;
	glm::vec3 rotationAxis;

	GLfloat vertex_buffer_data[72] = {	// Vertex definition for a canonical box
		// Front face
		-1.0f, -1.0f, 1.0f, 
		1.0f, -1.0f, 1.0f, 
		1.0f, 1.0f, 1.0f, 
		-1.0f, 1.0f, 1.0f, 
		
		// Back face 
		1.0f, -1.0f, -1.0f, 
		-1.0f, -1.0f, -1.0f, 
		-1.0f, 1.0f, -1.0f, 
		1.0f, 1.0f, -1.0f,
		
		// Left face
		-1.0f, -1.0f, -1.0f, 
		-1.0f, -1.0f, 1.0f, 
		-1.0f, 1.0f, 1.0f, 
		-1.0f, 1.0f, -1.0f, 

		// Right face 
		1.0f, -1.0f, 1.0f, 
		1.0f, -1.0f, -1.0f, 
		1.0f, 1.0f, -1.0f, 
		1.0f, 1.0f, 1.0f,

		// Top face
		-1.0f, 1.0f, 1.0f, 
		1.0f, 1.0f, 1.0f, 
		1.0f, 1.0f, -1.0f, 
		-1.0f, 1.0f, -1.0f, 

		// Bottom face
		-1.0f, -1.0f, -1.0f, 
		1.0f, -1.0f, -1.0f, 
		1.0f, -1.0f, 1.0f, 
		-1.0f, -1.0f, 1.0f, 
	};

	GLfloat color_buffer_data[72] = {
		// Front, red
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,

		// Back, yellow
		1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,

		// Left, green
		0.0f, 1.0f, 0.0f, 
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,

		// Right, cyan
		0.0f, 1.0f, 1.0f, 
		0.0f, 1.0f, 1.0f, 
		0.0f, 1.0f, 1.0f, 
		0.0f, 1.0f, 1.0f, 

		// Top, blue
		0.0f, 0.0f, 1.0f, 
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,

		// Bottom, magenta
		1.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 1.0f, 
		1.0f, 0.0f, 1.0f, 
		1.0f, 0.0f, 1.0f,  
	};

	GLuint index_buffer_data[36] = {		// 12 triangle faces of a box
		0, 1, 2, 	
		0, 2, 3, 
		
		4, 5, 6, 
		4, 6, 7, 

		8, 9, 10, 
		8, 10, 11, 

		12, 13, 14, 
		12, 14, 15, 

		16, 17, 18, 
		16, 18, 19, 

		20, 21, 22, 
		20, 22, 23, 
	};
    
	// OpenGL buffers
	GLuint vertexArrayID; 
	GLuint vertexBufferID; 
	GLuint indexBufferID; 
	GLuint colorBufferID;

	// Shader variable IDs
	GLuint mvpMatrixID;
	GLuint programID;

	void setRotationRadian(glm::float32 radian) {
		this->rotationRadian = radian;
	}
	glm::float32 getRotationRadian() {
		return this->rotationRadian;
	}

	void initialize(glm::vec3 position, glm::vec3 scale, glm::float32 rotationRadian, glm::vec3 rotationAxis) {
		// Define scale of the box geometry
		this->position = position;
		this->scale = scale;
		this->rotationRadian = rotationRadian;
		this->rotationAxis = rotationAxis;

		// Create a vertex array object
		glGenVertexArrays(1, &vertexArrayID);
		glBindVertexArray(vertexArrayID);

		// Create a vertex buffer object to store the vertex data		
		glGenBuffers(1, &vertexBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

		// Create a vertex buffer object to store the color data
		glGenBuffers(1, &colorBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(color_buffer_data), color_buffer_data, GL_STATIC_DRAW);

		// Create an index buffer object to store the index data that defines triangle faces
		glGenBuffers(1, &indexBufferID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_buffer_data), index_buffer_data, GL_STATIC_DRAW);
		
		// Create and compile our GLSL program from the shaders
		programID = LoadShadersFromString(cubeVertexShader, cubeFragmentShader);
		if (programID == 0)
		{
			std::cerr << "Failed to load shaders." << std::endl;
		}

		// Get a handle for our "MVP" uniform
		mvpMatrixID = glGetUniformLocation(programID, "MVP");
	}

	void render(glm::mat4 cameraMatrix) {
		glUseProgram(programID);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);

		// TODO: Model transform 
		// ------------------------------------
        glm::mat4 modelMatrix = glm::mat4();
		modelMatrix = glm::translate(modelMatrix, position);
		modelMatrix = glm::scale(modelMatrix, scale);
		modelMatrix = glm::rotate(modelMatrix, rotationRadian, rotationAxis);

		// TODO: Set model-view-projection matrix
		glm::mat4 mvp = cameraMatrix * modelMatrix;
		// ------------------------------------
		glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);

		// Draw the box
		glDrawElements(
			GL_TRIANGLES,      // mode
			36,    			   // number of indices
			GL_UNSIGNED_INT,   // type
			(void*)0           // element array buffer offset
		);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
	}

	void cleanup() {
		glDeleteBuffers(1, &vertexBufferID);
		glDeleteBuffers(1, &colorBufferID);
		glDeleteBuffers(1, &indexBufferID);
		glDeleteVertexArrays(1, &vertexArrayID);
		glDeleteProgram(programID);
	}
}; 


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
	//glEnable(GL_CULL_FACE);

	// Our 3D character
	glm::mat4 modelMatrix = glm::mat4();
	modelMatrix = glm::translate(modelMatrix, glm::vec3(20.0f,0.0f,10.0f));
	modelMatrix = glm::scale(modelMatrix, glm::vec3(0.1f,0.1f,0.1f));
	modelMatrix = glm::rotate(modelMatrix, glm::radians(90.0f), glm::vec3(1.0f,0.0f,0.0f));
	MyBot bot("../src/model/flop/gas.gltf", true, modelMatrix);

	AxisXYZ axis;
	axis.initialize();

	Box box;
	box.initialize(glm::vec3(0, 10, 10),        // translation
						glm::vec3(5, 5, 5),
						glm::radians(20.0f),
						glm::vec3(0.0,0.0,1.0));

	Skybox skybox;
	std::vector<std::string> faces = {
		"../src/texture/skybox/right.jpg",
		"../src/texture/skybox/left.jpg",
		"../src/texture/skybox/top.jpg",
		"../src/texture/skybox/bottom.jpg",
		"../src/texture/skybox/front.jpg",
		"../src/texture/skybox/back.jpg"
	};
	skybox.initialize(faces, "../src/shader/skybox.vert", "../src/shader/skybox.frag");
   

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
			bot.update(time);
		}

		// Rendering
		
		projectionMatrix = glm::perspective(glm::radians(camera.Zoom), (float)windowWidth / windowHeight, zNear, zFar);
		
		
		viewMatrix = camera.GetViewMatrix();
		glm::mat4 vp = projectionMatrix * viewMatrix;
		axis.render(vp);
		box.render(vp);
		
		
		viewMatrix = glm::mat4(glm::mat3(viewMatrix));
		glm::mat4 vpSkybox = projectionMatrix * viewMatrix;
		skybox.render(vpSkybox);
		
		bot.render(vp);

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
	bot.cleanup();
	box.cleanup();
	axis.cleanup();

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
