#ifndef Lum_Shader_H
#define Lum_Shader_H

#include <stdint.h>
#include "lum_math2d.h"

typedef struct lum_Shader {
  uint32_t program;
  bool initialized;
} lum_Shader;

uint32_t lum_shader_compile_from_source(const char* source, const uint32_t shader_type);
uint32_t lum_shader_link_shaders(const unsigned int vertex, const unsigned int fragment);
uint32_t lum_shader_create_program_from_source(const char* vertex_source, const char* fragment_source);

int lum_shader_create_from_source(lum_Shader* shader, const char* vertex_source, const char* fragment_source);
void lum_shader_destroy(lum_Shader* shader);
void lum_shader_use(const lum_Shader* shader);

int lum_shader_get_uniform_location(const lum_Shader* shader, const char* name);
void lum_shader_uniform_set1i(const lum_Shader* shader, const char* name, int value);
void lum_shader_uniform_set1f(const lum_Shader* shader, const char* name, float value);
void lum_shader_uniform_set2v(const lum_Shader* shader, const char* name, lum_Vec2 value);
void lum_shader_uniform_set3v(const lum_Shader* shader, const char* name, lum_Vec3 value);
void lum_shader_uniform_set4v(const lum_Shader* shader, const char* name, lum_Vec4 value);
void lum_shader_uniform_set4m(const lum_Shader* shader, const char* name, lum_Mat4 value);

#endif // Lum_Shader_H
