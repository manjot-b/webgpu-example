#include <iostream>
#include <memory>
#include <vector>

#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>
#include <glfw3webgpu.h>

#include "webgpu-utils.hpp"
#include "webgputypes.hpp"

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

struct WGPUContext
{
	WGPUContext() :
		instance(nullptr, wgpuInstanceRelease),
		adapter(nullptr, wgpuAdapterRelease),
		device(nullptr, wgpuDeviceRelease),
		surface(nullptr, wgpuSurfaceRelease),
		queue(nullptr, wgpuQueueRelease),
		swapChain(nullptr, wgpuSwapChainRelease)
	{}
	bool initialized;

	WGPUInstancePtr instance;
	WGPUAdapterPtr adapter;
	WGPUDevicePtr device;
	WGPUSurfacePtr surface;
	WGPUQueuePtr queue;
	WGPUSwapChainPtr swapChain;
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
	GLFWwindow* pWindow = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Learn WebGPU", nullptr, nullptr);
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
	ctx.instance = WGPUInstancePtr
	(
		wgpuCreateInstance(&desc),
		wgpuInstanceRelease
	);

	if (!ctx.instance)
	{
		std::cerr << "Could not initialize WebGPU" << std::endl;
		return;
	}
	std::cout << "WebGPU initialized successfully: " << ctx.instance.get() << std::endl;

	// Retrieving the surface is platform dependant, so use a helper function
	ctx.surface = WGPUSurfacePtr
	(
		 glfwGetWGPUSurface(ctx.instance.get(), pWindow),
		 wgpuSurfaceRelease
	);
	if (!ctx.surface)
	{
		std::cerr << "Could not get surface" << std::endl;
		return;
	}

	// Retrieve the WebGPU adapter
	WGPURequestAdapterOptions adapterOptions{};
	adapterOptions.nextInChain = nullptr;
	adapterOptions.compatibleSurface = ctx.surface.get();

	ctx.adapter = WGPUAdapterPtr
	(
		wgpuUtils::requestAdapter(ctx.instance.get(), &adapterOptions),
		wgpuAdapterRelease
	);
	if (!ctx.adapter)
	{
		std::cerr << "Could not retrieve adapter" << std::endl;
		return;
	}
	std::cout << "Got adapter: " << ctx.adapter.get() << std::endl;
	wgpuUtils::printAdapterFeatures(ctx.adapter.get());

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

	ctx.device = WGPUDevicePtr
	(
		 wgpuUtils::requestDevice(ctx.adapter.get(), &deviceDesc),
		 wgpuDeviceRelease
	);
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
	wgpuDeviceSetUncapturedErrorCallback(ctx.device.get(), onDeviceError, nullptr);

	// Get the queue on the device
	ctx.queue = WGPUQueuePtr
	(
		wgpuDeviceGetQueue(ctx.device.get()),
		wgpuQueueRelease
	);
	auto onQueueWorkDone = [](WGPUQueueWorkDoneStatus status, void* /*pUserData*/)
	{
		std::cout << "Queued work completed with status: " << status << std::endl;
	};
#if defined(WEBGPU_BACKEND_DAWN)
	wgpuQueueOnSubmittedWorkDone(ctx.queue.get(), 0, onQueueWorkDone, nullptr);
#else
	wgpuQueueOnSubmittedWorkDone(ctx.queue.get(), onQueueWorkDone, nullptr);
#endif

	ctx.swapChain = WGPUSwapChainPtr
	(
		wgpuUtils::createSwapChain(ctx.device.get(), ctx.surface.get(), ctx.adapter.get(), WINDOW_WIDTH, WINDOW_HEIGHT),
		wgpuSwapChainRelease
	);
	if (!ctx.swapChain)
	{
		std::cerr << "Could not create swap chain" << std::endl;
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

	// On window resize create a new swap chain
	glfwSetWindowUserPointer(pWindow, static_cast<void*>(&wgpuCtx));
	glfwSetWindowSizeCallback(pWindow, [](GLFWwindow* pWindow, int width, int height){
		WGPUContext* pWGPUCtx = static_cast<WGPUContext*>(glfwGetWindowUserPointer(pWindow));

		if ( !(pWGPUCtx && pWGPUCtx->initialized) )
			return;

		// NOTE: resize callback is called in main thread so safe to release pointer
		pWGPUCtx->swapChain = WGPUSwapChainPtr
		(
		 wgpuUtils::createSwapChain(pWGPUCtx->device.get(), pWGPUCtx->surface.get(), pWGPUCtx->adapter.get(), width, height),
		 wgpuSwapChainRelease
		);
	});

	while (!glfwWindowShouldClose(pWindow))
	{
		glfwPollEvents();

		WGPUTextureView nextTexture = wgpuSwapChainGetCurrentTextureView(wgpuCtx.swapChain.get());
		if (nextTexture) // swap chain becomes invalidated on a window resize
		{
			// First create the command encoder for this frame
			WGPUCommandEncoderDescriptor encoderDesc{};
			encoderDesc.nextInChain = nullptr;
			encoderDesc.label = "My command encoder";
			WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(wgpuCtx.device.get(), &encoderDesc);

			// Next create the render pass encoder
			WGPURenderPassColorAttachment renderPassColorAttachment{};
			renderPassColorAttachment.view = nextTexture;
			renderPassColorAttachment.resolveTarget = nullptr;
			renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
			renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
			renderPassColorAttachment.clearValue = WGPUColor{ 0.8, 0.25, 0.4, 1.0 };

			WGPURenderPassDescriptor renderPassDesc{};
			renderPassDesc.colorAttachmentCount = 1;
			renderPassDesc.colorAttachments = &renderPassColorAttachment;
			renderPassDesc.nextInChain = nullptr;
			renderPassDesc.depthStencilAttachment = nullptr;

			// useful for debugging
			renderPassDesc.timestampWriteCount = 0;
			renderPassDesc.timestampWrites = nullptr;

			// Ending render pass right away causes clear color to be set
			WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
			wgpuRenderPassEncoderEnd(renderPass);

			// create the command
			WGPUCommandBufferDescriptor cmdBufferDesc{};
			cmdBufferDesc.nextInChain = nullptr;
			cmdBufferDesc.label = "Command Buffer";
			WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDesc);

			// Submit the command to the queue
			wgpuQueueSubmit(wgpuCtx.queue.get(), 1, &command);

			wgpuCommandEncoderRelease(encoder);
			wgpuCommandBufferRelease(command);
			wgpuRenderPassEncoderRelease(renderPass);
			wgpuTextureViewRelease(nextTexture);

			wgpuSwapChainPresent(wgpuCtx.swapChain.get());
		}

	}

	// cleanup
	glfwDestroyWindow(pWindow);
	glfwTerminate();
	return 0;
}
