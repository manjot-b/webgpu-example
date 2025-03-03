#include "App.hpp"

#if defined(WEBGPU_BACKEND_WGPU)
#include <webgpu/wgpu.h>
#endif
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <iostream>
#include <sstream>

#include "webgpu-utils.hpp"

namespace {
constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;
}

App::App() : m_terminated(false), m_pWindow(nullptr, glfwDestroyWindow)
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
	m_pWindow.reset();
	glfwTerminate();
	m_initialized = false;
	m_terminated = true;
}

void App::AddDeviceError(WGPUErrorType error, const char* message)
{
	std::stringstream ss;
	ss << "Uncaptured device error: type " << error;
	if (message)
		ss << " (" << message << ")";
	ss << std::endl;

	m_wgpuErrors.push( {error, ss.str()} );
}

bool App::LogDeviceErrors()
{
	if (m_wgpuErrors.empty())
		return false;

	while (!m_wgpuErrors.empty())
	{
		std::cerr << m_wgpuErrors.front().message << std::endl;
		m_wgpuErrors.pop();
	}

	return true;
}

bool App::IsRunning() const { return m_initialized && !m_terminated && !glfwWindowShouldClose(m_pWindow.get()); }

bool App::IsInitialized() const { return m_initialized; }

bool App::Initialize()
{
	// Init Glfw
	m_pWindow = GlfwWindowPtr(GlfwInitialize(), glfwDestroyWindow);
	if (m_pWindow == nullptr)
	{
		std::cerr << "Could not initialize glfw. Aborting initialization." << std::endl;
		return false;
	}

	// Init Wgpu
	m_wgpuCtx = WgpuInitialize();
	if (m_wgpuCtx.initialized == false)
	{
		std::cerr << "Could not initialize WebGPU. Aborting initialization." << std::endl;
		return false;
	}

	// On window resize reconfigure the surface
	glfwSetWindowUserPointer(m_pWindow.get(), static_cast<void*>(&m_wgpuCtx));
	glfwSetWindowSizeCallback(m_pWindow.get(), [](GLFWwindow* pWindow, int width, int height){
		WgpuContext* pWgpuCtx = static_cast<WgpuContext*>(glfwGetWindowUserPointer(pWindow));

		if ( !(pWgpuCtx && pWgpuCtx->initialized) )
			return;

		wgpuSurfaceUnconfigure(pWgpuCtx->surface.get());
		wgpuUtils::configureSurface(pWgpuCtx->surface.get(), pWgpuCtx->device.get(), pWgpuCtx->adapter.get(), width, height);
	});

	// Init Wgpu Pipeline
	m_wgpuCtx.pipeline = WgpuRenderPipelinePtr(WgpuRenderPipelineInitialize(), wgpuRenderPipelineRelease);
	if (!m_wgpuCtx.pipeline)
	{
		std::cerr << "Could not initialize WebGPU pipeline. Aborting initialization." << std::endl;
		return false;
	}

	// On Emscripten the errors might not be captured yet because the callback is asynchronous.
	if (LogDeviceErrors())
	{
		std::cerr << "Device errors encountered during initialization. Aborting initialization" << std::endl;
		return false;
	}

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
		glfwGetWGPUSurface(ctx.instance.get(), m_pWindow.get()),
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

	auto onDeviceError = [](WGPUErrorType type, char const* message, void* pUserData)
	{
		App* app = static_cast<App*>(pUserData);
		if (app)
			app->AddDeviceError(type, message);
	};
	wgpuDeviceSetUncapturedErrorCallback(ctx.device.get(), onDeviceError, this);

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

WGPURenderPipeline App::WgpuRenderPipelineInitialize()
{
	WGPURenderPipelineDescriptor pipelineDesc = {};
	pipelineDesc.nextInChain = nullptr;

	// Shader module
	WGPUShaderModuleWGSLDescriptor shaderCodeDesc{};
	shaderCodeDesc.chain.next = nullptr;
	shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
	shaderCodeDesc.code = GetShaderSource();

	WGPUShaderModuleDescriptor shaderDesc{};
	shaderDesc.nextInChain = &shaderCodeDesc.chain;
	WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(m_wgpuCtx.device.get(), &shaderDesc);

	// Vertex state
	pipelineDesc.vertex.bufferCount = 0;
	pipelineDesc.vertex.buffers = nullptr;
	pipelineDesc.vertex.module = shaderModule;
	pipelineDesc.vertex.entryPoint = "vs_main";
	pipelineDesc.vertex.constantCount = 0;
	pipelineDesc.vertex.constants = nullptr;

	// Primitive state
	pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
	pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
	pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
	pipelineDesc.primitive.cullMode = WGPUCullMode_Back;

	// Fragment state
	WGPUFragmentState fragment{};
	fragment.module = shaderModule;
	fragment.entryPoint = "fs_main";
	fragment.constantCount = 0;
	fragment.constants = nullptr;

	// Depth/Stencil state
	pipelineDesc.depthStencil = nullptr;

	// Blend State
	WGPUBlendState blend{};
	blend.color.srcFactor = WGPUBlendFactor_SrcAlpha;
	blend.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
	blend.color.operation = WGPUBlendOperation_Add;
	blend.alpha.srcFactor = WGPUBlendFactor_Zero;  // Alpha is already incorporated into the RGB color
	blend.alpha.dstFactor = WGPUBlendFactor_One;
	blend.alpha.operation = WGPUBlendOperation_Add;

	WGPUColorTargetState colorTarget{};
	colorTarget.format = wgpuSurfaceGetPreferredFormat(m_wgpuCtx.surface.get(), m_wgpuCtx.adapter.get());
	colorTarget.blend = &blend;
	colorTarget.writeMask = WGPUColorWriteMask_All;

	// Set target for fragment output
	fragment.targetCount = 1;
	fragment.targets = &colorTarget;
	pipelineDesc.fragment = &fragment;

	// Multisampling state
	pipelineDesc.multisample.count = 1;
	pipelineDesc.multisample.mask = ~0u;  // Enable all bits
	pipelineDesc.multisample.alphaToCoverageEnabled = false;

	pipelineDesc.layout = nullptr;

	WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(m_wgpuCtx.device.get(), &pipelineDesc);
	wgpuShaderModuleRelease(shaderModule);

	return pipeline;
}

const char* App::GetShaderSource()
{
	static const char* shaderSource = R"(
@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4f
{
	var p = vec2f(.0, .0);
	if (in_vertex_index == 0u) {
		p = vec2f(-.5, -.5);
	}
	else if (in_vertex_index == 1u) {
		p = vec2f(.5, -.5);
	}
	else {
		p = vec2f(.0, .5);
	}

	return vec4f(p, .0, 1.);
}

@fragment
fn fs_main() -> @location(0) vec4f
{
	return vec4f(.0, .6, .9, 1.);
})";

	return shaderSource;
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

	// Use render pipeline created during initialization to make a draw call
	WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
	wgpuRenderPassEncoderSetPipeline(renderPass, m_wgpuCtx.pipeline.get());
	wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);
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
	LogDeviceErrors();
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
