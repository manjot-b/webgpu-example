#include "build/wgpu/_deps/webgpu-backend-wgpu-src/include/webgpu/webgpu.h"
#include <cassert>
#include <iostream>
#include <vector>

#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>
#include <glfw3webgpu.h>

WGPUAdapter requestAdapter(WGPUInstance instance, const WGPURequestAdapterOptions* options)
{
	/**
	 * Holds local information shared with the onAdapterRequestEnded callback
	 */
	struct UserData
	{
		WGPUAdapter adapter = nullptr;
		bool requestEnded = false;
	} userData;

	auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* pUserData)
	{
		UserData& userData = *reinterpret_cast<UserData*>(pUserData);
		if (status == WGPURequestAdapterStatus_Success)
		{
			userData.adapter = adapter;
		}
		else
		{
			std::cerr << "Could not retrieve WebGPU adapter: " << message << std::endl;
		}
		userData.requestEnded = true;
	};

	wgpuInstanceRequestAdapter(instance, options, onAdapterRequestEnded, static_cast<void*>(&userData));

	// The callback is called synchronously so the following assert should never fire.
	assert(userData.requestEnded);

	return userData.adapter;
}

void printAdapterFeatures(WGPUAdapter adapter)
{
	std::vector<WGPUFeatureName> features;
	size_t featureCount = wgpuAdapterEnumerateFeatures(adapter, nullptr);
	features.resize(featureCount);
	wgpuAdapterEnumerateFeatures(adapter, features.data());

	std::cout << "Adapter features:" << std::endl;
	for (auto f : features)
	{
		std::cout << "-" << f << std::endl;
	}
}

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

	WGPUAdapter adapter = requestAdapter(instance, &adapterOptions);
	if (!adapter)
	{
		std::cerr << "Could not retrieve adapter" << std::endl;
		return 1;
	}
	std::cout << "Got adapter: " << adapter << std::endl;

	printAdapterFeatures(adapter);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	// cleanup
	wgpuSurfaceRelease(surface);
	wgpuAdapterRelease(adapter);
	wgpuInstanceRelease(instance);
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
