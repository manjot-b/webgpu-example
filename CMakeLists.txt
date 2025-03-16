cmake_minimum_required(VERSION 3.27)
project(LearnWebGPU
	LANGUAGES CXX C
)

add_executable(app
	App.cpp
	App.hpp
	glfw3webgpu.cpp
	glfw3webgpu.hpp
	main.cpp
	webgpu-utils.cpp
	webgpu-utils.hpp
)

set_target_properties(app PROPERTIES
	CMAKE_CXX_STANDARD 17
	COMPILE_WARNING_AS_ERROR ON
)

if (MSVC)
	target_compile_options(app PRIVATE /W4)
else()
	target_compile_options(app PRIVATE -Wall -Wextra -pedantic)
endif()

if (XCODE)
	set_target_properties(app PROPERTIES
		XCODE_GENERATE_SCHEME ON
		XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal"
	)
endif()

if (EMSCRIPTEN)
	set_target_properties(app PROPERTIES SUFFIX ".html")
	target_link_options(app PRIVATE -sASYNCIFY)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(LINUX_DISPLAY "WAYLAND" CACHE STRING "Linux display server. Either WAYLAND or X11")
	string(TOUPPER ${LINUX_DISPLAY} LINUX_DISPLAY)

	if (LINUX_DISPLAY STREQUAL "X11")
		set(LINUX_DISPLAY_DEF "LINUX_X11")
	else()
		set(LINUX_DISPLAY_DEF "LINUX_WAYLAND")
	endif()

	target_compile_definitions(app PRIVATE "${LINUX_DISPLAY_DEF}")
endif()

string(TOUPPER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE)

add_subdirectory(submodules)
#target_copy_webgpu_binaries(app)
