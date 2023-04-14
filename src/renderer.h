#ifndef RENDERER_H
#define RENDERER_H

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "matrix.h"
#include "texture.h"
#include "wgpuHelper.h"
#include "unused.h"
#include "../webgpu-headers/webgpu.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

typedef struct {
    SDL_Window *window;
    WGPUSwapChainDescriptor config;
    DepthTextureInfo depthTextureInfo;
    WGPUSurface surface;
    WGPUSwapChain swapChain;
    WGPUQueue queue;
    WGPUDevice device;
    WGPUBuffer uniformBuffer;
    WGPURenderPipeline pipeline;

    WGPURenderPassEncoder renderPass;
    WGPUCommandEncoder encoder;
    WGPUTextureView nextTexture;
    bool hasRenderPass;
} Renderer;

Renderer rendererCreate(SDL_Window *window);
void rendererResize(Renderer *renderer);
void rendererBegin(Renderer *renderer, float backgroundR, float backgroundG, float backgroundB);
void rendererEnd(Renderer *renderer);

#endif