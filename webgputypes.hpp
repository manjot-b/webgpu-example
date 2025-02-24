#pragma once

#include <memory>
#include <webgpu/webgpu.h>

using WGPUInstancePtr = std::unique_ptr<std::remove_pointer_t<WGPUInstance>, decltype(&wgpuInstanceRelease)>;
using WGPUAdapterPtr = std::unique_ptr<std::remove_pointer_t<WGPUAdapter>, decltype(&wgpuAdapterRelease)>;
using WGPUDevicePtr = std::unique_ptr<std::remove_pointer_t<WGPUDevice>, decltype(&wgpuDeviceRelease)>;
using WGPUSurfacePtr = std::unique_ptr<std::remove_pointer_t<WGPUSurface>, decltype(&wgpuSurfaceRelease)>;
using WGPUQueuePtr = std::unique_ptr<std::remove_pointer_t<WGPUQueue>, decltype(&wgpuQueueRelease)>;
