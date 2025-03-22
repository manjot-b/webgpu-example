#pragma once

#include <webgpu/webgpu.h>

#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
#include <iostream>
#endif

#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
std::ostream& operator<<(std::ostream& os, WGPUStringView strView);
#endif

// WGPUFuture is currently only implemtened in DAWN
#if defined(WEBGPU_BACKEND_WGPU) || defined(WEBGPU_BACKEND_EMSCRIPTEN)
#define WEBGPU_FUTURE_UNIMPLEMENTED
#endif

namespace wgpuUtils{

WGPUAdapter requestAdapter(WGPUInstance instance, const WGPURequestAdapterOptions* options);
WGPUDevice requestDevice(WGPUInstance instance, WGPUAdapter adapter, const WGPUDeviceDescriptor* descriptor);

void configureSurface(WGPUSurface surface, WGPUDevice device, WGPUAdapter adapter, int width, int height);

#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	WGPUTextureFormat getPreferredFormat(WGPUAdapter adapter, WGPUSurface surface);
#endif

void printAdapterFeatures(WGPUAdapter adapter);
void printAdapterProperties(WGPUAdapter adapter);
void printAdapterLimits(WGPUAdapter adapter);
void printDeviceLimits(WGPUDevice device);

} // namespace utils
