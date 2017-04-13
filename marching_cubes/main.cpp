#include <iostream>

#include <GL\glew.h>
#include <GLFW\glfw3.h>
#include <glm\glm.hpp>

using namespace std;

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void run()
{
    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
    {
        throw runtime_error("glfwInit failed");
    }

    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    GLFWwindow* window = glfwCreateWindow(640, 480, "Marching Cubes", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        throw runtime_error("glfwCreateWindow failed");
    }

    glfwSetKeyCallback(window, key_callback);

    glfwMakeContextCurrent(window);

    // initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
        throw runtime_error("glewInit failed");

    // GLEW throws some errors, so discard all the errors so far
    while (glGetError() != GL_NO_ERROR) {}

    //glfwSwapInterval(1);

    glClearColor(0.95f, 0.95f, 0.95f, 1.0f);

    double oldTime = glfwGetTime();


    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        double newTime = glfwGetTime();
        double timeElapsed = newTime - oldTime;

        oldTime = newTime;

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwDestroyWindow(window);
    glfwTerminate();
}

int main(int argc, char** argv)
{
    try
    {
        run();
    }
    catch (const exception& e)
    {
        char temp;
        cerr << e.what() << endl;
        cin >> temp;
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}