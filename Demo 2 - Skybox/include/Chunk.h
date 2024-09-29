#ifndef CHUNK_H
#define CHUNK_H

#include "VkUtils.h"
#include "PerlinNoise.hpp"


//TEMP
#include <iostream>

const int CHUNK_SIZE = 64;

struct Chunk {
    const int chunkX, chunkY, chunkZ;
    std::vector<VkUtils::BlockFaceInstance> blockFaces;
    uint32_t heightMap[CHUNK_SIZE*CHUNK_SIZE];

    Chunk(const int &chunkXPosition, const int &chunkYPosition, const int &chunkZPosition);

    uint32_t getHeight(const int &x, const int &y);

    void setHeight(const int &x, const int &y, const uint32_t &height);

    void addBlockFace(glm::vec3 pos, VkUtils::Orientation orientation, const uint32_t &blockID, const glm::vec2 widthHeight);

    void generateHeightMap(const siv::PerlinNoise &terrainNoise);

    void generate(const siv::PerlinNoise &terrainNoise);

    void generateMesh();
};

#endif