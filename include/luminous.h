#ifndef Luminous_H
#define Luminous_H

/*
* Luminous 伞头 = glimmer 基础平台 + 引擎高层模块。
* 分层与边界见 docs/architecture.md 与 include/glimmer/glimmer.h。
*/

#include "lum_application.h"

/* --- glimmer：基础平台（数学层 + 平台层 + 渲染器），独立可运行 --- */
#include "glimmer/glimmer.h"

/* --- 引擎高层：资源 / 序列化 / 场景 / 扩展组件 / 特效 / UI --- */
#include "lum_version.h"
#include "lum_collision.h"
#include "containers/lum_vector.h"
#include "containers/lum_list.h"
#include "lum_assets.h"
#include "lum_serial.h"
#include "lum_ecs.h"
#include "lum_scene.h"
#include "lum_scene_io.h"
#include "components/lum_components.h"
#include "effects/lum_effects.h"
#include "lum_ui.h"

#endif //Luminous_H
