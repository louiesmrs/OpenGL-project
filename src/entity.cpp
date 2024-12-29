#include "entity.h"



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

    void computeLocalNodeTransform(const tinygltf::Model& model, const int nodeIndex,
	 std::vector<glm::mat4> &localTransforms) {
		const tinygltf::Node& node = model.nodes[nodeIndex];
		glm::mat4 localTransform = getNodeTransform(node);
		localTransforms[nodeIndex] = localTransform;
		for (int childIndex : node.children) {
			computeLocalNodeTransform(model, childIndex, localTransforms);
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
			int rootNodeIndex = skin.joints[0];
			std::vector<glm::mat4> localNodeTransforms(model.nodes.size(), glm::mat4(1.0f));
			computeLocalNodeTransform(model,rootNodeIndex,localNodeTransforms);
			glm::mat4 parentTransform(1.0f);
			std::vector<glm::mat4> globalNodeTransforms(model.nodes.size(), glm::mat4(1.0f));
			for (size_t j = 0; j < model.scenes[model.defaultScene].nodes.size(); ++j) {
				int rootNodeIndex = model.scenes[model.defaultScene].nodes[j];
				computeGlobalNodeTransform(model, localNodeTransforms, rootNodeIndex, parentTransform, globalNodeTransforms);
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

				glm::vec3 currentTranslation = glm::mix(previousTranslation, nextTranslation, interpolationValue);
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
				
				glm::vec3 currentScale =  glm::mix(previousScale, nextScale, interpolationValue);
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
				// glm::mat4 parentTransform(1.0f);
				// int rootNodeIndex = model.scenes[model.defaultScene].nodes[0];
				// std::vector<glm::mat4> globalNodeTransforms(skin.joints.size());
				// computeGlobalNodeTransform(model, nodeTransforms, rootNodeIndex, parentTransform, globalNodeTransforms);



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

			
			int mat = mesh.primitives[i].material;
			if(mat >=0) {
				auto& material = model.materials[mat];
				if(material.pbrMetallicRoughness.baseColorTexture.index > -1) {
					int texidx = material.pbrMetallicRoughness.baseColorTexture.index;
					const tinygltf::Texture& tex = model.textures[texidx];
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
							textures.push_back(texid);
					}
				}
					if(material.normalTexture.index > -1) {
					int texidx = material.normalTexture.index;
					const tinygltf::Texture& tex = model.textures[texidx];
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
							textures.push_back(texid);
							std::cout << "normal texure" << std::endl;
					}
				}

				glm::vec4 baseColorFactor = glm::vec4(1.0f);
				if(material.pbrMetallicRoughness.baseColorFactor.size() == 4) {
					auto&& colorFactor = material.pbrMetallicRoughness.baseColorFactor;
					baseColorFactor = glm::vec4(colorFactor[0], colorFactor[1], colorFactor[2], colorFactor[3]);
					
					// Use baseColorFactor as needed, for example, pass it to the shader
					std::cout << "mesh: " << mesh.name << " BCF: " << glm::to_string(baseColorFactor) << std::endl;
				}
				baseColorFactors.push_back(baseColorFactor);
				
			}
			GLuint instanceMatricesID;
			if(instances != 1) {
				std::cout << "instances: " << instances << std::endl;
				std::cout << "matrix size: " << instanceMatrices.size() << std::endl;
				std::cout << "vao: " << vao << std::endl;
				
				glGenBuffers(1, &instanceMatricesID);
				glBindBuffer(GL_ARRAY_BUFFER, instanceMatricesID);
				glBufferData(GL_ARRAY_BUFFER, instances * sizeof(glm::mat4), glm::value_ptr(instanceMatrices[0]), GL_STATIC_DRAW);
			
				glEnableVertexAttribArray(5);
				glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)0);
				glEnableVertexAttribArray(6);
				glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4)));
				glEnableVertexAttribArray(7);
				glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(2 * sizeof(glm::vec4)));
				glEnableVertexAttribArray(8);
				glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(3 * sizeof(glm::vec4)));
				
				glVertexAttribDivisor(5, 1);
				glVertexAttribDivisor(6, 1);
				glVertexAttribDivisor(7, 1);
				glVertexAttribDivisor(8, 1);
				
			}
			glBindVertexArray(0);
			// Record VAO for later use
			PrimitiveObject primitiveObject;
			primitiveObject.vao = vao;
            primitiveObject.vbos = vbos;
			primitiveObject.instanceVBO = instanceMatricesID;
			primitiveObjects.push_back(primitiveObject);
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
		for(auto pr : primitiveObjects) {
			for (auto it = pr.vbos.cbegin(); it != pr.vbos.cend();) {
				tinygltf::BufferView bufferView = model.bufferViews[it->first];
				if (bufferView.target != GL_ELEMENT_ARRAY_BUFFER) {
				glDeleteBuffers(1, &pr.vbos[it->first]);
				pr.vbos.erase(it++);
				}
				else {
				++it;
				}
			}
		}

		return primitiveObjects;
	}


	void Entity::drawMesh(const std::vector<PrimitiveObject> &primitiveObjects,
				tinygltf::Model &model, tinygltf::Mesh &mesh, int &j) {
		
		
		for (size_t i = 0; i < mesh.primitives.size(); ++i) 
		{
			//std::cout << mesh.name << " " << " primitive " << i << " j " << j << std::endl; 
			GLuint vao = primitiveObjects[j+i].vao;
			std::map<int, GLuint> vbos = primitiveObjects[j+i].vbos;

			glBindVertexArray(vao);

			tinygltf::Primitive primitive = mesh.primitives[i];
			tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos.at(indexAccessor.bufferView));
			glUniform4fv(baseColorFactorID, 1, glm::value_ptr(baseColorFactors[j+i]));
			int matID = mesh.primitives[i].material;
            if (matID >= 0) {
				auto&& material = model.materials[matID];
                int texID = material.pbrMetallicRoughness.baseColorTexture.index;
                if (texID >= 0) {
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D,textures[j+i]);
					glUniform1i(diffuseTextureSamplerID,0);
					glUniform1i(isTextureID, true);
				}
				texID = material.normalTexture.index;
                if (texID >= 0) {
					glActiveTexture(GL_TEXTURE0+1);
					glBindTexture(GL_TEXTURE_2D,textures[j+i+1]);
					glUniform1i(normalTextureSamplerID,1);
					glUniform1i(isTextureID, true);
				}
			}
            if(instances != 1) {
                
				glBindBuffer(GL_ARRAY_BUFFER, primitiveObjects[j+i].instanceVBO);
				glBufferSubData(GL_ARRAY_BUFFER, 0, instancesInFrustum.size() * sizeof(glm::mat4), instancesInFrustum.data());
                glDrawElementsInstanced(primitive.mode, indexAccessor.count,
                            indexAccessor.componentType,
                            BUFFER_OFFSET(indexAccessor.byteOffset), instancesInFrustum.size());
                glGetError();
            
            } else{
                glDrawElements(primitive.mode, indexAccessor.count,
                            indexAccessor.componentType,
                            BUFFER_OFFSET(indexAccessor.byteOffset));
            }    
            
			glBindVertexArray(0);
			
		}
	}

	void Entity::drawModelNodes(const std::vector<PrimitiveObject>& primitiveObjects,
						tinygltf::Model &model, tinygltf::Node &node, int &j) {
		// Draw the mesh at the node, and recursively do so for children nodes
		if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
			drawMesh(primitiveObjects, model, model.meshes[node.mesh],j);
			j++;
		}
		for (size_t i = 0; i < node.children.size(); i++) {
			drawModelNodes(primitiveObjects, model, model.nodes[node.children[i]],j);
		}
	}
	void Entity::drawModel(const std::vector<PrimitiveObject>& primitiveObjects,
				tinygltf::Model &model) {
		// Draw all nodes
		const tinygltf::Scene &scene = model.scenes[model.defaultScene];
		//std::cout << "scene node size" <<  scene.nodes.size() << std::endl;
		
		for (size_t i = 0; i < scene.nodes.size(); ++i) {
			int j = 0;
			drawModelNodes(primitiveObjects, model, model.nodes[scene.nodes[i]], j);
		}
	}

	void Entity::render(glm::mat4 cameraMatrix, glm::vec3 cameraPosition, Shadow shadow, Light light) {
		
		
		// Set camera
		glUseProgram(programID);
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
		glUniform3fv(ambientID, 1, &light.ambient[0]);
		glUniform3fv(diffuseID, 1, &light.diffuse[0]);
		glUniform3fv(specularID, 1, &light.specular[0]);
		glUniform3fv(directionID, 1, &light.direction[0]);
		glUniform3fv(viewPosID, 1, &cameraPosition[0]);
		;
		glActiveTexture(GL_TEXTURE0+2);
		glBindTexture(GL_TEXTURE_2D,shadow.shadowMap);
		glUniform1i(depthSamplerID,2);
		glUniformMatrix4fv(lightSpaceMatrixID, 1, GL_FALSE, &shadow.lightSpaceView[0][0]);
		// Draw the GLTF model
		drawModel(primitiveObjects, model);
	}

	void Entity::render(GLuint depthID, glm::mat4 vp) {
		
		glUseProgram(depthID);
		glUniformMatrix4fv(glGetUniformLocation(depthID, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(vp));

		glUniformMatrix4fv(glGetUniformLocation(depthID, "u_model"), 1, GL_FALSE, &transform[0][0]);

		if(isSkinning) {
			glUniformMatrix4fv(glGetUniformLocation(depthID, "jointMat"), skinObjects[0].jointMatrices.size(), GL_FALSE, glm::value_ptr(skinObjects[0].jointMatrices[0]));
		}
		glUniform1i(glGetUniformLocation(depthID, "isSkinning"), isSkinning);
		drawModel(primitiveObjects, model);
	}


	void Entity::render(glm::mat4 view, glm::mat4 projection, glm::mat4 lightMat, GLuint deferredPrepass) {
		
		
		// Set camera
		glUseProgram(deferredPrepass);
		glUniformMatrix4fv(glGetUniformLocation(deferredPrepass, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightMat));

		glUniformMatrix4fv(glGetUniformLocation(deferredPrepass, "u_model"), 1, GL_FALSE, &transform[0][0]);

		glUniformMatrix4fv(glGetUniformLocation(deferredPrepass, "view"), 1, GL_FALSE, glm::value_ptr(view));

		glUniformMatrix4fv(glGetUniformLocation(deferredPrepass, "projection"), 1, GL_FALSE, &projection[0][0]);

		if(isSkinning) {
			glUniformMatrix4fv(glGetUniformLocation(deferredPrepass, "jointMat"), skinObjects[0].jointMatrices.size(), GL_FALSE, glm::value_ptr(skinObjects[0].jointMatrices[0]));
		}
		glUniform1i(glGetUniformLocation(deferredPrepass, "isSkinning"), isSkinning);
		drawModel(primitiveObjects, model);
	}


	void Entity::render(glm::mat4 cameraMatrix, glm::vec3 cameraPosition, Shadow shadow, Light light, std::vector<glm::mat4> instancesInFrust) {
		
		instancesInFrustum = instancesInFrust;
		// Set camera
		glUseProgram(programID);
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
		glUniform3fv(ambientID, 1, &light.ambient[0]);
		glUniform3fv(diffuseID, 1, &light.diffuse[0]);
		glUniform3fv(specularID, 1, &light.specular[0]);
		glUniform3fv(directionID, 1, &light.direction[0]);
		glUniform3fv(viewPosID, 1, &cameraPosition[0]);
		;
		glActiveTexture(GL_TEXTURE0+2);
		glBindTexture(GL_TEXTURE_2D,shadow.shadowMap);
		glUniform1i(depthSamplerID,2);
		glUniformMatrix4fv(lightSpaceMatrixID, 1, GL_FALSE, &shadow.lightSpaceView[0][0]);
		// Draw the GLTF model
		drawModel(primitiveObjects, model);
	}
	void Entity::render(GLuint depthID, glm::mat4 vp, std::vector<glm::mat4> instancesInFrust) {
		
		instancesInFrustum = instancesInFrust;
		glUseProgram(depthID);
		glUniformMatrix4fv(glGetUniformLocation(depthID, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(vp));

		glUniformMatrix4fv(glGetUniformLocation(depthID, "u_model"), 1, GL_FALSE, &transform[0][0]);

		if(isSkinning) {
			glUniformMatrix4fv(glGetUniformLocation(depthID, "jointMat"), skinObjects[0].jointMatrices.size(), GL_FALSE, glm::value_ptr(skinObjects[0].jointMatrices[0]));
		}
		glUniform1i(glGetUniformLocation(depthID, "isSkinning"), isSkinning);
		drawModel(primitiveObjects, model);
	}

	void Entity::render() {
		drawModel(primitiveObjects,model);
	}

	void Entity::setTransform(float time, float chunks, float chunkWidth, float origin, int xyz) {
		float drift = std::fmod(time * 0.1f, 127.0f * 3.0f);
		if(glm::vec3(transform[3]).z + drift < chunks*chunkWidth) {
			if(xyz == 1) {
				transform = glm::translate(transform, glm::vec3(0.0f, drift,0.0f ));
			} else {
				transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, drift ));
			}
		} else {
			transform = glm::translate(transform, glm::vec3(origin, 0.0f, origin));
		}
	}


	void Entity::cleanup() {
		glDeleteProgram(programID);
	}