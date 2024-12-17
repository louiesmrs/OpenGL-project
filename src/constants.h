#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>


struct Shadow {
    glm::mat4 lightSpaceView;
    GLuint shadowMap;
};

struct Light {
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    glm::vec3 direction;
};