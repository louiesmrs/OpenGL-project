
#ifndef PARTICLE_GENERATOR_H
#define PARTICLE_GENERATOR_H


#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

struct Particle {
    glm::vec3 pos;
    glm::vec3 velocity;
    float life;
    float scale;
    Particle() : pos(0.0f), velocity(0.0f), life(0.0f), scale(0.2) { }
    
};

class ParticleGenerator {
    public:
    GLuint VAO;
    unsigned int amount;
    size_t instanceCount;
    GLuint shaderID;
    GLuint textureID;
    glm::vec3 center;
    std::vector<Particle> particles;
    std::vector<glm::mat4> instanceMatrices;
    GLuint instanceID;
    GLuint textureSamplerID;
    GLuint mvpMatrixID;
    GLuint viewPosID;


    ParticleGenerator(GLuint shader, GLuint texture, unsigned int amount, glm::vec3 center);
    void update(float time, glm::vec3 center);
    void init();
    void render(glm::mat4 MVP, glm::vec3 cameraPosition);
    void setupInstancing();

};


#endif;