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

#include "./view.cpp"

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

static void glfw_error_callback(int error, const char* description)
{
    printf("GLFW Error %d: %s\n", error, description);
}

static void wgpu_error_callback(WGPUErrorType error_type, const char* message, void*)
{
    const char* error_type_lbl = "";
    switch (error_type)
    {
    case WGPUErrorType_Validation:  error_type_lbl = "Validation"; break;
    case WGPUErrorType_OutOfMemory: error_type_lbl = "Out of memory"; break;
    case WGPUErrorType_Unknown:     error_type_lbl = "Unknown"; break;
    case WGPUErrorType_DeviceLost:  error_type_lbl = "Device lost"; break;
    default:                        error_type_lbl = "Unknown";
    }
    printf("%s error: %s\n", error_type_lbl, message);
}

class GLWasm {
    protected:
        std::unique_ptr<char[]> m_canvasSelector;
        wgpu::Instance m_instance;
        WGPUDevice m_device;
        WGPUQueue m_queue;
        GLFWwindow* m_glfwWindow;
        WGPUSurface m_wgpu_surface;
        WGPUTextureFormat m_wgpu_preferred_fmt = WGPUTextureFormat_RGBA8Unorm;
        WGPUSwapChain m_wgpu_swap_chain;
        int m_wgpu_swap_chain_width = 0;
        int m_wgpu_swap_chain_height = 0;
        int m_initial_window_width = 400;
        int m_initial_window_height = 300;
        int m_window_width = m_initial_window_width;
        int m_window_height = m_initial_window_height;
        View* m_view;

        bool InitWGPU() {
            if (!m_device) {
                printf("device not set\n");
                return false;
            }

            wgpuDeviceSetUncapturedErrorCallback(m_device, wgpu_error_callback, nullptr);

            // Use C++ wrapper due to misbehavior in Emscripten.
            // Some offset computation for wgpuInstanceCreateSurface in JavaScript
            // seem to be inline with struct alignments in the C++ structure
            wgpu::SurfaceDescriptorFromCanvasHTMLSelector html_surface_desc = {};

            html_surface_desc.selector = m_canvasSelector.get();

            wgpu::SurfaceDescriptor surface_desc = {};
            surface_desc.nextInChain = &html_surface_desc;

            wgpu::Surface surface = m_instance.CreateSurface(&surface_desc);
            wgpu::Adapter adapter = {};
            m_wgpu_preferred_fmt = (WGPUTextureFormat)surface.GetPreferredFormat(adapter);
            m_wgpu_surface = surface.MoveToCHandle();

            return true;
        }

        void CreateSwapChain(int width, int height) {
            if (m_wgpu_swap_chain)
                wgpuSwapChainRelease(m_wgpu_swap_chain);
            m_wgpu_swap_chain_width = width;
            m_wgpu_swap_chain_height = height;
            WGPUSwapChainDescriptor swap_chain_desc = {};
            swap_chain_desc.usage = WGPUTextureUsage_RenderAttachment;
            swap_chain_desc.format = m_wgpu_preferred_fmt;
            swap_chain_desc.width = width;
            swap_chain_desc.height = height;
            swap_chain_desc.presentMode = WGPUPresentMode_Fifo;
            m_wgpu_swap_chain = wgpuDeviceCreateSwapChain(m_device, m_wgpu_surface, &swap_chain_desc);
        }

        void InitGlfw() {
            glfwSetErrorCallback(glfw_error_callback);
            glfwInit();

            // Make sure GLFW does not initialize any graphics context.
            // This needs to be done explicitly later.
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            m_glfwWindow = glfwCreateWindow(m_window_width, m_window_height, m_view->GetGlWindowTitle(), nullptr, nullptr);

            // Initialize the WebGPU environment
            if (!InitWGPU())
            {
                if (m_glfwWindow)
                    glfwDestroyWindow(m_glfwWindow);
                glfwTerminate();
                return;
            }
            glfwShowWindow(m_glfwWindow);
        }

        void SetUp() {
            InitGlfw();

            m_view->SetUp(m_canvasSelector.get(), m_device, m_glfwWindow, m_wgpu_preferred_fmt);
            m_view->PrepareForRender();
        }

        void HandleScreenSizeChanged() {
            int width, height;
            glfwGetFramebufferSize((GLFWwindow*)m_glfwWindow, &width, &height);
            if (width != m_wgpu_swap_chain_width && height != m_wgpu_swap_chain_height)
            {
                CreateSwapChain(width, height);

                m_view->HandleScreenSizeChanged();
            }
        }

        GLWasm(View* v) {
            m_instance = wgpu::CreateInstance();
            m_view = v;
        }

    public:
        static GLWasm& GetInstance(View* v) {
            static GLWasm instance(v);

            return instance;
        }

        void SetWindowSize(int width, int height) {
            m_window_width = width;
            m_window_height = height;

            if (m_glfwWindow) {
                glfwSetWindowSize(m_glfwWindow, width, height);
            }
        }

        void SetDeviceAndStart(WGPUDevice& cDevice) {
            m_device = cDevice;

            Start();
        }

        static void GetDevice(wgpu::Instance wgpuInstance, GLWasm* glWasmInstance) {
            wgpuInstance.RequestAdapter(
                nullptr,
                [](WGPURequestAdapterStatus status, WGPUAdapter cAdapter,
                    const char* message, void* userdata) {

                    if (status != WGPURequestAdapterStatus_Success) {
                        exit(0);
                    }

                    wgpu::Adapter adapter = wgpu::Adapter::Acquire(cAdapter);

                    adapter.RequestDevice(
                        nullptr,
                        [](WGPURequestDeviceStatus status, WGPUDevice cDevice,
                        const char* message, void* userdata) {
                            reinterpret_cast<GLWasm*>(userdata)->SetDeviceAndStart(cDevice);
                        },
                        userdata);
                }, reinterpret_cast<void*>(glWasmInstance));
        }

        void Init(std::string& cs) {
            m_canvasSelector = std::make_unique<char[]>(cs.length() + 1);
            strcpy(m_canvasSelector.get(), cs.c_str());

            GetDevice(m_instance, this);
        }

        void PerformWGPURendering() {
            WGPURenderPassColorAttachment color_attachments = {};
            color_attachments.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
            color_attachments.loadOp = WGPULoadOp_Clear;
            color_attachments.storeOp = WGPUStoreOp_Store;
            color_attachments.clearValue = m_view->GetClearColor();
            color_attachments.view = wgpuSwapChainGetCurrentTextureView(m_wgpu_swap_chain);

            WGPURenderPassDescriptor render_pass_desc = {};
            render_pass_desc.colorAttachmentCount = 1;
            render_pass_desc.colorAttachments = &color_attachments;
            render_pass_desc.depthStencilAttachment = nullptr;

            WGPUCommandEncoderDescriptor enc_desc = {};
            WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, &enc_desc);

            WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_desc);
            m_view->RenderDrawData(pass);
            wgpuRenderPassEncoderEnd(pass);

            WGPUCommandBufferDescriptor cmd_buffer_desc = {};
            WGPUCommandBuffer cmd_buffer = wgpuCommandEncoderFinish(encoder, &cmd_buffer_desc);
            m_queue = wgpuDeviceGetQueue(m_device);
            wgpuQueueSubmit(m_queue, 1, &cmd_buffer);
        }

        void Start() {
            SetUp();

            // Main loop
        #ifdef __EMSCRIPTEN__
            EMSCRIPTEN_MAINLOOP_BEGIN
        #else
            while (!glfwWindowShouldClose(m_glfwWindow))
        #endif
            {
                glfwPollEvents();

                HandleScreenSizeChanged();

                m_view->Render(m_window_width, m_window_height);

                PerformWGPURendering();
            }
        #ifdef __EMSCRIPTEN__
            EMSCRIPTEN_MAINLOOP_END;
        #endif
            // Clean up
            m_view->CleanUp();


            glfwDestroyWindow(m_glfwWindow);
            glfwTerminate();
        }
};
