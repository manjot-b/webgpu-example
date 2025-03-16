#include "App.hpp"

#if defined(WEBGPU_BACKEND_WGPU)
#include <webgpu/wgpu.h>
#endif

#include <GLFW/glfw3.h>
#include <iostream>
#include <sstream>

#include "glfw3webgpu.hpp"
#include "webgpu-utils.hpp"


App::App() : m_terminated(false), m_pWindow(nullptr, glfwDestroyWindow), m_windowDim{1280, 720}
{
	m_initialized = Initialize();
}

App::~App()
{
	if (!m_terminated)
		Terminate();
}

void App::Terminate()
{
	m_wgpuCtx = {};
	m_pWindow.reset();
	glfwTerminate();
	m_initialized = false;
	m_terminated = true;
}

void App::AddDeviceError(WGPUErrorType error, std::string_view message)
{
	std::stringstream ss;
	ss << "Uncaptured device error: type " << error;
	if (message.length())
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

	glfwSetWindowUserPointer(m_pWindow.get(), static_cast<void*>(&m_wgpuCtx));


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
	GLFWwindow* pWindow = glfwCreateWindow(m_windowDim.width, m_windowDim.height, "Learn WebGPU", nullptr, nullptr);
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
		glfwCreateWindowWGPUSurface(ctx.instance.get(), m_pWindow.get()),
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

	auto onDeviceError = [](
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
			[[maybe_unused]]WGPUDevice const *device, WGPUErrorType type, WGPUStringView message,
			[[maybe_unused]]void* pUserData1, [[maybe_unused]]void* pUserData2)
#else
			WGPUErrorType type, char const *message, void* pUserData1)
#endif  // EMSCRIPTEN_WEBGPU_DEPRECATED
	{
		App* app = static_cast<App*>(pUserData1);
		if (app)
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
			app->AddDeviceError(type, std::string_view(message.data, message.length));
#else
			app->AddDeviceError(type, message);
#endif  // EMSCRIPTEN_WEBGPU_DEPRECATED
	};

	auto onDeviceLost = [](
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
			[[maybe_unused]]WGPUDevice const *pDevice, WGPUDeviceLostReason reason, WGPUStringView message,
			[[maybe_unused]]void* pUserData1, [[maybe_unused]]void* pUserData2)
#else
			WGPUDeviceLostReason reason, char const * message, [[maybe_unused]]void* pUserdata1)
#endif  // EMSCRIPTEN_WEBGPU_DEPRECATED
	{
		std::cerr << "WGPU Device Lost: " << reason;

#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
		if (message.length)
#else
		if (message)
#endif  // EMSCRIPTEN_WEBGPU_DEPRECATED
			std::cerr << " (" << message << ")";
		std::cout << std::endl;
	};
	// Use adapter and device description to retrieve a device
	WGPUDeviceDescriptor deviceDesc{};
	deviceDesc.nextInChain = nullptr;
	deviceDesc.requiredFeatureCount = 0;
	deviceDesc.requiredLimits = nullptr;
	deviceDesc.defaultQueue.nextInChain = nullptr;
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	deviceDesc.label = {"My Device", WGPU_STRLEN};
	deviceDesc.defaultQueue.label = {"Default Queue", WGPU_STRLEN};
	deviceDesc.deviceLostCallbackInfo.nextInChain = nullptr;
	deviceDesc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
	deviceDesc.deviceLostCallbackInfo.callback = onDeviceLost;
	deviceDesc.uncapturedErrorCallbackInfo.nextInChain = nullptr;
	deviceDesc.uncapturedErrorCallbackInfo.userdata1 = this;
	deviceDesc.uncapturedErrorCallbackInfo.callback = onDeviceError;
#else
	deviceDesc.label = "My Device";
	deviceDesc.defaultQueue.label = "Default Queue";
	deviceDesc.deviceLostCallback = onDeviceLost;
#endif

	ctx.device = WgpuDevicePtr
	(
		 wgpuUtils::requestDevice(ctx.instance.get(), ctx.adapter.get(), &deviceDesc),
		 wgpuDeviceRelease
	);
	if (!ctx.device)
	{
		std::cerr << "Could not retrieve device" << std::endl;
		return ctx;
	}
	std::cout << "Device retrieved" << std::endl;

#if defined (EMSCRIPTEN_WEBGPU_DEPRECATED)
	wgpuDeviceSetUncapturedErrorCallback(ctx.device.get(), onDeviceError, this);
#endif

	// Get the queue on the device
	ctx.queue = WgpuQueuePtr
	(
		wgpuDeviceGetQueue(ctx.device.get()),
		wgpuQueueRelease
	);

	// Callback only invoked when all work submitted up to this point in complete.
	// No work has been submitted to the queue yet...
	auto onQueueWorkDone = [](
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
			WGPUQueueWorkDoneStatus status, [[maybe_unused]]void* pUserData1, [[maybe_unused]]void* pUserData2)
#else
			WGPUQueueWorkDoneStatus status, [[maybe_unused]]void* pUserData1)
#endif
	{
		std::cout << "Queued work completed with status: " << status << std::endl;
	};

#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	WGPUQueueWorkDoneCallbackInfo queueWorkDoneInfo{};
	queueWorkDoneInfo.mode = WGPUCallbackMode_AllowProcessEvents;
	queueWorkDoneInfo.callback = onQueueWorkDone;
	[[maybe_unused]]WGPUFuture queueFuture = wgpuQueueOnSubmittedWorkDone(ctx.queue.get(), queueWorkDoneInfo);

	#if !defined(WEBGPU_FUTURE_UNIMPLEMENTED)
		WGPUFutureWaitInfo queueFutureWait{queueFuture, false};
		wgpuInstanceWaitAny(ctx.instance.get(), 1, &queueFutureWait, 0);
	#endif
#else
	wgpuQueueOnSubmittedWorkDone(ctx.queue.get(), onQueueWorkDone, nullptr);
#endif  // EMSCRIPTEN_WEBGPU_DEPRECATED

	wgpuUtils::configureSurface(ctx.surface.get(), ctx.device.get(), ctx.adapter.get(), m_windowDim.width, m_windowDim.height);

	ctx.initialized = true;
	return ctx;
}

WGPURenderPipeline App::WgpuRenderPipelineInitialize()
{
	WGPURenderPipelineDescriptor pipelineDesc = {};
	pipelineDesc.nextInChain = nullptr;

	// Shader module
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	WGPUShaderSourceWGSL shaderSourceDesc{};
	shaderSourceDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
	shaderSourceDesc.code = WGPUStringView{GetShaderSource(), WGPU_STRLEN};
#else
	WGPUShaderModuleWGSLDescriptor shaderSourceDesc{};
	shaderSourceDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
	shaderSourceDesc.code = GetShaderSource();
#endif
	shaderSourceDesc.chain.next = nullptr;

	WGPUShaderModuleDescriptor shaderDesc{};
	shaderDesc.nextInChain = &shaderSourceDesc.chain;
	WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(m_wgpuCtx.device.get(), &shaderDesc);

	// Vertex state
	pipelineDesc.vertex.bufferCount = 0;
	pipelineDesc.vertex.buffers = nullptr;
	pipelineDesc.vertex.module = shaderModule;
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	pipelineDesc.vertex.entryPoint = WGPUStringView{"vs_main", WGPU_STRLEN};
#else
	pipelineDesc.vertex.entryPoint = "vs_main";
#endif
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
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	fragment.entryPoint = WGPUStringView{"fs_main", WGPU_STRLEN};
#else
	fragment.entryPoint = "fs_main";
#endif
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
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	colorTarget.format = wgpuUtils::getPreferredFormat(m_wgpuCtx.adapter.get(), m_wgpuCtx.surface.get());
#else
	colorTarget.format = wgpuSurfaceGetPreferredFormat(m_wgpuCtx.surface.get(), m_wgpuCtx.adapter.get());
#endif
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

	auto [nextTextureView, texture] = GetNextSurfaceTextureView();
	if (nextTextureView == nullptr) // swap chain becomes invalidated on a window resize
	{
		std::cerr << "Skipping render frame" << std::endl;
		return;
	}

	// First create the command encoder for this frame
	WGPUCommandEncoderDescriptor encoderDesc{};
	encoderDesc.nextInChain = nullptr;
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	encoderDesc.label = {"My command encoder", WGPU_STRLEN};
#else
	encoderDesc.label = "My command encoder";
#endif
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
	renderPassColorAttachment.view = nextTextureView;
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
	wgpuRenderPassEncoderRelease(renderPass);

	// create the command
	WGPUCommandBufferDescriptor cmdBufferDesc{};
	cmdBufferDesc.nextInChain = nullptr;
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	cmdBufferDesc.label = {"Command Buffer", WGPU_STRLEN};
#else
	cmdBufferDesc.label = "Command Buffer";
#endif
	WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDesc);

	// Submit the command to the queue
	wgpuQueueSubmit(m_wgpuCtx.queue.get(), 1, &command);

#if !defined(WEBGPU_BACKEND_EMSCRIPTEN)
	wgpuSurfacePresent(m_wgpuCtx.surface.get());
#endif

	// Cleanup
	wgpuCommandEncoderRelease(encoder);
	wgpuCommandBufferRelease(command);
	wgpuTextureViewRelease(nextTextureView);
	wgpuTextureRelease(texture);  // WGPU requires releasing texture after texture view

#if defined(WEBGPU_BACKEND_DAWN)
	wgpuDeviceTick(m_wgpuCtx.device.get());
#elif defined(WEBGPU_BACKEND_WGPU)
	wgpuDevicePoll(m_wgpuCtx.device.get(), false, nullptr);
#endif

	++tick;
	LogDeviceErrors();
}

std::tuple<WGPUTextureView, WGPUTexture> App::GetNextSurfaceTextureView()
{
	WGPUSurfaceTexture surfaceTexture;
	wgpuSurfaceGetCurrentTexture(m_wgpuCtx.surface.get(), &surfaceTexture);

	switch(surfaceTexture.status)
	{
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
		case WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal:
		case WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal:
#else
		case WGPUSurfaceGetCurrentTextureStatus_Success:
#endif
			break;
		case WGPUSurfaceGetCurrentTextureStatus_Outdated:
		case WGPUSurfaceGetCurrentTextureStatus_Timeout:
		case WGPUSurfaceGetCurrentTextureStatus_Lost:
		{
			// Reconfigure the surface skip current frame
			if (surfaceTexture.texture != nullptr)
				wgpuTextureRelease(surfaceTexture.texture);

			glfwGetWindowSize(m_pWindow.get(), &m_windowDim.width, &m_windowDim.height);
			wgpuUtils::configureSurface(m_wgpuCtx.surface.get(), m_wgpuCtx.device.get(), m_wgpuCtx.adapter.get(), m_windowDim.width, m_windowDim.height);

			return {nullptr, nullptr};
		}
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
		case WGPUSurfaceGetCurrentTextureStatus_Error:
#endif
		case WGPUSurfaceGetCurrentTextureStatus_OutOfMemory:
		case WGPUSurfaceGetCurrentTextureStatus_DeviceLost:
		case WGPUSurfaceGetCurrentTextureStatus_Force32:
			std::cerr << "Texture could not be retrieved. Error: " << std::hex << surfaceTexture.status << std::dec << std::endl;
			return {nullptr, nullptr};
	}

	// Successfully retrieved texture, so create a TextureView from it
	WGPUTextureViewDescriptor viewDesc = {};
	viewDesc.nextInChain = nullptr;
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	viewDesc.label = {"Surface texture view", WGPU_STRLEN};
#else
	viewDesc.label = "Surface texture view";
#endif
	viewDesc.format = wgpuTextureGetFormat(surfaceTexture.texture);
	viewDesc.dimension = WGPUTextureViewDimension_2D;
	viewDesc.baseMipLevel = 0;
	viewDesc.mipLevelCount = 1;
	viewDesc.baseArrayLayer = 0;
	viewDesc.arrayLayerCount = 1;
	viewDesc.aspect = WGPUTextureAspect_All;

	WGPUTextureView textureView = wgpuTextureCreateView(surfaceTexture.texture, &viewDesc);

	// REMEMBER to release both texture and texture view at end of frame!
	return {textureView, surfaceTexture.texture};
}
