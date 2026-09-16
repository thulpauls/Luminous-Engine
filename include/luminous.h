#ifndef Luminous_H
#define Luminous_H

#include "lum_application.h"

/* --- 基础层 --- */
#include "lum_version.h"
#include "lum_log.h"
#include "lum_assert.h"
#include "lum_math2d.h"
#include "lum_collision.h"
#include "containers/lum_vector.h"
#include "containers/lum_list.h"

/* --- 平台层 --- */
#include "lum_filesystem.h"
#include "lum_window.h"
#include "lum_input_codes.h"
#include "lum_input.h"
#include "lum_time.h"

/* --- 渲染层 --- */
#include "lum_shader.h"
#include "lum_texture.h"
#include "lum_atlas.h"
#include "lum_camera.h"
#include "lum_transform.h"
#include "lum_renderer2d.h"
#include "lum_material.h"
#include "lum_render_target.h"

/* --- 资源层 --- */
#include "lum_assets.h"

/* --- 序列化层 --- */
#include "lum_serial.h"

/* --- 场景层 --- */
#include "lum_ecs.h"
#include "lum_scene.h"
#include "lum_scene_io.h"

/* --- 内置扩展组件 --- */
#include "components/lum_components.h"

/* --- 特效 --- */
#include "effects/lum_effects.h"

/* --- 子系统 --- */
#include "lum_text_renderer.h"
#include "lum_ui.h"

#endif //Luminous_H