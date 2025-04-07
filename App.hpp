#pragma once

#include <webgpu/webgpu.h>
#include "webgputypes.hpp"

#include <queue>
#include <string>
#include <tuple>
#include <cassert>
#include <array>

class GLFWwindow;

class App
{
public:
	App();
	~App();
	void Tick();
	void Terminate();
	bool IsInitialized() const;
	bool IsRunning() const;
private:
	struct WgpuContext
	{
		WgpuContext() :
			initialized(false),
			instance(nullptr, wgpuInstanceRelease),
			adapter(nullptr, wgpuAdapterRelease),
			device(nullptr, wgpuDeviceRelease),
			surface(nullptr, wgpuSurfaceRelease),
			queue(nullptr, wgpuQueueRelease),
			pipeline(nullptr, wgpuRenderPipelineRelease)
		{}
		bool initialized;

		WgpuInstancePtr instance;
		WgpuAdapterPtr adapter;
		WgpuDevicePtr device;
		WgpuSurfacePtr surface;
		WgpuQueuePtr queue;
		WgpuRenderPipelinePtr pipeline;
	};

	struct WgpuError
	{
		WGPUErrorType error;
		std::string message;
	};

	struct WindowDimensions
	{
		int width;
		int height;
	};

	struct WgpuBuffer
	{
		WgpuBuffer() :
			m_wgpuBuffer(nullptr, wgpuBufferRelease),
			m_size{}, m_count{}, m_components{}, m_componentSize{}, m_stride{}, m_attributes{}
		{}

		WgpuBuffer(size_t count, size_t componentSize, std::vector<size_t> attributeComponents, WgpuBufferPtr wgpuBuffer) :
			WgpuBuffer()
		{
			SetInfo(count, componentSize, std::move(attributeComponents), std::move(wgpuBuffer));
		}

		bool SetInfo(size_t count, size_t componentSize, std::vector<size_t> attributeComponents, WgpuBufferPtr wgpuBuffer);

		WgpuBufferPtr m_wgpuBuffer;
		size_t m_size;
		size_t m_count;
		size_t m_components;
		size_t m_componentSize;
		size_t m_stride;
		size_t m_attributes;
		std::vector<size_t> m_attributeComponents;
		std::vector<size_t> m_attributeOffset;
	};

	struct Uniforms
	{
		Uniforms() : ratio(0), color{} {}

		float ratio;
		float gamma;
		// vec4f must align on 16 byte boundary. Same for matching struct in WGSL
		alignas(16) std::array<float, 4> color{};
	};
	static_assert(sizeof(Uniforms) % sizeof(std::array<float, 4>) == 0);

	struct WgpuTexture
	{
		WgpuTexture() :
			texture(nullptr, wgpuTextureRelease),
			textureView(nullptr, wgpuTextureViewRelease)
		{}

		WgpuTexturePtr texture;
		WgpuTextureViewPtr textureView;
	};

	using GlfwWindowPtr = std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)>;

#if defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	WGPURequiredLimits GetRequiredLimits(WGPUAdapter adapter) const;
#else
	WGPULimits GetRequiredLimits(WGPUAdapter adapter) const;
#endif
	void AddDeviceError(WGPUErrorType error, std::string_view message);
	bool LogDeviceErrors();

	bool Initialize();
	GlfwWindowPtr GlfwInitialize();
	WgpuContext WgpuInitialize();
	void BuffersInitialize();
	WgpuRenderPipelinePtr WgpuRenderPipelineInitialize();
	void WgpuBindGroupsInitialize();
	void WgpuTextureInitialize();

	const char* GetShaderSource() const;
	std::tuple<WGPUTextureView, WGPUTexture> GetNextSurfaceTextureView();
	void  UpdateGamma(const WGPUTexture texture);

	bool m_initialized;
	bool m_terminated;
	WgpuContext m_wgpuCtx;
	std::queue<WgpuError> m_wgpuErrors;

	GlfwWindowPtr m_window;
	WindowDimensions m_windowDim;

	WgpuBuffer m_verticies;
	WgpuBuffer m_indicies;

	WgpuBufferPtr m_uniformsBuffer;
	WgpuBindGroupLayoutPtr m_bindGroupLayout;
	WgpuPipelineLayoutPtr m_pipelineLayout;
	WgpuBindGroupPtr m_bindGroup;
	Uniforms m_uniforms;

	WgpuTexture m_texture;
};
