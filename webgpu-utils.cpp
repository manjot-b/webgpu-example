#include "webgpu-utils.hpp"

#if defined(WEBGPU_BACKEND_EMSCRIPTEN)
#include <emscripten.h>
#endif

#include <cassert>
#include <iostream>
#include <vector>

namespace wgpuUtils{

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

#if defined(WEBGPU_BACKEND_EMSCRIPTEN)
	while (!userData.requestEnded)
		emscripten_sleep(100);
#endif

	// The callback is called synchronously so the following assert should never fire.
	assert(userData.requestEnded);

	return userData.adapter;
}

WGPUDevice requestDevice(WGPUAdapter adapter, const WGPUDeviceDescriptor* descriptor)
{
	/**
	 * Holds local information shared with the onDeviceRequest callback
	 */
	struct UserData
	{
		WGPUDevice device = nullptr;
		bool requestEnded = false;
	} userData;

	auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, const char* message, void* pUserData)
	{
		UserData& userData = *reinterpret_cast<UserData*>(pUserData);
		if (status == WGPURequestDeviceStatus_Success)
		{
			userData.device = device;
		}
		else
		{
			std::cerr << "Could not retrieve WebGPU device: " << message << std::endl;
		}
		userData.requestEnded = true;
	};

	wgpuAdapterRequestDevice(adapter, descriptor, onDeviceRequestEnded, static_cast<void*>(&userData));

#if defined(WEBGPU_BACKEND_EMSCRIPTEN)
	while (!userData.requestEnded)
		emscripten_sleep(100);
#endif

	// The callback is called synchronously so the following assert should never fire.
	assert(userData.requestEnded);

	return userData.device;
}

void printAdapterFeatures(WGPUAdapter adapter)
{
	std::vector<WGPUFeatureName> features;
	size_t featureCount = wgpuAdapterEnumerateFeatures(adapter, nullptr);
	features.resize(featureCount);
	wgpuAdapterEnumerateFeatures(adapter, features.data());

	std::cout << "Adapter features:" << std::endl;
	std::cout << std::hex;
	for (auto f : features)
	{
		std::cout << "0x" << f << std::endl;
	}
	std::cout << std::dec;
}

void printAdapterProperties(WGPUAdapter adapter)
{
#if defined(WEBGPU_BACKEND_WGPU)
	WGPUAdapterProperties properties = {};
	wgpuAdapterGetProperties(adapter, &properties);

	std::cout << "Adapter properties: " << std::endl;
	std::cout << "vendorID: " << properties.vendorID << std::endl;
	std::cout << "vendorName: " << properties.vendorName << std::endl;
	std::cout << "architecture: " << properties.architecture << std::endl;
	std::cout << "deviceID: " << properties.deviceID << std::endl;
	std::cout << "name: " << properties.name << std::endl;
	std::cout << "driverDescription: " << properties.driverDescription << std::endl;
	std::cout << std::hex;
	std::cout << "adapterType: 0x" << properties.adapterType << std::endl;
	std::cout << "backendType: 0x" << properties.backendType << std::endl;
	std::cout << std::dec;
#else
	WGPUAdapterInfo info = {};
	wgpuAdapterGetInfo(adapter, &info);

	std::cout << "Adapter info: " << std::endl;
	std::cout << "vendorID: " << info.vendorID << std::endl;
	std::cout << "architecture: " << info.architecture << std::endl;
	std::cout << "device: " << info.device << std::endl;
	std::cout << "description: " << info.description << std::endl;
	std::cout << std::hex;
	std::cout << "adapterType: 0x" << info.adapterType << std::endl;
	std::cout << "backendType: 0x" << info.backendType << std::endl;
	std::cout << std::dec;
#endif
}

void configureSurface(WGPUSurface surface, WGPUDevice device, WGPUAdapter adapter, int width, int height)
{
	WGPUTextureFormat surfaceFormat = wgpuSurfaceGetPreferredFormat(surface, adapter);

	WGPUSurfaceConfiguration surfaceConfig = {};
	surfaceConfig.width = width;
	surfaceConfig.height = height;
	surfaceConfig.format = surfaceFormat;
	surfaceConfig.viewFormatCount = 0;
	surfaceConfig.viewFormats = nullptr;
	surfaceConfig.usage = WGPUTextureUsage_RenderAttachment;
	surfaceConfig.device = device;
	surfaceConfig.presentMode = WGPUPresentMode_Fifo;
	surfaceConfig.alphaMode = WGPUCompositeAlphaMode_Auto;

	wgpuSurfaceConfigure(surface, &surfaceConfig);
}

} // namespace utils
