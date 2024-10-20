#include "glfw3renderer.h"

void glfw_error_callback(int error, const char* description)
{
    printf("GLFW Error %d: %s\n", error, description);
}

GLFW3Renderer::GLFW3Renderer(View* v) {
    m_view = v;
}

void GLFW3Renderer::InitGlfw() {
    glfwSetErrorCallback(glfw_error_callback);
    glfwInit();

    // Make sure GLFW does not initialize any graphics context.
    // This needs to be done explicitly later.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_glfwWindow = glfwCreateWindow(m_window_width, m_window_height, m_view->GetGlWindowTitle(), nullptr, nullptr);
}

void GLFW3Renderer::CleanUp() {
    glfwDestroyWindow(m_glfwWindow);
    glfwTerminate();
}
