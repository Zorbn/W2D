#ifndef TEXTURE_H
#define TEXTURE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include "../webgpu-headers/webgpu.h"

SDL_Surface *loadSurface(const char *path);

typedef struct {
    WGPUTexture texture;
    WGPUTextureView view;
    WGPURenderPassDepthStencilAttachment attachment;
    WGPUTextureFormat format;
} DepthTextureInfo;

typedef struct {
    WGPUTexture texture;
    WGPUTextureView view;
    WGPUSampler sampler;
    int width;
    int height;
} TextureInfo;

typedef enum {
    TextureWrapModeRepeat,
    TextureWrapModeClamp,
} TextureWrapMode;

typedef enum {
    TextureFilteringModeLinear,
    TextureFilteringModeNearest,
} TextureFilteringMode;

void loadTextureData(WGPUQueue queue, WGPUTexture texture,
                     SDL_Surface *textureSurface);

TextureInfo textureCreate(WGPUDevice device, WGPUQueue queue, char *path,
                          TextureWrapMode wrapMode,
                          TextureFilteringMode filteringMode);

DepthTextureInfo depthTextureCreate(WGPUDevice device,
                                    WGPUTextureFormat depthTextureFormat,
                                    uint32_t windowWidth,
                                    uint32_t windowHeight);

#endif