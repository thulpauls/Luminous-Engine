#include "lum_transform.h"
#include "lum_math2d.h"
#include <assert.h>

void lum_transform2d_init(lum_Transform2d* t) {
  assert(t);

  t->position = lum_vec2_0();
  t->size = lum_vec2_1();
  t->scale = lum_vec2_1();
  t->rotation = 0.0f;
  t->origin = lum_vec2_0();
}

lum_Transform2d lum_transform2d_create(lum_Vec2 position, lum_Vec2 size, lum_Vec2 scale, float rotation, lum_Vec2 origin) {
  lum_Transform2d t;
  t.position = position;
  t.size = size;
  t.scale = scale;
  t.rotation = rotation;
  t.origin = origin;

  return t;
}

void lum_transform2d_set_position(lum_Transform2d* t, lum_Vec2 position) {
  assert(t);
  t->position = position;
}

void lum_transform2d_set_size(lum_Transform2d* t, lum_Vec2 size) {
  assert(t);
  t->size = size;
}

void lum_transform2d_set_scale(lum_Transform2d* t, lum_Vec2 scale) {
  assert(t);
  t->scale = scale;
}

void lum_transform2d_set_rotation(lum_Transform2d* t, float rad) {
  assert(t);
  t->rotation = rad;
}

void lum_transform2d_set_origin(lum_Transform2d* t, lum_Vec2 origin) {
  assert(t);
  t->origin = origin;
}

void lum_transform2d_translate(lum_Transform2d* t, lum_Vec2 delta) {
  assert(t);
  t->position = lum_vec2_add(t->position, delta);
}

void lum_transform2d_rotate(lum_Transform2d* t, float delta_rad) {
  assert(t);
  t->rotation += delta_rad;
}

void lum_transform2d_set_center_origin(lum_Transform2d* t) {
  assert(t);
  t->origin = lum_vec2_create(t->size.x * 0.5f, t->size.y * 0.5f);
}

lum_Mat4 lum_transform2d_get_matrix(const lum_Transform2d* t) {
  assert(t);
  return lum_mat4_trans_2d_with_size_and_origin(t->position, t->size, t->scale, t->rotation, t->origin);
}
