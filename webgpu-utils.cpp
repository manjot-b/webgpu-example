#include "webgpu-utils.hpp"

#if defined(WEBGPU_BACKEND_EMSCRIPTEN)
#include <emscripten.h>
#endif

#include <cassert>
#include <iostream>


#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
#include <string_view>
#endif

#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
std::ostream& operator<<(std::ostream& os, WGPUStringView strView)
{
	return os << std::string_view(strView.data, strView.length);
}
#endif

namespace wgpuUtils{

WGPUAdapter requestAdapter(WGPUInstance instance, const WGPURequestAdapterOptions* options)
{
	/**
	 * Holds local information shared with the onAdapterRequestEnded callback
	 */
	struct UserData
	{
		WGPUAdapter adapter = nullptr;
		bool requestEnded = false;	// TODO: Don't need this if using new WGPUFutureWaitInfo
	} userData;

#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* pUserData1, [[maybe_unused]]void* pUserData2)
#else
	auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* pUserData1)
#endif
	{
		UserData& userData = *reinterpret_cast<UserData*>(pUserData1);
		if (status == WGPURequestAdapterStatus_Success)
			userData.adapter = adapter;
		else
			std::cerr << "Could not retrieve WebGPU adapter: " << message << std::endl;
		userData.requestEnded = true;
	};

#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	WGPURequestAdapterCallbackInfo callbackInfo;
	callbackInfo.callback = onAdapterRequestEnded;
	callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
	callbackInfo.nextInChain = nullptr;
	callbackInfo.userdata1 = static_cast<void*>(&userData);
	callbackInfo.userdata2 = nullptr;

	[[maybe_unused]]WGPUFuture future = wgpuInstanceRequestAdapter(instance, options, callbackInfo);

	#if !defined(WEBGPU_FUTURE_UNIMPLEMENTED)
		// TODO: Wait on Future once implemented
		WGPUFutureWaitInfo futureWait;
		futureWait.future = future;
		futureWait.completed = false;
		while(wgpuInstanceWaitAny(instance, 1, &futureWait, 0) != WGPUWaitStatus_Success)
		{
		#if defined(WEBGPU_BACKEND_EMSCRIPTEN)
			emscripten_sleep(100);
		#endif  // WEBGPU_BACKEND_EMSCRIPTEN
		}
	#endif  // WEBGPU_FUTURE_UNIMPLEMENTED
#else  // EMSCRIPTEN_WEBGPU_DEPRECATED
	wgpuInstanceRequestAdapter(instance, options, onAdapterRequestEnded, static_cast<void*>(&userData));
	while (!userData.requestEnded)
		emscripten_sleep(100);
#endif  // EMSCRIPTEN_WEBGPU_DEPRECATED

	// The callback is called synchronously so the following assert should never fire.
	assert(userData.requestEnded);

	return userData.adapter;
}

WGPUDevice requestDevice([[maybe_unused]]WGPUInstance instance, WGPUAdapter adapter, const WGPUDeviceDescriptor* descriptor)
{
	/**
	 * Holds local information shared with the onDeviceRequest callback
	 */
	struct UserData
	{
		WGPUDevice device = nullptr;
		bool requestEnded = false; // TODO: Is this still required with WGPUFuture?
	} userData;

#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* pUserData1, [[maybe_unused]]void* pUserData2)
#else
	auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, const char* message, void* pUserData1)
#endif
	{
		UserData& userData = *reinterpret_cast<UserData*>(pUserData1);
		if (status == WGPURequestDeviceStatus_Success)
			userData.device = device;
		else
			std::cerr << "Could not retrieve WebGPU device: " << message << std::endl;
		userData.requestEnded = true;
	};

#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	WGPURequestDeviceCallbackInfo callbackInfo;
	callbackInfo.callback = onDeviceRequestEnded;
	callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
	callbackInfo.nextInChain = nullptr;
	callbackInfo.userdata1 = static_cast<void*>(&userData);
	callbackInfo.userdata2 = nullptr;

	[[maybe_unused]]WGPUFuture future = wgpuAdapterRequestDevice(adapter, descriptor, callbackInfo);

	#if !defined(WEBGPU_FUTURE_UNIMPLEMENTED)
		// TODO: Wait on Future once implemented
		WGPUFutureWaitInfo futureWait;
		futureWait.future = future;
		futureWait.completed = false;

		while(wgpuInstanceWaitAny(instance, 1, &futureWait, 0) != WGPUWaitStatus_Success)
		{
		#if defined(WEBGPU_BACKEND_EMSCRIPTEN)
			// TODO: Does emscripten still need this. Will timeout work on all platforms?
			emscripten_sleep(100);
		#endif  // WEBGPU_BACKEND_EMSCRIPTEN
		}
	#endif // WEBGPU_FUTURE_UNIMPLEMENTED
#else  // EMSCRIPTEN_WEBGPU_DEPRECATED
	wgpuAdapterRequestDevice(adapter, descriptor, onDeviceRequestEnded, static_cast<void*>(&userData));
	while (!userData.requestEnded)
		emscripten_sleep(100);
#endif // EMSCRIPTEN_WEBGPU_DEPRECATED
	// The callback is called synchronously so the following assert should never fire.
	assert(userData.requestEnded);

	return userData.device;
}

void printAdapterFeatures(WGPUAdapter adapter)
{
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	WGPUSupportedFeatures features{};
	wgpuAdapterGetFeatures(adapter, &features);
#else
	std::vector<WGPUFeatureName> features;
	size_t featureCount = wgpuAdapterEnumerateFeatures(adapter, nullptr);
	features.resize(featureCount);
	wgpuAdapterEnumerateFeatures(adapter, features.data());
#endif

	std::cout << "Adapter features:" << std::endl;
	std::cout << std::hex;
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	for (size_t i = 0; i < features.featureCount; ++i)
		std::cout << "0x" << features.features[i] << std::endl;
#else
	for (auto f : features)
		std::cout << "0x" << f << std::endl;
#endif
	std::cout << std::dec;
}

void printAdapterProperties(WGPUAdapter adapter)
{
	WGPUAdapterInfo info{};
	wgpuAdapterGetInfo(adapter, &info);

	std::cout << "Adapter info: " << std::endl;
	std::cout << "vendorID: "     << info.vendorID << std::endl;
	std::cout << "vendor: "       << info.vendor << std::endl;
	std::cout << "architecture: " << info.architecture << std::endl;
	std::cout << "deviceID: "     << info.deviceID << std::endl;
	std::cout << "device: "       << info.device << std::endl;
	std::cout << "description: "  << info.description << std::endl;
	std::cout << std::hex;
	std::cout << "adapterType: 0x" << info.adapterType << std::endl;
	std::cout << "backendType: 0x" << info.backendType << std::endl;
	std::cout << std::dec;
}

void configureSurface(WGPUSurface surface, WGPUDevice device, WGPUAdapter adapter, int width, int height)
{
	WGPUSurfaceConfiguration surfaceConfig = {};
	surfaceConfig.width = width;
	surfaceConfig.height = height;
#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
	surfaceConfig.format = getPreferredFormat(adapter, surface);
#else
	surfaceConfig.format = wgpuSurfaceGetPreferredFormat(surface, adapter);
#endif // EMSCRIPTEN_WEBGPU_DEPRECATED
	surfaceConfig.viewFormatCount = 0;
	surfaceConfig.viewFormats = nullptr;
	surfaceConfig.usage = WGPUTextureUsage_RenderAttachment;
	surfaceConfig.device = device;
	surfaceConfig.presentMode = WGPUPresentMode_Fifo;
	surfaceConfig.alphaMode = WGPUCompositeAlphaMode_Auto;

	wgpuSurfaceConfigure(surface, &surfaceConfig);
}

#if !defined(EMSCRIPTEN_WEBGPU_DEPRECATED)
WGPUTextureFormat getPreferredFormat(WGPUAdapter adapter, WGPUSurface surface)
{
	WGPUSurfaceCapabilities capabilities{};
	if (wgpuSurfaceGetCapabilities(surface, adapter, &capabilities) != WGPUStatus_Success
			|| capabilities.formatCount == 0)
	{
		assert(false && "Surface capabilities could not be retrieved");
		return WGPUTextureFormat_Undefined;
	}

	// Store format before freeing. Formats are listed in order of preference
	WGPUTextureFormat preferredFormat = capabilities.formats[0];
	wgpuSurfaceCapabilitiesFreeMembers(capabilities);
	return preferredFormat;
}
#endif

} // namespace utils
