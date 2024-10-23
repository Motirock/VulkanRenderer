#include "Chunk.h"

void Chunk::addBlockFace(glm::vec3 pos, VkUtils::Orientation orientation) {
    VkUtils::BlockFaceInstance blockFaceInstance;
    blockFaceInstance.pos = pos;
    blockFaceInstance.orientation = orientation;
    blockFaces.push_back(blockFaceInstance);
}

void Chunk::generate(const siv::PerlinNoise &terrainNoise, const int &chunkX, const int &chunkY, const int &chunkZ) {
    int cubeWidth = 50;

    for (int z = chunkZ; z < chunkZ+cubeWidth; z++) {
        for (int y = chunkY; y < chunkY+cubeWidth; y++) {
            for (int x = chunkX; x < chunkX+cubeWidth; x++) {
                addBlockFace({x*2.0f-cubeWidth, y*2.0f-cubeWidth, z*2.0f-cubeWidth}, VkUtils::POSITIVE_X);
                addBlockFace({x*2.0f-cubeWidth, y*2.0f-cubeWidth, z*2.0f-cubeWidth}, VkUtils::NEGATIVE_X);
                addBlockFace({x*2.0f-cubeWidth, y*2.0f-cubeWidth, z*2.0f-cubeWidth}, VkUtils::POSITIVE_Y);
                addBlockFace({x*2.0f-cubeWidth, y*2.0f-cubeWidth, z*2.0f-cubeWidth}, VkUtils::NEGATIVE_Y);
                addBlockFace({x*2.0f-cubeWidth, y*2.0f-cubeWidth, z*2.0f-cubeWidth}, VkUtils::POSITIVE_Z);
                addBlockFace({x*2.0f-cubeWidth, y*2.0f-cubeWidth, z*2.0f-cubeWidth}, VkUtils::NEGATIVE_Z);
            }
        }
    }
}