if(BUILD_SHARED_LIBS)
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		set(LIB_EXT "so")
	else()
		# TODO: Add Windows and MacOS builds of wgpu-native
		message(FATAL_ERROR "Build for wgpu-native on your platform is not set up yet!")
	endif()
else()
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		set(LIB_EXT "a")
	else()
		# TODO: Add Windows and MacOS builds of wgpu-native
		message(FATAL_ERROR "Build for wgpu-native on your platform is not set up yet!")
	endif()
endif()

set(WGPU_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/wgpu")

if(${CMAKE_BUILD_TYPE} STREQUAL "RELEASE")
	set(CARGO_BUILD_ARGS "--release")
endif()

include(ExternalProject)
ExternalProject_Add(
	wgpu_external
	BUILD_ALWAYS ON
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/wgpu"
	BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/wgpu"
	PREFIX "webgpu/wgpu"
	CONFIGURE_COMMAND ""
	BUILD_COMMAND cargo build ${CARGO_BUILD_ARGS} --target-dir ${WGPU_BUILD_DIR}
	BUILD_BYPRODUCTS "${WGPU_BUILD_DIR}/$<IF:$<CONFIG:Release>,release,debug>/libwgpu_native.${LIB_EXT}"
	INSTALL_COMMAND ""
	LOG_BUILD ON
	LOG_OUTPUT_ON_FAILURE ON
)

# Has to be a target which copies the file everytime. add_custom_command is incompatible
# with IMPORTED libraries
set(WGPU_INCLUDE_DIR "${WGPU_BUILD_DIR}/include")
file(MAKE_DIRECTORY "${WGPU_INCLUDE_DIR}/webgpu")
add_custom_target(
	wgpu_copy_headers ALL
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/wgpu"
	COMMAND cmake -E copy "ffi/webgpu-headers/webgpu.h" "${WGPU_INCLUDE_DIR}/webgpu/"
	COMMAND cmake -E copy "ffi//wgpu.h" "${WGPU_INCLUDE_DIR}/webgpu/"
	COMMENT "Copying wgpu headers..."
)

add_library(webgpu SHARED IMPORTED GLOBAL)
add_dependencies(webgpu wgpu_external wgpu_copy_headers)

set_target_properties(webgpu PROPERTIES
	IMPORTED_LOCATION "${WGPU_BUILD_DIR}/release/libwgpu_native.${LIB_EXT}"
	IMPORTED_LOCATION_DEBUG "${WGPU_BUILD_DIR}/debug/libwgpu_native.${LIB_EXT}"
	IMPORTED_NO_SONAME TRUE
	INTERFACE_INCLUDE_DIRECTORIES "${WGPU_INCLUDE_DIR}"
)

target_include_directories(webgpu INTERFACE "${WGPU_BUILD_DIR}/include")
target_compile_definitions(webgpu INTERFACE WEBGPU_BACKEND_WGPU)
