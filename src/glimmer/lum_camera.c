#include "glimmer/lum_camera.h"
#include "glimmer/lum_assert.h"

#include <math.h>          /* 3D 部分的 cosf/sinf（2D 部分不需要，但同一个文件里没必要分两处包含） */

/* ============================ 2D：正交相机 ============================ */

void lum_camera2d_init(lum_Camera2d* camera, float viewport_w, float viewport_h) {
  Lum_Assert(camera);

  camera->position = lum_vec2_0();
  camera->rotation = 0.0f;
  camera->zoom = 1.0f;
  camera->viewport_w = viewport_w;
  camera->viewport_h = viewport_h;
  camera->near_plane = -1.0f;
  camera->far_plane = 1.0f;
}

void lum_camera2d_set_position(lum_Camera2d* camera, lum_Vec2 position) {
  Lum_Assert(camera);
  camera->position = position;
}

void lum_camera2d_set_rotation(lum_Camera2d* camera, float rad) {
  Lum_Assert(camera);
  camera->rotation = rad;
}

void lum_camera2d_set_zoom(lum_Camera2d* camera, float zoom) {
  Lum_Assert(camera);
  camera->zoom = zoom > 0.0f ? zoom : 0.0001f;
}

void lum_camera2d_translate(lum_Camera2d* camera, lum_Vec2 delta) {
  Lum_Assert(camera);
  camera->position = lum_vec2_add(camera->position, delta);
}

void lum_camera2d_rotate(lum_Camera2d* camera, float delta_rad) {
  Lum_Assert(camera);
  camera->rotation += delta_rad;
}

void lum_camera2d_set_viewport(lum_Camera2d* camera, float w, float h) {
  Lum_Assert(camera);
  camera->viewport_w = w;
  camera->viewport_h = h;
}

void lum_camera2d_set_planes(lum_Camera2d* camera, float near, float far) {
  Lum_Assert(camera);
  camera->near_plane = near;
  camera->far_plane = far;
}

lum_Mat4 lum_camera2d_get_view_matrix(const lum_Camera2d* camera) {
  Lum_Assert(camera);

  lum_Mat4 t = lum_mat4_translate(-camera->position.x, -camera->position.y, 0.0f);
  lum_Mat4 r = lum_mat4_rotate_z(-camera->rotation);
  return lum_mat4_mul(r, t);
}

lum_Mat4 lum_camera2d_get_projection_matrix(const lum_Camera2d* camera) {
  Lum_Assert(camera);

  float width = camera->viewport_w / camera->zoom, height = camera->viewport_h / camera->zoom;
  float half_width = width * 0.5f, half_height = height * 0.5f;
  
  return lum_mat4_ortho(-half_width, half_width, -half_height, half_height, camera->near_plane, camera->far_plane);
}

lum_Mat4 lum_camera2d_get_view_projection_matrix(const lum_Camera2d* camera) {
  Lum_Assert(camera);
  return lum_mat4_mul(lum_camera2d_get_projection_matrix(camera), lum_camera2d_get_view_matrix(camera));
}

lum_Vec2 lum_camera2d_screen_to_world(const lum_Camera2d* cam, lum_Vec2 screen_pos, lum_Vec2 window_size) {
  Lum_Assert(cam && window_size.x > 0.0f && window_size.y > 0.0f);

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
  Lum_Assert(cam && window_size.x > 0.0f && window_size.y > 0.0f);

  lum_Mat4 vp = lum_camera2d_get_view_projection_matrix(cam);
  lum_Vec4 ndc = lum_mat4_mul_vec4(vp, lum_vec4_create(world_pos.x, world_pos.y, 0.0f, 1.0f));
  lum_Vec2 screen;
  screen.x = (ndc.x + 1.0f) * 0.5f * window_size.x;
  screen.y = (1.0f - ndc.y) * 0.5f * window_size.y;
  return screen;
}

/* ============================ 3D：透视相机 ============================ */

#define Lum_Camera3d_Default_Fov_Y_Rad (60.0f * Lum_Pi / 180.0f)
#define Lum_Camera3d_Default_Near      0.1f
#define Lum_Camera3d_Default_Far       100.0f

void lum_camera3d_init(lum_Camera3d* camera, float aspect) {
  Lum_Assert(camera);
  if (!camera) return;

  camera->position   = lum_vec3_create(0.0f, 0.0f, 3.0f);
  camera->target     = lum_vec3_0();
  camera->up         = lum_vec3_create(0.0f, 1.0f, 0.0f);
  camera->fov_y_rad  = Lum_Camera3d_Default_Fov_Y_Rad;
  camera->near_plane = Lum_Camera3d_Default_Near;
  camera->far_plane  = Lum_Camera3d_Default_Far;
  camera->aspect     = (aspect > 0.0f) ? aspect : 1.0f;
}

void lum_camera3d_set_viewport(lum_Camera3d* camera, float width, float height) {
  Lum_Assert(camera);
  if (!camera) return;
  if (!(width > 0.0f) || !(height > 0.0f)) return;   /* 高 DPI 下拿到 0 尺寸也不该把 aspect 弄成 NaN */
  camera->aspect = width / height;
}

void lum_camera3d_orbit(lum_Camera3d* camera, float yaw, float pitch, float distance) {
  Lum_Assert(camera);
  if (!camera) return;

  const float limit = Lum_Pi * 0.5f - 1e-3f;
  if (pitch >  limit) pitch =  limit;
  if (pitch < -limit) pitch = -limit;
  if (distance < 1e-3f) distance = 1e-3f;

  const float cp = cosf(pitch);
  const lum_Vec3 offset = lum_vec3_create(distance * cp * sinf(yaw),
                                          distance * sinf(pitch),
                                          distance * cp * cosf(yaw));
  camera->position = lum_vec3_add(camera->target, offset);
}

lum_Mat4 lum_camera3d_view(const lum_Camera3d* camera) {
  Lum_Assert(camera);
  if (!camera) return lum_mat4_identity();
  return lum_mat4_look_at(camera->position, camera->target, camera->up);
}

lum_Mat4 lum_camera3d_projection(const lum_Camera3d* camera) {
  Lum_Assert(camera);
  if (!camera) return lum_mat4_identity();
  return lum_mat4_perspective(camera->fov_y_rad, camera->aspect,
                              camera->near_plane, camera->far_plane);
}

lum_Mat4 lum_camera3d_view_projection(const lum_Camera3d* camera) {
  Lum_Assert(camera);
  if (!camera) return lum_mat4_identity();
  /* mul(a, b) 语义是"先应用 b"（列向量右乘），所以是 projection × view */
  return lum_mat4_mul(lum_camera3d_projection(camera), lum_camera3d_view(camera));
}

lum_Vec3 lum_camera3d_forward(const lum_Camera3d* camera) {
  Lum_Assert(camera);
  if (!camera) return lum_vec3_create(0.0f, 0.0f, -1.0f);
  return lum_vec3_normalise(lum_vec3_sub(camera->target, camera->position));
}
