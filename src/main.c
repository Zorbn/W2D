#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "framework.h"
#include "unused.h"
#include "../webgpu-headers/webgpu.h"
#include "../wgpu.h"

#define WGPU_TARGET_MACOS 1
#define WGPU_TARGET_LINUX_X11 2
#define WGPU_TARGET_WINDOWS 3
#define WGPU_TARGET_LINUX_WAYLAND 4

#if WGPU_TARGET == WGPU_TARGET_MACOS
#include <Foundation/Foundation.h>
#include <QuartzCore/CAMetalLayer.h>
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_syswm.h>

// TODO: This shouldn't be in main.
SDL_Surface *LoadSurface(const char *path) {
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

WGPUInstance instance = NULL;

static void handle_device_lost(WGPUDeviceLostReason reason, char const *message,
                               void *userdata) {
    UNUSED(userdata);

    printf("DEVICE LOST (%d): %s\n", reason, message);
}

static void handle_uncaptured_error(WGPUErrorType type, char const *message,
                                    void *userdata) {
    UNUSED(userdata);

    printf("UNCAPTURED ERROR (%d): %s\n", type, message);
}

typedef struct {
    WGPUTexture texture;
    WGPUTextureView view;
    WGPURenderPassDepthStencilAttachment attachment;
} DepthTextureInfo;

typedef struct {
    WGPUTexture texture;
    WGPUTextureView view;
    WGPUSampler sampler;
    int width;
    int height;
} TextureInfo;

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

TextureInfo createTexture(WGPUDevice device, WGPUQueue queue, char *path) {
    SDL_Surface *textureSurface = LoadSurface(path);
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

DepthTextureInfo createDepthTexture(WGPUDevice device,
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
    };
}

#define maxZDistance 1000
#define matrix4Components 16

void orthographicProjection(float matrix[matrix4Components], float left,
                            float right, float bottom, float top, float zNear,
                            float zFar) {
    matrix[0] = 2.0f / (right - left);
    matrix[1] = 0;
    matrix[2] = 0;
    matrix[3] = 0;

    matrix[4] = 0;
    matrix[5] = 2.0f / (top - bottom);
    matrix[6] = 0;
    matrix[7] = 0;

    matrix[8] = 0;
    matrix[9] = 0;
    matrix[10] = 1.0f / (zNear - zFar);
    matrix[11] = 0;

    matrix[12] = (right + left) / (left - right);
    matrix[13] = (top + bottom) / (bottom - top);
    matrix[14] = zNear / (zNear - zFar);
    matrix[15] = 1.0f;
}

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

const float spriteVertexData[] = {
    // X Y Z, R G B, TextureX, TextureY
    +0.0f, +0.0f, +0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f,  // Vertex 1
    +1.0f, +0.0f, +0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // Vertex 2
    +1.0f, +1.0f, +0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,  // Vertex 3
    +0.0f, +1.0f, +0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,  // Vertex 4
};

const uint32_t spriteIndexData[] = {
    0, 1, 2, 0, 2, 3,  // Quad
};

const int spriteVertexComponents = 8;
const int verticesPerSprite = 4;
const int indicesPerSprite = 6;

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

SpriteBatch spriteBatchCreate(int maxSprites, char *texturePath,
                              WGPUDevice device, WGPUQueue queue,
                              WGPUBuffer uniformBuffer) {
    // Create the sprite's vertex buffer.
    WGPUBufferDescriptor bufferDescriptor = (WGPUBufferDescriptor){
        .nextInChain = NULL,
        .size = maxSprites * verticesPerSprite * spriteVertexComponents *
                sizeof(float),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
        .mappedAtCreation = false,
    };
    WGPUBuffer vertexBuffer = wgpuDeviceCreateBuffer(device, &bufferDescriptor);

    // Create the sprite's index buffer.
    bufferDescriptor.size = maxSprites * indicesPerSprite * sizeof(uint32_t);
    bufferDescriptor.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
    WGPUBuffer indexBuffer = wgpuDeviceCreateBuffer(device, &bufferDescriptor);

    TextureInfo textureInfo = createTexture(device, queue, texturePath);

    // Create the sprite's bind group:
    WGPUBindGroupLayoutEntry bindGroupLayoutEntries[3] = {
        (WGPUBindGroupLayoutEntry){
            .binding = 0,
            .visibility = WGPUShaderStage_Vertex,
            .buffer =
                (WGPUBufferBindingLayout){
                    .type = WGPUBufferBindingType_Uniform,
                    .minBindingSize = matrix4Components * sizeof(float),
                },
        },
        (WGPUBindGroupLayoutEntry){
            .binding = 1,
            .visibility = WGPUShaderStage_Fragment,
            .texture =
                (WGPUTextureBindingLayout){
                    .sampleType = WGPUTextureSampleType_Float,
                    .viewDimension = WGPUTextureViewDimension_2D,
                },
        },
        (WGPUBindGroupLayoutEntry){
            .binding = 2,
            .visibility = WGPUShaderStage_Fragment,
            .sampler =
                (WGPUSamplerBindingLayout){
                    .type = WGPUSamplerBindingType_Filtering,
                },
        },
    };

    WGPUBindGroupLayoutDescriptor bindGroupLayoutDescriptor = {
        .nextInChain = NULL,
        .entryCount = 3,
        .entries = bindGroupLayoutEntries,
    };
    WGPUBindGroupLayout bindGroupLayout =
        wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDescriptor);

    WGPUBindGroupEntry bindings[3] = {
        (WGPUBindGroupEntry){
            .nextInChain = NULL,
            .binding = 0,
            .buffer = uniformBuffer,
            .offset = 0,
            .size = matrix4Components * sizeof(float),
        },
        (WGPUBindGroupEntry){
            .nextInChain = NULL,
            .binding = 1,
            .textureView = textureInfo.view,
        },
        (WGPUBindGroupEntry){
            .nextInChain = NULL,
            .binding = 2,
            .sampler = textureInfo.sampler,
        },
    };
    WGPUBindGroupDescriptor bindGroupDescriptor = {
        .nextInChain = NULL,
        .layout = bindGroupLayout,
        .entryCount = bindGroupLayoutDescriptor.entryCount,
        .entries = bindings,
    };
    WGPUBindGroup bindGroup =
        wgpuDeviceCreateBindGroup(device, &bindGroupDescriptor);

    return (SpriteBatch){
        .maxSprites = maxSprites,
        .spriteCount = 0,
        .vertexData =
            calloc(maxSprites * verticesPerSprite * spriteVertexComponents,
                   sizeof(float)),
        .indexData = calloc(maxSprites * indicesPerSprite, sizeof(uint32_t)),
        .vertexBuffer = vertexBuffer,
        .indexBuffer = indexBuffer,
        .bindGroup = bindGroup,
        .textureInfo = textureInfo,
        .inverseTexWidth = 1.0f / textureInfo.width,
        .inverseTexHeight = 1.0f / textureInfo.height,
    };
}

void spriteBatchClear(SpriteBatch *spriteBatch) {
    spriteBatch->spriteCount = 0;
}

void spriteBatchAdd(SpriteBatch *spriteBatch, Sprite sprite) {
    if (spriteBatch->spriteCount >= spriteBatch->maxSprites) {
        return;
    }

    int spriteI = spriteBatch->spriteCount;
    ++spriteBatch->spriteCount;
    int vertexI = spriteI * verticesPerSprite;
    int vertexComponentI = vertexI * spriteVertexComponents;
    int indexI = spriteI * indicesPerSprite;

    // Convert the sprite's texture coordinates from pixel values to 0-1 floats.
    float normalTextureWidth = sprite.texWidth * spriteBatch->inverseTexWidth;
    float normalTextureHeight =
        sprite.texHeight * spriteBatch->inverseTexHeight;
    float normalTextureX = sprite.texX * spriteBatch->inverseTexWidth;
    float normalTextureY = sprite.texY * spriteBatch->inverseTexHeight;

    for (int i = 0; i < verticesPerSprite; ++i) {
        int componentI = i * spriteVertexComponents;
        int vertexDataI = vertexComponentI + componentI;
        spriteBatch->vertexData[vertexDataI + 0] =
            spriteVertexData[componentI + 0] * sprite.width + sprite.x;
        spriteBatch->vertexData[vertexDataI + 1] =
            spriteVertexData[componentI + 1] * sprite.height + sprite.y;
        spriteBatch->vertexData[vertexDataI + 2] =
            spriteVertexData[componentI + 2] + sprite.z;
        spriteBatch->vertexData[vertexDataI + 3] =
            spriteVertexData[componentI + 3];
        spriteBatch->vertexData[vertexDataI + 4] =
            spriteVertexData[componentI + 4];
        spriteBatch->vertexData[vertexDataI + 5] =
            spriteVertexData[componentI + 5];
        spriteBatch->vertexData[vertexDataI + 6] =
            spriteVertexData[componentI + 6] * normalTextureWidth +
            normalTextureX;
        spriteBatch->vertexData[vertexDataI + 7] =
            spriteVertexData[componentI + 7] * normalTextureHeight +
            normalTextureY;
    }

    for (int i = 0; i < indicesPerSprite; ++i) {
        spriteBatch->indexData[indexI + i] = spriteIndexData[i] + vertexI;
    }
}

void spriteBatchDraw(SpriteBatch *spriteBatch, WGPUQueue queue,
                     WGPURenderPassEncoder renderPass,
                     WGPURenderPipeline pipeline) {
    if (spriteBatch->spriteCount == 0) {
        return;
    }

    int indexCount = spriteBatch->spriteCount * indicesPerSprite;
    int vertexComponentCount =
        spriteBatch->spriteCount * verticesPerSprite * spriteVertexComponents;

    wgpuQueueWriteBuffer(queue, spriteBatch->vertexBuffer, 0,
                         spriteBatch->vertexData,
                         vertexComponentCount * sizeof(float));
    wgpuQueueWriteBuffer(queue, spriteBatch->indexBuffer, 0,
                         spriteBatch->indexData, indexCount * sizeof(uint32_t));

    wgpuRenderPassEncoderSetPipeline(renderPass, pipeline);
    wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0,
                                         spriteBatch->vertexBuffer, 0,
                                         vertexComponentCount * sizeof(float));
    wgpuRenderPassEncoderSetIndexBuffer(renderPass, spriteBatch->indexBuffer,
                                        WGPUIndexFormat_Uint32, 0,
                                        indexCount * sizeof(uint32_t));
    wgpuRenderPassEncoderSetBindGroup(renderPass, 0, spriteBatch->bindGroup, 0,
                                      NULL);

    wgpuRenderPassEncoderDrawIndexed(renderPass, indexCount, 1, 0, 0, 0);
}

int main(int argc, char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);
    initializeLog();

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Cannot initialize SDL");
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("WGPU C", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 640, 480,
                                          SDL_WINDOW_RESIZABLE);

    if (!window) {
        printf("Cannot create window");
        return 1;
    }

    instance =
        wgpuCreateInstance(&(WGPUInstanceDescriptor){.nextInChain = NULL});

    WGPUSurface surface;

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);

#if WGPU_TARGET == WGPU_TARGET_MACOS
    {
        id metal_layer = NULL;
        NSWindow *ns_window = wmInfo.info.cocoa.window;
        [ns_window.contentView setWantsLayer:YES];
        metal_layer = [CAMetalLayer layer];
        [ns_window.contentView setLayer:metal_layer];
        surface = wgpuInstanceCreateSurface(
            instance,
            &(WGPUSurfaceDescriptor){
                .label = NULL,
                .nextInChain =
                    (const WGPUChainedStruct *)&(
                        WGPUSurfaceDescriptorFromMetalLayer){
                        .chain =
                            (WGPUChainedStruct){
                                .next = NULL,
                                .sType =
                                    WGPUSType_SurfaceDescriptorFromMetalLayer,
                            },
                        .layer = metal_layer,
                    },
            });
    }
#elif WGPU_TARGET == WGPU_TARGET_LINUX_X11
    {
        Display *x11_display = wmInfo.info.x11.display;
        Window x11_window = wmInfo.info.x11.surface;
        surface = wgpuInstanceCreateSurface(
            instance,
            &(WGPUSurfaceDescriptor){
                .label = NULL,
                .nextInChain =
                    (const WGPUChainedStruct *)&(
                        WGPUSurfaceDescriptorFromXlibWindow){
                        .chain =
                            (WGPUChainedStruct){
                                .next = NULL,
                                .sType =
                                    WGPUSType_SurfaceDescriptorFromXlibWindow,
                            },
                        .display = x11_display,
                        .window = x11_window,
                    },
            });
    }
#elif WGPU_TARGET == WGPU_TARGET_LINUX_WAYLAND
    {
        struct wl_display *wayland_display = wmInfo.info.wl.display;
        struct wl_surface *wayland_surface = wmInfo.info.wl.surface;
        surface = wgpuInstanceCreateSurface(
            instance,
            &(WGPUSurfaceDescriptor){
                .label = NULL,
                .nextInChain =
                    (const WGPUChainedStruct *)&(
                        WGPUSurfaceDescriptorFromWaylandSurface){
                        .chain =
                            (WGPUChainedStruct){
                                .next = NULL,
                                .sType =
                                    WGPUSType_SurfaceDescriptorFromWaylandSurface,
                            },
                        .display = wayland_display,
                        .surface = wayland_surface,
                    },
            });
    }
#elif WGPU_TARGET == WGPU_TARGET_WINDOWS
    {
        HWND hwnd = wmInfo.info.win.window;
        HINSTANCE hinstance = GetModuleHandle(NULL);
        surface = wgpuInstanceCreateSurface(
            instance,
            &(WGPUSurfaceDescriptor){
                .label = NULL,
                .nextInChain =
                    (const WGPUChainedStruct *)&(
                        WGPUSurfaceDescriptorFromWindowsHWND){
                        .chain =
                            (WGPUChainedStruct){
                                .next = NULL,
                                .sType =
                                    WGPUSType_SurfaceDescriptorFromWindowsHWND,
                            },
                        .hinstance = hinstance,
                        .hwnd = hwnd,
                    },
            });
    }
#else
#error "Unsupported WGPU_TARGET"
#endif

    WGPURequestAdapterOptions adapterOptions = {0};
    adapterOptions.compatibleSurface = surface;
    WGPUAdapter adapter;
    wgpuInstanceRequestAdapter(instance, &adapterOptions,
                               request_adapter_callback, (void *)&adapter);

    printAdapterFeatures(adapter);
    printSurfaceCapabilities(surface, adapter);

    WGPUDevice device;
    wgpuAdapterRequestDevice(adapter, NULL, request_device_callback,
                             (void *)&device);

    wgpuDeviceSetUncapturedErrorCallback(device, handle_uncaptured_error, NULL);
    wgpuDeviceSetDeviceLostCallback(device, handle_device_lost, NULL);

    WGPUShaderModuleDescriptor shaderSource = load_wgsl("shader.wgsl");
    WGPUShaderModule shader =
        wgpuDeviceCreateShaderModule(device, &shaderSource);

    WGPUTextureFormat swapChainFormat =
        wgpuSurfaceGetPreferredFormat(surface, adapter);

    WGPUVertexAttribute vertexAttributes[3] = {
        (WGPUVertexAttribute){
            .shaderLocation = 0,
            .format = WGPUVertexFormat_Float32x3,
            .offset = 0,
        },
        (WGPUVertexAttribute){
            .shaderLocation = 1,
            .format = WGPUVertexFormat_Float32x3,
            .offset = 3 * sizeof(float),
        },
        (WGPUVertexAttribute){
            .shaderLocation = 2,
            .format = WGPUVertexFormat_Float32x2,
            .offset = 6 * sizeof(float),
        },
    };

    WGPUTextureFormat depthTextureFormat = WGPUTextureFormat_Depth24Plus;
    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(
        device,
        &(WGPURenderPipelineDescriptor){
            .label = "Render pipeline",
            .vertex =
                (WGPUVertexState){
                    .module = shader,
                    .entryPoint = "vs_main",
                    .bufferCount = 1,
                    .buffers =
                        &(WGPUVertexBufferLayout){
                            .attributeCount = 3,
                            .arrayStride =
                                spriteVertexComponents * sizeof(float),
                            .stepMode = WGPUVertexStepMode_Vertex,
                            .attributes = vertexAttributes,
                        },
                },
            .primitive =
                (WGPUPrimitiveState){
                    .topology = WGPUPrimitiveTopology_TriangleList,
                    .stripIndexFormat = WGPUIndexFormat_Undefined,
                    .frontFace = WGPUFrontFace_CCW,
                    .cullMode = WGPUCullMode_None},
            .multisample =
                (WGPUMultisampleState){
                    .count = 1,
                    .mask = (uint32_t)(~0),
                    .alphaToCoverageEnabled = false,
                },
            .fragment =
                &(WGPUFragmentState){
                    .module = shader,
                    .entryPoint = "fs_main",
                    .targetCount = 1,
                    .targets =
                        &(WGPUColorTargetState){
                            .format = swapChainFormat,
                            .blend =
                                &(WGPUBlendState){
                                    .color =
                                        (WGPUBlendComponent){
                                            .srcFactor = WGPUBlendFactor_One,
                                            .dstFactor = WGPUBlendFactor_Zero,
                                            .operation = WGPUBlendOperation_Add,
                                        },
                                    .alpha =
                                        (WGPUBlendComponent){
                                            .srcFactor = WGPUBlendFactor_One,
                                            .dstFactor = WGPUBlendFactor_Zero,
                                            .operation = WGPUBlendOperation_Add,
                                        }},
                            .writeMask = WGPUColorWriteMask_All,
                        },
                },
            .depthStencil =
                &(WGPUDepthStencilState){
                    .depthCompare = WGPUCompareFunction_Less,
                    .depthWriteEnabled = true,
                    .format = depthTextureFormat,
                    .stencilReadMask = 0,
                    .stencilWriteMask = 0,
                    .stencilFront =
                        (WGPUStencilFaceState){
                            .compare = WGPUCompareFunction_Always,
                            .failOp = WGPUStencilOperation_Keep,
                            .depthFailOp = WGPUStencilOperation_Keep,
                            .passOp = WGPUStencilOperation_Keep,
                        },
                    .stencilBack =
                        (WGPUStencilFaceState){
                            .compare = WGPUCompareFunction_Always,
                            .failOp = WGPUStencilOperation_Keep,
                            .depthFailOp = WGPUStencilOperation_Keep,
                            .passOp = WGPUStencilOperation_Keep,
                        },
                },
        });

    WGPUSwapChainDescriptor config = (WGPUSwapChainDescriptor){
        .nextInChain =
            (const WGPUChainedStruct *)&(WGPUSwapChainDescriptorExtras){
                .chain =
                    (WGPUChainedStruct){
                        .next = NULL,
                        .sType = (WGPUSType)WGPUSType_SwapChainDescriptorExtras,
                    },
                .alphaMode = WGPUCompositeAlphaMode_Auto,
                .viewFormatCount = 0,
                .viewFormats = NULL,
            },
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = swapChainFormat,
        .width = 0,
        .height = 0,
        .presentMode = WGPUPresentMode_Fifo,
    };

    SDL_GetWindowSize(window, (int *)&config.width, (int *)&config.height);

    DepthTextureInfo depthTextureInfo = createDepthTexture(
        device, depthTextureFormat, config.width, config.height);

    WGPUSwapChain swapChain =
        wgpuDeviceCreateSwapChain(device, surface, &config);
    WGPUQueue queue = wgpuDeviceGetQueue(device);

    // Create the projection uniform buffer.
    WGPUBufferDescriptor bufferDescriptor = (WGPUBufferDescriptor){
        .nextInChain = NULL,
        .size = matrix4Components * sizeof(float),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
        .mappedAtCreation = false,
    };
    WGPUBuffer uniformBuffer =
        wgpuDeviceCreateBuffer(device, &bufferDescriptor);

    SpriteBatch spriteBatch =
        spriteBatchCreate(10, "test.png", device, queue, uniformBuffer);
    spriteBatchAdd(&spriteBatch, (Sprite){
                                     .x = 8.0f,
                                     .y = 8.0f,
                                     .z = -1.0f,
                                     .width = 160.0f,
                                     .height = 160.0f,
                                     .texWidth = 16.0f,
                                     .texHeight = 16.0f,
                                 });
    spriteBatchAdd(&spriteBatch, (Sprite){
                                     .x = 0.0f,
                                     .y = 0.0f,
                                     .z = 0.0f,
                                     .width = 64.0f,
                                     .height = 64.0f,
                                     .texX = 8.0f,
                                     .texY = 8.0f,
                                     .texWidth = 8.0f,
                                     .texHeight = 8.0f,
                                 });
    // spriteBatchAdd(&spriteBatch, (Sprite){
    //                                  .x = 16.0f,
    //                                  .y = 16.0f,
    //                                  .z = 1.0f,
    //                                  .width = 16.0f,
    //                                  .height = 16.0f,
    //                              });

    bool isRunning = true;
    while (isRunning) {
        WGPUTextureView nextTexture = NULL;

        for (int attempt = 0; attempt < 2; attempt++) {
            uint32_t prevWidth = config.width;
            uint32_t prevHeight = config.height;
            SDL_GetWindowSize(window, (int *)&config.width,
                              (int *)&config.height);

            if (prevWidth != config.width || prevHeight != config.height) {
                depthTextureInfo = createDepthTexture(
                    device, depthTextureFormat, config.width, config.height);
                swapChain = wgpuDeviceCreateSwapChain(device, surface, &config);
            }

            nextTexture = wgpuSwapChainGetCurrentTextureView(swapChain);
            if (attempt == 0 && !nextTexture) {
                printf(
                    "wgpuSwapChainGetCurrentTextureView() failed; trying to "
                    "create "
                    "a new swap chain...\n");
                config.width = 0;
                config.height = 0;
                continue;
            }

            // Resize projection matrix to match window.
            float projectionMatrix[matrix4Components];
            orthographicProjection(projectionMatrix, 0.0f, (float)config.width,
                                   0.0f, (float)config.height, -maxZDistance,
                                   maxZDistance);
            wgpuQueueWriteBuffer(queue, uniformBuffer, 0, &projectionMatrix,
                                 matrix4Components * sizeof(float));

            break;
        }

        if (!nextTexture) {
            printf("Cannot acquire next swap chain texture\n");
            return 1;
        }

        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(
            device,
            &(WGPUCommandEncoderDescriptor){.label = "Command Encoder"});

        WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(
            encoder, &(WGPURenderPassDescriptor){
                         .colorAttachments =
                             &(WGPURenderPassColorAttachment){
                                 .view = nextTexture,
                                 .resolveTarget = NULL,
                                 .loadOp = WGPULoadOp_Clear,
                                 .storeOp = WGPUStoreOp_Store,
                                 .clearValue =
                                     (WGPUColor){
                                         .r = 0.05,
                                         .g = 0.05,
                                         .b = 0.05,
                                         .a = 1.0,
                                     },
                             },
                         .colorAttachmentCount = 1,
                         .depthStencilAttachment = &depthTextureInfo.attachment,
                     });

        spriteBatchDraw(&spriteBatch, queue, renderPass, pipeline);

        wgpuRenderPassEncoderEnd(renderPass);
        wgpuTextureViewDrop(nextTexture);

        WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(
            encoder, &(WGPUCommandBufferDescriptor){.label = NULL});
        wgpuQueueSubmit(queue, 1, &cmdBuffer);
        wgpuSwapChainPresent(swapChain);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                isRunning = false;
            }
        }
    }

    SDL_Quit();

    return 0;
}