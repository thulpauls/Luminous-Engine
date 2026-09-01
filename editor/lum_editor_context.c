/* editor_testbed —— 编辑器模块：状态单例、面板构建、每帧编排。 */
#include "editor/lum_editor_internal.h"
#include "luminous.h"
#include <stdio.h>

static lum_Editor g_editor;

lum_Editor* lum_editor_instance(void) {
  return &g_editor;
}

static void on_quit_clicked(lum_Widget* w, void* user) {
  (void)w; (void)user;
  lum_app_quit();
}

void lum_editor_build_ui(lum_Editor* ed) {
  ed->selected = Lum_Entity_Null;
  ed->prop_built_for = Lum_Entity_Null;
  ed->viewport_rect = (lum_Rect){ {600, 320}, {350, 270} };   /* 视口区域：调试面板下方 */

  /* 视口标题（画在区域左上、UI 层） */
  lum_Widget* viewport_title = lum_ui_label(lum_ui_root(), "viewport");
  lum_ui_set_position(viewport_title, lum_vec2_create(604, 324));
  ed->prop_dirty = true;   /* 首帧构建 "no selection"（原实现靠 0==Null 巧合跳过，此处兑现注释意图） */

  /* --- 大纲树面板：实体行可重建区（滚动容器）+ 创建按钮 --- */
  lum_Widget* outliner = lum_ui_box(lum_ui_root(), false);
  lum_ui_set_position(outliner, lum_vec2_create(10, 60));
  lum_ui_set_fixed_size(outliner, lum_vec2_create(190, 340));
  lum_ui_label(outliner, "outliner");
  ed->outliner_rows = lum_ui_scroll(outliner);
  lum_ui_set_fixed_size(ed->outliner_rows, lum_vec2_create(0, 272));   /* 340 − 标签/创建行/padding */
  lum_Widget* create_row = lum_ui_box(outliner, true);
  {
    lum_Widget* add_glow = lum_ui_button(create_row, "+ glow");
    lum_ui_on_click(add_glow, lum_editor_on_create_glow, NULL);
    lum_Widget* add_beam = lum_ui_button(create_row, "+ beam");
    lum_ui_on_click(add_beam, lum_editor_on_create_beam, NULL);
  }

  /* --- 属性面板：字段表自动生成编辑器（行区滚动：glow 全字段一屏可达） --- */
  lum_Widget* properties = lum_ui_box(lum_ui_root(), false);
  lum_ui_set_position(properties, lum_vec2_create(210, 60));
  lum_ui_set_fixed_size(properties, lum_vec2_create(380, 460));
  lum_ui_label(properties, "properties");
  ed->prop_rows = lum_ui_scroll(properties);
  lum_ui_set_fixed_size(ed->prop_rows, lum_vec2_create(0, 422));   /* 460 − 标签行/padding */

  /* --- 调试面板：统计/选中/进度/文本框/退出 --- */
  lum_Widget* panel = lum_ui_box(lum_ui_root(), false);
  lum_ui_set_position(panel, lum_vec2_create(600, 60));
  lum_ui_set_fixed_size(panel, lum_vec2_create(350, 250));

  ed->lbl_fps      = lum_ui_label(panel, "fps --        draw calls --");
  ed->lbl_entities = lum_ui_label(panel, "entities --");
  ed->lbl_selected = lum_ui_label(panel, "selected: (none)");
  ed->lbl_debug    = lum_ui_label(panel, "debug --");

  ed->prg_load = lum_ui_progress(panel, 0.0f);
  lum_ui_set_fixed_size(ed->prg_load, lum_vec2_create(0, 10));

  ed->tbx_callsign = lum_ui_textbox(panel, "JIE");

  lum_Widget* quit = lum_ui_button(panel, "quit");
  lum_ui_on_click(quit, on_quit_clicked, NULL);
}

void lum_editor_update(lum_Editor* ed, float dt) {
  (void)dt;
  lum_editor_camera_update(ed);

  lum_Ecs* ecs = lum_app_get_ecs();
  lum_editor_viewport_pick(ed, ecs);
  if (ed->selected != Lum_Entity_Null && !lum_ecs_alive(ecs, ed->selected)) {
    ed->selected = Lum_Entity_Null;   /* 选中实体已销毁（如 F9 读档级联清除） */
  }

  /* 面板重建触发：实体集合指纹变化 / 换选中 / 名字编辑 / 首帧 */
  uint32_t signature = 0;
  for (uint32_t i = 0; i < lum_ecs_entity_slot_count(ecs); ++i) {
    signature = signature * 31u + lum_ecs_entity_at_slot(ecs, i);
  }
  if (ed->outliner_dirty || signature != ed->outliner_signature) {
    lum_editor_outliner_rebuild(ed, ecs);
    ed->outliner_signature = signature;
    ed->outliner_dirty = false;
  }
  lum_Entity prop_target = (ed->selected != Lum_Entity_Null) ? ed->selected : Lum_Entity_Null;
  if (ed->prop_dirty || ed->prop_built_for != prop_target) {
    lum_editor_properties_rebuild(ed, ecs, prop_target);
    ed->prop_built_for = prop_target;
    ed->prop_dirty = false;
  }

  /* 统计刷新：flush 计数在渲染阶段还是 0，此处缓存上一帧数值 */
  ed->stats.draw_calls = lum_renderer2d_get_draw_call_count();
  ed->stats.fps = lum_time_get_fps();
  ed->stats.entities = 0;
  for (uint32_t i = 0; i < lum_ecs_entity_slot_count(ecs); ++i) {
    if (lum_ecs_entity_at_slot(ecs, i) != Lum_Entity_Null) ed->stats.entities++;
  }
  if (ed->lbl_fps) {
    char line[64];
    snprintf(line, sizeof(line), "fps %.1f   draw calls %u", ed->stats.fps, ed->stats.draw_calls);
    lum_ui_set_text(ed->lbl_fps, line);
    snprintf(line, sizeof(line), "entities %u", ed->stats.entities);
    lum_ui_set_text(ed->lbl_entities, line);
    /* 进度条：帧率满帧率归一（240 上限） */
    float load = ed->stats.fps / 240.0f;
    lum_ui_set_value(ed->prg_load, load > 1.0f ? 1.0f : load);
  }
  if (ed->lbl_selected) {
    char line[96];
    if (ed->selected != Lum_Entity_Null) {
      const char* name = lum_scene_name(ecs, ed->selected);
      snprintf(line, sizeof(line), "selected: %s", name ? name : "(unnamed)");
    } else {
      snprintf(line, sizeof(line), "selected: (none)");
    }
    lum_ui_set_text(ed->lbl_selected, line);
  }
  if (ed->lbl_debug) {
    lum_Camera2d* cam = lum_app_get_camera();
    lum_Vec2 mouse = lum_input_get_mouse_position();
    int in_region = lum_ui_point_in_rect(ed->viewport_rect, mouse);
    char line[128];
    snprintf(line, sizeof(line), "cam %.0f,%.0f z%.2f | m %.0f,%.0f %c | win %.0fx%.0f",
             cam->position.x, cam->position.y, cam->zoom,
             mouse.x, mouse.y, in_region ? 'R' : '-',
             (float)lum_window_get_width(), (float)lum_window_get_height());
    lum_ui_set_text(ed->lbl_debug, line);
  }

  lum_editor_io_update(ed);
}

/* lum_editor_render_overlay 实现在 lum_editor_viewport.c（与区域剪裁同模块） */
