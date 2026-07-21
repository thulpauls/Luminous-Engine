#include "lum_camera.h"
#include <assert.h>

void lum_camera2d_init(lum_Camera2d* camera, float viewport_w, float viewport_h) {
  assert(camera);

  camera->position = lum_vec2_0();
  camera->rotation = 0.0f;
  camera->zoom = 1.0f;
  camera->viewport_w = viewport_w;
  camera->viewport_h = viewport_h;
  camera->near_plane = -1.0f;
  camera->far_plane = 1.0f;
}

void lum_camera2d_set_position(lum_Camera2d* camera, lum_Vec2 position) {
  assert(camera);
  camera->position = position;
}

void lum_camera2d_set_rotation(lum_Camera2d* camera, float rad) {
  assert(camera);
  camera->rotation = rad;
}

void lum_camera2d_set_zoom(lum_Camera2d* camera, float zoom) {
  assert(camera);
  camera->zoom = zoom > 0.0f ? zoom : 0.0001f;
}

void lum_camera2d_translate(lum_Camera2d* camera, lum_Vec2 delta) {
  assert(camera);
  camera->position = lum_vec2_add(camera->position, delta);
}

void lum_camera2d_rotate(lum_Camera2d* camera, float delta_rad) {
  assert(camera);
  camera->rotation += delta_rad;
}

void lum_camera2d_set_viewport(lum_Camera2d* camera, float w, float h) {
  assert(camera);
  camera->viewport_w = w;
  camera->viewport_h = h;
}

void lum_camera2d_set_planes(lum_Camera2d* camera, float near, float far) {
  assert(camera);
  camera->near_plane = near;
  camera->far_plane = far;
}

lum_Mat4 lum_camera2d_get_view_matrix(const lum_Camera2d* camera) {
  assert(camera);

  lum_Mat4 t = lum_mat4_translate(-camera->position.x, -camera->position.y, 0.0f);
  lum_Mat4 r = lum_mat4_rotate_z(-camera->rotation);
  return lum_mat4_mul(t, r);
}

lum_Mat4 lum_camera2d_get_projection_matrix(const lum_Camera2d* camera) {
  assert(camera);

  float width = camera->viewport_w / camera->zoom, height = camera->viewport_h / camera->zoom;
  float half_width = width * 0.5f, half_height = height * 0.5f;
  
  return lum_mat4_ortho(-half_width, half_width, -half_height, half_height, camera->near_plane, camera->far_plane);
}

lum_Mat4 lum_camera2d_get_view_projection_matrix(const lum_Camera2d* camera) {
  assert(camera);
  return lum_mat4_mul(lum_camera2d_get_projection_matrix(camera), lum_camera2d_get_view_matrix(camera));
}
