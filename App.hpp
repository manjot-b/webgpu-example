#pragma once

#include <webgpu/webgpu.h>
#include "webgputypes.hpp"

#include <queue>
#include <string>
#include <tuple>

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
		// TODO: Handle interleaving multiple attributes
		WgpuBuffer() :
			wgpuBuffer(nullptr, wgpuBufferRelease),
			size(0), count(0), components(1), componentSize(1)
		{}
		WgpuBufferPtr wgpuBuffer;
		size_t size;
		size_t count;
		size_t components;
		size_t componentSize;
	};

#if defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	WGPURequiredLimits GetRequiredLimits(WGPUAdapter adapter) const;
#else
	WGPULimits GetRequiredLimits(WGPUAdapter adapter) const;
#endif
	void AddDeviceError(WGPUErrorType error, std::string_view message);
	bool LogDeviceErrors();
	bool Initialize();
	GLFWwindow* GlfwInitialize();
	WgpuContext WgpuInitialize();
	void BuffersInitialize();
	WGPURenderPipeline WgpuRenderPipelineInitialize();
	const char* GetShaderSource() const;
	std::tuple<WGPUTextureView, WGPUTexture> GetNextSurfaceTextureView();

	bool m_initialized;
	bool m_terminated;
	WgpuContext m_wgpuCtx;
	std::queue<WgpuError> m_wgpuErrors;

	using GlfwWindowPtr = std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)>;
	GlfwWindowPtr m_pWindow;
	WindowDimensions m_windowDim;

	WgpuBuffer m_buffer;
};
