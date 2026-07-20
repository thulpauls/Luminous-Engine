#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D u_texture;
uniform vec4 u_color;

void main() {
    FragColor = texture(u_texture, vUV) * u_color;
}