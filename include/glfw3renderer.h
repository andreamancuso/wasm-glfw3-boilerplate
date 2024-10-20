#include <stdio.h>
#include <cstring>
#include <memory>

#include <GLFW/glfw3.h>

#include "./view.h"

#ifndef GLFW3_RENDERER_H
#define GLFW3_RENDERER_H
static void glfw_error_callback(int error, const char* description);

class GLFW3Renderer {
    protected:
        GLFWwindow* m_glfwWindow;
        int m_initial_window_width = 400;
        int m_initial_window_height = 300;
        int m_window_width = m_initial_window_width;
        int m_window_height = m_initial_window_height;
        View* m_view;

        virtual void InitGlfw();

        virtual void SetUp();

        virtual void HandleScreenSizeChanged();

        explicit GLFW3Renderer(View* v);

    public:
        virtual ~GLFW3Renderer() = default;

        // static GLFW3Renderer& GetInstance(View* v);

        virtual void SetWindowSize(int width, int height);

        virtual void PerformRendering();

        virtual void Start();

        virtual void CleanUp();
};

#endif