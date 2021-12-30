#include <GLFW/glfw3.h>
#include <iostream>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
using namespace std;

int main(int argc, const char** argv)
{
    GLFWwindow* window;

    if(!glfwInit())
    {
        cout<<"Couldn't initialize GLFW!"<<endl;
        return 1;
    }

    window = glfwCreateWindow(640, 480, "Liquid Media Player", NULL, NULL);
    if(!window)
    {
        cout<<"Couldn't create window!"<<endl;
        return 1;
    }

    glfwMakeContextCurrent(window);
    while(!glfwWindowShouldClose(window))
    {
        glfwWaitEvents();
    }

    return 0;
}