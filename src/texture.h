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

void loadTextureData(WGPUQueue queue, WGPUTexture texture,
                     SDL_Surface *textureSurface);

TextureInfo textureCreate(WGPUDevice device, WGPUQueue queue, char *path);

DepthTextureInfo depthTextureCreate(WGPUDevice device,
                                    WGPUTextureFormat depthTextureFormat,
                                    uint32_t windowWidth,
                                    uint32_t windowHeight);

#endif