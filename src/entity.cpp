#include "entity.h"


#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>





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

    void computeLocalNodeTransform(const tinygltf::Model& model, const tinygltf::Node& node,
	 std::vector<glm::mat4> &localTransforms) {
		localTransforms.push_back(getNodeTransform(node));
		for (size_t i = 0; i < node.children.size(); i++) {
			computeLocalNodeTransform(model, model.nodes[node.children[i]], localTransforms);
  		}
	 }

	void computeLocalNodeTransform(const tinygltf::Model& model, 
		int nodeIndex, 
		std::vector<glm::mat4> &localTransforms)
	{
		tinygltf::Node root = model.nodes[nodeIndex];
		computeLocalNodeTransform(model, root, localTransforms);
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

	std::vector<SkinObject> Entity::prepareSkinning(const tinygltf::Model &model) {
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

	std::vector<AnimationObject> Entity::prepareAnimation(const tinygltf::Model &model) 
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

	void Entity::update(float time) {
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

	bool Entity::loadModel(tinygltf::Model &model, const char *filename) {
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

	

	void Entity::bindMesh(std::vector<PrimitiveObject> &primitiveObjects,
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
        if(instances != 1) {
                glEnableVertexAttribArray(3);
                glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)0);
                glEnableVertexAttribArray(4);
                glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4)));
                glEnableVertexAttribArray(5);
                glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(2 * sizeof(glm::vec4)));
                glEnableVertexAttribArray(6);
                glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(3 * sizeof(glm::vec4)));

                glVertexAttribDivisor(3, 1);
                glVertexAttribDivisor(4, 1);
                glVertexAttribDivisor(5, 1);
                glVertexAttribDivisor(6, 1);
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

	void Entity::bindModelNodes(std::vector<PrimitiveObject> &primitiveObjects, 
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

	std::vector<PrimitiveObject> Entity::bindModel(tinygltf::Model &model) {
		std::vector<PrimitiveObject> primitiveObjects;

		const tinygltf::Scene &scene = model.scenes[model.defaultScene];
		for (size_t i = 0; i < scene.nodes.size(); ++i) {
			assert((scene.nodes[i] >= 0) && (scene.nodes[i] < model.nodes.size()));
			bindModelNodes(primitiveObjects, model, model.nodes[scene.nodes[i]]);
		}

		return primitiveObjects;
	}

	void Entity::drawMesh(const std::vector<PrimitiveObject> &primitiveObjects,
				tinygltf::Model &model, tinygltf::Mesh &mesh) {
		
		for (size_t i = 0; i < mesh.primitives.size(); ++i) 
		{
			GLuint vao = primitiveObjects[i].vao;
			std::map<int, GLuint> vbos = primitiveObjects[i].vbos;

			glBindVertexArray(vao);

			tinygltf::Primitive primitive = mesh.primitives[i];
			tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos.at(indexAccessor.bufferView));
            if(instances != 1) {
                glDrawElementsInstanced(primitive.mode, indexAccessor.count,
                            indexAccessor.componentType,
                            BUFFER_OFFSET(indexAccessor.byteOffset), instances);
            
            } else{
                glDrawElements(primitive.mode, indexAccessor.count,
                            indexAccessor.componentType,
                            BUFFER_OFFSET(indexAccessor.byteOffset));
            }    
            
			glBindVertexArray(0);
		}
	}

	void Entity::drawModelNodes(const std::vector<PrimitiveObject>& primitiveObjects,
						tinygltf::Model &model, tinygltf::Node &node) {
		// Draw the mesh at the node, and recursively do so for children nodes
		if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
			drawMesh(primitiveObjects, model, model.meshes[node.mesh]);
		}
		for (size_t i = 0; i < node.children.size(); i++) {
			drawModelNodes(primitiveObjects, model, model.nodes[node.children[i]]);
		}
	}
	void Entity::drawModel(const std::vector<PrimitiveObject>& primitiveObjects,
				tinygltf::Model &model) {
		// Draw all nodes
		const tinygltf::Scene &scene = model.scenes[model.defaultScene];
		for (size_t i = 0; i < scene.nodes.size(); ++i) {
			drawModelNodes(primitiveObjects, model, model.nodes[scene.nodes[i]]);
		}
	}

	void Entity::render(glm::mat4 cameraMatrix, glm::vec3 cameraPosition, glm::vec3 ambient, glm::vec3 diffuse,  glm::vec3 specular, glm::vec3 direction) {
		glUseProgram(programID);
		
		// Set camera
        
		glm::mat4 mvp = cameraMatrix;
		glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);
        glUniformMatrix4fv(modelID, 1, GL_FALSE, &transform[0][0]);

		// -----------------------------------------------------------------
		// Set animation data for linear blend skinning in shader
		// -----------------------------------------------------------------
		
		
		if(isSkinning) {
			glUniformMatrix4fv(jointMatricesID, skinObjects[0].jointMatrices.size(), GL_FALSE, glm::value_ptr(skinObjects[0].jointMatrices[0]));
		}
		glUniform1i(isSkinningID, isSkinning);
		// -----------------------------------------------------------------

		// Set light data 
		glUniform3fv(ambientID, 1, &ambient[0]);
        glUniform3fv(diffuseID, 1, &diffuse[0]);
        glUniform3fv(specularID, 1, &specular[0]);
        glUniform3fv(directionID, 1, &direction[0]);
        glUniform3fv(viewPosID, 1, &cameraPosition[0]);
		 

		// Draw the GLTF model
		drawModel(primitiveObjects, model);
	}

	void Entity::cleanup() {
		glDeleteProgram(programID);
	}