#include "shader.h"
#include <stdio.h>
#include <assert.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

uint32_t lum_shader_compile_from_source(const char* source, const uint32_t shader_type) {
  assert(source);
  GLint success;
  char infoLog[512];

  GLuint shader = glCreateShader(shader_type);
  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);
  
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(shader, 512, NULL, infoLog);
    fprintf(stderr, "Shader compilation failed: %s\n", infoLog);
    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

uint32_t lum_shader_link_shaders(const unsigned int vertex, const unsigned int fragment) {
  assert(vertex && fragment);
  GLint success;
  char infoLog[512];
  
  GLuint shader_program = glCreateProgram();
  glAttachShader(shader_program, vertex);
  glAttachShader(shader_program, fragment);
  glLinkProgram(shader_program);
  
  glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(shader_program, 512, NULL, infoLog);
    fprintf(stderr, "Shader program linking failed: %s\n", infoLog);
    glDeleteProgram(shader_program);
    return 0;
  }

  return shader_program;
}

uint32_t lum_shader_create_program_from_source(const char* vertex_source, const char* fragment_source) {
  auto vertex_shader   = lum_shader_compile_from_source(vertex_source, GL_VERTEX_SHADER);
  auto fragment_shader = lum_shader_compile_from_source(fragment_source, GL_FRAGMENT_SHADER);

  if (!(vertex_shader && fragment_shader)) {
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return 0;
  }
  
  auto shader_program = lum_shader_link_shaders(vertex_shader, fragment_shader);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  return shader_program;
}

int lum_shader_create_from_source(lum_Shader* shader, const char* vertex_source, const char* fragment_source) {
  assert(shader);

  uint32_t program = lum_shader_create_program_from_source(vertex_source, fragment_source);
  if (!program) return 0;

  shader->program = program;
  shader->initialized = true;
  return 1;
}

void lum_shader_destroy(lum_Shader* shader) {
  assert(shader);
  if (!shader->initialized) return;
  glDeleteProgram(shader->program);
  shader->program = 0;
  shader->initialized = false;
}

void lum_shader_use(const lum_Shader* shader) {
  assert(shader);
  if (!shader->initialized) return;
  glUseProgram(shader->program);
}

int lum_shader_get_uniform_location(const lum_Shader* shader, const char* name) {
  assert(shader && name);
  if (!shader->initialized) return -1;
  return glGetUniformLocation(shader->program, name);
}

void lum_shader_uniform_set1i(const lum_Shader* shader, const char* name, int value) {
  assert(shader && name);
  if (shader->initialized) return;
  int loc = lum_shader_get_uniform_location(shader, name);
  if (loc < 0) return;
  glUniform1i(loc, value);
}

void lum_shader_uniform_set1f(const lum_Shader* shader, const char* name, float value) {
  assert(shader && name);
  if (!shader->initialized) return;
  int loc = lum_shader_get_uniform_location(shader, name);
  if (loc < 0) return;
  glUniform1f(loc, value);
}

void lum_shader_uniform_set2v(const lum_Shader* shader, const char* name, lum_Vec2 value) {
  assert(shader && name);
  if (!shader->initialized) return;
  int loc = lum_shader_get_uniform_location(shader, name);
  if (loc < 0) return;
  glUniform2f(loc, value.x, value.y);
}

void lum_shader_uniform_set3v(const lum_Shader* shader, const char* name, lum_Vec3 value) {
  assert(shader && name);
  if (!shader->initialized) return;
  int loc = lum_shader_get_uniform_location(shader, name);
  if (loc < 0) return;
  glUniform3f(loc, value.x, value.y, value.z);
}

void lum_shader_uniform_set4v(const lum_Shader* shader, const char* name, lum_Vec4 value) {
  assert(shader && name);
  if (!shader->initialized) return;
  int loc = lum_shader_get_uniform_location(shader, name);
  if (loc < 0) return;
  glUniform4f(loc, value.x, value.y, value.z, value.w);
}

void lum_shader_uniform_set4m(const lum_Shader* shader, const char* name, lum_Mat4 value) {
  assert(shader && name);
  if (!shader->initialized) return;
  int loc = lum_shader_get_uniform_location(shader, name);
  if (loc < 0) return;
  glUniformMatrix4fv(loc, 1, GL_FALSE, value.m);
}
