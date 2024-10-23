#ifndef CHUNK_H
#define CHUNK_H

#include "VkUtils.h"
#include "PerlinNoise.hpp"


//TEMP
#include <iostream>

struct Chunk {
    std::vector<VkUtils::BlockFaceInstance> blockFaces;

    void addBlockFace(glm::vec3 pos, VkUtils::Orientation orientation);

    void generate(const siv::PerlinNoise &terrainNoise, const int &chunkX, const int &chunkY, const int &chunkZ);
};

#endif