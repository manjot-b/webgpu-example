#include "App.hpp"

#if defined(WEBGPU_BACKEND_WGPU)
#include <webgpu/wgpu.h>
#endif
#include <glfw3webgpu.h>
#include <iostream>

#include "webgpu-utils.hpp"

namespace {
constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;
}

App::App() : m_terminated(false), m_pWindow(nullptr)
{
	m_initialized = Initialize();
}

App::~App()
{
	if (m_terminated)
		Terminate();
}

void App::Terminate()
{
	glfwDestroyWindow(m_pWindow);
	m_pWindow = nullptr;
	glfwTerminate();
	m_initialized = false;
	m_terminated = true;
}

bool App::IsInitialized() const { return m_initialized; }

bool App::Initialize()
{
	m_pWindow = GlfwInitialize();
	if (m_pWindow == nullptr)
	{
		std::cerr << "Could not initialize glfw. Aborting initialization." << std::endl;
		return false;
	}

	m_wgpuCtx = WgpuInitialize();
	if (m_wgpuCtx.initialized == false)
	{
		std::cerr << "Could not initialize WebGPU. Aborting initialization." << std::endl;
		return false;
	}

	// On window resize reconfigure the surface
	glfwSetWindowUserPointer(m_pWindow, static_cast<void*>(&m_wgpuCtx));
	glfwSetWindowSizeCallback(m_pWindow, [](GLFWwindow* pWindow, int width, int height){
		WgpuContext* pWgpuCtx = static_cast<WgpuContext*>(glfwGetWindowUserPointer(pWindow));

		if ( !(pWgpuCtx && pWgpuCtx->initialized) )
			return;

		wgpuSurfaceUnconfigure(pWgpuCtx->surface.get());
		wgpuUtils::configureSurface(pWgpuCtx->surface.get(), pWgpuCtx->device.get(), pWgpuCtx->adapter.get(), width, height);
	});

	return true;
}

GLFWwindow* App::GlfwInitialize()
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

App::WgpuContext App::WgpuInitialize()
{
	WgpuContext ctx;
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

#if !defined(WEBGPU_BACKEND_EMSCRIPTEN)
	WGPUInstanceDescriptor desc = {};
	desc.nextInChain = nullptr;
#endif

	ctx.instance = WgpuInstancePtr
	(
#if defined(WEBGPU_BACKEND_EMSCRIPTEN)
		wgpuCreateInstance(nullptr),
#else
		wgpuCreateInstance(&desc),
#endif
		wgpuInstanceRelease
	);

	if (!ctx.instance)
	{
		std::cerr << "Could not initialize WebGPU" << std::endl;
		return ctx;
	}
	std::cout << "WebGPU initialized successfully: " << ctx.instance.get() << std::endl;

	// Retrieving the surface is platform dependant, so use a helper function
	ctx.surface = WgpuSurfacePtr
	(
		glfwGetWGPUSurface(ctx.instance.get(), m_pWindow),
		[](WGPUSurface surface){
			wgpuSurfaceUnconfigure(surface);
			wgpuSurfaceRelease(surface);
		}
	);
	if (!ctx.surface)
	{
		std::cerr << "Could not get surface" << std::endl;
		return ctx;
	}

	// Retrieve the WebGPU adapter
	WGPURequestAdapterOptions adapterOptions{};
	adapterOptions.nextInChain = nullptr;
	adapterOptions.compatibleSurface = ctx.surface.get();

	ctx.adapter = WgpuAdapterPtr
	(
		wgpuUtils::requestAdapter(ctx.instance.get(), &adapterOptions),
		wgpuAdapterRelease
	);
	if (!ctx.adapter)
	{
		std::cerr << "Could not retrieve adapter" << std::endl;
		return ctx;
	}
	std::cout << "Got adapter: " << ctx.adapter.get() << std::endl;
	wgpuUtils::printAdapterFeatures(ctx.adapter.get());
	wgpuUtils::printAdapterProperties(ctx.adapter.get());

	// Use adapter and device description to retrieve a device
	WGPUDeviceDescriptor deviceDesc;
	deviceDesc.nextInChain = nullptr;
	deviceDesc.label = "My Device";
	deviceDesc.requiredFeatureCount = 0;
	deviceDesc.requiredLimits = nullptr;
	deviceDesc.defaultQueue.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = "Default Queue";
	deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const * message, void* /*pUserdata*/){
		std::cerr << "WGPU Device Lost: " << reason;
		if (message)
			std::cerr << " (" << message << ")";
		std::cout << std::endl;
	};

	ctx.device = WgpuDevicePtr
	(
		 wgpuUtils::requestDevice(ctx.adapter.get(), &deviceDesc),
		 wgpuDeviceRelease
	);
	if (!ctx.device)
	{
		std::cerr << "Could not retrieve device" << std::endl;
		return ctx;
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
	ctx.queue = WgpuQueuePtr
	(
		wgpuDeviceGetQueue(ctx.device.get()),
		wgpuQueueRelease
	);
	auto onQueueWorkDone = [](WGPUQueueWorkDoneStatus status, void* /*pUserData*/)
	{
		std::cout << "Queued work completed with status: " << status << std::endl;
	};
	wgpuQueueOnSubmittedWorkDone(ctx.queue.get(), onQueueWorkDone, nullptr);

	wgpuUtils::configureSurface(ctx.surface.get(), ctx.device.get(), ctx.adapter.get(), WINDOW_WIDTH, WINDOW_HEIGHT);

	ctx.initialized = true;
	return ctx;
}

void App::Tick()
{
	glfwPollEvents();

	WGPUTextureView nextTexture = GetNextSurfaceTextureView(m_wgpuCtx.surface.get());
	if (nextTexture == nullptr) // swap chain becomes invalidated on a window resize
	{
		std::cerr << "Skipping render frame" << std::endl;
		return;
	}

	// First create the command encoder for this frame
	WGPUCommandEncoderDescriptor encoderDesc{};
	encoderDesc.nextInChain = nullptr;
	encoderDesc.label = "My command encoder";
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_wgpuCtx.device.get(), &encoderDesc);

	static unsigned long tick = 0;
	static double colorVal = 0;
	static double delta = .01;
	if (tick % 10 == 0)
	{
		colorVal += delta;
		if (colorVal < 0 || colorVal > 1) { delta *= -1; }
	}

	// Next create the render pass encoder
	WGPURenderPassColorAttachment renderPassColorAttachment{};
	renderPassColorAttachment.view = nextTexture;
	renderPassColorAttachment.resolveTarget = nullptr;
	renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
	renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
	renderPassColorAttachment.clearValue = WGPUColor{ colorVal, .25, .4, 1.0 };
#if !defined(WEBGPU_BACKEND_WGPU)
	renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

	WGPURenderPassDescriptor renderPassDesc{};
	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;
	renderPassDesc.nextInChain = nullptr;
	renderPassDesc.depthStencilAttachment = nullptr;

	// useful for debugging
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
	wgpuQueueSubmit(m_wgpuCtx.queue.get(), 1, &command);

	// Cleanup
	wgpuCommandEncoderRelease(encoder);
	wgpuCommandBufferRelease(command);
	wgpuRenderPassEncoderRelease(renderPass);
	wgpuTextureViewRelease(nextTexture);

#if !defined(WEBGPU_BACKEND_EMSCRIPTEN)
	wgpuSurfacePresent(m_wgpuCtx.surface.get());
#endif

#if defined(WEBGPU_BACKEND_DAWN)
	wgpuDeviceTick(m_wgpuCtx.device.get());
#elif defined(WEBGPU_BACKEND_WGPU)
	wgpuDevicePoll(m_wgpuCtx.device.get(), false, nullptr);
#endif

	++tick;
}

WGPUTextureView App::GetNextSurfaceTextureView(WGPUSurface surface)
{
	WGPUSurfaceTexture surfaceTexture;
	wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);

	if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success)
		return nullptr;

	WGPUTextureViewDescriptor viewDesc = {};
	viewDesc.nextInChain = nullptr;
	viewDesc.label = "Surface texture view";
	viewDesc.format = wgpuTextureGetFormat(surfaceTexture.texture);
	viewDesc.dimension = WGPUTextureViewDimension_2D;
	viewDesc.baseMipLevel = 0;
	viewDesc.mipLevelCount = 1;
	viewDesc.baseArrayLayer = 0;
	viewDesc.arrayLayerCount = 1;
	viewDesc.aspect = WGPUTextureAspect_All;
	WGPUTextureView textureView = wgpuTextureCreateView(surfaceTexture.texture, &viewDesc);

#if !defined(WEBGPU_BACKEND_WGPU)
	wgpuTextureRelease(surfaceTexture.texture);
#endif

	return textureView;
}

bool App::IsRunning() const { return m_initialized && !m_terminated && !glfwWindowShouldClose(m_pWindow); }
