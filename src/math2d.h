#ifndef Lum_Math2d_H
#define Lum_Math2d_H

#include <stdint.h>

#define Lum_Pi (3.14159265358979323846)

#define Lum_Float_Abs_Epsilon (1e-6f)
#define Lum_Float_Rel_Epsilon (1e-5f)

typedef struct lum_Vec2 {
  float x, y;
} lum_Vec2;

typedef struct lum_Vec3 {
  float x, y, z;
} lum_Vec3;

typedef struct lum_Vec4 {
  float x, y, z, w;
} lum_Vec4;

typedef struct lum_Mat4 {
  float m[16];
} lum_Mat4;

typedef struct lum_Rect {
  lum_Vec2 position;
  lum_Vec2 size;
} lum_Rect;

int lum_mini(int a, int b);
int lum_maxi(int a, int b);
float lum_minf(float a, float b);
float lum_maxf(float a, float b);

int lum_float_equal(float a, float b, float epsilon);
int lum_float_nearly_equal(float a, float b, float abs_epsilon, float rel_epsilon);
int lum_float_eq(float a, float b);
int lum_float_is_zero(float x);

float lum_deg_to_rad(float deg);
float lum_rad_to_deg(float rad);
float lum_clamp(float value, float min, float max);
float lum_lerp(float a, float b, float t);

lum_Vec2 lum_vec2_create(float x, float y);
lum_Vec2 lum_vec2_0(void);
lum_Vec2 lum_vec2_1(void);
lum_Vec2 lum_vec2_add(lum_Vec2 a, lum_Vec2 b);
lum_Vec2 lum_vec2_sub(lum_Vec2 a, lum_Vec2 b);
lum_Vec2 lum_vec2_mul(lum_Vec2 a, lum_Vec2 b);
lum_Vec2 lum_vec2_div(lum_Vec2 a, lum_Vec2 b);
lum_Vec2 lum_vec2_scale(lum_Vec2 v, float s);
float lum_vec2_dot(lum_Vec2 a, lum_Vec2 b);
float lum_vec2_len_square(lum_Vec2 v);
float lum_vec2_len(lum_Vec2 v);
lum_Vec2 lum_vec2_normalise(lum_Vec2 v);
float lum_vec2_distance_square(lum_Vec2 a, lum_Vec2 b);
float lum_vec2_distance(lum_Vec2 a, lum_Vec2 b);
lum_Vec2 lum_vec2_lerp(lum_Vec2 a, lum_Vec2 b, float t);
int lum_vec2_eq(lum_Vec2 a, lum_Vec2 b);
lum_Vec2 lum_vec2_from_angle(float rad);

lum_Vec3 lum_vec3_create(float x, float y, float z);
lum_Vec3 lum_vec3_0(void);
lum_Vec3 lum_vec3_1(void);
lum_Vec3 lum_vec3_add(lum_Vec3 a, lum_Vec3 b);
lum_Vec3 lum_vec3_sub(lum_Vec3 a, lum_Vec3 b);
lum_Vec3 lum_vec3_scale(lum_Vec3 v, float s);
float lum_vec3_dot(lum_Vec3 a, lum_Vec3 b);
float lum_vec3_len_square(lum_Vec3 v);
float lum_vec3_len(lum_Vec3 v);
lum_Vec3 lum_vec3_normalise(lum_Vec3 v);
int lum_vec3_eq(lum_Vec3 a, lum_Vec3 b);

lum_Vec4 lum_vec4_create(float x, float y, float z, float w);
lum_Vec4 lum_vec4_0(void);
lum_Vec4 lum_vec4_1(void);
lum_Vec4 lum_vec4_add(lum_Vec4 a, lum_Vec4 b);
lum_Vec4 lum_vec4_sub(lum_Vec4 a, lum_Vec4 b);
lum_Vec4 lum_vec4_scale(lum_Vec4 v, float s);
int lum_vec4_eq(lum_Vec4 a, lum_Vec4 b);

lum_Mat4 lum_mat4_0(void);
lum_Mat4 lum_mat4_identity(void);
lum_Mat4 lum_mat4_mul(lum_Mat4 a, lum_Mat4 b);
lum_Mat4 lum_mat4_translate(float x, float y, float z);
lum_Mat4 lum_mat4_scale(float x, float y, float z);
lum_Mat4 lum_mat4_rotate_z(float rad);
lum_Mat4 lum_mat4_ortho(float left, float right, float bottom, float top, float near_plane, float far_plane);
lum_Vec4 lum_mat4_mul_vec4(lum_Mat4 m, lum_Vec4 v);
int lum_mat4_eq(lum_Mat4 a, lum_Mat4 b);

lum_Rect lum_rect_create(float x, float y, float w, float h);
int lum_rect_contains(lum_Rect r, lum_Vec2 point);
int lum_rect_intersects(lum_Rect a, lum_Rect b);
lum_Vec2 lum_rect_center(lum_Rect r);
lum_Vec2 lum_rect_corner(lum_Rect r, uint8_t index);
int lum_rect_eq(lum_Rect a, lum_Rect b);

lum_Mat4 lum_mat4_trans_2d(lum_Vec2 pos, lum_Vec2 scale, float rotation_rad);
lum_Mat4 lum_mat4_trans_2d_with_size(lum_Vec2 pos, lum_Vec2 size, lum_Vec2 scale, float rotation_rad);
lum_Mat4 lum_mat4_trans_2d_with_size_and_origin(lum_Vec2 pos, lum_Vec2 size, lum_Vec2 scale, float rotation_rad, lum_Vec2 origin);

#ifdef Lum_Debug
void lum_vec2_print(lum_Vec2 v);
void lum_vec3_print(lum_Vec3 v);
void lum_vec4_print(lum_Vec4 v);
void lum_mat4_print(lum_Mat4 m);
#endif

#endif //Lum_Math2d_H
