#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>
#include <string>

#include "view.h"

View::View(const char* windowId, const char* glWindowTitle) {
    m_windowId = windowId;
    m_glWindowTitle = glWindowTitle;
};

const char* View::GetGlWindowTitle() const {
    return m_glWindowTitle;
};

WGPUDevice& View::GetDevice() {
    return m_device;
};
