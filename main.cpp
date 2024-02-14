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
	WGPUQueue queue;
	WGPUCommandEncoder encoder;

	~WGPUContext()
	{
		std::cout << "Releasing WebGPU context" << std::endl;

		// Works on wgpu?
		wgpuCommandEncoderRelease(encoder);

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

void wgpuInitialize(GLFWwindow* pWindow, WGPUContext& ctx)
{
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
		return;
	}
	std::cout << "WebGPU initialized successfully: " << ctx.instance << std::endl;

	// Retrieving the surface is platform dependant, so use a helper function
	ctx.surface = glfwGetWGPUSurface(ctx.instance, pWindow);
	if (!ctx.surface)
	{
		std::cerr << "Could not get surface" << std::endl;
		return;
	}

	// Retrieve the WebGPU adapter
	WGPURequestAdapterOptions adapterOptions{};
	adapterOptions.nextInChain = nullptr;
	adapterOptions.compatibleSurface = ctx.surface;

	ctx.adapter = wgpuUtils::requestAdapter(ctx.instance, &adapterOptions);
	if (!ctx.adapter)
	{
		std::cerr << "Could not retrieve adapter" << std::endl;
		return;
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
	deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const * message, void* /*pUserdata*/){
		std::cerr << "WGPU Device Lost: " << reason;
		if (message)
			std::cerr << " (" << message << ")";
		std::cout << std::endl;
	};

	ctx.device = wgpuUtils::requestDevice(ctx.adapter, &deviceDesc);
	if (!ctx.device)
	{
		std::cerr << "Could not retrieve device" << std::endl;
		return;
	}
	std::cout << "Device retrieved" << std::endl;

	auto onDeviceError = [](WGPUErrorType type, char const* message, void* /*pUserData*/)
	{
		std::cout << "Uncaptured device error: type " << type;
		if (message)
			std::cout << " (" << message << ")";
		std::cout << std::endl;
	};
	wgpuDeviceSetUncapturedErrorCallback(ctx.device, onDeviceError, nullptr);

	// Get the queue on the device
	ctx.queue = wgpuDeviceGetQueue(ctx.device);
	auto onQueueWorkDone = [](WGPUQueueWorkDoneStatus status, void* /*pUserData*/)
	{
		std::cout << "Queued work completed with status: " << status << std::endl;
	};
#if defined(WEBGPU_BACKEND_DAWN)
	wgpuQueueOnSubmittedWorkDone(ctx.queue, 0, onQueueWorkDone, nullptr);
#else
	wgpuQueueOnSubmittedWorkDone(ctx.queue, onQueueWorkDone, nullptr);
#endif

	WGPUCommandEncoderDescriptor encoderDesc{};
	encoderDesc.nextInChain = nullptr;
	encoderDesc.label = "My command encoder";
	ctx.encoder = wgpuDeviceCreateCommandEncoder(ctx.device, &encoderDesc);

	if (!ctx.encoder)
	{
		std::cerr << "Could not retrieve encoder" << std::endl;
		return;
	}
	
	ctx.initialized = true;
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

	WGPUContext wgpuCtx;
	wgpuInitialize(pWindow, wgpuCtx);

	if (!wgpuCtx.initialized)
	{
		std::cerr << "WebGPU not initialized correctly" << std::endl;
		return 1;
	}
	std::cout << "WebGPU initialized successfully" << std::endl;

	// Use encoder to generate commands
	wgpuCommandEncoderInsertDebugMarker(wgpuCtx.encoder, "Do one thing");
	wgpuCommandEncoderInsertDebugMarker(wgpuCtx.encoder, "Do another thing");

	WGPUCommandBufferDescriptor cmdBufferDesc{};
	cmdBufferDesc.nextInChain = nullptr;
	cmdBufferDesc.label = "Command Buffer";
	WGPUCommandBuffer command = wgpuCommandEncoderFinish(wgpuCtx.encoder, &cmdBufferDesc);

	// Submit the command to the queue
	std::cout << "Submitting command..." << std::endl;
	wgpuQueueSubmit(wgpuCtx.queue, 1, &command);
	wgpuCommandBufferRelease(command);

	while (!glfwWindowShouldClose(pWindow))
	{
		glfwPollEvents();
	}

	// cleanup
	glfwDestroyWindow(pWindow);
	glfwTerminate();
	return 0;
}
