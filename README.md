# Build
## Emscripten (Web Browser)
- Install the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) somewhere on
your system
- Set the environment variables by running `source /path/to/emsdk_env.sh`
    - This script is found in the root of the emsdk/ folder
## Building
- Use the following cmake config from `CMakePresets.json` like so:
```
emcmake cmake --preset wasm-<release|debug>
```
### Enable on Browser
- On Chrome go to chrome:://flags and enable the `#enable-unsafe-webgpu` flag
    - See [Chrome WebGPU Troublshooting](https://developer.chrome.com/docs/web-platform/webgpu/troubleshooting-tips) for more info
- For Firefox you will need to install Firefox Nightly at the moment
    - Check [current support](https://developer.mozilla.org/en-US/docs/Web/API/WebGPU_API#browser_compatibility) for WebGPU
    - Go to about:config and make sure `dom.webgpu.enabled` is enabled

## Native
### Pre-Requisites
- Install `cargo` to build WGPU
