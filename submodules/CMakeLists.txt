add_subdirectory(glfw)
add_subdirectory(webgpu)
add_subdirectory(glfw3webgpu)

target_link_libraries(app PRIVATE glfw webgpu glfw3webgpu)
