#include "webgpu-utils.hpp"
#include "build/wgpu/_deps/webgpu-backend-wgpu-src/include/webgpu/webgpu.h"

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

	// The callback is called synchronously so the following assert should never fire.
	assert(userData.requestEnded);

	return userData.device;
}

} // namespace utils
