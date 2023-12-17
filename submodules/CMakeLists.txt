add_subdirectory(glfw)
add_subdirectory(webgpu)

target_link_libraries(app PRIVATE glfw webgpu)
