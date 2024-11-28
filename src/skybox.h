

#ifndef SKYBOX_H
#define SKYBOX_H

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>



class Skybox
{
public:
   
    GLuint skyboxVertexArrayID;
    GLuint skyboxVertexBufferID;
    GLuint cubeMapTexture;
    GLuint programID;
    GLuint mvpMatrixID;
    GLuint textureSamplerID;
    // constructor
    Skybox();

    void initialize(std::vector<std::string> faces,  const char *vertexShaderPath,  const char *fragmentShaderPath);

    void render(glm::mat4 mvp);

    GLuint loadCubeMap(std::vector<std::string> faces);

};

#endif;
