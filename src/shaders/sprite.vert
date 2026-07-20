#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;

uniform mat4 u_model;
uniform mat4 u_view_projection;

out vec2 vUV;

void main() {
    vUV = aUV;
    gl_Position = u_view_projection * u_model * vec4(aPos, 1.0);
}