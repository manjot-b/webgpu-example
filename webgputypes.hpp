#pragma once

#include <memory>
#include <webgpu/webgpu.h>

/*
 * Create a unique_ptr<WGPU<wgpuType>> alias as Wgpu<wgpuType>Ptr.
 * For example, the WGPUInstance class is defined as WGPU_PTR_ALIAS(Instance)
 * and results in "using WgpuInstancePtr = ..."
 */
#define WGPU_PTR_ALIAS(wgpuType) \
	using Wgpu ## wgpuType ## Ptr = std::unique_ptr<std::remove_pointer_t<WGPU ## wgpuType>, decltype(&wgpu ## wgpuType ## Release)>;

WGPU_PTR_ALIAS(Instance)
WGPU_PTR_ALIAS(Adapter)
WGPU_PTR_ALIAS(Device)
WGPU_PTR_ALIAS(Surface)
WGPU_PTR_ALIAS(RenderPipeline)
WGPU_PTR_ALIAS(Queue)
WGPU_PTR_ALIAS(ShaderModule)
WGPU_PTR_ALIAS(RenderPassEncoder)
WGPU_PTR_ALIAS(CommandEncoder)
WGPU_PTR_ALIAS(CommandBuffer)
WGPU_PTR_ALIAS(Texture)
WGPU_PTR_ALIAS(TextureView)
WGPU_PTR_ALIAS(Buffer)
WGPU_PTR_ALIAS(PipelineLayout)
WGPU_PTR_ALIAS(BindGroupLayout)
WGPU_PTR_ALIAS(BindGroup)

#undef WGPU_PTR_ALIAS
