#include <math.h>
#include <assert.h>
#include <string.h>

#include "math2d.h"
#ifdef Lum_Debug
#include <stdio.h>
#endif

int lum_mini(int a, int b) {
  return (a < b) ? a : b;
}

int lum_maxi(int a, int b) {
  return (a > b) ? a : b;
}

float lum_minf(float a, float b) {
  return (a < b) ? a : b;
}

float lum_maxf(float a, float b) {
  return (a > b) ? a : b;
}

int lum_float_equal(float a, float b, float epsilon) {
  return fabsf(a - b) <= epsilon;
}

int lum_float_nearly_equal(float a, float b, float abs_epsilon, float rel_epsilon) {
  float difference = fabsf(a - b);
  if (difference <= abs_epsilon) return 1;

  float abs_a = fabsf(a), abs_b = fabsf(b);
  float max_ab = lum_maxf(a, b);
  return difference <= max_ab * rel_epsilon;
}

int lum_float_eq(float a, float b) {
  return lum_float_nearly_equal(a, b, Lum_Float_Abs_Epsilon, Lum_Float_Rel_Epsilon);
}

int lum_float_is_zero(float x) {
  return lum_float_equal(x, 0, Lum_Float_Abs_Epsilon);
}

float lum_deg_to_rad(float deg) {
  return deg * Lum_Pi / 180.0f;
}

float lum_rad_to_deg(float rad) {
  return rad / Lum_Pi * 180.0f;
}

float lum_clamp(float value, float min, float max) {
  assert(min < max);
  value = lum_minf(value, max);
  value = lum_maxf(value, min);
  return value;
}

float lum_lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

lum_Vec2 lum_vec2_create(float x, float y) {
  lum_Vec2 v;
  v.x = x;
  v.y = y;
  return v;
}

lum_Vec2 lum_vec2_0(void) {
  return lum_vec2_create(0.0f, 0.0f);
}

lum_Vec2 lum_vec2_1(void) {
  return lum_vec2_create(1.0f, 1.0f);
}

lum_Vec2 lum_vec2_add(lum_Vec2 a, lum_Vec2 b) {
  return lum_vec2_create(a.x + b.x, a.y + b.y);
}

lum_Vec2 lum_vec2_sub(lum_Vec2 a, lum_Vec2 b) {
  return lum_vec2_create(a.x - b.x, a.y - b.y);
}

lum_Vec2 lum_vec2_mul(lum_Vec2 a, lum_Vec2 b) {
  return lum_vec2_create(a.x * b.x, a.y * b.y);
}

lum_Vec2 lum_vec2_div(lum_Vec2 a, lum_Vec2 b) {
  assert(!lum_float_is_zero(b.x) && !lum_float_is_zero(b.y));
  return lum_vec2_create(a.x / b.x, a.y / b.y);
}

lum_Vec2 lum_vec2_scale(lum_Vec2 v, float s) {
  return lum_vec2_create(v.x * s, v.y * s);
}

float lum_vec2_dot(lum_Vec2 a, lum_Vec2 b) {
  return a.x * b.x + a.y * b.y;
}

float lum_vec2_len_square(lum_Vec2 v) {
  return lum_vec2_dot(v, v);
}

float lum_vec2_len(lum_Vec2 v) {
  return sqrtf(lum_vec2_len_square(v));
}

lum_Vec2 lum_vec2_normalise(lum_Vec2 v) {
  float len = lum_vec2_len(v);
  return lum_float_is_zero(len) ? lum_vec2_0() : lum_vec2_scale(v, 1.0f / len);
}

float lum_vec2_distance_square(lum_Vec2 a, lum_Vec2 b) {
  return lum_vec2_len_square(lum_vec2_sub(a, b));
}

float lum_vec2_distance(lum_Vec2 a, lum_Vec2 b) {
  return sqrtf(lum_vec2_distance_square(a, b));
}

lum_Vec2 lum_vec2_lerp(lum_Vec2 a, lum_Vec2 b, float t) {
  return lum_vec2_create(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
}

int lum_vec2_eq(lum_Vec2 a, lum_Vec2 b) {
  return lum_float_eq(a.x, b.x) && lum_float_eq(a.y, b.y);
}

lum_Vec2 lum_vec2_from_angle(float rad) {
  return lum_vec2_create(cosf(rad), sinf(rad));
}

lum_Vec3 lum_vec3_create(float x, float y, float z) {
  lum_Vec3 v;
  v.x = x;
  v.y = y;
  v.z = z;
}

lum_Vec3 lum_vec3_0(void) {
  return lum_vec3_create(0.0f, 0.0f, 0.0f);
}

lum_Vec3 lum_vec3_1(void) {
  return lum_vec3_create(1.0f, 1.0f, 1.0f);
}

lum_Vec3 lum_vec3_add(lum_Vec3 a, lum_Vec3 b) {
  return lum_vec3_create(a.x + b.x, a.y + b.y, a.z + b.z);
}

lum_Vec3 lum_vec3_sub(lum_Vec3 a, lum_Vec3 b) {
  return lum_vec3_create(a.x - b.x, a.y - b.y, a.z - b.z);
}

lum_Vec3 lum_vec3_scale(lum_Vec3 v, float s) {
  return lum_vec3_create(v.x * s, v.y * s, v.z * s);
}

float lum_vec3_dot(lum_Vec3 a, lum_Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

float lum_vec3_len_square(lum_Vec3 v) {
  return lum_vec3_dot(v, v);
}

float lum_vec3_len(lum_Vec3 v) {
  return sqrtf(lum_vec3_len_square(v));
}

lum_Vec3 lum_vec3_normalise(lum_Vec3 v) {
  float len = lum_vec3_len(v);
  return lum_float_is_zero(len) ? lum_vec3_0() : lum_vec3_scale(v, 1.0f / len);
}

int lum_vec3_eq(lum_Vec3 a, lum_Vec3 b) {
  return lum_float_eq(a.x, b.x) && lum_float_eq(a.y, b.y) && lum_float_eq(a.z, b.z);
}

lum_Vec4 lum_vec4_create(float x, float y, float z, float w) {
  lum_Vec4 v;
  v.x = x;
  v.y = y;
  v.z = z;
  v.w = w;
  return v;
}

lum_Vec4 lum_vec4_0(void) {
  return lum_vec4_create(0.0f, 0.0f, 0.0f, 0.0f);
}

lum_Vec4 lum_vec4_1(void) {
  return lum_vec4_create(1.0f, 1.0f, 1.0f, 1.0f);
}

lum_Vec4 lum_vec4_add(lum_Vec4 a, lum_Vec4 b) {
  return lum_vec4_create(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

lum_Vec4 lum_vec4_sub(lum_Vec4 a, lum_Vec4 b) {
  return lum_vec4_create(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

lum_Vec4 lum_vec4_scale(lum_Vec4 v, float s) {
  return lum_vec4_create(v.x * s, v.y * s, v.z * s, v.w * s);
}

int lum_vec4_eq(lum_Vec4 a, lum_Vec4 b) {
  return lum_float_eq(a.x, b.x) && lum_float_eq(a.y, b.y) && lum_float_eq(a.z, b.z) && lum_float_eq(a.w, b.w);
}

lum_Mat4 lum_mat4_0(void) {
  lum_Mat4 m;
  memset(m.m, 0, sizeof(m.m));
  return m;
}

lum_Mat4 lum_mat4_identity(void) {
  lum_Mat4 m = lum_mat4_0();
  m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
  return m;
}

lum_Mat4 lum_mat4_mul(lum_Mat4 a, lum_Mat4 b) {
  lum_Mat4 result = lum_mat4_0();
  for (size_t col = 0; col < 4; ++col) {
    for (size_t row = 0; row < 4; ++row) {
      float sum = 0.0f;
      for (size_t k = 0; k < 4; ++k) {
	sum += a.m[k * 4 + row] * b.m[col * 4 + k];
      }
      result.m[col * 4 + row] = sum;
    }
  }
  return result;
}

lum_Mat4 lum_mat4_translate(float x, float y, float z) {
  lum_Mat4 m = lum_mat4_identity();
  m.m[12] = x;
  m.m[13] = y;
  m.m[14] = z;
  return m;
}

lum_Mat4 lum_mat4_scale(float x, float y, float z) {
  lum_Mat4 m = lum_mat4_identity();
  m.m[0] = x;
  m.m[5] = y;
  m.m[10] = z;
  return m;
}

lum_Mat4 lum_mat4_rotate_z(float rad) {
  lum_Mat4 m = lum_mat4_identity();
  float c = cosf(rad), s = sinf(rad);
  m.m[0] = c;
  m.m[1] = s;
  m.m[4] = -s;
  m.m[5] = c;
  return m;
}

lum_Mat4 lum_mat4_ortho(float left, float right, float bottom, float top, float near_plane, float far_plane) {
  lum_Mat4 m = lum_mat4_0();
  m.m[0]  =  2.0f / (right - left);
  m.m[5]  =  2.0f / (top - bottom);
  m.m[10] = -2.0f / (far_plane - near_plane);
  m.m[12] = -(right + left) / (right - left);
  m.m[13] = -(top + bottom) / (top - bottom);
  m.m[14] = -(far_plane + near_plane) / (far_plane - near_plane);
  m.m[15] =  1.0f;
  return m;
}

lum_Vec4 lum_mat4_mul_vec4(lum_Mat4 m, lum_Vec4 v) {
  lum_Vec4 result;
  result.x = m.m[0] * v.x + m.m[4] * v.y + m.m[8]  * v.z + m.m[12] * v.w;
  result.y = m.m[1] * v.x + m.m[5] * v.y + m.m[9]  * v.z + m.m[13] * v.w;
  result.z = m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14] * v.w;
  result.w = m.m[3] * v.x + m.m[7] * v.y + m.m[11] * v.z + m.m[15] * v.w;
  return result;
}

int lum_mat4_eq(lum_Mat4 a, lum_Mat4 b) {
  for (size_t i = 0; i < 16; i++) {
    if (!lum_float_eq(a.m[i], b.m[i])) return 0;
  }
  return 1;
}

lum_Rect lum_rect_create(float x, float y, float w, float h) {
  lum_Rect r;
  r.position.x = x;
  r.position.y = y;
  r.size.x = w;
  r.size.y = h;
  return r;
}

int lum_rect_contains(lum_Rect r, lum_Vec2 point) {
  return (point.x >= r.position.x && point.x <= r.position.x + r.size.x &&
	  point.y >= r.position.y && point.y <= r.position.y + r.size.y);
}

int lum_rect_intersects(lum_Rect a, lum_Rect b) {
  return (a.position.x < b.position.x + b.size.x &&
	  a.position.x + a.size.x > b.position.x &&
	  a.position.y < b.position.y + b.size.y &&
	  a.position.y + a.size.y > b.position.y);
}

lum_Vec2 lum_rect_center(lum_Rect r) {
  return lum_vec2_create(r.position.x + r.size.x * 0.5f, r.position.y + r.size.y * 0.5f);
}

lum_Vec2 lum_rect_corner(lum_Rect r, uint8_t index) {
  assert(index < 4);
  switch(index) {
  case 0: return r.position;
  case 1: return lum_vec2_create(r.position.x + r.size.x, r.position.y);
  case 2: return lum_vec2_create(r.position.x + r.size.x, r.position.y + r.size.y);
  case 3: return lum_vec2_create(r.position.x, r.position.y + r.size.y);
  }
}

int lum_rect_eq(lum_Rect a, lum_Rect b) {
  return lum_vec2_eq(a.position, b.position) && lum_vec2_eq(a.size, b.size);
}

lum_Mat4 lum_mat4_trans_2d(lum_Vec2 pos, lum_Vec2 scale, float rotation_rad) {
  lum_Mat4 t = lum_mat4_translate(pos.x, pos.y, 0.0f);
  lum_Mat4 r = lum_mat4_rotate_z(rotation_rad);
  lum_Mat4 s = lum_mat4_scale(scale.x, scale.y, 1.0f);
  return lum_mat4_mul(t, lum_mat4_mul(r, s));
}

lum_Mat4 lum_mat4_trans_2d_with_size(lum_Vec2 pos, lum_Vec2 size, lum_Vec2 scale, float rotation_rad) {
  lum_Mat4 t = lum_mat4_translate(pos.x, pos.y, 0.0f);
  lum_Mat4 r = lum_mat4_rotate_z(rotation_rad);
  lum_Mat4 s = lum_mat4_scale(scale.x * size.x, scale.y * size.y, 1.0f);
  return lum_mat4_mul(t, lum_mat4_mul(r, s));
};

lum_Mat4 lum_mat4_trans_2d_with_size_and_origin(lum_Vec2 pos, lum_Vec2 size, lum_Vec2 scale, float rotation_rad, lum_Vec2 origin) {
  lum_Mat4 t_pos = lum_mat4_translate(pos.x, pos.y, 0.0f);
  lum_Mat4 r = lum_mat4_rotate_z(rotation_rad);
  lum_Mat4 s = lum_mat4_scale(scale.x * size.x, scale.y * size.y, 1.0f);
  lum_Mat4 t_origin = lum_mat4_translate(-origin.x, -origin.y, 0.0f);
  return lum_mat4_mul(t_pos, lum_mat4_mul(r, lum_mat4_mul(s, t_origin)));
};


#ifdef Lum_Debug
void lum_vec2_print(lum_Vec2 v) {
  printf("[x]" "\e[7m" "%*.3f" "\e[0m  ",(lum_float_is_zero(v.x) ? 1 : (int)floor(log10((double)floor(fabsf(v.x)))) + 1) + 5, v.x);
  printf("[y]" "\e[7m" "%*.3f" "\e[0m" "\n",(lum_float_is_zero(v.y) ? 1 : (int)floor(log10((double)floor(fabsf(v.y)))) + 1) + 5, v.y);
}

void lum_vec3_print(lum_Vec3 v) {
  printf("[x]" "\e[7m" "%*.3f" "\e[0m  ",(lum_float_is_zero(v.x) ? 1 : (int)floor(log10((double)floor(fabsf(v.x)))) + 1) + 5, v.x);
  printf("[y]" "\e[7m" "%*.3f" "\e[0m  ",(lum_float_is_zero(v.y) ? 1 : (int)floor(log10((double)floor(fabsf(v.y)))) + 1) + 5, v.y);
  printf("[z]" "\e[7m" "%*.3f" "\e[0m" "\n",(lum_float_is_zero(v.z) ? 1 : (int)floor(log10((double)floor(fabsf(v.z)))) + 1) + 5, v.z);
}

void lum_vec4_print(lum_Vec4 v) {
  printf("[x]" "\e[7m" "%*.3f" "\e[0m  ",(lum_float_is_zero(v.x) ? 1 : (int)floor(log10((double)floor(fabsf(v.x)))) + 1) + 5, v.x);
  printf("[y]" "\e[7m" "%*.3f" "\e[0m  ",(lum_float_is_zero(v.y) ? 1 : (int)floor(log10((double)floor(fabsf(v.y)))) + 1) + 5, v.y);
  printf("[z]" "\e[7m" "%*.3f" "\e[0m  ",(lum_float_is_zero(v.z) ? 1 : (int)floor(log10((double)floor(fabsf(v.z)))) + 1) + 5, v.z);
  printf("[w]" "\e[7m" "%*.3f" "\e[0m" "\n",(lum_float_is_zero(v.w) ? 1 : (int)floor(log10((double)floor(fabsf(v.w)))) + 1) + 5, v.w);
}

void lum_mat4_print(lum_Mat4 m) {
  float max = 0.0f;
  for (size_t col = 0; col < 4; ++col) {
    for (size_t row = 0; row < 4; ++row) {
      float value = m.m[row * 4 + col];
      if (fabsf(value) > max) max = fabsf(value);
    }
  }
  
  for (size_t col = 0, w = (lum_float_is_zero(max) ? 1 : (int)floor(log10((double)floor(max))) + 1) + 5; col < 4; ++col) {
    for (size_t row = 0; row < 4; ++row) {
      float value = m.m[row * 4 + col];
      printf("[%02d]" "\e[7m" "%*.3f" "\e[0m  ", row * 4 + col, w, value);
    }
    printf("\n");
  }
}
#endif
