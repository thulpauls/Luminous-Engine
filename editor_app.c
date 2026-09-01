/* Luminous 编辑器 shell：应用编排（面板/拾取/相机细节都在 editor/ 模块内）。 */
#include "luminous.h"
#include "editor/lum_editor.h"

static void on_start(void) {
  lum_renderer2d_set_clear_color(lum_vec4_create(0.03f, 0.03f, 0.05f, 1.0f));
  lum_ui_init(lum_assets_font("resources/FiraCode-Regular.ttf", 18));

  lum_Ecs* ecs = lum_app_get_ecs();

  /* 引擎内置扩展组件：注册 + 字段表序列化（编辑器创建的 glow/beam 实体可渲染、可随 F5/F9 存读档） */
  lum_components_register(ecs);
  lum_scene_add_render_system(ecs, lum_component_glow_render_system, NULL);
  lum_scene_add_render_system(ecs, lum_component_beam_render_system, NULL);

  lum_editor_build_ui(lum_editor_instance());
}

static void on_update(float dt) {
  if (lum_input_is_key_pressed(Lum_Key_Q)) lum_app_quit();
  lum_editor_update(lum_editor_instance(), dt);
}

static void on_render(void) {
  lum_editor_render_overlay(lum_editor_instance());

  /* --- 保留式 UI：控件树建于 on_start，此处仅驱动帧（输入/布局/绘制） --- */
  lum_ui_frame();

  /* 场景 pass：绑定区域 VP + 剪裁，其后的 lum_scene_render 只画进视口区域 */
  lum_editor_begin_scene_pass(lum_editor_instance());
}

static void on_shutdown(void) {
  lum_ui_shutdown();
}

int main(void) {
  lum_App_desc desc;
  lum_app_init_desc(&desc, on_start, on_update, on_render, on_shutdown);
  desc.log_level = Lum_Log_Level_Info;
  desc.title = "Luminous Editor";
  desc.width = 960;
  desc.height = 600;
  lum_app_run(&desc);
  return 0;
}
