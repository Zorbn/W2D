#include "renderer.h"

#include "spriteModel.h"

#define WGPU_TARGET_MACOS 1
#define WGPU_TARGET_LINUX_X11 2
#define WGPU_TARGET_WINDOWS 3
#define WGPU_TARGET_LINUX_WAYLAND 4

#if WGPU_TARGET == WGPU_TARGET_MACOS
#include <Foundation/Foundation.h>
#include <QuartzCore/CAMetalLayer.h>
#endif

#include <SDL2/SDL_syswm.h>

#define maxZDistance 1000

static void handleDeviceLost(WGPUDeviceLostReason reason, char const *message,
                             void *userdata) {
    UNUSED(userdata);

    printf("DEVICE LOST (%d): %s\n", reason, message);
}

static void handleUncapturedError(WGPUErrorType type, char const *message,
                                  void *userdata) {
    UNUSED(userdata);

    printf("UNCAPTURED ERROR (%d): %s\n", type, message);
}

Renderer rendererCreate(SDL_Window *window) {
    initializeLog();

    Renderer renderer = (Renderer){
        .window = window,
    };

    WGPUInstance instance =
        wgpuCreateInstance(&(WGPUInstanceDescriptor){.nextInChain = NULL});

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
        renderer.surface = wgpuInstanceCreateSurface(
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
        renderer.surface = wgpuInstanceCreateSurface(
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
        renderer.surface = wgpuInstanceCreateSurface(
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
        renderer.surface = wgpuInstanceCreateSurface(
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
    adapterOptions.compatibleSurface = renderer.surface;
    WGPUAdapter adapter;
    wgpuInstanceRequestAdapter(instance, &adapterOptions,
                               requestAdapterCallback, (void *)&adapter);

    printAdapterFeatures(adapter);
    printSurfaceCapabilities(renderer.surface, adapter);

    wgpuAdapterRequestDevice(adapter, NULL, requestDeviceCallback,
                             (void *)&renderer.device);

    wgpuDeviceSetUncapturedErrorCallback(renderer.device, handleUncapturedError,
                                         NULL);
    wgpuDeviceSetDeviceLostCallback(renderer.device, handleDeviceLost, NULL);

    WGPUShaderModuleDescriptor shaderSource = loadWgsl("shader.wgsl");
    WGPUShaderModule shader =
        wgpuDeviceCreateShaderModule(renderer.device, &shaderSource);

    WGPUTextureFormat swapChainFormat =
        wgpuSurfaceGetPreferredFormat(renderer.surface, adapter);

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
    renderer.pipeline = wgpuDeviceCreateRenderPipeline(
        renderer.device,
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

    renderer.config = (WGPUSwapChainDescriptor){
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = swapChainFormat,
        .width = 0,
        .height = 0,
        .presentMode = WGPUPresentMode_Fifo,
    };

    SDL_GetWindowSize(window, (int *)&renderer.config.width,
                      (int *)&renderer.config.height);

    renderer.depthTextureInfo =
        depthTextureCreate(renderer.device, depthTextureFormat,
                           renderer.config.width, renderer.config.height);

    renderer.swapChain = wgpuDeviceCreateSwapChain(
        renderer.device, renderer.surface, &renderer.config);
    renderer.queue = wgpuDeviceGetQueue(renderer.device);

    // Create the projection uniform buffer.
    WGPUBufferDescriptor bufferDescriptor = (WGPUBufferDescriptor){
        .nextInChain = NULL,
        .size = matrix4Components * sizeof(float),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
        .mappedAtCreation = false,
    };
    renderer.uniformBuffer =
        wgpuDeviceCreateBuffer(renderer.device, &bufferDescriptor);

    rendererResize(&renderer);

    return renderer;
}

void rendererResize(Renderer *renderer) {
    // Resize projection matrix to match window.
    float projectionMatrix[matrix4Components];
    orthographicProjection(
        projectionMatrix, 0.0f, (float)renderer->config.width, 0.0f,
        (float)renderer->config.height, -maxZDistance, maxZDistance);
    wgpuQueueWriteBuffer(renderer->queue, renderer->uniformBuffer, 0,
                         &projectionMatrix, matrix4Components * sizeof(float));
}

void rendererBegin(Renderer *renderer, float backgroundR, float backgroundG,
                   float backgroundB) {
    if (renderer->hasRenderPass) {
        return;
    }

    renderer->hasRenderPass = true;

    renderer->nextTexture = NULL;

    for (int attempt = 0; attempt < 2; attempt++) {
        uint32_t prevWidth = renderer->config.width;
        uint32_t prevHeight = renderer->config.height;

        SDL_GetWindowSize(renderer->window, (int *)&renderer->config.width,
                          (int *)&renderer->config.height);

        // It's invalid to create a swapchain of size 0, so just wait instead.
        // This could happen either when the window is actually size (0, 0) or
        // when the window is minimized.
        while (renderer->config.width == 0 || renderer->config.height == 0 ||
               (SDL_GetWindowFlags(renderer->window) & SDL_WINDOW_MINIMIZED)) {

            SDL_GetWindowSize(renderer->window, (int *)&renderer->config.width,
                              (int *)&renderer->config.height);
            SDL_WaitEvent(NULL);
        }

        if (prevWidth != renderer->config.width ||
            prevHeight != renderer->config.height) {
            // Resize the window.
            renderer->swapChain = wgpuDeviceCreateSwapChain(
                renderer->device, renderer->surface, &renderer->config);
            renderer->depthTextureInfo = depthTextureCreate(
                renderer->device, renderer->depthTextureInfo.format,
                renderer->config.width, renderer->config.height);

            rendererResize(renderer);
        }

        renderer->nextTexture =
            wgpuSwapChainGetCurrentTextureView(renderer->swapChain);
        if (attempt == 0 && !renderer->nextTexture) {
            printf(
                "wgpuSwapChainGetCurrentTextureView() failed! Trying to "
                "create a new swap chain...\n");
            renderer->config.width = 0;
            renderer->config.height = 0;
            continue;
        }

        break;
    }

    if (!renderer->nextTexture) {
        printf("Cannot acquire next swap chain texture!\n");
        exit(-1);
    }

    renderer->encoder = wgpuDeviceCreateCommandEncoder(
        renderer->device,
        &(WGPUCommandEncoderDescriptor){.label = "Command Encoder"});

    renderer->renderPass = wgpuCommandEncoderBeginRenderPass(
        renderer->encoder,
        &(WGPURenderPassDescriptor){
            .colorAttachments =
                &(WGPURenderPassColorAttachment){
                    .view = renderer->nextTexture,
                    .resolveTarget = NULL,
                    .loadOp = WGPULoadOp_Clear,
                    .storeOp = WGPUStoreOp_Store,
                    .clearValue =
                        (WGPUColor){
                            .r = backgroundR,
                            .g = backgroundG,
                            .b = backgroundB,
                            .a = 1.0,
                        },
                },
            .colorAttachmentCount = 1,
            .depthStencilAttachment = &renderer->depthTextureInfo.attachment,
        });
}

void rendererEnd(Renderer *renderer) {
    if (!renderer->hasRenderPass) {
        return;
    }

    renderer->hasRenderPass = false;

    wgpuRenderPassEncoderEnd(renderer->renderPass);
    wgpuTextureViewDrop(renderer->nextTexture);

    WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(
        renderer->encoder, &(WGPUCommandBufferDescriptor){.label = NULL});
    wgpuQueueSubmit(renderer->queue, 1, &cmdBuffer);
    wgpuSwapChainPresent(renderer->swapChain);
}