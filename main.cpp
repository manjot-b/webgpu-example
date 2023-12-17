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
