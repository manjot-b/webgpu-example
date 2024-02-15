#pragma once

#include <webgpu/webgpu.h>

namespace wgpuUtils{

WGPUAdapter requestAdapter(WGPUInstance instance, const WGPURequestAdapterOptions* options);
WGPUDevice requestDevice(WGPUAdapter adapter, const WGPUDeviceDescriptor* descriptor);

void printAdapterFeatures(WGPUAdapter adapter);

} // namespace utils
