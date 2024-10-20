#include <memory>
#include <cstdio>
#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>

#include "glfw3renderer.h"
#include "glwasm.h"

void wgpu_error_callback(WGPUErrorType error_type, const char* message, void*)
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

GLWasm::GLWasm(View* v): GLFW3Renderer(v) {
    m_instance = wgpu::CreateInstance();
}

GLWasm& GLWasm::GetInstance(View* v) {
    static GLWasm instance(v);

    return instance;
}

bool GLWasm::InitWGPU() {
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

void GLWasm::CreateSwapChain(int width, int height) {
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

void GLWasm::InitGlfw() {
    GLFW3Renderer::InitGlfw();

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

void GLWasm::SetUp() {
    InitGlfw();

#ifdef __EMSCRIPTEN__
#endif

    m_view->SetUp(m_canvasSelector.get(), m_device, m_glfwWindow, m_wgpu_preferred_fmt);
    m_view->PrepareForRender();
}

void GLWasm::HandleScreenSizeChanged() {
    int width, height;
    glfwGetFramebufferSize((GLFWwindow*)m_glfwWindow, &width, &height);
    if (width != m_wgpu_swap_chain_width && height != m_wgpu_swap_chain_height)
    {
        CreateSwapChain(width, height);

        m_view->HandleScreenSizeChanged();
    }
}

void GLWasm::SetWindowSize(int width, int height) {
    m_window_width = width;
    m_window_height = height;

    if (m_glfwWindow) {
        glfwSetWindowSize(m_glfwWindow, width, height);
    }
}

void GLWasm::SetDeviceAndStart(WGPUDevice& cDevice) {
    m_device = cDevice;

    Start();
}

void GLWasm::GetDevice(wgpu::Instance wgpuInstance, GLWasm* glWasmInstance) {
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

void GLWasm::Init(std::string& cs) {
    m_canvasSelector = std::make_unique<char[]>(cs.length() + 1);
    strcpy(m_canvasSelector.get(), cs.c_str());

    GetDevice(m_instance, this);
}

void GLWasm::PerformRendering() {
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

void GLWasm::Start() {
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

        PerformRendering();
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif
    // Clean up
    m_view->CleanUp();

    GLFW3Renderer::CleanUp();
}
