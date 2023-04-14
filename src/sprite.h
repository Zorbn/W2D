#ifndef SPRITE_H
#define SPRITE_H

#include "texture.h"
#include "matrix.h"
#include "renderer.h"

typedef struct {
    float x;
    float y;
    float z;

    float width;
    float height;

    float texX;
    float texY;
    float texWidth;
    float texHeight;
} Sprite;

typedef struct {
    int maxSprites;
    int spriteCount;

    float *vertexData;
    uint32_t *indexData;
    WGPUBuffer vertexBuffer;
    WGPUBuffer indexBuffer;
    WGPUBindGroup bindGroup;
    TextureInfo textureInfo;

    float inverseTexWidth;
    float inverseTexHeight;
} SpriteBatch;

SpriteBatch spriteBatchCreate(int maxSprites, char *texturePath, Renderer *renderer);

void spriteBatchClear(SpriteBatch *spriteBatch);

void spriteBatchAdd(SpriteBatch *spriteBatch, Sprite sprite);

void spriteBatchDraw(SpriteBatch *spriteBatch, Renderer *renderer);

#endif