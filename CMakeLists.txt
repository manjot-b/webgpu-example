cmake_minimum_required(VERSION 3.27)
project(LearnWebGPU
	LANGUAGES CXX C
)

add_executable(app
	main.cpp
	App.cpp
	App.hpp
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

add_subdirectory(submodules)
target_copy_webgpu_binaries(app)
