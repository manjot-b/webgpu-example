#pragma once

#include <webgpu/webgpu.h>
#include "webgputypes.hpp"

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
			queue(nullptr, wgpuQueueRelease)
		{}
		bool initialized;

		WgpuInstancePtr instance;
		WgpuAdapterPtr adapter;
		WgpuDevicePtr device;
		WgpuSurfacePtr surface;
		WgpuQueuePtr queue;
	};

	bool Initialize();
	GLFWwindow* GlfwInitialize();
	WgpuContext WgpuInitialize();
	WGPUTextureView GetNextSurfaceTextureView(WGPUSurface surface);

	bool m_initialized;
	bool m_terminated;
	WgpuContext m_wgpuCtx;
	GLFWwindow* m_pWindow;
};
