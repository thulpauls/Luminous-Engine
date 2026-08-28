#include "src/luminous.h"
#include <math.h>

/* glow_demo —— 演示 lum_glow 程序化光效原语（光晕 + 光圈）。
 * 几个移动的发光点，additive 叠加产生柔和光晕；另有中空亮环（光圈）。
 * lum_glow_init 由 app 主循环自动调用。 */

static lum_Font* g_font;
static const lum_Texture2d* g_ring;

typedef struct Component_test {
  int a;
  int b;
} Component_test;

static const lum_Component_field_desc component_test_fields[2] = {
  Lum_Component_Make_Field_I32(Component_test, a, "a", 1),
  Lum_Component_Make_Field_I32(Component_test, b, "b", 2),
};

static void register_component_fields() {
  lum_serial_register_component(lum_app_get_ecs(), "test", sizeof(Component_test), component_test_fields, 2);
}

static void write_components() {
  lum_Entity e = lum_ecs_create_entity(lum_app_get_ecs());
  lum_ecs_add(lum_app_get_ecs(), e, lum_ecs_component_type_from_name(lum_app_get_ecs(), "test"));
  ((Component_test*)lum_ecs_get(lum_app_get_ecs(), e, lum_ecs_component_type_from_name(lum_app_get_ecs(), "test")))->b = 100;
  lum_Archive writer;
  lum_serial_init_writer(&writer);
  lum_serial_w_linef(&writer, "entity %d", 1);
  lum_serial_component_write(lum_app_get_ecs(), lum_ecs_component_type_from_name(lum_app_get_ecs(), "test"), 
      lum_ecs_get(lum_app_get_ecs(), e, lum_ecs_component_type_from_name(lum_app_get_ecs(), "test")), &writer);
  size_t sz = 0;
  const char* out = lum_serial_writer_finish(&writer, &sz);
  lum_fs_write("test.txt", out);
}

/* --- ECS 冒烟：父子层级 + 世界矩阵合成 --- */
static lum_Entity g_ship, g_moon;

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

static void ship_update(lum_Ecs* ecs, lum_Entity self, float dt) {
  (void)dt;
  lum_Component_transform* t = (lum_Component_transform*)lum_ecs_get(ecs, self, lum_Component_transform_id);
  float now = lum_time_get_total();
  t->position = sw(480.0f + cosf(now * 0.5f) * 160.0f, 160.0f + sinf(now * 0.8f) * 40.0f);
  t->rotation = now * 0.8f;
}

/* 渲染系统注册表冒烟：sprite 内置系统之后按注册序执行 */
static void hud_render_system(lum_Ecs* ecs, void* user_data) {
  (void)ecs; (void)user_data;
  lum_renderer2d_draw_rect(sw(20, 160), lum_vec2_create(140, 18), 0.0f, lum_vec2_0(),
                           3, lum_vec4_create(0.2f, 0.6f, 1.0f, 0.35f));
}

/* update 系统注册表冒烟：script 之后、世界矩阵之前执行——本帧改 size 本帧可见 */
static void moon_pulse_system(lum_Ecs* ecs, float dt, void* user_data) {
  (void)dt; (void)user_data;
  lum_Component_sprite* sp = (lum_Component_sprite*)lum_ecs_get(ecs, g_moon, lum_Component_sprite_id);
  if (!sp) return;
  float pulse = 16.0f + sinf(lum_time_get_total() * 3.0f) * 5.0f;
  sp->size = lum_vec2_create(pulse, pulse);
}

static void on_start(void) {
  register_component_fields();
  write_components();

  lum_renderer2d_set_clear_color(lum_vec4_create(0.03f, 0.03f, 0.05f, 1.0f));
  g_font = lum_assets_font("resources/FiraCode-Regular.ttf", 18);
  g_ring = lum_assets_texture("resources/ring.png", Lum_Texture_Flag_None);

  /* 去重自检：同请求二次调用应命中同一份资源 */
  if (lum_assets_texture("resources/ring.png", Lum_Texture_Flag_None) == g_ring) {
    Lum_Log_Info("assets: dedup ok, %u assets loaded", lum_assets_count());
  }

  lum_Ecs* ecs = lum_app_get_ecs();
  lum_scene_add_render_system(ecs, hud_render_system, NULL);
  lum_scene_add_update_system(ecs, moon_pulse_system, NULL);

  g_ship = lum_ecs_create_entity(ecs);
  lum_scene_add_transform(ecs, g_ship);
  lum_Component_sprite* ship_sprite = lum_scene_add_sprite(ecs, g_ship, g_ring);
  ship_sprite->size = lum_vec2_create(48, 48);
  ship_sprite->layer = 1;
  lum_scene_add_script(ecs, g_ship, NULL, ship_update, NULL);

  g_moon = lum_ecs_create_entity(ecs);
  lum_scene_add_transform(ecs, g_moon)->position = lum_vec2_create(70, 0);   /* 父级 scale=1，70 即 70 世界单位 */
  lum_Component_sprite* moon_sprite = lum_scene_add_sprite(ecs, g_moon, g_ring);
  moon_sprite->size = lum_vec2_create(16, 16);
  moon_sprite->color = lum_vec4_create(0.4f, 0.9f, 1.0f, 1.0f);
  lum_scene_set_parent(ecs, g_moon, g_ship);
}
static void on_update(float dt) {
  (void)dt;
  if (lum_input_is_key_pressed(Lum_Key_Q)) lum_app_quit();
  /* 场景序列化：F5 存档 / F9 读档（读档会清空并恢复，ship/moon 回到存档瞬间状态） */
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

  draw_text("glow demo (procedural, additive)  [Q] quit", lum_vec2_create(20,38), white);

  float t = lum_time_get_total();

  /* additive blend 下画光晕：颜色直接加到背景，越叠越亮 */
  lum_renderer2d_set_blend_mode(Lum_Blend_Additive);

  /* 鼠标头部光晕：保持原参数不变（半径 90，全亮度） */
  lum_glow_draw_advanced(mw(), 75.0f, 0, lum_vec4_create(1.0f, 0.85f, 0.45f, 1.0f),
      0.15f, 2.0f, 4.0f, 0.4f);

  lum_glow_draw_advanced(sw(480 + cosf(t*0.8f)*70, 300 + sinf(t*1.1f)*50), 80, 0,
                   lum_vec4_create(0.25f, 1.0f, 0.4f, 1.0f), 0.30f, 0.7f, Lum_Glow_Default_Edge_Decay, Lum_Glow_Default_Halo_Intensity);   /* 更大更柔的核 */
  lum_glow_draw_advanced(sw(720 + sinf(t*1.2f)*55, 300 + cosf(t*0.9f)*45), 85, 0,
                   lum_vec4_create(0.3f, 0.5f, 1.0f, 1.0f), 0.08f, 1.2f, 8.0f, Lum_Glow_Default_Halo_Intensity);          /* 更尖的核 + 更宽的外晕 */
  /* 重叠处更亮（additive 累加）—— 拖动 t 让它们交错 */
  lum_glow_draw(sw(480, 300), 60, 0, lum_vec4_create(0.9f, 0.9f, 0.9f, 1.0f));

  /* 光圈（光环）：中空亮环，可与光晕叠加 */
  lum_glow_draw_ring(sw(240, 200), 70, 0, lum_vec4_create(1.0f, 0.65f, 0.2f, 1.0f));       /* 默认环 */
  lum_glow_draw_ring_advanced(sw(240, 400), 65, 0,
      lum_vec4_create(0.4f, 0.85f, 1.0f, 1.0f), 0.70f, 0.06f, 1.6f, 3.0f, 0.6f);        /* 硬内缘 + 软外缘 */

  lum_glow_draw_ring_advanced(mw(), 40, 5, lum_vec4_create(1, 1, 0.7, 1), 0.8, 0.03, 0.15, 0.15f, 0.5f);
  lum_beam_draw(lum_vec2_sub(mw(), lum_vec2_create(60, 50)), lum_vec2_add(mw(), lum_vec2_create(60, 50)), 17, Lum_Beam_Mode_Center, 0, lum_vec4_create(1, 1.0, 0.6, 1.0f));
  lum_beam_draw(lum_vec2_sub(mw(), lum_vec2_create(60, -50)), lum_vec2_add(mw(), lum_vec2_create(60, -50)), 17, Lum_Beam_Mode_Center, 0, lum_vec4_create(1, 1.0f, 0.6f, 1.0f));
  /* 横向模式：亮度从中心线向两侧衰减（光带，沿长度均匀） */
  lum_beam_draw(sw(120, 500), sw(840, 500), 24, Lum_Beam_Mode_Lateral, 0,
                lum_vec4_create(1.0f, 0.6f, 0.2f, 1.0f));

  /* 纵向模式：亮度从中心点向两端衰减（光柱，中心最亮） */
  lum_beam_draw(sw(120, 545), sw(840, 545), 14, Lum_Beam_Mode_Longitudinal, 0,
                lum_vec4_create(0.3f, 0.6f, 1.0f, 1.0f));

  /* 中心模式：亮度从中心点同时向宽度和长度方向衰减 */
  lum_beam_draw(sw(120, 576), sw(840, 576), 32, Lum_Beam_Mode_Center, 0,
                lum_vec4_create(0.6f, 1.0f, 0.6f, 1.0f));
  lum_renderer2d_set_blend_mode(Lum_Blend_Alpha);

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
