#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>
#include <string>

#ifndef VIEW
#define VIEW

class View {
    protected:
        const char* m_windowId;
        const char* m_glWindowTitle;
        WGPUDevice m_device;

    public:
        View(const char* windowId, const char* glWindowTitle) {
            m_windowId = windowId;
            m_glWindowTitle = glWindowTitle;
        }

        const char* GetGlWindowTitle() {
            return m_glWindowTitle;
        }

        virtual void SetUp(char* pCanvasSelector, WGPUDevice device, GLFWwindow* glfwWindow, WGPUTextureFormat wgpu_preferred_fmt) = 0;
        virtual void PrepareForRender() = 0;
        virtual void Render(int window_width, int window_height) = 0;
        virtual void RenderDrawData(WGPURenderPassEncoder pass) = 0;
        virtual void CleanUp() = 0;
        virtual void HandleScreenSizeChanged() = 0;
        virtual WGPUColor GetClearColor() = 0;
};

#endif
