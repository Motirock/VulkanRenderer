#include "Chunk.h"

Chunk::Chunk(const int &chunkXPosition, const int &chunkYPosition, const int &chunkZPosition)
    : chunkX(chunkXPosition), chunkY(chunkYPosition), chunkZ(chunkZPosition) {

}

uint32_t Chunk::getHeight(const int &x, const int &y) {
    return heightMap[y*CHUNK_SIZE+x];
}

void Chunk::setHeight(const int &x, const int &y, const uint32_t &height) {
    heightMap[y*CHUNK_SIZE+x] = height;
}

void Chunk::addBlockFace(glm::vec3 pos, VkUtils::Orientation orientation, const uint32_t &blockID, const glm::vec2 widthHeight) {
    VkUtils::BlockFaceInstance blockFaceInstance;
    blockFaceInstance.pos = pos;
    blockFaceInstance.orientation = orientation;
    blockFaceInstance.blockID = blockID;
    blockFaceInstance.widthHeight = widthHeight;
    blockFaces.push_back(blockFaceInstance);
}

void Chunk::generateHeightMap(const siv::PerlinNoise &terrainNoise) {
    for (int j = 0; j < CHUNK_SIZE; j++) {
        int y = chunkY*CHUNK_SIZE + j;
        for (int i = 0; i < CHUNK_SIZE; i++) {
            int x = chunkX*CHUNK_SIZE + i;
            heightMap[j*CHUNK_SIZE+i] = (uint32_t) (terrainNoise.octave2D_01((float) x/CHUNK_SIZE*0.5f, (float) y/CHUNK_SIZE*0.5f, 4, 0.7) * CHUNK_SIZE * 0.7);
        }
    }
}

void Chunk::generate(const siv::PerlinNoise &terrainNoise) {
    generateHeightMap(terrainNoise);
}

void Chunk::generateMesh() {
    for (int i = 0; i < CHUNK_SIZE; i++) {
        int x = chunkX*CHUNK_SIZE+i;
        for (int j = 0; j < CHUNK_SIZE; j++) {
            int y = chunkY*CHUNK_SIZE+j;
            uint32_t height = getHeight(i, j);
            for (int k = 0; k < CHUNK_SIZE; k++) {
                int z = chunkZ*CHUNK_SIZE+k;

                if (z > height) {
                    break;
                }

                if (i+1 < CHUNK_SIZE && z > getHeight(i+1, j)) {
                    addBlockFace(glm::vec3(x, y, z), VkUtils::POSITIVE_X, rand()%2+1, glm::vec2(1, 1));
                }
                if (i-1 >= 0 && z > getHeight(i-1, j)) {
                    addBlockFace(glm::vec3(x, y, z), VkUtils::NEGATIVE_X, rand()%2+1, glm::vec2(1, 1));
                }

                if (j+1 < CHUNK_SIZE && z > getHeight(i, j+1)) {
                    addBlockFace(glm::vec3(x, y, z), VkUtils::POSITIVE_Y, rand()%2+1, glm::vec2(1, 1));
                }
                if (j-1 >= 0 && z > getHeight(i, j-1)) {
                    addBlockFace(glm::vec3(x, y, z), VkUtils::NEGATIVE_Y, rand()%2+1, glm::vec2(1, 1));
                }

                if (z+1 > height) {
                    addBlockFace(glm::vec3(x, y, z), VkUtils::POSITIVE_Z, rand()%2+1, glm::vec2(1, 1));
                }
            }
        }
    }

    for (int k = 0; k < CHUNK_SIZE; k++) {
        int z = chunkZ*CHUNK_SIZE+k;
        uint64_t maskArray[CHUNK_SIZE];

        for (int j = 0; j < CHUNK_SIZE; j++) {
            uint64_t &mask = maskArray[j]; 
            int y = chunkY*CHUNK_SIZE+j;

            for (int i = 0; i < CHUNK_SIZE; i++) {
                int x = chunkX*CHUNK_SIZE+i;
                
                if (z+1 < CHUNK_SIZE && getHeight(i, j) > z+1) {
                    mask |= 1 << i;
                }
            }
        }
    }
    

}