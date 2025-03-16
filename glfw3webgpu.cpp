/**
 * This is an extension of GLFW for WebGPU, abstracting away the details of
 * OS-specific operations.
 *
 * This file is part of the "Learn WebGPU for C++" book.
 *   https://eliemichel.github.io/LearnWebGPU
 *
 * Most of this code comes from the wgpu-native triangle example:
 *   https://github.com/gfx-rs/wgpu-native/blob/master/examples/triangle/main.c
 *
 * MIT License
 * Copyright (c) 2022-2024 Elie Michel and the wgpu-native authors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "glfw3webgpu.hpp"

#include <webgpu/webgpu.h>

#include <GLFW/glfw3.h>


// Define #GLFW_EXPOSE_* before include  glfw3native.h header
#if defined(__EMSCRIPTEN__)
	#define GLFW_PLATFORM_EMSCRIPTEN -8 // This isn't defined on Emscripten yet. Chose a random number
#elif defined(__APPLE__)
	#define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(_WIN32)
	#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(LINUX_X11)
	#define GLFW_EXPOSE_NATIVE_X11
#elif defined(LINUX_WAYLAND)
	#define GLFW_EXPOSE_NATIVE_WAYLAND
#else
	#error No supported platform detected!
#endif

#if defined(__APPLE__)
	#include <Foundation/Foundation.h>
	#include <QuartzCore/CAMetalLayer.h>
#endif

#if !defined(__EMSCRIPTEN__)
	#include <GLFW/glfw3native.h>
#endif

WGPUSurface glfwCreateWindowWGPUSurface(WGPUInstance instance, [[maybe_unused]]GLFWwindow* window) {
#if !defined(__EMSCRIPTEN__)
	switch (glfwGetPlatform()) {
#else
	// glfwGetPlatform is not available in older versions of emscripten
	switch (GLFW_PLATFORM_EMSCRIPTEN) {
#endif

#if defined(LINUX_X11)
	case GLFW_PLATFORM_X11: {
		Display* x11_display = glfwGetX11Display();
		Window x11_window = glfwGetX11Window(window);

		WGPUSurfaceSourceXlibWindow fromXlibWindow;
		fromXlibWindow.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
		fromXlibWindow.chain.next = nullptr;
		fromXlibWindow.display = x11_display;
		fromXlibWindow.window = x11_window;

		WGPUSurfaceDescriptor surfaceDescriptor;
		surfaceDescriptor.nextInChain = &fromXlibWindow.chain;
		surfaceDescriptor.label = {};

		return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
	}
#endif // linux

#if defined(LINUX_WAYLAND)
	case GLFW_PLATFORM_WAYLAND: {
		struct wl_display* wayland_display = glfwGetWaylandDisplay();
		struct wl_surface* wayland_surface = glfwGetWaylandWindow(window);

		WGPUSurfaceSourceWaylandSurface fromWaylandSurface;
		fromWaylandSurface.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
		fromWaylandSurface.chain.next = nullptr;
		fromWaylandSurface.display = wayland_display;
		fromWaylandSurface.surface = wayland_surface;

		WGPUSurfaceDescriptor surfaceDescriptor;
		surfaceDescriptor.nextInChain = &fromWaylandSurface.chain;
		surfaceDescriptor.label = {};

		return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
	}
#endif // linux

#if defined(__APPLE__)
	case GLFW_PLATFORM_COCOA: {
		id metal_layer = [CAMetalLayer layer];
		NSWindow* ns_window = glfwGetCocoaWindow(window);
		[ns_window.contentView setWantsLayer : YES] ;
		[ns_window.contentView setLayer : metal_layer] ;

		WGPUSurfaceSourceMetalLayer fromMetalLayer;
		fromMetalLayer.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
		fromMetalLayer.chain.next = nullptr;
		fromMetalLayer.layer = metal_layer;

		WGPUSurfaceDescriptor surfaceDescriptor;
		surfaceDescriptor.nextInChain = &fromMetalLayer.chain;
		surfaceDescriptor.label = {};

		return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
	}
#endif // __APPLE__

#if defined(_WIN32)
	case GLFW_PLATFORM_WIN32: {
		HWND hwnd = glfwGetWin32Window(window);
		HINSTANCE hinstance = GetModuleHandle(nullptr);

		WGPUSurfaceSourceWindowsHWND fromWindowsHWND;
		fromWindowsHWND.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
		fromWindowsHWND.chain.next = nullptr;
		fromWindowsHWND.hinstance = hinstance;
		fromWindowsHWND.hwnd = hwnd;

		WGPUSurfaceDescriptor surfaceDescriptor;
		surfaceDescriptor.nextInChain = &fromWindowsHWND.chain;
		surfaceDescriptor.label = {};

		return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
	}
#endif // _WIN32

#if defined(__EMSCRIPTEN__)
	case GLFW_PLATFORM_EMSCRIPTEN: {
#if defined(EMSCRIPTEN_WEBGPU_DEPRECATED)  // Emscripten compiler isn't updated yet to use new types
		WGPUSurfaceDescriptorFromCanvasHTMLSelector fromCanvasHTMLSelector;
		fromCanvasHTMLSelector.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
#else
		WGPUSurfaceSourceCanvasHTMLSelector_Emscripten fromCanvasHTMLSelector;
		fromCanvasHTMLSelector.chain.sType = WGPUSType_SurfaceSourceCanvasHTMLSelector_Emscripten;
#endif
		fromCanvasHTMLSelector.chain.next = nullptr;
		fromCanvasHTMLSelector.selector = "canvas";

		WGPUSurfaceDescriptor surfaceDescriptor;
		surfaceDescriptor.nextInChain = &fromCanvasHTMLSelector.chain;
		surfaceDescriptor.label = {};

		return wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
	}
#endif // __EMSCRIPTEN__

	default:
		// Unsupported platform
		return nullptr;
	}
}
