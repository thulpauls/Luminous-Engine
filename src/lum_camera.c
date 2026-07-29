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
  return lum_mat4_mul(r, t);
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

lum_Vec2 lum_camera2d_screen_to_world(const lum_Camera2d* cam, lum_Vec2 screen_pos, lum_Vec2 window_size) {
  assert(cam && window_size.x > 0.0f && window_size.y > 0.0f);

  lum_Vec4 ndc;
  ndc.x = (screen_pos.x / window_size.x) * 2.0f - 1.0f;
  ndc.y = 1.0f - (screen_pos.y / window_size.y) * 2.0f;
  ndc.z = 0.0f;
  ndc.w = 1.0f;

  lum_Mat4 vp = lum_camera2d_get_view_projection_matrix(cam);
  lum_Mat4 vp_inv = lum_mat4_inv(vp);
  lum_Vec4 world = lum_mat4_mul_vec4(vp_inv, ndc);

  return lum_vec2_create(world.x, world.y);
}

lum_Vec2 lum_camera2d_world_to_screen(const lum_Camera2d* cam, lum_Vec2 world_pos, lum_Vec2 window_size) {
  assert(cam && window_size.x > 0.0f && window_size.y > 0.0f);

  lum_Mat4 vp = lum_camera2d_get_view_projection_matrix(cam);
  lum_Vec4 ndc = lum_mat4_mul_vec4(vp, lum_vec4_create(world_pos.x, world_pos.y, 0.0f, 1.0f));
  lum_Vec2 screen;
  screen.x = (ndc.x + 1.0f) * 0.5f * window_size.x;
  screen.y = (1.0f - ndc.y) * 0.5f * window_size.y;
  return screen;
}