#include <iostream>
#include <vector>

#include <GL\glew.h>
#include <GLFW\glfw3.h>
#include <glm\glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "MatrixStack.h"
#include "Program.h"

using namespace std;

GLFWwindow* window;

Program prog;

GLuint VAO;
GLuint VBO;
GLuint VBO_vert;
GLuint VBO_norm;

vector<glm::vec3> verts;
vector<glm::vec3> norms;
vector<GLuint> elements;

MatrixStack M;
MatrixStack V;
MatrixStack P;

void render() {
    glUseProgram(prog.prog);

    glUniformMatrix4fv(prog.getUniformHandle("M"), 1, GL_FALSE, glm::value_ptr(M.topMatrix()));
    glUniformMatrix4fv(prog.getUniformHandle("V"), 1, GL_FALSE, glm::value_ptr(V.topMatrix()));
    glUniformMatrix4fv(prog.getUniformHandle("P"), 1, GL_FALSE, glm::value_ptr(P.topMatrix()));

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, elements.size() / 3);
    glBindVertexArray(0);

    glUseProgram(0);
}

void march() {
    verts = vector<glm::vec3>();
    verts.push_back(glm::vec3(-0.5f, -0.5f, 0.0f));
    verts.push_back(glm::vec3(0.5f, -0.5f, 0.0f));
    verts.push_back(glm::vec3(0.0f, 0.7f, 0.0f));
    norms = vector<glm::vec3>();
    norms.push_back(glm::vec3());
    norms.push_back(glm::vec3());
    norms.push_back(glm::vec3());
    elements = vector<GLuint>();
    elements.push_back(0);
    elements.push_back(1);
    elements.push_back(2);
}

void init() {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    float aspect = width / (float)height;

    prog = Program("./vert.glsl", "./frag.glsl");
    march();

    M = MatrixStack();
    M.pushMatrix();

    V = MatrixStack();
    V.pushMatrix();
    V.lookAt(vec3(0, 0, -5), vec3(0), vec3(0, 1, 0));

    P = MatrixStack();
    P.pushMatrix();
    P.ortho(-1, 1, -1, 1, -1, 1);

    glGenBuffers(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO_vert);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_vert);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec3), &verts[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glGenBuffers(1, &VBO_norm);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_norm);
    glBufferData(GL_ARRAY_BUFFER, norms.size() * sizeof(glm::vec3), &norms[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, elements.size() * sizeof(GLuint), &elements[0], GL_STATIC_DRAW);

    int size;
    glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

static void error_callback(int error, const char* description) {
    fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void run() {
    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
    {
        throw runtime_error("glfwInit failed");
    }

    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    window = glfwCreateWindow(640, 480, "Marching Cubes", NULL, NULL);
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

    init();

    glClearColor(0.95f, 0.95f, 0.95f, 1.0f);

    double oldTime = glfwGetTime();


    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        double newTime = glfwGetTime();
        double timeElapsed = newTime - oldTime;

        oldTime = newTime;
        render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwDestroyWindow(window);
    glfwTerminate();
}

int main(int argc, char** argv) {
    try {
        run();
    }
    catch (const exception& e) {
        char temp;
        cerr << e.what() << endl;
        cin >> temp;
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}