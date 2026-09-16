#ifndef Lum_Transform_H
#define Lum_Transform_H

#include "lum_math2d.h"

typedef struct lum_Transform2d {
  lum_Vec2 position, size, scale;
  float rotation;
  lum_Vec2 origin;
} lum_Transform2d;

void lum_transform2d_init(lum_Transform2d* t);
lum_Transform2d lum_transform2d_create(lum_Vec2 position, lum_Vec2 size, lum_Vec2 scale, float rotation_rad, lum_Vec2 origin);

void lum_transform2d_set_position(lum_Transform2d* t, lum_Vec2 position);
void lum_transform2d_set_size(lum_Transform2d* t, lum_Vec2 size);
void lum_transform2d_set_scale(lum_Transform2d* t, lum_Vec2 scale);
void lum_transform2d_set_rotation(lum_Transform2d* t, float rad);
void lum_transform2d_set_origin(lum_Transform2d* t, lum_Vec2 origin);
void lum_transform2d_translate(lum_Transform2d* t, lum_Vec2 delta);
void lum_transform2d_rotate(lum_Transform2d* t, float delta_rad);
void lum_transform2d_set_center_origin(lum_Transform2d* t);

lum_Mat4 lum_transform2d_get_matrix(const lum_Transform2d* t);

#endif //Lum_Transform_H
