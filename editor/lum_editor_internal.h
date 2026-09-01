#ifndef Lum_Editor_Internal_H
#define Lum_Editor_Internal_H

/*
* 编辑器模块内部共享声明：lum_Editor 状态本体 + 跨编译单元的模块内 API。
* 只被 editor/*.c 包含，不对外。
*/

#include "lum_editor.h"
#include "lum_scene.h"
#include "lum_ui.h"

struct lum_Editor {
  /* 选中：视口拾取与大纲行点选共同写入（editor_select）；高亮框绘制读取 */
  lum_Entity selected;
  lum_Vec2   selected_center;   /* 选中包围盒中心偏移（实体本地空间，绘制时过 world 矩阵） */
  lum_Vec2   selected_half;     /* 选中包围盒半宽（世界单位） */

  /* 视口区域：世界只渲染进这块 UI 像素矩形（区域 VP + NDC 自动裁剪），拾取/相机同区域换算 */
  lum_Rect viewport_rect;

  /* 大纲树 */
  lum_Widget* outliner_rows;
  uint32_t    outliner_signature;   /* 实体集合指纹：句柄变化（建/删/回收）触发重建 */
  bool        outliner_dirty;       /* 名字编辑等不影响指纹的变更手动置脏 */

  /* 属性面板 */
  lum_Widget* prop_rows;
  lum_Entity  prop_built_for;       /* 属性行当前对应的实体 */
  bool        prop_dirty;           /* 首帧强制构建 */

  /* 实体创建命名序号 */
  uint32_t create_sequence;

  /* 调试面板 */
  lum_Widget* lbl_fps;
  lum_Widget* lbl_entities;
  lum_Widget* lbl_selected;
  lum_Widget* lbl_debug;
  lum_Widget* prg_load;
  lum_Widget* tbx_callsign;
  struct { float fps; uint32_t draw_calls; uint32_t entities; } stats;
};

/* ---------- viewport（lum_editor_viewport.c） ---------- */

/* 统一选中入口：视口拾取与大纲行点选都走这里（保证高亮框与几何贴合） */
void lum_editor_select(lum_Editor* ed, lum_Ecs* ecs, lum_Entity e);
/* 中键平移 + 滚轮缩放（仅当鼠标在视口区域内且 UI 未占用输入） */
void lum_editor_camera_update(lum_Editor* ed);
/* 左键点选（点空白取消）。UI 占用鼠标时让路（吞噬查询） */
void lum_editor_viewport_pick(lum_Editor* ed, lum_Ecs* ecs);
/* 选中高亮：青色描边框（世界空间 4 细边），层 900 = 世界之上、UI 之下 */
void lum_editor_render_selection_box(lum_Editor* ed);
/* 视口区域边框（UI 像素空间描边） */
void lum_editor_render_region_border(const lum_Rect* region);
/* 区域 VP：区域局部像素 → 全窗口裁剪空间（场景/选中框/区域渲染共用）；
   lum_editor_render_overlay 绑定后，本帧稍后的 lum_scene_render 自动落进区域 */
lum_Mat4 lum_editor_region_view_projection(const lum_Rect* region);

/* ---------- outliner（lum_editor_outliner.c） ---------- */

void lum_editor_outliner_rebuild(lum_Editor* ed, lum_Ecs* ecs);

/* ---------- properties（lum_editor_properties.c） ---------- */

void lum_editor_properties_rebuild(lum_Editor* ed, lum_Ecs* ecs, lum_Entity target);

/* ---------- create（lum_editor_create.c） ---------- */

void lum_editor_on_create_glow(lum_Widget* w, void* user);
void lum_editor_on_create_beam(lum_Widget* w, void* user);

/* ---------- io（lum_editor_io.c） ---------- */

/* F5 存档 / F9 读档。UI 占用键盘（有焦点）时让路：热键不打进面板交互 */
void lum_editor_io_update(lum_Editor* ed);

#endif //Lum_Editor_Internal_H
