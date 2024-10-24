#ifndef MARCHING_CUBES_H
#define MARCHING_CUBES_H

#include "VkUtils.h"

struct MarchingCubes {
    static glm::vec3 interpolateVertex(const glm::vec3 &p1, const glm::vec3 &p2, float v1, float v2, float isoLevel);
    static std::vector<glm::vec3> polygonize(const float *data, float isoLevel);

};

#endif