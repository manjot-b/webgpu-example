#pragma once

#include <webgpu/webgpu.h>

namespace wgpuUtils{

WGPUAdapter requestAdapter(WGPUInstance instance, const WGPURequestAdapterOptions* options);
WGPUDevice requestDevice(WGPUAdapter adapter, const WGPUDeviceDescriptor* descriptor);

WGPUSwapChain createSwapChain(WGPUDevice device, WGPUSurface, WGPUAdapter adapter, int width, int height);

void printAdapterFeatures(WGPUAdapter adapter);
void printAdapterProperties(WGPUAdapter adapter);

} // namespace utils
