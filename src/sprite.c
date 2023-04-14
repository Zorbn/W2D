#include "sprite.h"

#include "spriteModel.h"

SpriteBatch spriteBatchCreate(int maxSprites, char *texturePath, Renderer *renderer) {
    // Create the sprite's vertex buffer.
    WGPUBufferDescriptor bufferDescriptor = (WGPUBufferDescriptor){
        .nextInChain = NULL,
        .size = maxSprites * verticesPerSprite * spriteVertexComponents *
                sizeof(float),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
        .mappedAtCreation = false,
    };
    WGPUBuffer vertexBuffer = wgpuDeviceCreateBuffer(renderer->device, &bufferDescriptor);

    // Create the sprite's index buffer.
    bufferDescriptor.size = maxSprites * indicesPerSprite * sizeof(uint32_t);
    bufferDescriptor.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
    WGPUBuffer indexBuffer = wgpuDeviceCreateBuffer(renderer->device, &bufferDescriptor);

    TextureInfo textureInfo = textureCreate(renderer->device, renderer->queue, texturePath);

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
        wgpuDeviceCreateBindGroupLayout(renderer->device, &bindGroupLayoutDescriptor);

    WGPUBindGroupEntry bindings[3] = {
        (WGPUBindGroupEntry){
            .nextInChain = NULL,
            .binding = 0,
            .buffer = renderer->uniformBuffer,
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
        wgpuDeviceCreateBindGroup(renderer->device, &bindGroupDescriptor);

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

void spriteBatchDraw(SpriteBatch *spriteBatch, Renderer *renderer) {
    if (!renderer->hasRenderPass) {
        return;
    }

    if (spriteBatch->spriteCount == 0) {
        return;
    }

    int indexCount = spriteBatch->spriteCount * indicesPerSprite;
    int vertexComponentCount =
        spriteBatch->spriteCount * verticesPerSprite * spriteVertexComponents;

    wgpuQueueWriteBuffer(renderer->queue, spriteBatch->vertexBuffer, 0,
                         spriteBatch->vertexData,
                         vertexComponentCount * sizeof(float));
    wgpuQueueWriteBuffer(renderer->queue, spriteBatch->indexBuffer, 0,
                         spriteBatch->indexData, indexCount * sizeof(uint32_t));

    wgpuRenderPassEncoderSetPipeline(renderer->renderPass, renderer->pipeline);
    wgpuRenderPassEncoderSetVertexBuffer(renderer->renderPass, 0,
                                         spriteBatch->vertexBuffer, 0,
                                         vertexComponentCount * sizeof(float));
    wgpuRenderPassEncoderSetIndexBuffer(renderer->renderPass, spriteBatch->indexBuffer,
                                        WGPUIndexFormat_Uint32, 0,
                                        indexCount * sizeof(uint32_t));
    wgpuRenderPassEncoderSetBindGroup(renderer->renderPass, 0, spriteBatch->bindGroup, 0,
                                      NULL);

    wgpuRenderPassEncoderDrawIndexed(renderer->renderPass, indexCount, 1, 0, 0, 0);
}
