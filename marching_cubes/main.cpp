#define _USE_MATH_DEFINES

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include <GL\glew.h>
#include <GLFW\glfw3.h>
#include <glm\glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../imgui/imgui.h"
#include "../imgui/examples/opengl3_example/imgui_impl_glfw_gl3.h"

#include "GLSL.h"
#include "LookupTables.h"
#include "MatrixStack.h"
#include "Program.h"

#define GRID_SIZE 128
#define HALF_GRID GRID_SIZE / 2
#define VOX_VERTS 8
#define EDGE_VERTS 12
#define MAX_ISO 5
#define MIN_ISO -.5
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

int function;
float isovalue;
float time_elapsed;
bool pause;
float cam_dist;

float min_max[][2] = {
    {-.5, 5},
    {-1000, 2000},
    {-6, 1},
    {-20, 20}
};

struct pt_data {
    double value;
    vec3 norm;
};

struct pt3 {
    int x, y, z;
    pt3::pt3() {}

    pt3::pt3(int _x, int _y, int _z) {
        x = _x;
        y = _y;
        z = _z;
    }
};

struct Voxel {
    pt3 verts[VOX_VERTS];
    GLuint edges[EDGE_VERTS];
};

Voxel v1[GRID_SIZE - 1][GRID_SIZE - 1];
Voxel v2[GRID_SIZE - 1][GRID_SIZE - 1];

Voxel (* current_vox)[GRID_SIZE - 1][GRID_SIZE -1 ];
Voxel (* prev_vox)[GRID_SIZE - 1][GRID_SIZE - 1];

pt_data values[GRID_SIZE][GRID_SIZE][GRID_SIZE];

void render() {
    glUseProgram(prog.prog);

    if (!pause) {
        M.rotate(time_elapsed * M_PI / 4, vec3(0, 1, 0));
    }
    V.popMatrix();
    V.pushMatrix();
    V.lookAt(vec3(0, 0, -cam_dist), vec3(0), vec3(0, 1, 0));

    glUniformMatrix4fv(prog.getUniformHandle("M"), 1, GL_FALSE, glm::value_ptr(M.topMatrix()));
    glUniformMatrix4fv(prog.getUniformHandle("V"), 1, GL_FALSE, glm::value_ptr(V.topMatrix()));
    glUniformMatrix4fv(prog.getUniformHandle("P"), 1, GL_FALSE, glm::value_ptr(P.topMatrix()));

    glBindVertexArray(VAO);

    glDrawArrays(GL_TRIANGLES, 0, elements.size());

    glBindVertexArray(0);

    glUseProgram(0);
}

double value_at(pt3 pt) {
    return values[pt.x + HALF_GRID][pt.y + HALF_GRID][pt.z + HALF_GRID].value;
}

vec3 normal_at(pt3 pt) {
    return values[pt.x + HALF_GRID][pt.y + HALF_GRID][pt.z + HALF_GRID].norm;
}

Voxel get_voxel(int x, int y, int z) {
    Voxel vox = Voxel();
    vox.verts[0] = pt3(x, y, z);
    vox.verts[1] = pt3(x + 1, y, z);
    vox.verts[2] = pt3(x + 1, y, z + 1);
    vox.verts[3] = pt3(x, y, z + 1);
    vox.verts[4] = pt3(x, y + 1, z);
    vox.verts[5] = pt3(x + 1, y + 1, z);
    vox.verts[6] = pt3(x + 1, y + 1, z + 1);
    vox.verts[7] = pt3(x, y + 1, z + 1);
    return vox;
}

double data_function(double x, double y, double z) {
    switch (function) {
    case 0:
        return .003 * z*z - cos(.1*3.14*sqrt(x*x + y*y));
    case 1:
        return x*x + y*y + z*z - 2500;
    case 2:
        return -abs(10 - sqrt(x*x + y*y)) + 2;
    case 3:
        return max(max(abs(x), abs(y)), abs(z)) - 30;
    }
}

void compute_values() {
    for (int x = -HALF_GRID; x < HALF_GRID; x++) {
        for (int y = -HALF_GRID; y < HALF_GRID; y++) {
            for (int z = -HALF_GRID; z < HALF_GRID; z++) {
                double value = data_function(x, y, z);
                double xl, xg, yl, yg, zl, zg;

                xl = data_function(x - 1, y, z);
                xg = data_function(x + 1, y, z);
                yl = data_function(x, y - 1, z);
                yg = data_function(x, y + 1, z);
                zl = data_function(x, y, z - 1);
                zg = data_function(x, y, z + 1);

                vec3 norm = vec3(xg - xl, yg - yl, zg - zl);

                pt_data data;
                data.value = value;
                data.norm = norm;
                values[x + HALF_GRID][y + HALF_GRID][z + HALF_GRID] = data;
            }
        }
    }
}

vec3 * interpolate(pt3 p1, pt3 p2, vec3* ret) {
    double val1 = value_at(p1);
    double val2 = value_at(p2);
    vec3 norm1 = normal_at(p1);
    vec3 norm2 = normal_at(p2);
    double mu;

    mu = (isovalue - val1) / (val2 - val1);
    ret[0].x = p1.x + mu * (p2.x - p1.x);
    ret[0].y = p1.y + mu * (p2.y - p1.y);
    ret[0].z = p1.z + mu * (p2.z - p1.z);
    ret[1] = normalize(norm2 * (float)mu + norm1 * (float)(1 - mu));
    return ret;
}

int get_vert_idx(int x, int y, int z, int e) {
    int elem = -1;
    if (y > -HALF_GRID && e >= 0 && e <= 3) {
        elem = (*current_vox)[x + HALF_GRID][y - 1 + HALF_GRID].edges[e + 4];
    }
    else if (x > -HALF_GRID && (e == 0 || e == 4 || e == 8 || e == 9)) {
        switch (e) {
        case 0:
            e = 2;
        case 4:
            e = 6;
        case 8:
            e = 11;
        case 9:
            e = 10;
        }
        elem = (*current_vox)[x - 1 + HALF_GRID][y + HALF_GRID].edges[e];
    }
    else if (z > -HALF_GRID && (e == 3 || e == 7 || e == 8 || e == 11)) {
        switch (e) {
        case 3:
            e = 1;
        case 7:
            e = 5;
        case 8:
            e = 9;
        case 11:
            e = 10;
        }
        GLuint elem = (*prev_vox)[x + HALF_GRID][y + HALF_GRID].edges[e];
    }
    return elem;
}

void swap_slices() {
    Voxel (* c)[GRID_SIZE - 1][GRID_SIZE - 1] = current_vox;
    current_vox = prev_vox;
    prev_vox = c;
}

void compute_tris() {
    for (int x = -HALF_GRID; x < HALF_GRID - 1; x++) {
        for (int y = -HALF_GRID; y < HALF_GRID - 1; y++) {
            for (int z = -HALF_GRID; z < HALF_GRID - 1; z++) {
                int idx = 0;
                Voxel vox = get_voxel(x, y, z);

                for (int i = 0; i < VOX_VERTS; i++) {
                    idx |= value_at(vox.verts[i]) < isovalue ? 1 << i : 0;
                }

                vec3 vertList[EDGE_VERTS][2];

                if (!edgeTable[idx]) {
                    Voxel (*temp)[GRID_SIZE - 1] = (*current_vox);
                    temp[x + HALF_GRID][y + HALF_GRID] = vox;
                    continue;
                }

                for (int i = 0; triTable[idx][i] != -1; i += 3) {
                    int p1, p2, p3;
                    p1 = triTable[idx][i];
                    p2 = triTable[idx][i+1];
                    p3 = triTable[idx][i+2];

                    int e1, e2, e3;
                    e1 = -1; //  get_vert_idx(x, y, z, p1);
                    e2 = -1; //  get_vert_idx(x, y, z, p2);
                    e3 = -1; //  get_vert_idx(x, y, z, p3);

                    if (e1 < 0) {
                        int i1 = interp_table[p1][0];
                        int i2 = interp_table[p1][1];
                        vec3 vert[2];
                        interpolate(vox.verts[i1], vox.verts[i2], vert);
                        e1 = verts.size();
                        verts.push_back(vert[0]);
                        norms.push_back(vert[1]);
                    }

                    if (e2 < 0) {
                        int i1 = interp_table[p2][0];
                        int i2 = interp_table[p2][1];
                        vec3 vert[2];
                        interpolate(vox.verts[i1], vox.verts[i2], vert);
                        e2 = verts.size();
                        verts.push_back(vert[0]);
                        norms.push_back(vert[1]);
                    }

                    if (e3 < 0) {
                        int i1 = interp_table[p3][0];
                        int i2 = interp_table[p3][1];
                        vec3 vert[2];
                        interpolate(vox.verts[i1], vox.verts[i2], vert);
                        e3 = verts.size();
                        verts.push_back(vert[0]);
                        norms.push_back(vert[1]);
                    }
                    elements.push_back(e1);
                    elements.push_back(e2);
                    elements.push_back(e3);
                    vox.edges[p1] = e1;
                    vox.edges[p2] = e2;
                    vox.edges[p3] = e3;
                }
                (*current_vox)[x + HALF_GRID][y + HALF_GRID] = vox;
            }
            swap_slices();
        }
    }
}

void march() {
    verts = vector<vec3>();
    norms = vector<vec3>();
    elements = vector<GLuint>();

    compute_values();
    compute_tris();

}

void refresh() {
    march();

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_vert);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec3), &verts[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_norm);
    glBufferData(GL_ARRAY_BUFFER, norms.size() * sizeof(glm::vec3), &norms[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, elements.size() * sizeof(GLuint), &elements[0], GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void init() {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    float aspect = width / (float)height;

    v1[0][0] = Voxel();
    current_vox = &v1;
    prev_vox = &v2;

    prog = Program("./vert.glsl", "./frag.glsl");
    march();
    M = MatrixStack();
    M.pushMatrix();

    V = MatrixStack();
    V.pushMatrix();

    P = MatrixStack();
    P.pushMatrix();
    P.perspective(45, aspect, .1, 1000);

    glGenVertexArrays(1, &VAO);
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
    window = glfwCreateWindow(1280, 720, "Marching Cubes", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        throw runtime_error("glfwCreateWindow failed");
    }

    glfwMakeContextCurrent(window);

    // initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
        throw runtime_error("glewInit failed");

    // GLEW throws some errors, so discard all the errors so far
    while (glGetError() != GL_NO_ERROR) {}

    ImGui_ImplGlfwGL3_Init(window, true);

    glEnable(GL_DEPTH_TEST);
    //glfwSwapInterval(1);

    init();

    glClearColor(0.95f, 0.95f, 0.95f, 1.0f);

    double oldTime = glfwGetTime();


    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplGlfwGL3_NewFrame();

        ImGui::Begin("Settings and Stuff");
        ImGui::Combo("Function", &function, "ripples\0sphere\0cylinder\0cube");
        ImGui::SliderFloat("Iso Level", &isovalue, min_max[function][0], min_max[function][1]);
        if (ImGui::Button("March")) {
            refresh();
        }
        ImGui::SliderFloat("Camera Distance", &cam_dist, 0.0, 150.0);
        ImGui::Checkbox("Pause", &pause);
        ImGui::End();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        double newTime = glfwGetTime();
        time_elapsed = newTime - oldTime;

        oldTime = newTime;
        render();

        ImGui::Render();
        glfwSwapBuffers(window);
        
    }
    glfwDestroyWindow(window);
    glfwTerminate();
}

int main(int argc, char** argv) {
    isovalue = 0;
    function = 0;
    pause = false;
    cam_dist = 120;
    if (argc > 1) {
        isovalue = stod(argv[1]);
    }

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