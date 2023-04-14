#include "texture.h"

SDL_Surface *loadSurface(const char *path) {
    SDL_Surface *loadedSurface = IMG_Load(path);

    if (!loadedSurface) {
        printf("Failed to load file at path: %s", path);
        exit(-1);
    }

    SDL_Surface *surface =
        SDL_ConvertSurfaceFormat(loadedSurface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(loadedSurface);

    return surface;
}

void loadTextureData(WGPUQueue queue, WGPUTexture texture,
                     SDL_Surface *textureSurface) {
    WGPUImageCopyTexture destination = {
        .texture = texture,
        .mipLevel = 0,
        .origin = {0, 0, 0},
        .aspect = WGPUTextureAspect_All,
    };
    WGPUTextureDataLayout source = {
        .offset = 0,
        .bytesPerRow = 4 * textureSurface->w,
        .rowsPerImage = textureSurface->h,
    };

    uint8_t *data = (uint8_t *)(textureSurface->pixels);
    WGPUExtent3D size = (WGPUExtent3D){textureSurface->w, textureSurface->h, 1};

    wgpuQueueWriteTexture(queue, &destination, data,
                          4 * textureSurface->w * textureSurface->h, &source,
                          &size);
}

TextureInfo textureCreate(WGPUDevice device, WGPUQueue queue, char *path) {
    SDL_Surface *textureSurface = loadSurface(path);
    int textureWidth = textureSurface->w;
    int textureHeight = textureSurface->h;
    WGPUTextureDescriptor textureDescriptor = {
        .dimension = WGPUTextureDimension_2D,
        .format = WGPUTextureFormat_RGBA8Unorm,
        .mipLevelCount = 1,
        .sampleCount = 1,
        .size = {textureWidth, textureHeight, 1},
        .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
        .viewFormatCount = 0,
        .viewFormats = NULL,
    };
    WGPUTexture texture = wgpuDeviceCreateTexture(device, &textureDescriptor);
    loadTextureData(queue, texture, textureSurface);
    SDL_FreeSurface(textureSurface);

    WGPUTextureViewDescriptor textureViewDescriptor = {
        .aspect = WGPUTextureAspect_All,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .dimension = WGPUTextureViewDimension_2D,
        .format = textureDescriptor.format,
    };
    WGPUTextureView view =
        wgpuTextureCreateView(texture, &textureViewDescriptor);
    WGPUSamplerDescriptor textureSamplerDescriptor = {
        .addressModeU = WGPUAddressMode_ClampToEdge,
        .addressModeV = WGPUAddressMode_ClampToEdge,
        .addressModeW = WGPUAddressMode_ClampToEdge,
        .magFilter = WGPUFilterMode_Nearest,
        .minFilter = WGPUFilterMode_Nearest,
        .mipmapFilter = WGPUFilterMode_Linear,
        .lodMinClamp = 0.0f,
        .lodMaxClamp = 1.0f,
        .compare = WGPUCompareFunction_Undefined,
        .maxAnisotropy = 0,
    };
    WGPUSampler sampler =
        wgpuDeviceCreateSampler(device, &textureSamplerDescriptor);

    return (TextureInfo){
        .texture = texture,
        .view = view,
        .sampler = sampler,
        .width = textureWidth,
        .height = textureHeight,
    };
}

DepthTextureInfo depthTextureCreate(WGPUDevice device,
                                    WGPUTextureFormat depthTextureFormat,
                                    uint32_t windowWidth,
                                    uint32_t windowHeight) {
    WGPUTextureDescriptor depthTextureDescriptor = {
        .dimension = WGPUTextureDimension_2D,
        .format = depthTextureFormat,
        .mipLevelCount = 1,
        .sampleCount = 1,
        .size = {windowWidth, windowHeight, 1},
        .usage = WGPUTextureUsage_RenderAttachment,
        .viewFormatCount = 1,
        .viewFormats = &depthTextureFormat,
    };
    WGPUTexture texture =
        wgpuDeviceCreateTexture(device, &depthTextureDescriptor);
    WGPUTextureViewDescriptor depthTextureViewDescriptor = {
        .aspect = WGPUTextureAspect_DepthOnly,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .dimension = WGPUTextureViewDimension_2D,
        .format = depthTextureFormat,
    };
    WGPUTextureView view =
        wgpuTextureCreateView(texture, &depthTextureViewDescriptor);
    WGPURenderPassDepthStencilAttachment attachment =
        (WGPURenderPassDepthStencilAttachment){
            .view = view,
            .depthClearValue = 1.0f,
            .depthLoadOp = WGPULoadOp_Clear,
            .depthStoreOp = WGPUStoreOp_Store,
            .depthReadOnly = false,
            .stencilClearValue = 0,
            .stencilLoadOp = WGPULoadOp_Clear,
            .stencilStoreOp = WGPUStoreOp_Store,
            .stencilReadOnly = true,
        };

    return (DepthTextureInfo){
        .texture = texture,
        .view = view,
        .attachment = attachment,
        .format = depthTextureFormat,
    };
}