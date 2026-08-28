#include "src/luminous.h"
#include <math.h>
#include <string.h>

/* glow_demo —— 程序化光效演示。
 * 全部特效均为 ECS 实体：C_glow / C_beam 为 demo 本地组件（带字段表，随 F5/F9 一起存读档），
 * 由渲染系统注册表驱动绘制；鼠标特效挂在一个跟随光标的隐形实体之下（层级联动）。 */

static lum_Font* g_font;
static const lum_Texture2d* g_ring;

/* ---------- demo 本地组件：光晕 / 光圈 / 光束 ---------- */

typedef struct {
  bool     ring;              /* false = 高斯光晕，true = 中空光环 */
  float    radius;
  int32_t  layer;
  lum_Vec4 color;
  float    core_size, core_decay, edge_decay, halo_intensity;             /* 光晕参数 */
  float    ring_radius, ring_width, ring_intensity;                       /* 光环参数 */
  float    ring_inner_decay, ring_outer_decay;
} C_glow;

typedef struct {
  lum_Vec2 start_offset, end_offset;   /* 相对实体 transform.position */
  float    thickness;
  int32_t  mode;                        /* lum_Beam_mode */
  int32_t  layer;
  lum_Vec4 color;
  float    core_size, core_decay, edge_decay;
} C_beam;

static lum_Component_type C_glow_id;
static lum_Component_type C_beam_id;

static const lum_Component_field_desc g_glow_fields[] = {
  Lum_Component_Make_Field_Bool(C_glow, ring,              "ring",            false),
  Lum_Component_Make_Field_F32 (C_glow, radius,            "radius",          60.0f),
  Lum_Component_Make_Field_I32 (C_glow, layer,             "layer",           0),
  Lum_Component_Make_Field_Vec4(C_glow, color,             "color",           1, 1, 1, 1),
  Lum_Component_Make_Field_F32 (C_glow, core_size,         "core_size",       0.18f),
  Lum_Component_Make_Field_F32 (C_glow, core_decay,        "core_decay",      1.0f),
  Lum_Component_Make_Field_F32 (C_glow, edge_decay,        "edge_decay",      4.0f),
  Lum_Component_Make_Field_F32 (C_glow, halo_intensity,    "halo_intensity",  0.35f),
  Lum_Component_Make_Field_F32 (C_glow, ring_radius,       "ring_radius",     0.6f),
  Lum_Component_Make_Field_F32 (C_glow, ring_width,        "ring_width",      0.12f),
  Lum_Component_Make_Field_F32 (C_glow, ring_intensity,    "ring_intensity",  1.0f),
  Lum_Component_Make_Field_F32 (C_glow, ring_inner_decay,  "ring_inner_decay",1.0f),
  Lum_Component_Make_Field_F32 (C_glow, ring_outer_decay,  "ring_outer_decay",1.0f),
};

static const lum_Component_field_desc g_beam_fields[] = {
  Lum_Component_Make_Field_Vec2(C_beam, start_offset, "start",     0, 0),
  Lum_Component_Make_Field_Vec2(C_beam, end_offset,   "end",       0, 0),
  Lum_Component_Make_Field_F32 (C_beam, thickness,    "thickness", 10.0f),
  Lum_Component_Make_Field_I32 (C_beam, mode,         "mode",      0),
  Lum_Component_Make_Field_I32 (C_beam, layer,        "layer",     0),
  Lum_Component_Make_Field_Vec4(C_beam, color,        "color",     1, 1, 1, 1),
  Lum_Component_Make_Field_F32 (C_beam, core_size,    "core_size", 0.25f),
  Lum_Component_Make_Field_F32 (C_beam, core_decay,   "core_decay",1.0f),
  Lum_Component_Make_Field_F32 (C_beam, edge_decay,   "edge_decay",4.0f),
};

static lum_Vec2 sw(float sx, float sy) {
  return lum_app_screen_to_world(lum_vec2_create(sx, sy));
}
static lum_Vec2 mw(void) {
  return lum_app_screen_to_world(lum_input_get_mouse_position());
}
static void draw_text(const char* t, lum_Vec2 c, lum_Vec4 col) {
  if (!g_font) return;
  lum_tr_draw_layer(g_font, t, lum_app_screen_to_world(c), 2, col);
}

/* ---------- 生成辅助 ---------- */

static lum_Entity spawn_glow(lum_Ecs* ecs, lum_Vec2 position, const C_glow* desc) {
  lum_Entity e = lum_ecs_create_entity(ecs);
  lum_scene_add_transform(ecs, e)->position = position;
  *(C_glow*)lum_ecs_add(ecs, e, C_glow_id) = *desc;
  return e;
}

static lum_Entity spawn_beam(lum_Ecs* ecs, lum_Vec2 center, const C_beam* desc) {
  lum_Entity e = lum_ecs_create_entity(ecs);
  lum_scene_add_transform(ecs, e)->position = center;
  *(C_beam*)lum_ecs_add(ecs, e, C_beam_id) = *desc;
  return e;
}

/* ---------- 动画脚本 ---------- */

static lum_Entity g_cursor;   /* 鼠标特效的父实体：每帧对齐光标 */

static void cursor_follow(lum_Ecs* ecs, lum_Entity self, float dt) {
  (void)dt;
  lum_Component_transform* t = (lum_Component_transform*)lum_ecs_get(ecs, self, lum_Component_transform_id);
  t->position = mw();
}

static lum_Entity g_orbit_a, g_orbit_b;   /* 两颗环绕光晕 */

static void orbit_a_update(lum_Ecs* ecs, lum_Entity self, float dt) {
  (void)dt;
  lum_Component_transform* t = (lum_Component_transform*)lum_ecs_get(ecs, self, lum_Component_transform_id);
  float now = lum_time_get_total();
  t->position = sw(480.0f + cosf(now * 0.8f) * 70.0f, 300.0f + sinf(now * 1.1f) * 50.0f);
}
static void orbit_b_update(lum_Ecs* ecs, lum_Entity self, float dt) {
  (void)dt;
  lum_Component_transform* t = (lum_Component_transform*)lum_ecs_get(ecs, self, lum_Component_transform_id);
  float now = lum_time_get_total();
  t->position = sw(720.0f + sinf(now * 1.2f) * 55.0f, 300.0f + cosf(now * 0.9f) * 45.0f);
}

/* ---------- 渲染系统：光晕/光环、光束 ---------- */

/* 世界矩阵平移分量（列主序：m[12], m[13]） */
static lum_Vec2 world_translation(const lum_Component_transform* t) {
  return lum_vec2_create(t->world.m[12], t->world.m[13]);
}
/* 世界矩阵变换本地偏移：父级的旋转/缩放会正确带动子实体光束端点 */
static lum_Vec2 world_offset(const lum_Component_transform* t, lum_Vec2 local_offset) {
  lum_Vec4 p = lum_mat4_mul_vec4(t->world, lum_vec4_create(local_offset.x, local_offset.y, 0.0f, 1.0f));
  return lum_vec2_create(p.x, p.y);
}

static void glow_render_system(lum_Ecs* ecs, void* user_data) {
  (void)user_data;
  lum_renderer2d_set_blend_mode(Lum_Blend_Additive);
  lum_Ecs_view v = lum_ecs_view_create2(ecs, lum_Component_transform_id, C_glow_id);
  lum_Component_transform* t; C_glow* glow; lum_Entity e;
  while ((e = lum_ecs_view_each2(&v, (void**)&t, (void**)&glow)) != Lum_Entity_Null) {
    lum_Vec2 center = world_translation(t);   /* 世界位置：跟随父级（如鼠标光标） */
    if (glow->ring) {
      lum_glow_draw_ring_advanced(center, glow->radius, glow->layer, glow->color,
                                  glow->ring_radius, glow->ring_width, glow->ring_intensity,
                                  glow->ring_inner_decay, glow->ring_outer_decay);
    } else {
      lum_glow_draw_advanced(center, glow->radius, glow->layer, glow->color,
                             glow->core_size, glow->core_decay, glow->edge_decay,
                             glow->halo_intensity);
    }
  }
}

static void beam_render_system(lum_Ecs* ecs, void* user_data) {
  (void)user_data;
  lum_renderer2d_set_blend_mode(Lum_Blend_Additive);
  lum_Ecs_view v = lum_ecs_view_create2(ecs, lum_Component_transform_id, C_beam_id);
  lum_Component_transform* t; C_beam* beam; lum_Entity e;
  while ((e = lum_ecs_view_each2(&v, (void**)&t, (void**)&beam)) != Lum_Entity_Null) {
    /* 端点偏移过世界矩阵：跟随实体位置与父级层级变换 */
    lum_beam_draw_advanced(world_offset(t, beam->start_offset),
                           world_offset(t, beam->end_offset),
                           beam->thickness, (lum_Beam_mode)beam->mode, beam->layer,
                           beam->color, beam->core_size, beam->core_decay, beam->edge_decay);
  }
}

/* ---------- 既有 ECS 冒烟保留：ship/moon 层级 ---------- */
static lum_Entity g_ship, g_moon;

static void ship_update(lum_Ecs* ecs, lum_Entity self, float dt) {
  (void)dt;
  lum_Component_transform* t = (lum_Component_transform*)lum_ecs_get(ecs, self, lum_Component_transform_id);
  float now = lum_time_get_total();
  t->position = sw(480.0f + cosf(now * 0.5f) * 160.0f, 160.0f + sinf(now * 0.8f) * 40.0f);
  t->rotation = now * 0.8f;
}

/* update 系统注册表冒烟：script 之后、世界矩阵之前执行——本帧改 size 本帧可见 */
static void moon_pulse_system(lum_Ecs* ecs, float dt, void* user_data) {
  (void)dt; (void)user_data;
  lum_Component_sprite* sp = (lum_Component_sprite*)lum_ecs_get(ecs, g_moon, lum_Component_sprite_id);
  if (!sp) return;
  float pulse = 16.0f + sinf(lum_time_get_total() * 3.0f) * 5.0f;
  sp->size = lum_vec2_create(pulse, pulse);
}

/* 渲染系统注册表冒烟：sprite 内置系统之后按注册序执行 */
static void hud_render_system(lum_Ecs* ecs, void* user_data) {
  (void)ecs; (void)user_data;
  lum_renderer2d_set_blend_mode(Lum_Blend_Alpha);
  lum_renderer2d_draw_rect(sw(20, 160), lum_vec2_create(140, 18), 0.0f, lum_vec2_0(),
                           3, lum_vec4_create(0.2f, 0.6f, 1.0f, 0.35f));
}

static void on_start(void) {
  lum_renderer2d_set_clear_color(lum_vec4_create(0.03f, 0.03f, 0.05f, 1.0f));
  g_font = lum_assets_font("resources/FiraCode-Regular.ttf", 18);
  lum_tr_preload_ascii(g_font);   /* 渲染前一次性光栅化全部 ASCII 字形 */
  g_ring = lum_assets_texture("resources/ring.png", Lum_Texture_Flag_None);

  if (lum_assets_texture("resources/ring.png", Lum_Texture_Flag_None) == g_ring) {
    Lum_Log_Info("assets: dedup ok, %u assets loaded", lum_assets_count());
  }

  lum_Ecs* ecs = lum_app_get_ecs();

  /* demo 组件：注册 + 绑定字段表（F5/F9 存读档会带上特效） */
  C_glow_id = lum_serial_register_component(ecs, "glow", sizeof(C_glow),
                                            g_glow_fields, sizeof(g_glow_fields) / sizeof(g_glow_fields[0]));
  C_beam_id = lum_serial_register_component(ecs, "beam", sizeof(C_beam),
                                            g_beam_fields, sizeof(g_beam_fields) / sizeof(g_beam_fields[0]));

  lum_scene_add_render_system(ecs, glow_render_system, NULL);
  lum_scene_add_render_system(ecs, beam_render_system, NULL);
  lum_scene_add_render_system(ecs, hud_render_system, NULL);
  lum_scene_add_update_system(ecs, moon_pulse_system, NULL);

  /* --- ship/moon 层级冒烟 --- */
  g_ship = lum_ecs_create_entity(ecs);
  lum_scene_add_transform(ecs, g_ship);
  lum_Component_sprite* ship_sprite = lum_scene_add_sprite(ecs, g_ship, g_ring);
  ship_sprite->size = lum_vec2_create(48, 48);
  ship_sprite->layer = 1;
  lum_scene_add_script(ecs, g_ship, NULL, ship_update, NULL);

  g_moon = lum_ecs_create_entity(ecs);
  lum_scene_add_transform(ecs, g_moon)->position = lum_vec2_create(70, 0);   /* 父级 scale=1，70 即 70 世界单位 */
  lum_Component_sprite* moon_sprite = lum_scene_add_sprite(ecs, g_moon, &g_ring ? g_ring : NULL);
  moon_sprite->size = lum_vec2_create(16, 16);
  moon_sprite->color = lum_vec4_create(0.4f, 0.9f, 1.0f, 1.0f);
  lum_scene_set_parent(ecs, g_moon, g_ship);

  /* --- 特效实体化：鼠标特效群（挂在跟随光标的父实体下） --- */
  g_cursor = lum_ecs_create_entity(ecs);
  lum_scene_add_transform(ecs, g_cursor);
  lum_scene_add_script(ecs, g_cursor, NULL, cursor_follow, NULL);

  C_glow glow;
  memset(&glow, 0, sizeof(glow));
  C_beam beam;
  memset(&beam, 0, sizeof(beam));

  /* 鼠标头部光晕 */
  glow.ring = false;  glow.radius = 75.0f;  glow.layer = 0;
  glow.color = lum_vec4_create(1.0f, 0.85f, 0.45f, 1.0f);
  glow.core_size = 0.15f; glow.core_decay = 2.0f; glow.edge_decay = 4.0f; glow.halo_intensity = 0.4f;
  lum_Entity mouse_glow = spawn_glow(ecs, lum_vec2_0(), &glow);
  lum_scene_set_parent(ecs, mouse_glow, g_cursor);

  /* 鼠标光圈（层 5） */
  glow.ring = true;  glow.radius = 40.0f;  glow.layer = 5;
  glow.color = lum_vec4_create(1.0f, 1.0f, 0.7f, 1.0f);
  glow.ring_radius = 0.8f; glow.ring_width = 0.03f; glow.ring_intensity = 0.15f;
  glow.ring_inner_decay = 0.15f; glow.ring_outer_decay = 0.5f;
  lum_Entity mouse_ring = spawn_glow(ecs, lum_vec2_0(), &glow);
  lum_scene_set_parent(ecs, mouse_ring, g_cursor);

  /* 鼠标双光束 */
  beam.thickness = 17.0f; beam.mode = Lum_Beam_Mode_Center; beam.layer = 0;
  beam.color = lum_vec4_create(1.0f, 1.0f, 0.6f, 1.0f);
  beam.core_size = 0.25f; beam.core_decay = 1.0f; beam.edge_decay = 4.0f;
  beam.start_offset = lum_vec2_create(-60.0f, -50.0f);
  beam.end_offset   = lum_vec2_create( 60.0f,  50.0f);
  lum_Entity mouse_beam_a = spawn_beam(ecs, lum_vec2_0(), &beam);
  lum_scene_set_parent(ecs, mouse_beam_a, g_cursor);
  beam.start_offset = lum_vec2_create(-60.0f, 50.0f);
  beam.end_offset   = lum_vec2_create( 60.0f, -50.0f);
  lum_Entity mouse_beam_b = spawn_beam(ecs, lum_vec2_0(), &beam);
  lum_scene_set_parent(ecs, mouse_beam_b, g_cursor);

  /* --- 环绕光晕两颗（script 驱动轨道） --- */
  glow.ring = false;  glow.layer = 0;
  glow.color = lum_vec4_create(0.25f, 1.0f, 0.4f, 1.0f);
  glow.radius = 80.0f; glow.core_size = 0.30f; glow.core_decay = 0.7f;
  glow.edge_decay = Lum_Glow_Default_Edge_Decay; glow.halo_intensity = Lum_Glow_Default_Halo_Intensity;
  g_orbit_a = spawn_glow(ecs, lum_vec2_0(), &glow);
  lum_scene_add_script(ecs, g_orbit_a, NULL, orbit_a_update, NULL);

  glow.color = lum_vec4_create(0.3f, 0.5f, 1.0f, 1.0f);
  glow.radius = 85.0f; glow.core_size = 0.08f; glow.core_decay = 1.2f; glow.edge_decay = 8.0f;
  g_orbit_b = spawn_glow(ecs, lum_vec2_0(), &glow);
  lum_scene_add_script(ecs, g_orbit_b, NULL, orbit_b_update, NULL);

  /* --- 静态特效：白色光晕 + 双光环 --- */
  glow.ring = false;  glow.radius = 60.0f;
  glow.color = lum_vec4_create(0.9f, 0.9f, 0.9f, 1.0f);
  glow.core_size = 0.18f; glow.core_decay = 1.0f; glow.edge_decay = 4.0f; glow.halo_intensity = 0.35f;
  spawn_glow(ecs, sw(480, 300), &glow);

  glow.ring = true;  glow.radius = 70.0f;
  glow.color = lum_vec4_create(1.0f, 0.65f, 0.2f, 1.0f);
  glow.ring_radius = Lum_Glow_Default_Ring_Radius; glow.ring_width = Lum_Glow_Default_Ring_Width;
  glow.ring_intensity = Lum_Glow_Default_Ring_Intensity;
  glow.ring_inner_decay = Lum_Glow_Default_Ring_Inner_Decay;
  glow.ring_outer_decay = Lum_Glow_Default_Ring_Outer_Decay;
  spawn_glow(ecs, sw(240, 200), &glow);

  glow.radius = 65.0f;
  glow.color = lum_vec4_create(0.4f, 0.85f, 1.0f, 1.0f);
  glow.ring_radius = 0.70f; glow.ring_width = 0.06f; glow.ring_intensity = 1.6f;
  glow.ring_inner_decay = 3.0f; glow.ring_outer_decay = 0.6f;
  spawn_glow(ecs, sw(240, 400), &glow);

  /* --- 静态光束三条（三种衰减模式各一条） --- */
  beam.layer = 0;
  beam.thickness = 24.0f; beam.mode = Lum_Beam_Mode_Lateral;
  beam.color = lum_vec4_create(1.0f, 0.6f, 0.2f, 1.0f);
  beam.start_offset = lum_vec2_create(-360.0f, 0.0f);
  beam.end_offset   = lum_vec2_create( 360.0f, 0.0f);
  spawn_beam(ecs, sw(480, 500), &beam);

  beam.thickness = 14.0f; beam.mode = Lum_Beam_Mode_Longitudinal;
  beam.color = lum_vec4_create(0.3f, 0.6f, 1.0f, 1.0f);
  spawn_beam(ecs, sw(480, 545), &beam);

  beam.thickness = 32.0f; beam.mode = Lum_Beam_Mode_Center;
  beam.color = lum_vec4_create(0.6f, 1.0f, 0.6f, 1.0f);
  spawn_beam(ecs, sw(480, 576), &beam);
}

static void on_update(float dt) {
  (void)dt;
  if (lum_input_is_key_pressed(Lum_Key_Q)) lum_app_quit();
  /* 场景序列化：F5 存档 / F9 读档——特效实体随 C_glow/C_beam 字段表一起持久化 */
  if (lum_input_is_key_pressed(Lum_Key_F5)) {
    if (lum_scene_save(lum_app_get_ecs(), "scene_demo.lum"))
      printf("[demo] scene saved.\n");
  }
  if (lum_input_is_key_pressed(Lum_Key_F9)) {
    if (lum_scene_load(lum_app_get_ecs(), "scene_demo.lum"))
      printf("[demo] scene loaded.\n");
  }
}

static void on_render(void) {
  lum_Vec4 white = {1,1,1,1};
  lum_Vec4 gray  = {0.6f,0.6f,0.6f,1};

  draw_text("glow demo (procedural, additive, ecs entities)  [Q] quit  [F5] save  [F9] load",
            lum_vec2_create(20, 38), white);
  draw_text("additive: overlapping glows sum -> brighter", lum_vec2_create(20, 70), gray);
  draw_text("per-glow: core size / core decay / edge decay", lum_vec2_create(20, 92), gray);
  draw_text("rings: hollow-center light ring", lum_vec2_create(20, 114), gray);
  draw_text("ring params: radius / width / intensity / inner & outer decay", lum_vec2_create(20, 136), gray);
}

static void on_shutdown(void) {}

int main(void) {
  lum_App_desc desc;
  lum_app_init_desc(&desc, on_start, on_update, on_render, on_shutdown);
  desc.log_level = Lum_Log_Level_Info;
  desc.title = "Glow Demo";
  desc.width = 960;
  desc.height = 600;
  lum_app_run(&desc);
  return 0;
}
