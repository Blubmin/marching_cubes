#version 330

in vec3 v_color;

out vec3 color;

void main() {
	color = normalize(v_color);
}