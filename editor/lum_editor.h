#ifndef Lum_Editor_H
#define Lum_Editor_H

#include "lum_ecs.h"

/*
* 编辑器模块：大纲树 / 属性面板（字段表自动生成）/ 视口拾取与选中 / 实体创建 / 存读档热键。
* 纯引擎消费者——引擎永远不知道本模块的存在。
*
* 用法（shell 侧四步）：
*   on_start   lum_editor_build_ui(lum_editor_instance())   // lum_ui_init 之后
*   on_update  lum_editor_update(lum_editor_instance(), dt) // 相机/拾取/面板重建/统计/热键
*   on_render  lum_editor_render_overlay(...) 后调 lum_ui_frame()
*   退出热键（Q）归 shell 处理
*/

typedef struct lum_Editor lum_Editor;

lum_Editor* lum_editor_instance(void);

void lum_editor_build_ui(lum_Editor* ed);
void lum_editor_update(lum_Editor* ed, float dt);
void lum_editor_render_overlay(lum_Editor* ed);
/* 场景 pass 绑定：on_render 里 lum_ui_frame 之后调用——其后的 lum_scene_render 只画进视口区域 */
void lum_editor_begin_scene_pass(lum_Editor* ed);

#endif //Lum_Editor_H
