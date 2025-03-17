# See https://dawn.googlesource.com/dawn/+/refs/heads/main/docs/quickstart-cmake.md for more info
# on CMake build options

# Use python script instead of depot_tools
set(DAWN_FETCH_DEPENDENCIES ON)
set(DAWN_BUILD_SAMPLES OFF)
set(DAWN_BUILD_TESTS OFF)
set(DAWN_ENABLE_DESKTOP_GL OFF)
set(DAWN_ENABLE_NULL OFF)
set(DAWN_ENABLE_OPENGLES OFF)

set(TINT_BUILD_CMD_TOOLS OFF)
set(TINT_BUILD_TESTS OFF)

if(LINUX_DISPLAY STREQUAL "WAYLAND")
	set(DAWN_USE_WAYLAND ON)
endif()

add_subdirectory(dawn)

add_library(webgpu INTERFACE)
target_link_libraries(webgpu INTERFACE webgpu_dawn)
target_compile_definitions(webgpu INTERFACE WEBGPU_BACKEND_DAWN)
