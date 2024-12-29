#include "terrain.h"
#include <glm/gtx/string_cast.hpp>
double fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); };
    
double lerp(double t, double a, double b) { return a + t * (b - a); }
    
double grad(int hash, double x, double y, double z) {
   int h = hash & 15;                      // CONVERT LO 4 BITS OF HASH CODE
   double u = h<8 ? x : y,                 // INTO 12 GRADIENT DIRECTIONS.
          v = h<4 ? y : h==12||h==14 ? x : z;
   return ((h&1) == 0 ? u : -u) + ((h&2) == 0 ? v : -v);
}
    
double perlin_noise(float x, float y, std::vector<int> &p) {
    int z = 0.5;
    
    int X = (int)floor(x) & 255,                  // FIND UNIT CUBE THAT
        Y = (int)floor(y) & 255,                  // CONTAINS POINT.
        Z = (int)floor(z) & 255;
    x -= floor(x);                                // FIND RELATIVE X,Y,Z
    y -= floor(y);                                // OF POINT IN CUBE.
    z -= floor(z);
    double u = fade(x),                                // COMPUTE FADE CURVES
           v = fade(y),                                // FOR EACH OF X,Y,Z.
           w = fade(z);
    int A = p[X  ]+Y, AA = p[A]+Z, AB = p[A+1]+Z,      // HASH COORDINATES OF
        B = p[X+1]+Y, BA = p[B]+Z, BB = p[B+1]+Z;      // THE 8 CUBE CORNERS,

    return lerp(w, lerp(v, lerp(u, grad(p[AA  ], x  , y  , z   ),  // AND ADD
                                   grad(p[BA  ], x-1, y  , z   )), // BLENDED
                           lerp(u, grad(p[AB  ], x  , y-1, z   ),  // RESULTS
                                   grad(p[BB  ], x-1, y-1, z   ))),// FROM  8
                   lerp(v, lerp(u, grad(p[AA+1], x  , y  , z-1 ),  // CORNERS
                                   grad(p[BA+1], x-1, y  , z-1 )), // OF CUBE
                           lerp(u, grad(p[AB+1], x  , y-1, z-1 ),
                                   grad(p[BB+1], x-1, y-1, z-1 ))));
}

std::vector<int> get_permutation_vector () {
    std::vector<int> p;

    std::vector<int> permutation = { 151,160,137,91,90,15,
    131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
    190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
    88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
    77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
    102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
    135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
    5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
    223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
    129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
    251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
    49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
    138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
    };

    for (int j = 0; j < 2; j++)
        for (int i=0; i < 256; i++) {
            p.push_back(permutation[i]);
        }
    
    return p;
}

void Terrain::render(glm::mat4 &mvp, glm::vec3 cameraPosition, Shadow shadow, Light light, GLuint tex) {
    // Per-frame time logic
    
    // Measures number of map chunks away from origin map chunk the camera is
    gridPosX = (int)(cameraPosition.x - originX) / chunkWidth + xMapChunks / 2;
    gridPosY = (int)(cameraPosition.z - originY) / chunkHeight + yMapChunks / 2;

    
    glUseProgram(programID);
    glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);
    glUniform3fv(ambientID, 1, &light.ambient[0]);
    glUniform3fv(diffuseID, 1, &light.diffuse[0]);
    glUniform3fv(specularID, 1, &light.specular[0]);
    glUniform3fv(directionID, 1, &light.direction[0]);
    glUniform3fv(viewPosID, 1, &cameraPosition[0]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadow.shadowMap);
    glUniform1i(depthSamplerID,0);
    glActiveTexture(GL_TEXTURE0+1);
    glBindTexture(GL_TEXTURE_2D,tex);
    glUniform1i(glGetUniformLocation(programID, "tex"),1);
    glUniformMatrix4fv(lightSpaceMatrixID, 1, GL_FALSE, &shadow.lightSpaceView[0][0]);
    // Render map chunks
    std::vector<glm::mat4> modelMatrices;
    for (int y = 0; y < yMapChunks; y++) 
        for (int x = 0; x < xMapChunks; x++) {
            // Only render chunk if it's within render distance
            if (std::abs(gridPosX - x) <= chunk_render_distance && (y - gridPosY) <= chunk_render_distance) {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(-chunkWidth / 2.0 + (chunkWidth - 1) * x, 0.0, -chunkHeight / 2.0 + (chunkHeight - 1) * y));
                modelMatrices.push_back(model);
                //std::cout << "translate " << glm::to_string(glm::vec3(-chunkWidth / 2.0 + (chunkWidth - 1) * x, 0.0, -chunkHeight / 2.0 + (chunkHeight - 1) * y)) << std::endl;
                glUniformMatrix4fv(modelID, 1, GL_FALSE, &model[0][0]);
                
                // Terrain chunk
                glBindVertexArray(map_chunks[x + y*xMapChunks]);
                glDrawElements(GL_TRIANGLES, nIndices, GL_UNSIGNED_INT, 0);
            }
        }
    std::vector<glm::mat4> instancesWithinRenderDist;
    for(glm::mat4& instance : instanceMatrices) {
        glm::vec3 pos = glm::vec3(instance[3]);
        if (std::abs(pos.x) <= chunk_render_distance*chunkWidth && std::abs(pos.z) <= chunk_render_distance*chunkHeight) {
            instancesWithinRenderDist.push_back(instance);
        }
    }
    
    glEnable(GL_CULL_FACE);
    tree.render(mvp, cameraPosition, shadow, light,instancesWithinRenderDist );
    bot.render(mvp, cameraPosition, shadow, light, instancesWithinRenderDist);
    fox.render(mvp, cameraPosition, shadow, light, instancesWithinRenderDist);
    bird.render(mvp, cameraPosition, shadow, light, instancesWithinRenderDist);
    goose.render(mvp, cameraPosition, shadow, light, instancesWithinRenderDist);
    for(ParticleGenerator& pGen : particleGenerators) {
        pGen.render(mvp,cameraPosition);
    }
    glDisable(GL_CULL_FACE);
}


void Terrain::update(float deltaTime, float particleTime, float chunks, float chunkWidth, float origin) {
    bot.update(deltaTime);
    bird.update(deltaTime);
    bird.setTransform(deltaTime/4,chunks,chunkWidth,origin, 1);
    fox.update(deltaTime);
    fox.setTransform(deltaTime*4,chunks,chunkWidth,origin, 0);
    float drift = std::fmod(deltaTime * 0.006f, 127.0f * 3.0f);
    for(ParticleGenerator& pGen : particleGenerators) {
        pGen.update(particleTime, glm::vec3(pGen.center.x,pGen.center.y,pGen.center.z+drift));
    }
    goose.update(deltaTime);
}

void Terrain::render(glm::vec3 cameraPosition, GLuint terrainDepthID, GLuint treeDepthID, glm::mat4 vp, GLuint botDepthID) {
    // Per-frame time logic
    // Measures number of map chunks away from origin map chunk the camera is
    glUseProgram(terrainDepthID);
    gridPosX = (int)(cameraPosition.x - originX) / chunkWidth + xMapChunks / 2;
    gridPosY = (int)(cameraPosition.z - originY) / chunkHeight + yMapChunks / 2;
    glUniformMatrix4fv(glGetUniformLocation(terrainDepthID, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(vp));
    // Render map chunks
    std::vector<glm::mat4> modelMatrices;
    for (int y = 0; y < yMapChunks; y++) 
        for (int x = 0; x < xMapChunks; x++) {
            // Only render chunk if it's within render distance
            if (std::abs(gridPosX - x) <= chunk_render_distance && (y - gridPosY) <= chunk_render_distance) {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(-chunkWidth / 2.0 + (chunkWidth - 1) * x, 0.0, -chunkHeight / 2.0 + (chunkHeight - 1) * y));
                modelMatrices.push_back(model);
                //std::cout << "translate " << glm::to_string(glm::vec3(-chunkWidth / 2.0 + (chunkWidth - 1) * x, 0.0, -chunkHeight / 2.0 + (chunkHeight - 1) * y)) << std::endl;
                glUniformMatrix4fv(glGetUniformLocation(terrainDepthID, "u_model"), 1, GL_FALSE, &model[0][0]);
                
                // Terrain chunk
                glBindVertexArray(map_chunks[x + y*xMapChunks]);
                glDrawElements(GL_TRIANGLES, nIndices, GL_UNSIGNED_INT, 0);
            }
    }
    std::vector<glm::mat4> instancesWithinRenderDist;
    for(glm::mat4& instance : instanceMatrices) {
        glm::vec3 pos = glm::vec3(instance[3]);
        if (std::abs(pos.x) <= chunk_render_distance*chunkWidth && std::abs(pos.z) <= chunk_render_distance*chunkHeight) {
            instancesWithinRenderDist.push_back(instance);
        }
    }
    
    glEnable(GL_CULL_FACE);
    tree.render(treeDepthID, vp, instancesWithinRenderDist);
    bot.render(botDepthID, vp, instancesWithinRenderDist);
    fox.render(botDepthID, vp, instancesWithinRenderDist);
    bird.render(botDepthID, vp, instancesWithinRenderDist);
    goose.render(botDepthID, vp, instancesWithinRenderDist);
    glDisable(GL_CULL_FACE);

}


void Terrain::setup_instancing(GLuint particleTex, GLuint particleShader) {
    
    instanceMatrices.reserve(treeCoords.size());
    std::cout << treeCoords.size() << std::endl;
    // Instancing prep
    glm::mat4 m = glm::mat4(1.0f);

    
    for (int i = 0; i < treeCoords.size(); i++) {
        glm::mat4 model = glm::mat4(1.0f);
        float xPos = treeCoords[i].xpos + treeCoords[i].xOffset * chunkWidth * (i%10+1);
        float yPos = treeCoords[i].ypos;
        float zPos = treeCoords[i].zpos + treeCoords[i].yOffset * chunkHeight * (i%10+1);
        model = glm::translate(model, glm::vec3(xPos, yPos, zPos));
        glm::vec3 coord = glm::vec3(xPos, yPos, zPos);
        std::cout << i << " " << glm::to_string(coord) << std::endl;
        instanceMatrices.push_back(model);
        ParticleGenerator particleGenerator(particleShader, particleTex, 25, glm::vec3(xPos+originX, yPos, zPos+originY-5.0f));
        particleGenerators.push_back(particleGenerator);
    }
    //m = glm::scale(m, glm::vec3(0.5f,0.5f,0.5f));
    // Duplicate trees in all four quadrants
    int originalSize = instanceMatrices.size();
    for (int i = 0; i < originalSize; i++) {
        glm::mat4 model = instanceMatrices[i];
        glm::vec3 pos = glm::vec3(model[3]);

        // First quadrant (already added)
        // Second quadrant
        glm::mat4 model2 = glm::translate(glm::mat4(1.0f), glm::vec3(-pos.x, pos.y, pos.z));
        instanceMatrices.push_back(model2);
        ParticleGenerator particleGenerator1(particleShader, particleTex, 25, glm::vec3(-pos.x+originX, pos.y, pos.z+originY-5.0f));
        particleGenerators.push_back(particleGenerator1);

        // Third quadrant
        glm::mat4 model3 = glm::translate(glm::mat4(1.0f), glm::vec3(-pos.x, pos.y, -pos.z));
        instanceMatrices.push_back(model3);
        ParticleGenerator particleGenerator2(particleShader, particleTex, 25, glm::vec3(-pos.x+originX, pos.y, -pos.z+originY-5.0f));
        particleGenerators.push_back(particleGenerator2);

        // Fourth quadrant
        glm::mat4 model4 = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, pos.y, -pos.z));
        instanceMatrices.push_back(model4);
        ParticleGenerator particleGenerator3(particleShader, particleTex, 25, glm::vec3(pos.x+originX, pos.y, -pos.z+originY-5.0f));
        particleGenerators.push_back(particleGenerator3);
    }
    glm::mat4 mTree = glm::translate(m, glm::vec3(originX, 0.0f, originY));
    glm::mat4 mBot = glm::translate(m, glm::vec3(originX, 0.0f, originY));
    glm::mat4 mBird = glm::translate(m, glm::vec3(originX, 20.0f, originY));
    mBot = glm::scale(mBot, glm::vec3(0.1f, 0.1f, 0.1f));
    glm::mat4 mFox = glm::scale(mTree, glm::vec3(0.03f, 0.03f, 0.03f));
    glm::mat4 mGoose = glm::scale(mTree, glm::vec3(0.2f, 0.2f, 0.2f));
    mTree = glm::rotate(mTree, glm::radians(135.0f), glm::vec3(0.0f,1.0f,0.0f));
    mBird = glm::rotate(mBird, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    
    mGoose = glm::rotate(mBot, glm::radians(90.0f), glm::vec3(1.0f,0.0f,0.0f));
    // //mBird = glm::rotate(mBird, glm::radians(90.0f), glm::vec3(0.0f,0.0f,1.0f));
    
    std::cout << "matrix_Size: " << instanceMatrices.size() << std::endl;
    tree = Entity("../src/model/low/low.gltf", "../src/shader/tree.vert", "../src/shader/bot.frag",
        mTree, false, instanceMatrices.size(), instanceMatrices);
    bot = Entity("../src/model/bot/bot.gltf", "../src/shader/bot.vert", "../src/shader/bot.frag",
    mBot, true, instanceMatrices.size(), instanceMatrices);
    goose = Entity("../src/model/goose/goose.gltf", "../src/shader/bot.vert", "../src/shader/bot.frag",
    mGoose, true, instanceMatrices.size(), instanceMatrices);
    fox = Entity("../src/model/fox/fox.gltf", "../src/shader/bot.vert", "../src/shader/bot.frag",
    mFox, true, instanceMatrices.size(), instanceMatrices);
    bird = Entity("../src/model/bird/bird.gltf", "../src/shader/bot.vert", "../src/shader/bot.frag",
        mBird, true, instanceMatrices.size(), instanceMatrices);
}




void Terrain::generate_map_chunk(GLuint &VAO, int xOffset, int yOffset, std::vector<treeCoord> &treeCoords) {
    std::vector<int> indices;
    std::vector<float> noise_map;
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> uvs;
    
    // Generate map
    indices = generate_indices();
    noise_map = generate_noise_map(xOffset, yOffset);
    vertices = generate_vertices(noise_map);
    normals = generate_normals(indices, vertices);
    uvs = generate_biome(vertices, treeCoords, xOffset, yOffset);
    
    GLuint VBO[3], EBO;
    
    // Create buffers and arrays
    glGenBuffers(3, VBO);
    glGenBuffers(1, &EBO);
    glGenVertexArrays(1, &VAO);
    
    // Bind vertices to VBO
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);
    
    // Create element buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), &indices[0], GL_STATIC_DRAW);
    
    // Configure vertex position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Bind vertices to VBO
    glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(float), &normals[0], GL_STATIC_DRAW);
    
    // Configure vertex normals attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    
    // Bind vertices to VBO
    glBindBuffer(GL_ARRAY_BUFFER, VBO[2]);
    glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(float), &uvs[0], GL_STATIC_DRAW);
    
    // Configure vertex colors attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(2);
}


std::vector<float>  Terrain::generate_noise_map(int offsetX, int offsetY) {
    std::vector<float> noiseValues;
    std::vector<float> normalizedNoiseValues;
    std::vector<int> p = get_permutation_vector();
    
    float amp  = 1;
    float freq = 1;
    float maxPossibleHeight = 0;
    
    for (int i = 0; i < octaves; i++) {
        maxPossibleHeight += amp;
        amp *= persistence;
    }
    
    for (int y = 0; y < chunkHeight; y++) {
        for (int x = 0; x < chunkWidth; x++) {
            amp  = 1;
            freq = 1;
            float noiseHeight = 0;
            for (int i = 0; i < octaves; i++) {
                float xSample = (x + offsetX * (chunkWidth-1))  / noiseScale * freq;
                float ySample = (y + offsetY * (chunkHeight-1)) / noiseScale * freq;
                
                float perlinValue = perlin_noise(xSample, ySample, p);
                noiseHeight += perlinValue * amp;
                amp  *= persistence;
                freq *= lacunarity;
            }
            
            noiseValues.push_back(noiseHeight);
        }
    }
    
    for (int y = 0; y < chunkHeight; y++) {
        for (int x = 0; x < chunkWidth; x++) {
            normalizedNoiseValues.push_back((noiseValues[x + y*chunkWidth] + 1) / maxPossibleHeight);
        }
    }

    return normalizedNoiseValues;
}


std::vector<float> Terrain::generate_biome(const std::vector<float> &vertices, std::vector<treeCoord> &treeCoords, int xOffset, int yOffset) {
    std::vector<float> uvs;
    std::vector<float> textureHeights;
    glm::vec2 uv = glm::vec2(0.0f);
    

    textureHeights.push_back(0.375);                               
    textureHeights.push_back(0.625);                              
    textureHeights.push_back(0.95);                
    
    for (int i = 1; i < vertices.size(); i += 3) {
        for (int j = 0; j < textureHeights.size(); j++) {
            // NOTE: The max height of a vertex is "meshHeight"
            if (vertices[i] <= textureHeights[j] * meshHeight) {
                uv = glm::vec2(textureHeights[j], 0.5f);
                // uv.x -= static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 0.1f - 0.05f;
                // uv.y -= static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 0.1f - 0.05f;
                if (j == 0) {
                    if (rand() % 10000 < 5) {
                        treeCoords.push_back(treeCoord{vertices[i-1], vertices[i], vertices[i+1], xOffset, yOffset});
                    }
                }
                break;
            }
        }
        uvs.push_back(uv.x);
        uvs.push_back(uv.y);
    }
    return uvs;
}

std::vector<float>  Terrain::generate_normals(const std::vector<int> &indices, const std::vector<float> &vertices) {
    int pos;
    glm::vec3 normal;
    std::vector<float> normals;
    std::vector<glm::vec3> verts;
    
    // Get the vertices of each triangle in mesh
    // For each group of indices
    for (int i = 0; i < indices.size(); i += 3) {
        
        // Get the vertices (point) for each index
        for (int j = 0; j < 3; j++) {
            pos = indices[i+j]*3;
            verts.push_back(glm::vec3(vertices[pos], vertices[pos+1], vertices[pos+2]));
        }
        
        // Get vectors of two edges of triangle
        glm::vec3 U = verts[i+1] - verts[i];
        glm::vec3 V = verts[i+2] - verts[i];
        
        // Calculate normal
        normal = glm::normalize(-glm::cross(U, V));
        normals.push_back(normal.x);
        normals.push_back(normal.y);
        normals.push_back(normal.z);
    }
    
    return normals;
}

std::vector<float>  Terrain::generate_vertices(const std::vector<float> &noise_map) {
    std::vector<float> v;
    
    for (int y = 0; y < chunkHeight + 1; y++)
        for (int x = 0; x < chunkWidth; x++) {
            v.push_back(x);
            // Apply cubic easing to the noise
            float easedNoise = std::pow(noise_map[x + y*chunkWidth] * 1.1, 3);
            // Scale noise to match meshHeight
            // Pervent vertex height from being below WATER_HEIGHT
            v.push_back(std::fmax(easedNoise * meshHeight, WATER_HEIGHT * 0.5 * meshHeight));
            v.push_back(y);
        }
    
    return v;
}

std::vector<int> Terrain::generate_indices() {
    std::vector<int> indices;
    
    for (int y = 0; y < chunkHeight; y++)
        for (int x = 0; x < chunkWidth; x++) {
            int pos = x + y*chunkWidth;
            
            if (x == chunkWidth - 1 || y == chunkHeight - 1) {
                // Don't create indices for right or top edge
                continue;
            } else {
                // Top left triangle of square
                indices.push_back(pos + chunkWidth);
                indices.push_back(pos);
                indices.push_back(pos + chunkWidth + 1);
                // Bottom right triangle of square
                indices.push_back(pos + 1);
                indices.push_back(pos + 1 + chunkWidth);
                indices.push_back(pos);
            }
        }

    return indices;
}

