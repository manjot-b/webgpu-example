#include "App.hpp"
#include "webgputypes.hpp"

#if defined(WEBGPU_BACKEND_WGPU)
#include <webgpu/wgpu.h>
#endif

#include <GLFW/glfw3.h>
#include <iostream>
#include <sstream>
#include <array>
#include <numeric>

#include "glfw3webgpu.hpp"
#include "webgpu-utils.hpp"

App::App() :
	m_terminated(false),
	m_window(nullptr, glfwDestroyWindow),
	m_windowDim{1280, 720},
	m_uniformsBuffer(nullptr, [](WGPUBuffer){}),
	m_bindGroupLayout(nullptr, wgpuBindGroupLayoutRelease),
	m_pipelineLayout(nullptr, wgpuPipelineLayoutRelease),
	m_bindGroup(nullptr, wgpuBindGroupRelease)
{
	m_uniforms.ratio = static_cast<float>(m_windowDim.width) / m_windowDim.height;
	m_uniforms.color = {};

	m_initialized = Initialize();
}

App::~App()
{
	if (!m_terminated)
		Terminate();
}

void App::Terminate()
{
	m_indicies.m_wgpuBuffer.reset();
	m_verticies.m_wgpuBuffer.reset();
	m_uniformsBuffer.reset();
	m_bindGroupLayout.reset();
	m_pipelineLayout.reset();
	m_bindGroup.reset();
	m_texture.~WgpuTexture();

	// Call dtor so that objects are destroyed in correct order
	m_wgpuCtx.~WgpuContext();

	m_window.reset();
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

bool App::IsRunning() const { return m_initialized && !m_terminated && !glfwWindowShouldClose(m_window.get()); }

bool App::IsInitialized() const { return m_initialized; }

bool App::Initialize()
{
	// Init Glfw
	m_window = GlfwInitialize();
	if (m_window == nullptr)
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

	glfwSetWindowUserPointer(m_window.get(), static_cast<void*>(this));
	glfwSetFramebufferSizeCallback(m_window.get(), [](GLFWwindow* pWindow, int width, int height){
			App &app = *static_cast<App*>(glfwGetWindowUserPointer(pWindow));
			app.m_windowDim = WindowDimensions{width, height};
			app.m_uniforms.ratio = static_cast<float>(width) / height;
	});

	BuffersInitialize();
	WgpuTextureInitialize();

	// Init Wgpu Pipeline
	m_wgpuCtx.pipeline = WgpuRenderPipelineInitialize();
	if (!m_wgpuCtx.pipeline)
	{
		std::cerr << "Could not initialize WebGPU pipeline. Aborting initialization." << std::endl;
		return false;
	}

	WgpuBindGroupsInitialize();

	// On Emscripten the errors might not be captured yet because the callback is asynchronous.
	if (LogDeviceErrors())
	{
		std::cerr << "Device errors encountered during initialization. Aborting initialization" << std::endl;
		return false;
	}

	return true;
}

App::GlfwWindowPtr App::GlfwInitialize()
{
	// Setup GLFW
	if (!glfwInit())
	{
		std::cerr << "Could not initialize GLFW" << std::endl;
		return GlfwWindowPtr(nullptr, glfwDestroyWindow);
	}

	// GLFW is unaware of WebGPU (at the moment) so do not setup any graphics API with it
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* pWindow = glfwCreateWindow(m_windowDim.width, m_windowDim.height, "Learn WebGPU", nullptr, nullptr);
	if (!pWindow)
	{
		std::cerr << "Could not create window" << std::endl;
		return GlfwWindowPtr(nullptr, glfwDestroyWindow);
	}

	return GlfwWindowPtr(pWindow, glfwDestroyWindow);
}

#if defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
WGPURequiredLimits App::GetRequiredLimits(WGPUAdapter adapter) const
{
	WGPUSupportedLimits supLimits{};
	wgpuAdapterGetLimits(adapter, &supLimits);
	const WGPULimits &supportedLimits = supLimits.limits;
#else
WGPULimits App::GetRequiredLimits(WGPUAdapter adapter) const
{
	WGPULimits supportedLimits{};
	wgpuAdapterGetLimits(adapter, &supportedLimits);
#endif

	// Good practice to set these limits as low as possible to support the largest amount of devices
	WGPULimits limits = supportedLimits;
	limits.maxVertexAttributes =         3;
	limits.maxVertexBuffers =            1;
	limits.maxBufferSize =               4 * 256 * 256;
	limits.maxVertexBufferArrayStride =  7 * sizeof(float);
	limits.maxBindGroups =               1;
	limits.maxUniformBufferBindingSize = 16 * sizeof(float);
	limits.maxSampledTexturesPerShaderStage = 1;
#if defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	limits.maxInterStageShaderComponents = WGPU_LIMIT_U32_UNDEFINED;  // This is removed in latest webgpu but firefox complains about this
#endif

#if defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	WGPURequiredLimits reqLimits{};
	reqLimits.limits = limits;
	return reqLimits;
#else
	return limits;
#endif
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
		glfwCreateWindowWGPUSurface(ctx.instance.get(), m_window.get()),
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
	wgpuUtils::printAdapterLimits(ctx.adapter.get());

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
	deviceDesc.defaultQueue.nextInChain = nullptr;
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	WGPULimits limits = GetRequiredLimits(ctx.adapter.get());
	deviceDesc.requiredLimits = &limits;
	deviceDesc.label = {"My Device", WGPU_STRLEN};
	deviceDesc.defaultQueue.label = {"Default Queue", WGPU_STRLEN};
	deviceDesc.deviceLostCallbackInfo.nextInChain = nullptr;
	deviceDesc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
	deviceDesc.deviceLostCallbackInfo.callback = onDeviceLost;
	deviceDesc.uncapturedErrorCallbackInfo.nextInChain = nullptr;
	deviceDesc.uncapturedErrorCallbackInfo.userdata1 = this;
	deviceDesc.uncapturedErrorCallbackInfo.callback = onDeviceError;
#else
	WGPURequiredLimits limits = GetRequiredLimits(ctx.adapter.get());
	deviceDesc.requiredLimits = &limits;
	deviceDesc.label = "My Device";
	deviceDesc.defaultQueue.label = "Default Queue";
	deviceDesc.deviceLostCallback = onDeviceLost;
#endif
	deviceDesc.requiredLimits = &limits;

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

	wgpuUtils::printDeviceLimits(ctx.device.get());

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
		while (wgpuInstanceWaitAny(ctx.instance.get(), 1, &queueFutureWait, 0) != WGPUWaitStatus_Success);
	#endif
#else
	wgpuQueueOnSubmittedWorkDone(ctx.queue.get(), onQueueWorkDone, nullptr);
#endif  // EMSCRIPTEN_WEBGPU_DEPRECATED

	wgpuUtils::configureSurface(ctx.surface.get(), ctx.device.get(), ctx.adapter.get(), m_windowDim.width, m_windowDim.height);

#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	WGPUTextureFormat textureFormat = wgpuUtils::getPreferredFormat(ctx.adapter.get(), ctx.surface.get());
#else
	WGPUTextureFormat textureFormat = wgpuSurfaceGetPreferredFormat(m_wgpuCtx.surface.get(), m_wgpuCtx.adapter.get());
#endif

	std::cout << "Preferred Format: 0x" << std::hex << textureFormat << std::endl;

	ctx.initialized = true;
	return ctx;
}

void App::BuffersInitialize()
{
	const std::vector<float> verticies = {
		// x,    y,   r,   g,   b,   u,   v
		-0.5, -0.5, 1.0, 0.0, 0.0, 0.0, 1.0,
		 0.5,  0.5, 0.0, 1.0, 0.0, 1.0, 0.0,
		-0.5,  0.5, 0.0, 0.0, 1.0, 0.0, 0.0,
		 0.5, -0.5, 0.0, 0.4, 0.6, 1.0, 1.0,

		// Rotated/skewed for anti-aliasing
		-0.9, -0.4, 1.0, 1.0, 0.0, 0.0, 1.0,
		-0.6,  0.0, 1.0, 0.0, 1.0, 1.0, 1.0,
		-0.7,  0.5, 0.0, 1.0, 1.0, 0.5, 0.0,
	};

	const std::vector<uint32_t> indicies = {
		0, 1, 2,
		0, 3, 1,
		4, 5, 6,
	};

	// Vertex buffer
	WGPUBufferDescriptor bufferDesc{};
	bufferDesc.size = verticies.size() * sizeof(verticies[0]);
	bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
	bufferDesc.mappedAtCreation = false;
	WgpuBufferPtr wgpuBuffer = WgpuBufferPtr(wgpuDeviceCreateBuffer(m_wgpuCtx.device.get(), &bufferDesc), wgpuBufferRelease);

	std::vector<size_t> attribComponents = {2, 3, 2};
	m_verticies = WgpuBuffer(verticies.size(), sizeof(verticies[0]), std::move(attribComponents), std::move(wgpuBuffer));

	// Index buffer
	bufferDesc.size = indicies.size() * sizeof(indicies[0]);
	bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
	bufferDesc.mappedAtCreation = false;
	wgpuBuffer = WgpuBufferPtr(wgpuDeviceCreateBuffer(m_wgpuCtx.device.get(), &bufferDesc), wgpuBufferRelease);

	attribComponents = {1};
	m_indicies = WgpuBuffer(indicies.size(), sizeof(indicies[0]), std::move(attribComponents), std::move(wgpuBuffer));

	wgpuQueueWriteBuffer(m_wgpuCtx.queue.get(), m_verticies.m_wgpuBuffer.get(), 0, verticies.data(), m_verticies.m_size);
	wgpuQueueWriteBuffer(m_wgpuCtx.queue.get(), m_indicies.m_wgpuBuffer.get(), 0, indicies.data(), m_indicies.m_size);

	// Uniform buffer
	bufferDesc.size = sizeof(Uniforms);
	bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
	bufferDesc.mappedAtCreation = false;
	m_uniformsBuffer = WgpuBufferPtr(wgpuDeviceCreateBuffer(m_wgpuCtx.device.get(), &bufferDesc), wgpuBufferRelease);
}

bool App::WgpuBuffer::SetInfo(size_t count, size_t componentSize, std::vector<size_t> attributeComponents, WgpuBufferPtr wgpuBuffer)
{
	if (count == 0 || componentSize == 0 || attributeComponents.size() == 0)
		return true;

	m_count = count;
	m_size = count * componentSize;

	m_components = std::accumulate(attributeComponents.begin(), attributeComponents.end(), 0);
	m_componentSize = componentSize;

	const bool validInfo = ( (m_components > 0) && (m_count % m_components) == 0);
	if (!validInfo)
	{
		assert(validInfo && "Valid attribute sizes not given");
		return false;
	}

	m_stride = m_components * m_componentSize;
	m_attributes = attributeComponents.size();

	m_attributeOffset.resize(attributeComponents.size());
	for (size_t i = 0; i < attributeComponents.size(); ++i)
	{
		if (i == 0)
			m_attributeOffset[i] = 0;
		else
			m_attributeOffset[i] = m_attributeOffset[i-1] + (attributeComponents[i-1] * componentSize);
	}

	m_attributeComponents = std::move(attributeComponents);
	m_wgpuBuffer = std::move(wgpuBuffer);

	return true;
}

WgpuRenderPipelinePtr App::WgpuRenderPipelineInitialize()
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
	WgpuShaderModulePtr shaderModule(
			wgpuDeviceCreateShaderModule(m_wgpuCtx.device.get(), &shaderDesc),
			wgpuShaderModuleRelease
	);

	// Vertex state
	std::array<WGPUVertexAttribute, 3> vertAttribs;
	// pos
	vertAttribs[0].shaderLocation = 0;
	vertAttribs[0].format = WGPUVertexFormat_Float32x2;
	vertAttribs[0].offset = m_verticies.m_attributeOffset[0];
	// color
	vertAttribs[1].shaderLocation = 1;
	vertAttribs[1].format = WGPUVertexFormat_Float32x3;
	vertAttribs[1].offset = m_verticies.m_attributeOffset[1];
	// uv
	vertAttribs[2].shaderLocation = 2;
	vertAttribs[2].format = WGPUVertexFormat_Float32x2;
	vertAttribs[2].offset = m_verticies.m_attributeOffset[2];

	WGPUVertexBufferLayout vertBufLayout{};
	vertBufLayout.attributeCount = vertAttribs.size();
	vertBufLayout.attributes = vertAttribs.data();
	vertBufLayout.arrayStride = m_verticies.m_stride;
	vertBufLayout.stepMode = WGPUVertexStepMode_Vertex;

	pipelineDesc.vertex.bufferCount = 1;
	pipelineDesc.vertex.buffers = &vertBufLayout;
	pipelineDesc.vertex.module = shaderModule.get();
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
	fragment.module = shaderModule.get();
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

	// Binding Layout
	std::array<WGPUBindGroupLayoutEntry, 2> bindingLayoutEntries;

	WGPUBindGroupLayoutEntry &bindingLayout = bindingLayoutEntries[0];
	bindingLayout = wgpuUtils::getDefault<WGPUBindGroupLayoutEntry>();
	bindingLayout.binding = 0;
	bindingLayout.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
	bindingLayout.buffer.type = WGPUBufferBindingType_Uniform;
	bindingLayout.buffer.minBindingSize = sizeof(Uniforms);

	WGPUBindGroupLayoutEntry &textureBindingLayout = bindingLayoutEntries[1];
	textureBindingLayout = wgpuUtils::getDefault<WGPUBindGroupLayoutEntry>();
	textureBindingLayout.binding = 1;
	textureBindingLayout.visibility = WGPUShaderStage_Fragment;
	textureBindingLayout.texture.sampleType = WGPUTextureSampleType_Float;
	textureBindingLayout.texture.viewDimension = WGPUTextureViewDimension_2D;

	WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc{};
	bindGroupLayoutDesc.entryCount = bindingLayoutEntries.size();
	bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
	WGPUBindGroupLayout bgLayout = wgpuDeviceCreateBindGroupLayout(m_wgpuCtx.device.get(), &bindGroupLayoutDesc);  // Needed to taked address of pointer
	m_bindGroupLayout = WgpuBindGroupLayoutPtr(bgLayout, wgpuBindGroupLayoutRelease);

	WGPUPipelineLayoutDescriptor pipelineLayoutDesc{};
	pipelineLayoutDesc.bindGroupLayoutCount = 1;
	pipelineLayoutDesc.bindGroupLayouts = &bgLayout;
	m_pipelineLayout = WgpuPipelineLayoutPtr(
			wgpuDeviceCreatePipelineLayout(m_wgpuCtx.device.get(), &pipelineLayoutDesc),
			wgpuPipelineLayoutRelease);
	pipelineDesc.layout = m_pipelineLayout.get();

	return WgpuRenderPipelinePtr(
			wgpuDeviceCreateRenderPipeline(m_wgpuCtx.device.get(), &pipelineDesc),
			wgpuRenderPipelineRelease
		);
}

const char* App::GetShaderSource() const
{
	static const char* shaderSource = R"(
struct VertexInput
{
	@location(0) position: vec2f,
	@location(1) color: vec3f,
	@location(2) uv: vec2f,
};

struct VertexOutput
{
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
	@location(1) uv: vec2f,
};

struct Uniforms
{
	ratio: f32,
	gamma: f32,
	color: vec4f,  // will be aligned to 16 byte boundary. Cpp struct must match
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var texture: texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput
{
	var out: VertexOutput;
	out.position = vec4f(in.position.x, in.position.y * uniforms.ratio, 0.0, 1.0);
	out.color = in.color;
	out.uv = in.uv;

	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f  // output location is the ColorAttachment
{
	//let color = in.color * uniforms.color.rgb;

	let texelCoords = vec2i( in.uv * vec2f(textureDimensions(texture)) );
	let color = textureLoad(texture, texelCoords, 0).rgb * uniforms.color.rgb;

	// Gamma correction
	let linearColor = pow(color, vec3f(uniforms.gamma));
	return vec4f(linearColor, 1.0);
})";

	return shaderSource;
}

void App::WgpuBindGroupsInitialize()
{
	std::array<WGPUBindGroupEntry, 2> bindings{};

	WGPUBindGroupEntry &binding = bindings[0];
	binding.binding = 0;
	binding.buffer = m_uniformsBuffer.get();
	binding.offset = 0;
	binding.size = sizeof(Uniforms);

	WGPUBindGroupEntry &textureBinding = bindings[1];
	textureBinding.binding = 1;
	textureBinding.textureView = m_texture.textureView.get();

	WGPUBindGroupDescriptor bindGroupDesc{};
	bindGroupDesc.layout = m_bindGroupLayout.get();
	bindGroupDesc.entryCount = bindings.size();
	bindGroupDesc.entries = bindings.data();
	m_bindGroup = WgpuBindGroupPtr(
			wgpuDeviceCreateBindGroup(m_wgpuCtx.device.get(), &bindGroupDesc),
			wgpuBindGroupRelease);
}

void App::WgpuTextureInitialize()
{
	WGPUTextureDescriptor textureDesc{};
	textureDesc.dimension = WGPUTextureDimension_2D;
	textureDesc.size = {256, 256, 1};
	textureDesc.mipLevelCount = 1;
	textureDesc.sampleCount = 1;
	textureDesc.format = WGPUTextureFormat_RGBA8Unorm;
	textureDesc.usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding;
	textureDesc.viewFormatCount = 0;
	textureDesc.viewFormats = nullptr;

	m_texture.texture = WgpuTexturePtr(wgpuDeviceCreateTexture(m_wgpuCtx.device.get(), &textureDesc), wgpuTextureRelease);

	WGPUTextureViewDescriptor viewDesc{};
	viewDesc.aspect = WGPUTextureAspect_All;
	viewDesc.baseArrayLayer = 0;
	viewDesc.arrayLayerCount = 1;
	viewDesc.baseMipLevel = 0;
	viewDesc.mipLevelCount = 1;
	viewDesc.dimension = WGPUTextureViewDimension_2D;
	viewDesc.format = textureDesc.format;

	m_texture.textureView = WgpuTextureViewPtr(wgpuTextureCreateView(m_texture.texture.get(), &viewDesc), wgpuTextureViewRelease);

	// sample image data
	std::vector<uint8_t> pixels(4 * textureDesc.size.width * textureDesc.size.height);
	for (size_t i = 0; i < textureDesc.size.height; ++i)
	{
		for (size_t j = 0; j < textureDesc.size.width; ++j)
		{
			uint8_t *p = &pixels[4 * (i * textureDesc.size.width + j)];
			p[0] = (uint8_t)i;
			p[1] = (uint8_t)j;
			p[2] = 255;
			p[3] = 255;
		}
	}

#if defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	WGPUImageCopyTexture destination{};
	WGPUTextureDataLayout source{};
#else
	WGPUTexelCopyTextureInfo destination{};
	WGPUTexelCopyBufferLayout source{};
#endif

	destination.texture = m_texture.texture.get();
	destination.mipLevel = 0;  // set to first (original) image mip level
	destination.origin = {0, 0, 0};
	destination.aspect = WGPUTextureAspect_All;  // only relevant for depth/stencil textures

	source.offset = 0;
	source.bytesPerRow = 4 * sizeof(pixels[0]) * textureDesc.size.width;
	source.rowsPerImage = textureDesc.size.height;

	wgpuQueueWriteTexture(m_wgpuCtx.queue.get(), &destination, pixels.data(), pixels.size(), &source, &textureDesc.size);
}

void App::Tick()
{
	glfwPollEvents();

	WgpuTexturePtr nextTexture( nullptr, [](WGPUTexture){} );
	WgpuTextureViewPtr nextTextureView( nullptr, [](WGPUTextureView){} );
	{
		auto [textureView, texture] = GetNextSurfaceTextureView();
		if (textureView == nullptr) // swap chain becomes invalidated on a window resize
		{
			std::cerr << "Skipping render frame" << std::endl;
			return;
		}
		nextTexture = WgpuTexturePtr(texture, wgpuTextureRelease);
		nextTextureView = WgpuTextureViewPtr(textureView, wgpuTextureViewRelease);
	}

	static unsigned long tick = 0;
	static float colorVal = 1.0f;
	static float delta = .01;
	if (tick % 10 == 0)
	{
		colorVal += delta;
		if (colorVal < 0 || colorVal > 1) { delta *= -1; }
	}

	// Update uniforms
	UpdateGamma(nextTexture.get());
	m_uniforms.color = {colorVal, colorVal, colorVal, 1.0f};
	wgpuQueueWriteBuffer(m_wgpuCtx.queue.get(), m_uniformsBuffer.get(), 0, &m_uniforms, sizeof(Uniforms));

	// First create the command encoder for this frame
	WGPUCommandEncoderDescriptor encoderDesc{};
	encoderDesc.nextInChain = nullptr;
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	encoderDesc.label = {"My command encoder", WGPU_STRLEN};
#else
	encoderDesc.label = "My command encoder";
#endif
	WgpuCommandEncoderPtr encoder(
			wgpuDeviceCreateCommandEncoder(m_wgpuCtx.device.get(), &encoderDesc),
			wgpuCommandEncoderRelease
	);

	// Next create the render pass encoder
	WGPURenderPassColorAttachment renderPassColorAttachment{};
	renderPassColorAttachment.view = nextTextureView.get();
	renderPassColorAttachment.resolveTarget = nullptr;
	renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
	renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
	renderPassColorAttachment.clearValue = WGPUColor{ colorVal, .25, .4, 1.0 };  // This will default to sRGB or RGB  depending on preferred texture format
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
	WgpuRenderPassEncoderPtr renderPass(
			wgpuCommandEncoderBeginRenderPass(encoder.get(), &renderPassDesc),
			wgpuRenderPassEncoderRelease
	);
	wgpuRenderPassEncoderSetPipeline(renderPass.get(), m_wgpuCtx.pipeline.get());

	wgpuRenderPassEncoderSetVertexBuffer(renderPass.get(), 0, m_verticies.m_wgpuBuffer.get(), 0, m_verticies.m_size);
	wgpuRenderPassEncoderSetIndexBuffer(renderPass.get(), m_indicies.m_wgpuBuffer.get(), WGPUIndexFormat_Uint32, 0, m_indicies.m_size);

	wgpuRenderPassEncoderSetBindGroup(renderPass.get(), 0, m_bindGroup.get(), 0, nullptr);

	wgpuRenderPassEncoderDrawIndexed(renderPass.get(), m_indicies.m_count, 1, 0, 0, 0);
	wgpuRenderPassEncoderEnd(renderPass.get());

	// create the command
	WGPUCommandBufferDescriptor cmdBufferDesc{};
	cmdBufferDesc.nextInChain = nullptr;
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	cmdBufferDesc.label = {"Command Buffer", WGPU_STRLEN};
#else
	cmdBufferDesc.label = "Command Buffer";
#endif
	WgpuCommandBufferPtr command(
			wgpuCommandEncoderFinish(encoder.get(), &cmdBufferDesc),
			wgpuCommandBufferRelease
	);

	{
		// Submit the command to the queue
		WGPUCommandBuffer buf = command.get();  // Hack to get the address of the pointer
		wgpuQueueSubmit(m_wgpuCtx.queue.get(), 1, &buf);
	}

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

			glfwGetWindowSize(m_window.get(), &m_windowDim.width, &m_windowDim.height);
			wgpuUtils::configureSurface(m_wgpuCtx.surface.get(), m_wgpuCtx.device.get(), m_wgpuCtx.adapter.get(), m_windowDim.width, m_windowDim.height);

			return {nullptr, nullptr};
		}
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
		case WGPUSurfaceGetCurrentTextureStatus_Error:
#endif
#if !defined(WEBGPU_BACKEND_DAWN)
		case WGPUSurfaceGetCurrentTextureStatus_OutOfMemory:
		case WGPUSurfaceGetCurrentTextureStatus_DeviceLost:
#endif
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

void App::UpdateGamma(const WGPUTexture texture)
{
	switch (wgpuTextureGetFormat(texture))
	{
		case WGPUTextureFormat_RGBA8UnormSrgb:
			m_uniforms.gamma = 2.2f;
			break;
		default:
			m_uniforms.gamma = 1.0f;
			break;
	}
}
