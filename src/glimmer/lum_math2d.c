#include <math.h>
#include "glimmer/lum_assert.h"
#include <string.h>
#include <stdio.h>
#include "glimmer/lum_math2d.h"
#include "glimmer/lum_log.h"

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
  float max_ab = lum_maxf(abs_a, abs_b);
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
  Lum_Assert(min < max);
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
  Lum_Assert(!lum_float_is_zero(b.x) && !lum_float_is_zero(b.y));
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

lum_Vec3 lum_vec2_to_vec3(lum_Vec2 v) {
  return lum_vec3_create(v.x, v.y, 0.0f);
}

lum_Vec3 lum_vec3_create(float x, float y, float z) {
  lum_Vec3 v;
  v.x = x;
  v.y = y;
  v.z = z;
  return v;
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

/* 右手系叉积。look_at 建正交基、法线变换都要它 */
lum_Vec3 lum_vec3_cross(lum_Vec3 a, lum_Vec3 b) {
  return lum_vec3_create(a.y * b.z - a.z * b.y,
                         a.z * b.x - a.x * b.z,
                         a.x * b.y - a.y * b.x);
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

lum_Vec4 lum_vec3_to_vec4(lum_Vec3 v) {
  return lum_vec4_create(v.x, v.y, v.z, 0.0f);
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

lum_Mat4 lum_mat4_inv(lum_Mat4 m) {
  float aug[4][8];
  for (int c = 0; c < 4; ++c) {
    for (int r = 0; r < 4; ++r) {
      aug[r][c]     = m.m[c * 4 + r];
      aug[r][c + 4] = (c == r) ? 1.0f : 0.0f;
    }
  }

  for (int col = 0; col < 4; ++col) {
    int pivot = col;
    float max_abs = fabsf(aug[col][col]);
    for (int r = col + 1; r < 4; ++r) {
      float v = fabsf(aug[r][col]);
      if (v > max_abs) { max_abs = v; pivot = r; }
    }
    if (max_abs < 1e-7f) return lum_mat4_identity();

    if (pivot != col) {
      for (int k = 0; k < 8; ++k) {
        float tmp = aug[col][k]; aug[col][k] = aug[pivot][k]; aug[pivot][k] = tmp;
      }
    }

    float inv_pivot = 1.0f / aug[col][col];
    for (int k = 0; k < 8; ++k) aug[col][k] *= inv_pivot;

    for (int r = 0; r < 4; ++r) {
      if (r == col) continue;
      float factor = aug[r][col];
      if (factor == 0.0f) continue;
      for (int k = 0; k < 8; ++k) aug[r][k] -= factor * aug[col][k];
    }
  }

  lum_Mat4 inv = lum_mat4_0();
  for (int c = 0; c < 4; ++c)
    for (int r = 0; r < 4; ++r)
      inv.m[c * 4 + r] = aug[r][c + 4];
  return inv;
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

lum_Mat4 lum_mat4_rotate_x(float rad) {
  lum_Mat4 m = lum_mat4_identity();
  float c = cosf(rad), s = sinf(rad);
  m.m[5]  = c;
  m.m[6]  = s;
  m.m[9]  = -s;
  m.m[10] = c;
  return m;
}

lum_Mat4 lum_mat4_rotate_y(float rad) {
  lum_Mat4 m = lum_mat4_identity();
  float c = cosf(rad), s = sinf(rad);
  m.m[0]  = c;
  m.m[2]  = -s;
  m.m[8]  = s;
  m.m[10] = c;
  return m;
}

/* 转置（将来算法线矩阵 inv-transpose 的另一半） */
lum_Mat4 lum_mat4_transpose(lum_Mat4 m) {
  lum_Mat4 r = lum_mat4_0();
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      r.m[col * 4 + row] = m.m[row * 4 + col];
    }
  }
  return r;
}

/*
* 透视投影：右手系、相机朝 -Z。参数非法一律返回单位矩阵并记日志 ——
* 相比"返回 NaN 矩阵"，画面异常时更容易定位（单位矩阵会画出平面正交效果，
* 一眼就能看出是投影参数错了）。3D 的第一批功能里这是最容易传错的地方
* （把角度当弧度、aspect 传成 0、near/far 反了）。
*/
lum_Mat4 lum_mat4_perspective(float fov_y_rad, float aspect, float near_plane, float far_plane) {
  if (!(fov_y_rad > 0.0f) || !(fov_y_rad < Lum_Pi) || !(aspect > 0.0f) ||
      !(near_plane > 0.0f) || !(far_plane > near_plane)) {
    Lum_Log_Error("lum_mat4_perspective: 参数非法 (fov=%.4f aspect=%.4f near=%.4f far=%.4f)",
                  (double)fov_y_rad, (double)aspect, (double)near_plane, (double)far_plane);
    return lum_mat4_identity();
  }

  const float f = 1.0f / tanf(fov_y_rad * 0.5f);
  lum_Mat4 m = lum_mat4_0();
  m.m[0]  = f / aspect;
  m.m[5]  = f;
  m.m[10] = (far_plane + near_plane) / (near_plane - far_plane);
  m.m[11] = -1.0f;
  m.m[14] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
  return m;
}

/* 视图矩阵：右手系正交基，(s, u, -f) 三轴 + 平移 -dot(轴, eye) */
lum_Mat4 lum_mat4_look_at(lum_Vec3 eye, lum_Vec3 target, lum_Vec3 up) {
  const lum_Vec3 f = lum_vec3_normalise(lum_vec3_sub(target, eye));
  /* 先把 up 归一化再叉乘：这样下面的退化判据只反映"夹角是否趋零"，
     而不会把"up 量级特别小"也误判成退化（up = (0, 1e-8, 0) 的方向完全合法）。 */
  const lum_Vec3 up_n = lum_vec3_normalise(up);
  const lum_Vec3 s = lum_vec3_cross(f, up_n);
  const lum_Vec3 u = lum_vec3_cross(s, f);

  /* 视线与 up 平行（或 eye == target、up 为零）→ 正交基退化，别让它产生 NaN。
     |s| = sin(f 与 up 的夹角)，所以阈值 1e-6 等价于"夹角小于 0.00006 度" ——
     此时叉积方向已被舍入误差支配，归一化出来是噪声方向，画面会抖。
     只查 s 就够：u = s × f 且 s ⊥ f，所以 |u| 恒等于 |s|。 */
  if (lum_vec3_len_square(s) < 1e-12f) {
    Lum_Log_Error("lum_mat4_look_at: up 与视线平行或 eye == target，返回单位矩阵");
    return lum_mat4_identity();
  }

  const lum_Vec3 sn = lum_vec3_normalise(s);
  const lum_Vec3 un = lum_vec3_normalise(u);

  lum_Mat4 m = lum_mat4_identity();
  m.m[0] = sn.x; m.m[4] = sn.y; m.m[8]  = sn.z;
  m.m[1] = un.x; m.m[5] = un.y; m.m[9]  = un.z;
  m.m[2] = -f.x; m.m[6] = -f.y; m.m[10] = -f.z;
  m.m[12] = -lum_vec3_dot(sn, eye);
  m.m[13] = -lum_vec3_dot(un, eye);
  m.m[14] =  lum_vec3_dot(f, eye);
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
  Lum_Assert(index < 4);
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
  return lum_mat4_mul(t_pos, lum_mat4_mul(r, lum_mat4_mul(t_origin, s)));
};

#define Lum__Print_Cell_Max  6      /* 一行最多几个分量（vec4 或矩阵一行 4 个） */
#define Lum__Print_Cell_Size 32     /* 单个分量的文本上限（超长浮点会被截断） */
#define Lum__Print_Buf_Size  768    /* 一行放得下 6 个分量 */

/* 把 count 个分量拼成 "[x] -1.500  [y] 2.000" 这样一行，数字右对齐到最宽的那个。
   返回 buf 本身，方便直接当 %s 用。 */
static const char* lum__format_row(const char* const* names, const float* values,
                                   int count, char* buf, size_t buf_size) {
  if (count > Lum__Print_Cell_Max) count = Lum__Print_Cell_Max;

  char cell[Lum__Print_Cell_Max][Lum__Print_Cell_Size];
  int width = 0;
  for (int i = 0; i < count; ++i) {
    snprintf(cell[i], sizeof(cell[i]), "%.3f", (double)values[i]);
    const int w = (int)strlen(cell[i]);
    if (w > width) width = w;
  }

  size_t pos = 0;
  buf[0] = '\0';
  for (int i = 0; i < count; ++i) {
    const int n = snprintf(buf + pos, buf_size - pos, "%s[%s] %*s",
                           (i > 0) ? "  " : "", names[i], width, cell[i]);
    if (n < 0 || (size_t)n >= buf_size - pos) break;   /* 放不下就停手 */
    pos += (size_t)n;
  }
  return buf;
}

void lum_vec2_print(lum_Vec2 v) {
  static const char* const names[] = { "x", "y" };
  const float values[] = { v.x, v.y };
  char buf[Lum__Print_Buf_Size];
  lum_log_message(Lum_Log_Level_Debug, __FILE__, __LINE__, __func__, "%s",
                  lum__format_row(names, values, 2, buf, sizeof(buf)));
}

void lum_vec3_print(lum_Vec3 v) {
  static const char* const names[] = { "x", "y", "z" };
  const float values[] = { v.x, v.y, v.z };
  char buf[Lum__Print_Buf_Size];
  lum_log_message(Lum_Log_Level_Debug, __FILE__, __LINE__, __func__, "%s",
                  lum__format_row(names, values, 3, buf, sizeof(buf)));
}

void lum_vec4_print(lum_Vec4 v) {
  static const char* const names[] = { "x", "y", "z", "w" };
  const float values[] = { v.x, v.y, v.z, v.w };
  char buf[Lum__Print_Buf_Size];
  lum_log_message(Lum_Log_Level_Debug, __FILE__, __LINE__, __func__, "%s",
                  lum__format_row(names, values, 4, buf, sizeof(buf)));
}

/* 矩阵按【存储顺序】成行读：第 r 行取 m[r]、m[r+4]、m[r+8]、m[r+12]
   （列主序，所以同一行里相邻索引相隔 4）。四行合成【一条】日志消息 ——
   一个前缀带四行比四条各自带前缀的消息清楚得多。 */
void lum_mat4_print(lum_Mat4 m) {
  char row[Lum__Print_Buf_Size];
  char buf[Lum__Print_Buf_Size * 5];

  const int head = snprintf(buf, sizeof(buf), "mat4 (column-major, row step 4)");
  if (head < 0) return;
  size_t pos = (size_t)head;

  for (int r = 0; r < 4; ++r) {
    const float values[] = { m.m[r], m.m[r + 4], m.m[r + 8], m.m[r + 12] };
    char label[4][3];
    const char* labels[4];
    for (int i = 0; i < 4; ++i) {
      snprintf(label[i], sizeof(label[i]), "%02d", r + i * 4);
      labels[i] = label[i];
    }

    const int n = snprintf(buf + pos, sizeof(buf) - pos, "\n%s",
                           lum__format_row(labels, values, 4, row, sizeof(row)));
    if (n < 0 || (size_t)n >= sizeof(buf) - pos) break;
    pos += (size_t)n;
  }

  lum_log_message(Lum_Log_Level_Debug, __FILE__, __LINE__, __func__, "%s", buf);
}
