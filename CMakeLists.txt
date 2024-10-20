cmake_minimum_required(VERSION 3.8)

set(CMAKE_GENERATOR "Ninja")
set(CMAKE_TOOLCHAIN_FILE ../deps/vcpkg/scripts/buildsystems/vcpkg.cmake)

if (DEFINED ENV{EMSDK})
    set(VCPKG_TARGET_TRIPLET "wasm32-emscripten")

    set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE $ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake)

    execute_process(
            COMMAND "npm install"
            WORKING_DIRECTORY "$ENV{EMSDK}/upstream/emscripten"
    )
else()
    set(VCPKG_TARGET_TRIPLET "x64-windows")
endif()

project(wasmGlfw3Boilerplate LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)

find_package(OpenGL REQUIRED)
find_package(Stb REQUIRED)

set(DEPS ${CMAKE_CURRENT_SOURCE_DIR}/../deps)

add_executable(wasmGlfw3Boilerplate
        src/glfw3renderer.cpp
        src/glwasm.cpp
        src/view.cpp
)

target_include_directories(
        wasmGlfw3Boilerplate PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include

        ${Stb_INCLUDE_DIR}
)

if (DEFINED ENV{EMSDK})
    set(CMAKE_EXECUTABLE_SUFFIX ".mjs")
    set_target_properties(wasmGlfw3Boilerplate PROPERTIES COMPILE_FLAGS "-O3 -Wall -Wformat -Wno-implicit-int -s DISABLE_EXCEPTION_CATCHING=0 --use-port=libjpeg --use-port=libpng")
    set_target_properties(wasmGlfw3Boilerplate PROPERTIES LINK_FLAGS "--no-heap-copy -O3 -lembind --emit-tsd interface.d.ts --use-port=libjpeg --use-port=libpng -lhtml5.js -lhtml5_webgl.js -lglfw.js -s ENVIRONMENT='web' -s MODULARIZE=1 -s ALLOW_MEMORY_GROWTH=1 -s WASM=1 -s USE_GLFW=3 -s USE_WEBGPU=1 -sFETCH -s NO_EXIT_RUNTIME=0 -s STANDALONE_WASM=0 -s EXIT_RUNTIME=1 -s ASSERTIONS=1 -s STACK_OVERFLOW_CHECK=2 -s DISABLE_EXCEPTION_CATCHING=0 -s SINGLE_FILE=1")
endif()