#ifndef SPRITE_H
#define SPRITE_H

#include "matrix.h"
#include "renderer.h"
#include "texture.h"

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

    float r;
    float g;
    float b;
    float a;
    float blend;
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

typedef struct {
    TextureWrapMode textureWrapMode;
    TextureFilteringMode textureFilteringMode;
} SpriteBatchOptions;

SpriteBatch spriteBatchCreate(int maxSprites, char *texturePath,
                              Renderer *renderer, SpriteBatchOptions options);

void spriteBatchClear(SpriteBatch *spriteBatch);

void spriteBatchAdd(SpriteBatch *spriteBatch, Sprite sprite);

void spriteBatchDraw(SpriteBatch *spriteBatch, Renderer *renderer);

#endif