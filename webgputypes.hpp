#pragma once

#include <memory>
#include <webgpu/webgpu.h>

using WgpuInstancePtr = std::unique_ptr<std::remove_pointer_t<WGPUInstance>, decltype(&wgpuInstanceRelease)>;
using WgpuAdapterPtr = std::unique_ptr<std::remove_pointer_t<WGPUAdapter>, decltype(&wgpuAdapterRelease)>;
using WgpuDevicePtr = std::unique_ptr<std::remove_pointer_t<WGPUDevice>, decltype(&wgpuDeviceRelease)>;
using WgpuSurfacePtr = std::unique_ptr<std::remove_pointer_t<WGPUSurface>, decltype(&wgpuSurfaceRelease)>;
using WgpuQueuePtr = std::unique_ptr<std::remove_pointer_t<WGPUQueue>, decltype(&wgpuQueueRelease)>;
