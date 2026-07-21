#version 330 core

in vec2 vUV;
in vec4 vColor;
out vec4 FragColor;

uniform sampler2D u_texture;

void main() {
    FragColor = texture(u_texture, vUV) * vColor;
}