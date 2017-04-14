#include <iostream>
#include <string>
#include <vector>

#include <GL\glew.h>
#include <GLFW\glfw3.h>
#include <glm\glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "GLSL.h"
#include "LookupTables.h"
#include "MatrixStack.h"
#include "Program.h"

#define GRID_SIZE 128
#define HALF_GRID GRID_SIZE / 2
#define VOX_VERTS 8
#define EDGE_VERTS 12

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

double isovalue;

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

Voxel voxels[GRID_SIZE - 1][GRID_SIZE - 1][GRID_SIZE- 1];
pt_data values[GRID_SIZE][GRID_SIZE][GRID_SIZE];

void render() {
    glUseProgram(prog.prog);

    M.rotate(.001, vec3(0, 1, 0));

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
    return x*x + y*y + z*z - 2500;
    // return cos(2 * 3.14*sqrt(x*x + y*y)) - z * 1;
    // return -abs(10 - sqrt(x*x + y*y)) + 2;
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

    /*if (abs(val1) < DBL_EPSILON || abs(val1 - val2) < DBL_EPSILON) {
        ret[0] = vec3(p1.x, p1.y, p1.z);
        ret[1] = norm1;
        return ret;
    }
    if (abs(val2) < DBL_EPSILON) {
        ret[0] = vec3(p2.x, p2.y, p2.z);
        ret[1] = norm2;
        return ret;
    }*/
    mu = (isovalue - val1) / (val2 - val1);
    ret[0].x = p1.x + mu * (p2.x - p1.x);
    ret[0].y = p1.y + mu * (p2.y - p1.y);
    ret[0].z = p1.z + mu * (p2.z - p1.z);
    ret[1] = normalize(norm2 * (float)mu + norm1 * (float)(1 - mu));
    return ret;
}

void compute_tris() {
    for (int x = -HALF_GRID; x < HALF_GRID - 1; x++) {
        for (int y = -HALF_GRID; y < HALF_GRID - 1; y++) {
            for (int z = -HALF_GRID; z < HALF_GRID - 1; z++) {
                int idx = 0;
                Voxel vox = get_voxel(x, y, z);
                idx |= value_at(vox.verts[0]) < isovalue ? 1 << 0 : 0;
                idx |= value_at(vox.verts[1]) < isovalue ? 1 << 1 : 0;
                idx |= value_at(vox.verts[2]) < isovalue ? 1 << 2 : 0;
                idx |= value_at(vox.verts[3]) < isovalue ? 1 << 3 : 0;
                idx |= value_at(vox.verts[4]) < isovalue ? 1 << 4 : 0;
                idx |= value_at(vox.verts[5]) < isovalue ? 1 << 5 : 0;
                idx |= value_at(vox.verts[6]) < isovalue ? 1 << 6 : 0;
                idx |= value_at(vox.verts[7]) < isovalue ? 1 << 7 : 0;

                vec3 vertList[EDGE_VERTS][2];

                if (!edgeTable[idx]) {
                    continue;
                }

                if (edgeTable[idx] & 1 << 0) {
                    interpolate(vox.verts[0], vox.verts[1], vertList[0]);
                }
                if (edgeTable[idx] & 1 << 1) {
                    interpolate(vox.verts[1], vox.verts[2], vertList[1]);
                }
                if (edgeTable[idx] & 1 << 2) {
                    interpolate(vox.verts[2], vox.verts[3], vertList[2]);
                }
                if (edgeTable[idx] & 1 << 3) {
                    interpolate(vox.verts[3], vox.verts[0], vertList[3]);
                }
                if (edgeTable[idx] & 1 << 4) {
                    interpolate(vox.verts[4], vox.verts[5], vertList[4]);
                }
                if (edgeTable[idx] & 1 << 5) {
                    interpolate(vox.verts[5], vox.verts[6], vertList[5]);
                }
                if (edgeTable[idx] & 1 << 6) {
                    interpolate(vox.verts[6], vox.verts[7], vertList[6]);
                }
                if (edgeTable[idx] & 1 << 7) {
                    interpolate(vox.verts[7], vox.verts[4], vertList[7]);
                }
                if (edgeTable[idx] & 1 << 8) {
                    interpolate(vox.verts[0], vox.verts[4], vertList[8]);
                }
                if (edgeTable[idx] & 1 << 9) {
                    interpolate(vox.verts[1], vox.verts[5], vertList[9]);
                }
                if (edgeTable[idx] & 1 << 10) {
                    interpolate(vox.verts[2], vox.verts[6], vertList[10]);
                }
                if (edgeTable[idx] & 1 << 11) {
                    interpolate(vox.verts[3], vox.verts[7], vertList[11]);
                }

                for (int i = 0; triTable[idx][i] != -1; i += 3) {
                    
                    vec3 vert1 = vertList[triTable[idx][i]][0];
                    elements.push_back(verts.size());
                    verts.push_back(vert1);
                    norms.push_back(vertList[triTable[idx][i]][1]);

                    vec3 vert2 = vertList[triTable[idx][i + 1]][0];
                    elements.push_back(verts.size());
                    verts.push_back(vert2);
                    norms.push_back(vertList[triTable[idx][i + 1]][1]);

                    vec3 vert3 = vertList[triTable[idx][i + 2]][0];
                    elements.push_back(verts.size());
                    verts.push_back(vert3);
                    norms.push_back(vertList[triTable[idx][i + 2]][1]);
                }
            }
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
    V.lookAt(vec3(0, 0, -120), vec3(0), vec3(0, 1, 0));

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

    glEnable(GL_DEPTH_TEST);
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
    isovalue = 0;
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