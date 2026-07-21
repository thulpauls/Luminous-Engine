#ifndef Lum_Camera_H
#define Lum_Camera_H

#include "lum_math2d.h"

typedef struct lum_Camera2d {
  lum_Vec2 position;
  float rotation;
  float zoom;

  float viewport_w, viewport_h;
  float near_plane, far_plane;
} lum_Camera2d;

void lum_camera2d_init(lum_Camera2d* camera, float viewport_w, float viewport_h);

void lum_camera2d_set_position(lum_Camera2d* camera, lum_Vec2 position);
void lum_camera2d_set_rotation(lum_Camera2d* camera, float rad);
void lum_camera2d_set_zoom(lum_Camera2d* camera, float zoom);

void lum_camera2d_translate(lum_Camera2d* camera, lum_Vec2 delta);
void lum_camera2d_rotate(lum_Camera2d* camera, float delta_rad);

void lum_camera2d_set_viewport(lum_Camera2d* camera, float w, float h);
void lum_camera2d_set_planes(lum_Camera2d* camera, float near, float far);

lum_Mat4 lum_camera2d_get_view_matrix(const lum_Camera2d* camera);
lum_Mat4 lum_camera2d_get_projection_matrix(const lum_Camera2d* camera);
lum_Mat4 lum_camera2d_get_view_projection_matrix(const lum_Camera2d* camera);

#endif //Lum_Camera_H
