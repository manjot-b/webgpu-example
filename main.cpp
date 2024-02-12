#include <iostream>

#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>

int main (int, char**)
{
	// Setup GLFW
	glfwInit();

	if (!glfwInit())
	{
		std::cerr << "Could not initialize GLFW" << std::endl;
		return 1;
	}

	GLFWwindow* window = glfwCreateWindow(1280, 720, "Learn WebGPU", nullptr, nullptr);
	if (!window)
	{
		std::cerr << "Could not create window" << std::endl;
		return 1;
	}

	std::cout << "GLFW initialized successfully: " << window << std::endl;

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
	WGPUInstance instance = wgpuCreateInstance(&desc);

	if (!instance)
	{
		std::cerr << "Could not initialize WebGPU" << std::endl;
		return 1;
	}
	std::cout << "WebGPU initialized successfully: " << instance << std::endl;

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	// cleanup
	wgpuInstanceRelease(instance);
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
