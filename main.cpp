#include <iostream>
#include <GLFW/glfw3.h>

int main (int, char**)
{
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

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
