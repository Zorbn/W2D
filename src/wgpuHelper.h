#ifndef FRAMEWORK_H
#define FRAMEWORK_H

#include "../webgpu-headers/webgpu.h"
#include "../wgpu.h"

WGPUShaderModuleDescriptor loadWgsl(const char *name);

void requestAdapterCallback(WGPURequestAdapterStatus status,
                              WGPUAdapter received, const char *message,
                              void *userdata);

void requestDeviceCallback(WGPURequestDeviceStatus status,
                             WGPUDevice received, const char *message,
                             void *userdata);

void readBufferMap(WGPUBufferMapAsyncStatus status, void *userdata);

void initializeLog(void);

void printGlobalReport(WGPUGlobalReport report);
void printAdapterFeatures(WGPUAdapter adapter);
void printSurfaceCapabilities(WGPUSurface surface, WGPUAdapter adapter);

#endif