#version 330
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 norm;

uniform mat4 P;

void main() {
	gl_Position = P * vec4(pos, 1.0);
}