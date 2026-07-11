#include "transform.h"
#include "math2d.h"

void lum_transform2d_init(lum_Transform2d* t) {
  assert(t);

  t->position = lum_Vec2_0();
  t->size = lum_Vec2_1();
  t->scale = lum_Vec2_1();
  t->rotation = 0.0f;
  t->origin = lum_Vec2_0();
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

lum_Mat4 lum_transform2d_get_matrix(const lum_Transform2d* t) {
  assert(t);
  return lum_mat4_trans_2d_with_size_and_origin(t->position, t->size, t->scale, t->rotation, t->origin);
}
