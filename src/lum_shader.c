#include "lum_shader.h"
#include "lum_log.h"
#include "lum_filesystem.h"
#include "lum_assert.h"
#include <stdlib.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

uint32_t lum_shader_compile_from_source(const char* source, const uint32_t shader_type) {
  Lum_Assert(source);
  GLint success;
  char info_log[512];

  GLuint shader = glCreateShader(shader_type);
  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);
  
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(shader, 512, NULL, info_log);
    Lum_Log_Error("Shader compilation failed: %s", info_log);
    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

uint32_t lum_shader_link_shaders(const unsigned int vertex, const unsigned int fragment) {
  Lum_Assert(vertex && fragment);
  GLint success;
  char info_log[512];
  
  GLuint shader_program = glCreateProgram();
  glAttachShader(shader_program, vertex);
  glAttachShader(shader_program, fragment);
  glLinkProgram(shader_program);
  
  glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(shader_program, 512, NULL, info_log);
    Lum_Log_Error("Shader program linking failed: %s", info_log);
    glDeleteProgram(shader_program);
    return 0;
  }

  return shader_program;
}

uint32_t lum_shader_create_program_from_source(const char* vertex_source, const char* fragment_source) {
  uint32_t vertex_shader   = lum_shader_compile_from_source(vertex_source, GL_VERTEX_SHADER);
  uint32_t fragment_shader = lum_shader_compile_from_source(fragment_source, GL_FRAGMENT_SHADER);

  if (!(vertex_shader && fragment_shader)) {
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return 0;
  }
  
  uint32_t shader_program = lum_shader_link_shaders(vertex_shader, fragment_shader);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  return shader_program;
}

int lum_shader_create_from_source(lum_Shader* shader, const char* vertex_source, const char* fragment_source) {
  Lum_Assert(shader);

  uint32_t program = lum_shader_create_program_from_source(vertex_source, fragment_source);
  if (!program) return 0;

  shader->program = program;
  shader->initialized = true;
  return 1;
}

int lum_shader_create_from_file(lum_Shader* shader, const char* vertex_path, const char* fragment_path) {
  Lum_Assert(shader);
  if (!vertex_path || !fragment_path) {
    Lum_Log_Error("shader: null shader path (vertex=%p, fragment=%p).",
                  (const void*)vertex_path, (const void*)fragment_path);
    return 0;
  }

  char* vertex_source = lum_fs_read(vertex_path);
  if (!vertex_source) {
    Lum_Log_Error("shader: failed to read vertex source '%s'.", vertex_path);
    return 0;
  }
  char* fragment_source = lum_fs_read(fragment_path);
  if (!fragment_source) {
    Lum_Log_Error("shader: failed to read fragment source '%s'.", fragment_path);
    free(vertex_source);
    return 0;
  }

  int result = lum_shader_create_from_source(shader, vertex_source, fragment_source);
  free(vertex_source);
  free(fragment_source);
  return result;
}

void lum_shader_destroy(lum_Shader* shader) {
  Lum_Assert(shader);
  if (!shader->initialized) return;
  glDeleteProgram(shader->program);
  shader->program = 0;
  shader->initialized = false;
}

void lum_shader_use(const lum_Shader* shader) {
  Lum_Assert(shader);
  if (!shader->initialized) return;
  glUseProgram(shader->program);
}

int lum_shader_get_uniform_location(const lum_Shader* shader, const char* name) {
  Lum_Assert(shader && name);
  if (!shader->initialized) return -1;
  return glGetUniformLocation(shader->program, name);
}

void lum_shader_uniform_set1i(const lum_Shader* shader, const char* name, int value) {
  Lum_Assert(shader && name);
  if (!shader->initialized) return;
  int loc = lum_shader_get_uniform_location(shader, name);
  if (loc < 0) return;
  glUniform1i(loc, value);
}

void lum_shader_uniform_set1f(const lum_Shader* shader, const char* name, float value) {
  Lum_Assert(shader && name);
  if (!shader->initialized) return;
  int loc = lum_shader_get_uniform_location(shader, name);
  if (loc < 0) return;
  glUniform1f(loc, value);
}

void lum_shader_uniform_set2v(const lum_Shader* shader, const char* name, lum_Vec2 value) {
  Lum_Assert(shader && name);
  if (!shader->initialized) return;
  int loc = lum_shader_get_uniform_location(shader, name);
  if (loc < 0) return;
  glUniform2f(loc, value.x, value.y);
}

void lum_shader_uniform_set3v(const lum_Shader* shader, const char* name, lum_Vec3 value) {
  Lum_Assert(shader && name);
  if (!shader->initialized) return;
  int loc = lum_shader_get_uniform_location(shader, name);
  if (loc < 0) return;
  glUniform3f(loc, value.x, value.y, value.z);
}

void lum_shader_uniform_set4v(const lum_Shader* shader, const char* name, lum_Vec4 value) {
  Lum_Assert(shader && name);
  if (!shader->initialized) return;
  int loc = lum_shader_get_uniform_location(shader, name);
  if (loc < 0) return;
  glUniform4f(loc, value.x, value.y, value.z, value.w);
}

void lum_shader_uniform_set4m(const lum_Shader* shader, const char* name, lum_Mat4 value) {
  Lum_Assert(shader && name);
  if (!shader->initialized) return;
  int loc = lum_shader_get_uniform_location(shader, name);
  if (loc < 0) return;
  glUniformMatrix4fv(loc, 1, GL_FALSE, value.m);
}
