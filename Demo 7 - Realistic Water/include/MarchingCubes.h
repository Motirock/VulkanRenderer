#ifndef MARCHING_CUBES_H
#define MARCHING_CUBES_H

//This version of the marching cubes algorithm was adapted from https://github.com/nihaljn/marching-cubes/tree/main

#include "VkUtils.h"

struct GridCell {
    glm::vec3 vertex[8];
    float value[8];
};

struct MarchingCubes {
    //12 edges, basically pairs of vertices (corners)
    static const int edgeToVertices[12][2];
    
    //12 bit mask
    static const int edgeTable[256];

    //Index for triagnleTable based on 
    static int getCubeIndex(const GridCell &cell, const float &isoValue);

    //Interpolates between two points based on the isoValue
    static glm::vec3 interpolateVertices(const glm::vec3 &vertex1, const float &value1, const glm::vec3 &vertex2, const float &value2, const float &isoValue);

    //Get the intersection points of the grid cell
    static std::vector<glm::vec3> getIntersections(const GridCell &cell, const float &isoValue);

    //Get the triangles based on the intersection points
    static std::vector<std::vector<glm::vec3>> getTriangles(const std::vector<glm::vec3> &intersections, const int &cubeIndex);

    //Get the triangles of a single cell. Basically depends on values of vertices of cell and isoValue
    static std::vector<std::vector<glm::vec3>> triangulateCell(const GridCell &cell, const float isoValue);
};

#endif