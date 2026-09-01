/* 编辑器场景 IO 热键：F5 存档 / F9 读档。 */
#include "editor/lum_editor_internal.h"
#include "luminous.h"
#include <stdio.h>

void lum_editor_io_update(lum_Editor* ed) {
  (void)ed;
  /* 场景序列化：实体随字段表一起持久化。UI 占用键盘（有焦点）时让路：热键不打进面板交互 */
  if (lum_ui_input_captured()) return;

  if (lum_input_is_key_pressed(Lum_Key_F5)) {
    if (lum_scene_save(lum_app_get_ecs(), "scene_demo.lum"))
      printf("[editor] scene saved.\n");
  }
  if (lum_input_is_key_pressed(Lum_Key_F9)) {
    if (lum_scene_load(lum_app_get_ecs(), "scene_demo.lum"))
      printf("[editor] scene loaded.\n");
  }
}
