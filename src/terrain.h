

#ifndef TERRAIN_H
#define TERRAIN_H

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <string>
#include <random>
#include <iostream>
#include <math.h>
#include <vector>
#include <cstdlib>
#include "entity.h"
#include "constants.h"
#include <render/shader.h>
#include "particle_generator.h"


// Structs
struct treeCoord {
    float xpos;
    float ypos;
    float zpos;
    int xOffset;
    int yOffset;
    
    treeCoord(float _xpos, float _ypos, float _zpos, int _xOffset, int _yOffset) {
        xpos = _xpos;
        ypos = _ypos;
        zpos = _zpos;
        xOffset = _xOffset;
        yOffset = _yOffset;
    }
};



class Terrain
{
public:

    GLuint programID;
    GLuint modelID;
    GLuint mvpMatrixID;
    GLuint ambientID;
    GLuint diffuseID;
    GLuint specularID;
    GLuint directionID;
    GLuint viewPosID;
    GLuint depthSamplerID;
    GLuint lightSpaceMatrixID;

    float WATER_HEIGHT = 0.1;
    int chunk_render_distance = 4;
    int xMapChunks;
    int yMapChunks;
    int chunkWidth;
    int chunkHeight;
    int gridPosX = 0;
    int gridPosY = 0;
    float originX;
    float originY;
    int nIndices;

    // Noise params
    int octaves = 5;
    float meshHeight = 12;  // Vertical scaling
    float noiseScale = 64;  // Horizontal scaling
    float persistence = 0.5;
    float lacunarity = 2;

    // Model params
    float MODEL_SCALE = 3;
    float MODEL_BRIGHTNESS = 6;

    std::vector<GLuint> map_chunks;
    std::vector<GLuint> trees;
    std::vector<treeCoord> treeCoords;
    std::vector<glm::mat4> instanceMatrices;
    std::vector<ParticleGenerator> particleGenerators;
    Entity tree;
    Entity bot;
    Entity fox;
    Entity bird;
    Entity goose;
    

    Terrain(int xMapChunks, int yMapChunks, int chunkWidth, int chunkHeight, float originX, float originY)
    : xMapChunks(xMapChunks), yMapChunks(yMapChunks), chunkWidth(chunkWidth), chunkHeight(chunkHeight),
    originX(originX), originY(originY) {
        nIndices = chunkWidth * chunkHeight * 6;
        programID = LoadShadersFromFile("../src/shader/terrain.vert", "../src/shader/terrain.frag");
        map_chunks.resize(xMapChunks * yMapChunks);
        for (int y = 0; y < yMapChunks; y++)
            for (int x = 0; x < xMapChunks; x++) {
                generate_map_chunk(map_chunks[x + y*xMapChunks], x, y, treeCoords);
        }
        mvpMatrixID = glGetUniformLocation(programID, "MVP");
        modelID = glGetUniformLocation(programID, "u_model");
        ambientID = glGetUniformLocation(programID, "ambient");
		diffuseID = glGetUniformLocation(programID, "diffuse");
        specularID = glGetUniformLocation(programID, "specular");
		directionID = glGetUniformLocation(programID, "direction");
		viewPosID = glGetUniformLocation(programID, "u_viewPos");
        depthSamplerID = glGetUniformLocation(programID, "shadowMap");
        lightSpaceMatrixID = glGetUniformLocation(programID, "lightSpaceMatrix");
        trees.reserve(xMapChunks * yMapChunks);

        
    }
    std::vector<int> generate_indices();
    std::vector<float> generate_noise_map(int xOffset, int yOffset);
    std::vector<float> generate_vertices(const std::vector<float> &noise_map);
    std::vector<float> generate_normals(const std::vector<int> &indices, const std::vector<float> &vertices);
    std::vector<float>  generate_biome(const std::vector<float> &vertices, std::vector<treeCoord> &treeCoords, int xOffset, int yOffset);
    void generate_map_chunk(GLuint &VAO, int xOffset, int yOffset, std::vector<treeCoord> &treeCoords);
    void render(glm::mat4 &mvp, glm::vec3 cameraPosition, Shadow shadow, Light light, GLuint tex);
    void render(glm::vec3 cameraPosition, GLuint terrainDepthID, GLuint treeDepthID, glm::mat4 vp, GLuint botDepthID);
    void setup_instancing(GLuint particleTex, GLuint particleShader);
    void update(float deltaTime, float particleTime, float chunks, float chunkWidth, float origin);

};

#endif;
