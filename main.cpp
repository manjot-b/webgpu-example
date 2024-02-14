#include <iostream>
#include <vector>

#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>
#include <glfw3webgpu.h>

#include "webgpu-utils.hpp"

struct WGPUContext
{
	bool initialized;

	WGPUInstance instance;
	WGPUAdapter adapter;
	WGPUDevice device;
	WGPUSurface surface;

	~WGPUContext()
	{
		wgpuDeviceRelease(device);
		wgpuSurfaceRelease(surface);
		wgpuAdapterRelease(adapter);
		wgpuInstanceRelease(instance);
	}
};

GLFWwindow* glfwInitialize()
{
	// Setup GLFW
	if (!glfwInit())
	{
		std::cerr << "Could not initialize GLFW" << std::endl;
		return nullptr;
	}

	// GLFW is unaware of WebGPU (at the moment) so do not setup any graphics API with it
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* pWindow = glfwCreateWindow(1280, 720, "Learn WebGPU", nullptr, nullptr);
	if (!pWindow)
	{
		std::cerr << "Could not create window" << std::endl;
		return nullptr;
	}

	return pWindow;
}

WGPUContext wgpuInitialize(GLFWwindow* pWindow)
{
	WGPUContext ctx;
	ctx.initialized = false;

	// Setup WebGPU
#if defined(WEBGPU_BACKEND_DAWN)
	std::cout << "Dawn backend detected" << std::endl;
#elif defined(WEBGPU_BACKEND_WGPU)
	std::cout << "WGPU backend detected" << std::endl;
#elif defined(WEBGPU_BACKEND_EMSCRIPTEN)
	std::cout << "Emscripten backend detected" << std::endl;
#else
	#error "No valid WebGPU backend detected"
#endif

	WGPUInstanceDescriptor desc = {};
	desc.nextInChain = nullptr;
	ctx.instance = wgpuCreateInstance(&desc);

	if (!ctx.instance)
	{
		std::cerr << "Could not initialize WebGPU" << std::endl;
		return ctx;
	}
	std::cout << "WebGPU initialized successfully: " << ctx.instance << std::endl;

	// Retrieving the surface is platform dependant, so use a helper function
	ctx.surface = glfwGetWGPUSurface(ctx.instance, pWindow);
	if (!ctx.surface)
	{
		std::cerr << "Could not get surface" << std::endl;
		return ctx;
	}

	// Retrieve the WebGPU adapter
	WGPURequestAdapterOptions adapterOptions{};
	adapterOptions.nextInChain = nullptr;
	adapterOptions.compatibleSurface = ctx.surface;

	ctx.adapter = wgpuUtils::requestAdapter(ctx.instance, &adapterOptions);
	if (!ctx.adapter)
	{
		std::cerr << "Could not retrieve adapter" << std::endl;
		return ctx;
	}
	std::cout << "Got adapter: " << ctx.adapter << std::endl;
	wgpuUtils::printAdapterFeatures(ctx.adapter);

	// Use adapter and device description to retrieve a device
	WGPUDeviceDescriptor deviceDesc;
	deviceDesc.nextInChain = nullptr;
	deviceDesc.label = "My Device";
	deviceDesc.requiredFeaturesCount = 0;
	deviceDesc.requiredLimits = nullptr;
	deviceDesc.defaultQueue.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = "Default Queue";

	ctx.device = wgpuUtils::requestDevice(ctx.adapter, &deviceDesc);
	if (!ctx.device)
	{
		std::cerr << "Could not retrieve device" << std::endl;
		return ctx;
	}

	auto onDeviceError = [](WGPUErrorType type, char const* message, void* /*pUserData*/)
	{
		std::cout << "Uncaptured device error: type " << type;
		if (message)
			std::cout << " (" << message << ")";
		std::cout << std::endl;
	};
	wgpuDeviceSetUncapturedErrorCallback(ctx.device, onDeviceError, nullptr);

	ctx.initialized = true;
	return ctx;
}

int main (int, char**)
{
	GLFWwindow* pWindow = glfwInitialize();
	if (!pWindow)
	{
		std::cerr << "GLFW not initialized correctly" << std::endl;
		return 1;
	}
	std::cout << "GLFW initialized successfully: " << pWindow << std::endl;

	WGPUContext wgpuCtx = wgpuInitialize(pWindow);
	if (!wgpuCtx.initialized)
	{
		std::cerr << "WebGPU not initialized correctly" << std::endl;
		return 1;
	}
	std::cout << "WebGPU initialized successfully" << std::endl;

	while (!glfwWindowShouldClose(pWindow))
	{
		glfwPollEvents();
	}

	// cleanup
	glfwDestroyWindow(pWindow);
	glfwTerminate();
	return 0;
}
