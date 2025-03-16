# Emscripten compiler already contains path to webgpu.h header. No need
# to provide our own
add_library(webgpu INTERFACE)
target_link_options(webgpu INTERFACE -sUSE_WEBGPU)


# EMSCRIPTEN_WEBGPU_DEPRECATED define will eventually be removed once the Emscripten webgpu.h header
# is caught up with Dawn's and WGPU's implementation of the header
target_compile_definitions(webgpu INTERFACE WEBGPU_BACKEND_EMSCRIPTEN EMSCRIPTEN_WEBGPU_DEPRECATED)
