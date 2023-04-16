#include "spriteModel.h"

const float spriteVertexData[] = {
    // X Y Z, R G B A, Blend, TextureX, TextureY
    +0.0f, +0.0f, +0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f,  // Vertex 1
    +1.0f, +0.0f, +0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // Vertex 2
    +1.0f, +1.0f, +0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,  // Vertex 3
    +0.0f, +1.0f, +0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,  // Vertex 4
};

const uint32_t spriteIndexData[] = {
    0, 1, 2, 0, 2, 3,  // Quad
};