#include <stdio.h>
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
        std::unique_ptr<char[]> canvasSelector;
        wgpu::Instance instance;
        WGPUDevice device;
        GLFWwindow* glfwWindow;
        WGPUSurface wgpu_surface;
        WGPUTextureFormat wgpu_preferred_fmt = WGPUTextureFormat_RGBA8Unorm;
        WGPUSwapChain wgpu_swap_chain;
        int wgpu_swap_chain_width = 0;
        int wgpu_swap_chain_height = 0;
        int initial_window_width = 400;
        int initial_window_height = 300;
        int window_width = initial_window_width;
        int window_height = initial_window_height;
        View* view;

        bool InitWGPU() {
            if (!device) {
                printf("device not set\n");
                return false;
            }

            wgpuDeviceSetUncapturedErrorCallback(device, wgpu_error_callback, nullptr);

            // Use C++ wrapper due to misbehavior in Emscripten.
            // Some offset computation for wgpuInstanceCreateSurface in JavaScript
            // seem to be inline with struct alignments in the C++ structure
            wgpu::SurfaceDescriptorFromCanvasHTMLSelector html_surface_desc = {};

            html_surface_desc.selector = canvasSelector.get();

            wgpu::SurfaceDescriptor surface_desc = {};
            surface_desc.nextInChain = &html_surface_desc;

            wgpu::Surface surface = instance.CreateSurface(&surface_desc);
            wgpu::Adapter adapter = {};
            wgpu_preferred_fmt = (WGPUTextureFormat)surface.GetPreferredFormat(adapter);
            wgpu_surface = surface.MoveToCHandle();

            return true;
        }

        void CreateSwapChain(int width, int height) {
            if (wgpu_swap_chain)
                wgpuSwapChainRelease(wgpu_swap_chain);
            wgpu_swap_chain_width = width;
            wgpu_swap_chain_height = height;
            WGPUSwapChainDescriptor swap_chain_desc = {};
            swap_chain_desc.usage = WGPUTextureUsage_RenderAttachment;
            swap_chain_desc.format = wgpu_preferred_fmt;
            swap_chain_desc.width = width;
            swap_chain_desc.height = height;
            swap_chain_desc.presentMode = WGPUPresentMode_Fifo;
            wgpu_swap_chain = wgpuDeviceCreateSwapChain(device, wgpu_surface, &swap_chain_desc);
        }

        void InitGlfw() {
            glfwSetErrorCallback(glfw_error_callback);
            glfwInit();

            // Make sure GLFW does not initialize any graphics context.
            // This needs to be done explicitly later.
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindow = glfwCreateWindow(window_width, window_height, view->GetGlWindowTitle(), nullptr, nullptr);

            // Initialize the WebGPU environment
            if (!InitWGPU())
            {
                if (glfwWindow)
                    glfwDestroyWindow(glfwWindow);
                glfwTerminate();
                return;
            }
            glfwShowWindow(glfwWindow);
        }

        void SetUp() {
            InitGlfw();

            view->SetUp(canvasSelector.get(), device, glfwWindow, wgpu_preferred_fmt);
            view->PrepareForRender();
        }

        void HandleScreenSizeChanged() {
            int width, height;
            glfwGetFramebufferSize((GLFWwindow*)glfwWindow, &width, &height);
            if (width != wgpu_swap_chain_width && height != wgpu_swap_chain_height)
            {
                CreateSwapChain(width, height);

                view->HandleScreenSizeChanged();
            }
        }

        GLWasm(View* v) {
            instance = wgpu::CreateInstance();
            view = v;
        }

    public:
        static GLWasm& GetInstance(View* v) {
            static GLWasm instance(v);

            return instance;
        }

        void SetWindowSize(int width, int height) {
            window_width = width;
            window_height = height;

            if (glfwWindow) {
                glfwSetWindowSize(glfwWindow, width, height);
            }
        }

        void Init(std::string cs) {
            this->canvasSelector = std::make_unique<char[]>(cs.length() + 1);
            strcpy(this->canvasSelector.get(), cs.c_str());

            instance.RequestAdapter(
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
                        reinterpret_cast<GLWasm*>(userdata)->Start(cDevice);
                    },
                    userdata);
            }, reinterpret_cast<void*>(this));
        }

        void PerformWGPURendering() {
            WGPURenderPassColorAttachment color_attachments = {};
            color_attachments.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
            color_attachments.loadOp = WGPULoadOp_Clear;
            color_attachments.storeOp = WGPUStoreOp_Store;
            color_attachments.clearValue = view->GetClearColor();
            color_attachments.view = wgpuSwapChainGetCurrentTextureView(wgpu_swap_chain);

            WGPURenderPassDescriptor render_pass_desc = {};
            render_pass_desc.colorAttachmentCount = 1;
            render_pass_desc.colorAttachments = &color_attachments;
            render_pass_desc.depthStencilAttachment = nullptr;

            WGPUCommandEncoderDescriptor enc_desc = {};
            WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &enc_desc);

            WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_desc);
            view->RenderDrawData(pass);
            wgpuRenderPassEncoderEnd(pass);

            WGPUCommandBufferDescriptor cmd_buffer_desc = {};
            WGPUCommandBuffer cmd_buffer = wgpuCommandEncoderFinish(encoder, &cmd_buffer_desc);
            WGPUQueue queue = wgpuDeviceGetQueue(device);
            wgpuQueueSubmit(queue, 1, &cmd_buffer);
        }

        void Start(WGPUDevice cDevice) {
            device = cDevice;

            SetUp();

            // Main loop
        #ifdef __EMSCRIPTEN__
            EMSCRIPTEN_MAINLOOP_BEGIN
        #else
            while (!glfwWindowShouldClose(glfwWindow))
        #endif
            {
                glfwPollEvents();

                HandleScreenSizeChanged();

                view->Render(window_width, window_height);

                PerformWGPURendering();
            }
        #ifdef __EMSCRIPTEN__
            EMSCRIPTEN_MAINLOOP_END;
        #endif
            // Clean up
            view->CleanUp();


            glfwDestroyWindow(glfwWindow);
            glfwTerminate();
        }
};
