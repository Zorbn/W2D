#include "webgpu-headers/webgpu.h"
#include "wgpu.h"

#include "framework.h"
#include "unused.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#define WGPU_TARGET_MACOS 1
#define WGPU_TARGET_LINUX_X11 2
#define WGPU_TARGET_WINDOWS 3
#define WGPU_TARGET_LINUX_WAYLAND 4

#if WGPU_TARGET == WGPU_TARGET_MACOS
#include <Foundation/Foundation.h>
#include <QuartzCore/CAMetalLayer.h>
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

const float vertexData[] = {
    -0.5f, -0.5f, +0.0f, 1.0f, 0.0f, 0.0f, // Vertex 1
    +0.5f, -0.5f, +0.0f, 0.0f, 1.0f, 0.0f, // Vertex 2
    +0.5f, +0.5f, +0.0f, 0.0f, 0.0f, 1.0f, // Vertex 3
    -0.5f, +0.5f, +0.0f, 0.0f, 1.0f, 1.0f, // Vertex 4

    +0.0f, +0.0f, +1.0f, 1.0f, 0.0f, 0.0f, // Vertex 1
    +1.0f, +0.0f, +1.0f, 0.0f, 1.0f, 0.0f, // Vertex 2
    +1.0f, +1.0f, +1.0f, 0.0f, 0.0f, 1.0f, // Vertex 3
    +0.0f, +1.0f, +1.0f, 0.0f, 1.0f, 1.0f, // Vertex 4
};
const int vertexDataCount = 48;
const int vertexComponents = 6;

const uint32_t indexData[] = {
    0,
    1,
    2,
    0,
    2,
    3,

    4,
    5,
    6,
    4,
    6,
    7,
};
const int indexCount = 12;

WGPUInstance instance = NULL;

static void handle_device_lost(WGPUDeviceLostReason reason, char const *message,
                               void *userdata)
{
    UNUSED(userdata);

    printf("DEVICE LOST (%d): %s\n", reason, message);
}

static void handle_uncaptured_error(WGPUErrorType type, char const *message,
                                    void *userdata)
{
    UNUSED(userdata);

    printf("UNCAPTURED ERROR (%d): %s\n", type, message);
}

typedef struct
{
    WGPUTexture texture;
    WGPUTextureView view;
    WGPURenderPassDepthStencilAttachment attachment;
} TextureInfo;

TextureInfo createDepthTexture(WGPUDevice device, WGPUTextureFormat depthTextureFormat,
                   uint32_t windowWidth, uint32_t windowHeight)
{
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
    WGPUTexture texture = wgpuDeviceCreateTexture(device, &depthTextureDescriptor);
    WGPUTextureViewDescriptor depthTextureViewDescriptor = {
        .aspect = WGPUTextureAspect_DepthOnly,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .dimension = WGPUTextureViewDimension_2D,
        .format = depthTextureFormat,
    };
    WGPUTextureView view = wgpuTextureCreateView(texture, &depthTextureViewDescriptor);
    WGPURenderPassDepthStencilAttachment attachment = (WGPURenderPassDepthStencilAttachment){
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

    return (TextureInfo){
        .texture = texture,
        .view = view,
        .attachment = attachment,
    };
}

int main(int argc, char *argv[])
{
    UNUSED(argc);
    UNUSED(argv);
    initializeLog();

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("Cannot initialize SDL");
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("WGPU C",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          640, 480, SDL_WINDOW_RESIZABLE);

    if (!window)
    {
        printf("Cannot create window");
        return 1;
    }

    instance = wgpuCreateInstance(&(WGPUInstanceDescriptor){.nextInChain = NULL});

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
                                .sType = WGPUSType_SurfaceDescriptorFromMetalLayer,
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
                                .sType = WGPUSType_SurfaceDescriptorFromXlibWindow,
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
                                .sType = WGPUSType_SurfaceDescriptorFromWindowsHWND,
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
    wgpuInstanceRequestAdapter(instance, &adapterOptions, request_adapter_callback,
                               (void *)&adapter);

    printAdapterFeatures(adapter);
    printSurfaceCapabilities(surface, adapter);

    WGPUDevice device;
    wgpuAdapterRequestDevice(adapter, NULL, request_device_callback,
                             (void *)&device);

    wgpuDeviceSetUncapturedErrorCallback(device, handle_uncaptured_error, NULL);
    wgpuDeviceSetDeviceLostCallback(device, handle_device_lost, NULL);

    WGPUShaderModuleDescriptor shaderSource = load_wgsl("shader.wgsl");
    WGPUShaderModule shader = wgpuDeviceCreateShaderModule(device, &shaderSource);

    WGPUTextureFormat swapChainFormat =
        wgpuSurfaceGetPreferredFormat(surface, adapter);

    WGPUVertexAttribute vertexAttributes[2] = {
        (WGPUVertexAttribute){
            .shaderLocation = 0,
            .format = WGPUVertexFormat_Float32x2,
            .offset = 0,
        },
        (WGPUVertexAttribute){
            .shaderLocation = 1,
            .format = WGPUVertexFormat_Float32x3,
            .offset = 2 * sizeof(float),
        }};

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
                    .buffers = &(WGPUVertexBufferLayout){
                        .attributeCount = 2,
                        .arrayStride = vertexComponents * sizeof(float),
                        .stepMode = WGPUVertexStepMode_Vertex,
                        .attributes = vertexAttributes,
                    },
                },
            .primitive = (WGPUPrimitiveState){.topology = WGPUPrimitiveTopology_TriangleList, .stripIndexFormat = WGPUIndexFormat_Undefined, .frontFace = WGPUFrontFace_CCW, .cullMode = WGPUCullMode_None},
            .multisample = (WGPUMultisampleState){
                .count = 1,
                .mask = (uint32_t)(~0),
                .alphaToCoverageEnabled = false,
            },
            .fragment = &(WGPUFragmentState){
                .module = shader,
                .entryPoint = "fs_main",
                .targetCount = 1,
                .targets = &(WGPUColorTargetState){
                    .format = swapChainFormat,
                    .blend = &(WGPUBlendState){.color = (WGPUBlendComponent){
                                                   .srcFactor = WGPUBlendFactor_One,
                                                   .dstFactor = WGPUBlendFactor_Zero,
                                                   .operation = WGPUBlendOperation_Add,
                                               },
                                               .alpha = (WGPUBlendComponent){
                                                   .srcFactor = WGPUBlendFactor_One,
                                                   .dstFactor = WGPUBlendFactor_Zero,
                                                   .operation = WGPUBlendOperation_Add,
                                               }},
                    .writeMask = WGPUColorWriteMask_All,
                },
            },
            .depthStencil = &(WGPUDepthStencilState){
                .depthCompare = WGPUCompareFunction_Less,
                .depthWriteEnabled = true,
                .format = depthTextureFormat,
                .stencilReadMask = 0,
                .stencilWriteMask = 0,
                .stencilFront = (WGPUStencilFaceState){
                    .compare = WGPUCompareFunction_Always,
                    .failOp = WGPUStencilOperation_Keep,
                    .depthFailOp = WGPUStencilOperation_Keep,
                    .passOp = WGPUStencilOperation_Keep,
                },
                .stencilBack = (WGPUStencilFaceState){
                    .compare = WGPUCompareFunction_Always,
                    .failOp = WGPUStencilOperation_Keep,
                    .depthFailOp = WGPUStencilOperation_Keep,
                    .passOp = WGPUStencilOperation_Keep,
                },
            },
        });
    // WGPUTextureDescriptor depthTextureDescriptor = {
    //     .dimension = WGPUTextureDimension_2D,
    //     .format = depthTextureFormat,
    //     .mipLevelCount = 1,
    //     .sampleCount = 1,
    //     .size = {640, 480, 1}, // TODO: Handle resizing by creating a new depth texture.
    //     .usage = WGPUTextureUsage_RenderAttachment,
    //     .viewFormatCount = 1,
    //     .viewFormats = &depthTextureFormat,
    // };
    // WGPUTexture depthTexture = wgpuDeviceCreateTexture(device, &depthTextureDescriptor);

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

    TextureInfo depthTextureInfo = createDepthTexture(device, depthTextureFormat, config.width, config.height);

    WGPUSwapChain swapChain = wgpuDeviceCreateSwapChain(device, surface, &config);
    WGPUQueue queue = wgpuDeviceGetQueue(device);

    // Create the vertex buffer.
    WGPUBufferDescriptor bufferDescriptor = (WGPUBufferDescriptor){
        .nextInChain = NULL,
        .size = vertexDataCount * sizeof(float),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
        .mappedAtCreation = false,
    };
    WGPUBuffer vertexBuffer = wgpuDeviceCreateBuffer(device, &bufferDescriptor);
    wgpuQueueWriteBuffer(queue, vertexBuffer, 0, vertexData, bufferDescriptor.size);

    // Create the index buffer.
    bufferDescriptor.size = indexCount * sizeof(uint32_t);
    bufferDescriptor.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
    WGPUBuffer indexBuffer = wgpuDeviceCreateBuffer(device, &bufferDescriptor);
    wgpuQueueWriteBuffer(queue, indexBuffer, 0, indexData, bufferDescriptor.size);

    // Create the screen ratio uniform buffer.
    bufferDescriptor.size = 2 * sizeof(float);
    bufferDescriptor.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
    WGPUBuffer uniformBuffer = wgpuDeviceCreateBuffer(device, &bufferDescriptor);

    // Create the bind group for the uniform buffer.
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDescriptor = {
        .nextInChain = NULL,
        .entryCount = 1,
        .entries = &(WGPUBindGroupLayoutEntry){
            .binding = 0,
            .visibility = WGPUShaderStage_Vertex,
            .buffer = (WGPUBufferBindingLayout){
                .type = WGPUBufferBindingType_Uniform,
                .minBindingSize = 2 * sizeof(float),
            },
        },
    };
    WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDescriptor);
    WGPUPipelineLayoutDescriptor layoutDescriptor = {
        .nextInChain = NULL,
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &bindGroupLayout,
    };
    /* WGPUPipelineLayout layout = */ wgpuDeviceCreatePipelineLayout(device, &layoutDescriptor);

    WGPUBindGroupEntry binding = {
        .nextInChain = NULL,
        .binding = 0,
        .buffer = uniformBuffer,
        .offset = 0,
        .size = 2 * sizeof(float),
    };
    WGPUBindGroupDescriptor bindGroupDescriptor = {
        .nextInChain = NULL,
        .layout = bindGroupLayout,
        .entryCount = bindGroupLayoutDescriptor.entryCount,
        .entries = &binding,
    };
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDescriptor);

    bool isRunning = true;
    while (isRunning)
    {

        WGPUTextureView nextTexture = NULL;

        for (int attempt = 0; attempt < 2; attempt++)
        {
            uint32_t prevWidth = config.width;
            uint32_t prevHeight = config.height;
            SDL_GetWindowSize(window, (int *)&config.width, (int *)&config.height);

            if (prevWidth != config.width || prevHeight != config.height)
            {
                depthTextureInfo = createDepthTexture(device, depthTextureFormat, config.width, config.height);
                swapChain = wgpuDeviceCreateSwapChain(device, surface, &config);
            }

            nextTexture = wgpuSwapChainGetCurrentTextureView(swapChain);
            if (attempt == 0 && !nextTexture)
            {
                printf("wgpuSwapChainGetCurrentTextureView() failed; trying to create "
                       "a new swap chain...\n");
                config.width = 0;
                config.height = 0;
                continue;
            }

            float screenRatio[2] = {(float)config.width, (float)config.height};
            wgpuQueueWriteBuffer(queue, uniformBuffer, 0, &screenRatio, 2 * sizeof(float));

            break;
        }

        if (!nextTexture)
        {
            printf("Cannot acquire next swap chain texture\n");
            return 1;
        }

        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(
            device, &(WGPUCommandEncoderDescriptor){.label = "Command Encoder"});

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

        wgpuRenderPassEncoderSetPipeline(renderPass, pipeline);
        wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, vertexBuffer, 0, vertexDataCount * sizeof(float));
        wgpuRenderPassEncoderSetIndexBuffer(renderPass, indexBuffer, WGPUIndexFormat_Uint32, 0, indexCount * sizeof(float));
        wgpuRenderPassEncoderSetBindGroup(renderPass, 0, bindGroup, 0, NULL);

        wgpuRenderPassEncoderDrawIndexed(renderPass, indexCount, 1, 0, 0, 0);
        wgpuRenderPassEncoderEnd(renderPass);
        wgpuTextureViewDrop(nextTexture);

        WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(
            encoder, &(WGPUCommandBufferDescriptor){.label = NULL});
        wgpuQueueSubmit(queue, 1, &cmdBuffer);
        wgpuSwapChainPresent(swapChain);

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                isRunning = false;
            }
        }
    }

    SDL_Quit();

    return 0;
}