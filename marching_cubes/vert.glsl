#version 330
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 norm;

uniform mat4 M;
uniform mat4 V;
uniform mat4 P;

out vec3 v_color;

void main() {
	v_color = norm;
	gl_Position = P * V * M * vec4(pos, 1.0);
}