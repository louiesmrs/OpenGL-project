#include "particle_generator.h"

ParticleGenerator::ParticleGenerator(GLuint shader, GLuint texture, unsigned int amount, glm::vec3 center)
 : shaderID(shader), textureID(texture), amount(amount), center(center)
{
    this->init();
}

float randomFloatBetween(float low, float high) {
	return low + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (high - (low))));
}


void ParticleGenerator::setupInstancing() {
    for (int i = 0; i < amount; ++i) {
			Particle p;

			float angle = static_cast<float>(rand()) / RAND_MAX * 2.0f * glm::pi<float>();
			float radius = static_cast<float>(rand()) / RAND_MAX * 3.0f;
			float randomX = radius * cos(angle);
			float randomZ = radius * sin(angle);
			float randomStartHeight = randomFloatBetween(center.y, center.y+1.0f);
			float randomSpeed = randomFloatBetween(1.0f,5.0f);
			float randomLifetime = randomFloatBetween(0.0f,1.0f);
			p.pos = glm::vec3(center.x + randomX, randomStartHeight, center.z + randomZ);
			p.scale = randomFloatBetween(0.1f, 1.1f);
			p.velocity = glm::vec3(randomSpeed);
			p.life = randomLifetime;

			particles.push_back(p);

			// Create instance transforms
			glm::mat4 transform(1.0f);
			transform = glm::translate(transform, p.pos);
			transform = glm::scale(transform, glm::vec3(p.scale));
			instanceMatrices.push_back(transform);
    }
}


void ParticleGenerator::init() {
    GLuint VBO;
    glm::vec3 green = glm::vec3(95.0/255.0, 165.0/255.0, 30.0/255.0);
    GLfloat particle_quad[] = {
        // Positions         // Texture Coords
        0.0f, 1.0f, 0.0f,    0.0f, 1.0f,  green.r, green.g, green.b, // Top-left
        0.5f, 0.0f, 0.0f,    1.0f, 0.0f,  green.r, green.g, green.b,  // Bottom-right
        0.0f, 0.0f, 0.0f,    0.0f, 0.0f,  green.r, green.g, green.b,  // Bottom-left

        0.0f, 1.0f, 0.0f,    0.0f, 1.0f,  green.r, green.g, green.b,  // Top-left
        0.5f, 1.0f, 0.0f,    1.0f, 1.0f,  green.r, green.g, green.b,  // Top-right
        0.5f, 0.0f, 0.0f,    1.0f, 0.0f,  green.r, green.g, green.b,   // Bottom-right
    }; 

    setupInstancing();

    glGenVertexArrays(1, &this->VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(this->VAO);
    // fill mesh buffer
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(particle_quad), particle_quad, GL_STATIC_DRAW);
    // set mesh attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));

    glGenBuffers(1, &instanceID);
    glBindBuffer(GL_ARRAY_BUFFER, instanceID);
    glBufferData(GL_ARRAY_BUFFER, instanceMatrices.size() * sizeof(glm::mat4), instanceMatrices.data(), GL_STATIC_DRAW);
    instanceCount = instanceMatrices.size() * sizeof(glm::mat4);
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
    
    glBindVertexArray(0);

    textureSamplerID = glGetUniformLocation(shaderID, "tex");
    viewPosID = glGetUniformLocation(shaderID, "u_viewPos");
    mvpMatrixID = glGetUniformLocation(shaderID, "MVP");
}

void updateInstances(const std::vector<glm::mat4>& instanceTransforms, GLuint instanceID) {
		// Update instance buffer
		glBindBuffer(GL_ARRAY_BUFFER, instanceID);
		static size_t currentBufferSize = 0;
		size_t newSize = instanceTransforms.size() * sizeof(glm::mat4);
		if (newSize > currentBufferSize) {
			glBufferData(GL_ARRAY_BUFFER, newSize, nullptr, GL_DYNAMIC_DRAW);
			currentBufferSize = newSize;
		}
		glBufferSubData(GL_ARRAY_BUFFER, 0, newSize, instanceTransforms.data());

	;
	}



void ParticleGenerator::update(float time, glm::vec3 newCenter) {
    glm::vec3 centerOffset = newCenter - center;
    for (size_t i = 0; i < particles.size(); ++i) {
			Particle& p = particles[i];

			// Update particle's lifetime
			p.life -= time;
			if (p.life <= 0.0f) {
				// Reset the particle
				float angle = static_cast<float>(rand()) / RAND_MAX * 2.0f * glm::pi<float>();
                float radius = static_cast<float>(rand()) / RAND_MAX * 3.0f;
                float randomX = radius * cos(angle);
                float randomZ = radius * sin(angle);
                float randomStartHeight = randomFloatBetween(newCenter.y,center.y+1.0f);
                float randomSpeed = randomFloatBetween(1.0f,5.0f);
                float randomLifetime = randomFloatBetween(0.0f,1.0f);
                p.pos = glm::vec3(newCenter.x + randomX, randomStartHeight, newCenter.z + randomZ);
                p.scale = randomFloatBetween(0.1f, 1.1f);
                p.velocity = glm::vec3(randomSpeed);
                p.life = randomLifetime;
				
            }else {
				// Move particle upward
                p.pos.z += centerOffset.z;
                p.pos.y += p.velocity.y * time;
                p.pos.z -= p.velocity.z * time;
			}

			// Update instance transform
			glm::mat4 transform(1.0f);
			transform = glm::translate(transform, p.pos);
			transform = glm::scale(transform, glm::vec3(p.scale));
			instanceMatrices[i] = transform;
		}
        updateInstances(instanceMatrices, instanceID);
        center = newCenter;

}

void ParticleGenerator::render(glm::mat4 MVP, glm::vec3 cameraPosition) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(shaderID);

    glBindVertexArray(this->VAO);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glUniform1i(textureSamplerID, 0);

    glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &MVP[0][0]);
    glUniform3fv(viewPosID, 1, &cameraPosition[0]);

    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, instanceMatrices.size());
   
    glDisable(GL_BLEND);
}