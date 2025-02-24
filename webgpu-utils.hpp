#pragma once

#include <webgpu/webgpu.h>

namespace wgpuUtils{

WGPUAdapter requestAdapter(WGPUInstance instance, const WGPURequestAdapterOptions* options);
WGPUDevice requestDevice(WGPUAdapter adapter, const WGPUDeviceDescriptor* descriptor);

void configureSurface(WGPUSurface surface, WGPUDevice device, WGPUAdapter adapter, int width, int height);

void printAdapterFeatures(WGPUAdapter adapter);
void printAdapterProperties(WGPUAdapter adapter);

} // namespace utils
