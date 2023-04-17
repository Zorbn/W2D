### Building
Requires SDL2, and SDL2_image. The default `CMakeLists.txt` is setup to find these through VCPKG.
Also requires WGPU. Build wgpu-native and place the resulting binaries as well as `wgpu.h` into the
top level of this directory, place `webgpu.h` into `webgpu-headers`.

You may also need to make sure that binaries for SDL2 and SDL2_image are available when running the application.
For example, by putting SDL2.dll and SDL2_image.dll in the directory you run the application from.
SDL2_image also requires libpng16.dll and zlib1.dll.