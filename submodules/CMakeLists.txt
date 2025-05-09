if (NOT EMSCRIPTEN)
	set(GLFW_BUILD_DOCS OFF CACHE BOOL "GLFW lib only")
	set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "GLFW lib only")
	set(GLFW_BUILD_TESTS OFF CACHE BOOL "GLFW lib only")
	set(GLFW_INSTALL OFF CACHE BOOL "GLFW lib only")
	add_subdirectory(glfw)
else()
	add_library(glfw INTERFACE)
	target_link_options(glfw INTERFACE -sUSE_GLFW=3)
endif()

string(TOUPPER ${WEBGPU_BACKEND} WEBGPU_BACKEND_U)
if (NOT EMSCRIPTEN AND WEBGPU_BACKEND_U STREQUAL "WGPU")
	include(wgpu.cmake)
elseif(NOT EMSCRIPTEN AND WEBGPU_BACKEND_U STREQUAL "DAWN")
	include(dawn.cmake)
elseif (EMSCRIPTEN OR WEBGPU_BACKEND_U STREQUAL "EMSCRIPTEN")
	include(webgpu_emscripten.cmake)
endif()

target_link_libraries(app PRIVATE glfw webgpu)
