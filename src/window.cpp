#include <window.hpp>

window::window(int width, int height, const char* title) : m_width(width), m_height(height), m_title(title)
{
    m_width=width;
    m_height=height;
    m_title=title;
}

void window::initWindow()
{
    if(!glfwInit())
    {
        return;
    }
    m_window = glfwCreateWindow(m_width, m_height, m_title, NULL, NULL);
    if (!m_window)
    {
        glfwTerminate();
        return;
    }
    glfwMakeContextCurrent(m_window);
}

void window::updateWindow()
{
    glfwSwapBuffers(m_window);
    glfwPollEvents();
}

void window::destroyWindow()
{
    glfwTerminate();
    return;
}

GLFWwindow* window::getWindow()
{
    return m_window;
}

bool window::isWindowNotClosed()
{
    return !glfwWindowShouldClose(m_window);
}