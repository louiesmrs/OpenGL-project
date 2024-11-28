#include "skybox.h"
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <render/shader.h>

GLfloat skyboxVertices[] = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };
    // GLuint index_buffer_data[36] = {		// 12 triangle faces of a box
	// 	0, 1, 2, 	
	// 	0, 2, 3, 
		
	// 	4, 5, 6, 
	// 	4, 6, 7, 

	// 	8, 9, 10, 
	// 	8, 10, 11, 

	// 	12, 13, 14, 
	// 	12, 14, 15, 

	// 	16, 17, 18, 
	// 	16, 18, 19, 

	// 	20, 21, 22, 
	// 	20, 22, 23, 
	// };
Skybox::Skybox()
{
}

void Skybox::initialize(std::vector<std::string> faces, const char *vertexShaderPath, const char *fragmentShaderPath)
{

    glGenVertexArrays(1, &skyboxVertexArrayID);
    glBindVertexArray(skyboxVertexArrayID);

    glGenBuffers(1, &skyboxVertexBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVertexBufferID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    
    // glGenBuffers(1, &indexBufferID);
    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
    // glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_buffer_data), index_buffer_data, GL_STATIC_DRAW);
    cubeMapTexture = loadCubeMap(faces);

    programID = LoadShadersFromFile(vertexShaderPath, fragmentShaderPath);
    // Get a handle for our "MVP" uniform
    mvpMatrixID = glGetUniformLocation(programID, "MVP");
   
    textureSamplerID = glGetUniformLocation(programID, "skybox");
    glGetError();
}

void Skybox::render(glm::mat4 mvp) {
    glUseProgram(programID);
    glDepthMask(GL_FALSE);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVertexBufferID);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(skyboxVertexArrayID);
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
    glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);

    glActiveTexture(GL_TEXTURE0);
    glGetError();
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapTexture);
    glGetError();
    glUniform1i(textureSamplerID,0);
    glGetError();
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glGetError();
    glDepthMask(GL_TRUE); // set depth function back to default
    
}


// loads a cubemap texture from 6 individual texture faces
// order:
// +X (right)
// -X (left)
// +Y (top)
// -Y (bottom)
// +Z (front) 
// -Z (back)
// -------------------------------------------------------
GLuint Skybox::loadCubeMap(std::vector<std::string> faces) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrComponents;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrComponents, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}