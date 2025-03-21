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

	void AddDeviceError(WGPUErrorType error, std::string_view message);
	bool LogDeviceErrors();
	bool Initialize();
	GLFWwindow* GlfwInitialize();
	WgpuContext WgpuInitialize();
	WGPURenderPipeline WgpuRenderPipelineInitialize();
	const char* GetShaderSource();
	std::tuple<WGPUTextureView, WGPUTexture> GetNextSurfaceTextureView();

	bool m_initialized;
	bool m_terminated;
	WgpuContext m_wgpuCtx;
	std::queue<WgpuError> m_wgpuErrors;

	using GlfwWindowPtr = std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)>;
	GlfwWindowPtr m_pWindow;
	WindowDimensions m_windowDim;
};
