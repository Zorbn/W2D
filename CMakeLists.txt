cmake_minimum_required(VERSION 3.0.0)
project(WGPUTest VERSION 0.1.0)

include(CTest)
enable_testing()

set(TARGET_NAME WGPUTest)
set(CMAKE_C_STANDARD 11)

add_executable(${TARGET_NAME} src/main.c src/framework.c src/framework.h wgpu.h webgpu-headers/webgpu.h)

if(MSVC)
    add_definitions(-DWGPU_TARGET=WGPU_TARGET_WINDOWS)
    target_compile_options(${TARGET_NAME} PRIVATE /W4)
    set(OS_LIBRARIES "userenv" "ws2_32" "Dwmapi" "dbghelp" "d3dcompiler" "D3D12" "D3D11" "DXGI" "setupapi" "Bcrypt" "ntdll")
elseif(APPLE)
    add_definitions(-DWGPU_TARGET=WGPU_TARGET_MACOS)
    set(OS_LIBRARIES "-framework Cocoa" "-framework CoreVideo" "-framework IOKit" "-framework QuartzCore")
    target_compile_options(${TARGET_NAME} PRIVATE -x objective-c)
else(MSVC)
if(USE_WAYLAND)
    add_definitions(-DWGPU_TARGET=WGPU_TARGET_LINUX_WAYLAND)
else(USE_WAYLAND)
    add_definitions(-DWGPU_TARGET=WGPU_TARGET_LINUX_X11)
endif(USE_WAYLAND)
    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -pedantic)
endif(MSVC)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

include($ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)

find_library(WGPU_LIBRARY wgpu_native HINTS "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(
    WGPUTest PRIVATE
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
    ${WGPU_LIBRARY}
    ${OS_LIBRARIES}
)