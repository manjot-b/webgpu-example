if (NOT EMSCRIPTEN)
	add_subdirectory(glfw)
else()
	add_library(glfw INTERFACE)
	target_link_options(glfw INTERFACE -sUSE_GLFW=3)
endif()

add_subdirectory(webgpu)
add_subdirectory(glfw3webgpu)

target_link_libraries(app PRIVATE glfw webgpu glfw3webgpu)
