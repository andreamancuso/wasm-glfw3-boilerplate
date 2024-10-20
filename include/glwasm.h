#include <stdio.h>
#include <cstring>
#include <memory>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#endif
#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>

#include "./glfw3renderer.h"
#include "./view.h"

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include <functional>
static std::function<void()>            MainLoopForEmscriptenP;
static void MainLoopForEmscripten()     { MainLoopForEmscriptenP(); }
#define EMSCRIPTEN_MAINLOOP_BEGIN       MainLoopForEmscriptenP = [&]()
#define EMSCRIPTEN_MAINLOOP_END         ; emscripten_set_main_loop(MainLoopForEmscripten, 30, true) // 24 frames / second, use 0 for browser's default
#else
#define EMSCRIPTEN_MAINLOOP_BEGIN
#define EMSCRIPTEN_MAINLOOP_END
#endif

#ifndef GLWASM_H
#define GLWASM_H
static void wgpu_error_callback(WGPUErrorType error_type, const char* message, void*);

class GLWasm final : public GLFW3Renderer {
    protected:
        std::unique_ptr<char[]> m_canvasSelector;
        wgpu::Instance m_instance;
        WGPUDevice m_device;
        WGPUQueue m_queue;
        WGPUSurface m_wgpu_surface;
        WGPUTextureFormat m_wgpu_preferred_fmt = WGPUTextureFormat_RGBA8Unorm;
        WGPUSwapChain m_wgpu_swap_chain;
        int m_wgpu_swap_chain_width = 0;
        int m_wgpu_swap_chain_height = 0;

        bool InitWGPU();

        void InitGlfw() override;

        void SetUp() override;

        void CreateSwapChain(int width, int height);

        void HandleScreenSizeChanged() override;

        explicit GLWasm(View* v);

    public:
        static GLWasm& GetInstance(View* v);

        void SetWindowSize(int width, int height) override;

        void SetDeviceAndStart(WGPUDevice& cDevice);

        static void GetDevice(wgpu::Instance wgpuInstance, GLWasm* glWasmInstance);

        void Init(std::string& cs);

        void PerformRendering() override;

        void Start() override;
};

#endif