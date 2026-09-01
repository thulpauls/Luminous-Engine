/* 编辑器大纲树：实体集合指纹、层级 DFS 行生成、重建。 */
#include "editor/lum_editor_internal.h"
#include "luminous.h"
#include <stdint.h>
#include <stdio.h>

static void on_outliner_pick(lum_Widget* w, void* user) {
  (void)w;
  lum_editor_select(lum_editor_instance(), lum_app_get_ecs(), (lum_Entity)(uintptr_t)user);
}

/* 鼠标特效组不可选的排除逻辑随旧演示删除；行 = 名称（未命名兜底）+ 选中标记 + 层级缩进 */
static void outliner_emit(lum_Editor* ed, lum_Ecs* ecs, lum_Entity e, uint32_t depth) {
  const char* name = lum_scene_name(ecs, e);
  char text[96];
  snprintf(text, sizeof(text), "%*s%s%s", (int)(depth * 2), "",
           e == ed->selected ? "> " : "", name ? name : "(unnamed)");
  lum_Widget* row = lum_ui_button(ed->outliner_rows, text);
  lum_ui_on_click(row, on_outliner_pick, (void*)(uintptr_t)e);

  for (lum_Entity child = lum_scene_first_child(ecs, e);
       child != Lum_Entity_Null; child = lum_scene_next_sibling(ecs, child)) {
    outliner_emit(ed, ecs, child, depth + 1);
  }
}

void lum_editor_outliner_rebuild(lum_Editor* ed, lum_Ecs* ecs) {
  /* 保留模式无销毁：重建 = 隐藏旧行 + 新建（重建仅在实体集合/名字/选中变化时发生） */
  for (uint32_t i = 0; i < ed->outliner_rows->child_count; ++i) {
    lum_ui_set_visible(ed->outliner_rows->children[i], false);
  }
  for (uint32_t i = 0; i < lum_ecs_entity_slot_count(ecs); ++i) {
    lum_Entity e = lum_ecs_entity_at_slot(ecs, i);
    if (e == Lum_Entity_Null) continue;
    if (lum_scene_parent(ecs, e) == Lum_Entity_Null) outliner_emit(ed, ecs, e, 0);
  }
}
