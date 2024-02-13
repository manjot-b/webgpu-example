#include <iostream>
#include <vector>

#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>
#include <glfw3webgpu.h>

#include "webgpu-utils.hpp"


int main (int, char**)
{
	// Setup GLFW
	glfwInit();

	if (!glfwInit())
	{
		std::cerr << "Could not initialize GLFW" << std::endl;
		return 1;
	}

	// GLFW is unaware of WebGPU (at the moment) so do not setup any graphics API with it
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(1280, 720, "Learn WebGPU", nullptr, nullptr);
	if (!window)
	{
		std::cerr << "Could not create window" << std::endl;
		return 1;
	}

	std::cout << "GLFW initialized successfully: " << window << std::endl;

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
	WGPUInstance instance = wgpuCreateInstance(&desc);

	if (!instance)
	{
		std::cerr << "Could not initialize WebGPU" << std::endl;
		return 1;
	}
	std::cout << "WebGPU initialized successfully: " << instance << std::endl;

	// Retrieving the surface is platform dependant, so use a helper function
	WGPUSurface surface = glfwGetWGPUSurface(instance, window);
	if (!surface)
	{
		std::cerr << "Could get surface" << std::endl;
		return 1;
	}

	// Retrieve the WebGPU adapter
	WGPURequestAdapterOptions adapterOptions{};
	adapterOptions.nextInChain = nullptr;
	adapterOptions.compatibleSurface = surface;

	WGPUAdapter adapter = wgpuUtils::requestAdapter(instance, &adapterOptions);
	if (!adapter)
	{
		std::cerr << "Could not retrieve adapter" << std::endl;
		return 1;
	}
	std::cout << "Got adapter: " << adapter << std::endl;
	wgpuUtils::printAdapterFeatures(adapter);

	// Use adapter to and device description to retrieve a device
	WGPUDeviceDescriptor deviceDesc;
	deviceDesc.nextInChain = nullptr;
	deviceDesc.label = "My Device";
	deviceDesc.requiredFeaturesCount = 0;
	deviceDesc.requiredLimits = nullptr;
	deviceDesc.defaultQueue.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = "Default Queue";

	WGPUDevice device = wgpuUtils::requestDevice(adapter, &deviceDesc);
	if (!device)
	{
		std::cerr << "Could not retrieve device" << std::endl;
		return 1;
	}

	auto onDeviceError = [](WGPUErrorType type, char const* message, void* /*pUserData*/)
	{
		std::cout << "Uncaptured device error: type " << type;
		if (message)
			std::cout << " (" << message << ")";
		std::cout << std::endl;
	};
	wgpuDeviceSetUncapturedErrorCallback(device, onDeviceError, nullptr);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	// cleanup
	wgpuDeviceRelease(device);
	wgpuSurfaceRelease(surface);
	wgpuAdapterRelease(adapter);
	wgpuInstanceRelease(instance);
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
