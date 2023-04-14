### Building
Requires SDL2, and SDL2_image. The default `CMakeLists.txt` is setup to find these through VCPKG.
Also requires WGPU. Build wgpu-native and place the resulting binaries as well as `wgpu.h` into the
top level of this directory, place `webgpu.h` into `webgpu-headers`.