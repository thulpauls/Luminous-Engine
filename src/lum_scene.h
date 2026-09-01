#ifndef Lum_Scene_H
#define Lum_Scene_H

#include "lum_ecs.h"
#include "lum_math2d.h"
#include "lum_texture.h"
#include <stdbool.h>

/*
* 引擎组合层：在 lum_ecs 核心之上注册内置组件，提供层级管理与帧管线。
*
* 帧管线（lum_scene_update 内部顺序，数据流单向：组件是系统间唯一接口）：
*   ① script 系统：新实体跑一次 start → 按 dense 序跑 update
*   ② 用户 update 系统：按注册序执行（lum_scene_add_update_system）
*   ③ transform 系统：自根实体下传重算全部 world 矩阵
*   ④ lum_ecs_flush：回收本帧墓碑（无需手动调）
*
* 契约：改 local（position/rotation/scale）在 script 或 update 系统内（世界矩阵之前）；读 world 在管线后。
* 渲染：lum_scene_render 先跑内置 sprite 系统，再按注册序执行用户渲染系统
* （lum_scene_add_render_system）。跨系统叠放由 renderer2d 的 layer 排序统一。
*/

enum {
  lum_Component_transform_id  = 0,
  lum_Component_hierarchy_id  = 1,
  lum_Component_sprite_id     = 2,
  lum_Component_script_id     = 3,
  lum_Component_name_id       = 4,
  lum_Component_builtin_count = 5
};

/* 位置与变换。local 字段由用户写；world 每帧由管线覆盖，只读。
   scale 为层级缩放倍率（默认 1）：会同时作用于子实体的偏移与尺寸；origin 为缩放/旋转锚点 */
typedef struct lum_Component_transform {
  lum_Vec2 position, scale, origin;
  float    rotation;
  lum_Mat4 world;
} lum_Component_transform;

typedef struct lum_Component_hierarchy {
  lum_Entity parent;
  lum_Entity first_child, next_sibling, prev_sibling;
  uint32_t   child_count;
} lum_Component_hierarchy;

/* 渲染。uv 默认 (0,0,1,1) 整图；atlas 子区域用 lum_texture2d_pixel_rect_to_uv 预算。
   size 为自身绘制尺寸（本地单位）：不进层级矩阵，不受父级缩放影响，也不影响子实体 */
typedef struct lum_Component_sprite {
  const lum_Texture2d* texture;
  lum_Vec2 size;
  lum_Vec4 uv;
  lum_Vec4 color;
  int32_t  layer;
  bool     flip_x, flip_y;
} lum_Component_sprite;

/* 实体名：编辑器大纲树、属性面板标题、调试显示的唯一依据。
   定长缓冲，set 超长截断；序列化随场景存档往返 */
#define Lum_Scene_Name_Max 64
typedef struct lum_Component_name {
  char text[Lum_Scene_Name_Max];
} lum_Component_name;

typedef void (*lum_entity_start_fn) (lum_Ecs* ecs, lum_Entity e);
typedef void (*lum_entity_update_fn)(lum_Ecs* ecs, lum_Entity e, float dt);

/* 脚本名注册表：名字 → 回调对。序列化按名恢复 script（函数指针不可落盘）；
   user_data 不参与序列化（恢复后为 NULL），需状态的脚本自行按实体重建 */
void lum_scene_register_script(const char* name, lum_entity_start_fn start, lum_entity_update_fn update);
const char* lum_scene_script_name(lum_entity_start_fn start, lum_entity_update_fn update);
bool lum_scene_script_lookup(const char* name, lum_entity_start_fn* out_start, lum_entity_update_fn* out_update);

/* 行为挂在个别实体上（群体行为写系统更合适）。user_data 放长命内存，勿放组件指针 */
typedef struct lum_Component_script {
  lum_entity_start_fn  start;
  lum_entity_update_fn update;
  void* user_data;
  bool  started;
} lum_Component_script;

int lum_scene_init(lum_Ecs* ecs);   /* world 创建后调用一次 */

/* 用户渲染系统：一个系统处理一类实体的绘制（如全部文字、全部粒子）。
   lum_scene_render 内置于 sprite 系统之后按注册序执行；系统内自行 view + 提交 renderer2d */
typedef void (*lum_render_system_fn)(lum_Ecs* ecs, void* user_data);
int lum_scene_add_render_system(lum_Ecs* ecs, lum_render_system_fn fn, void* user_data);

/* 用户 update 系统：一个系统处理一类实体的逻辑（如全部速度积分、全部碰撞解算）。
   在 script 回调之后、世界矩阵计算之前按注册序执行——script 本帧写的状态本帧被推进 */
typedef void (*lum_update_system_fn)(lum_Ecs* ecs, float dt, void* user_data);
int lum_scene_add_update_system(lum_Ecs* ecs, lum_update_system_fn fn, void* user_data);

/* 便捷添加：设默认值（transform scale=1；sprite size=(1,1)/全图/白色/layer 0） */
lum_Component_transform* lum_scene_add_transform(lum_Ecs* ecs, lum_Entity e);
lum_Component_sprite*    lum_scene_add_sprite(lum_Ecs* ecs, lum_Entity e, const lum_Texture2d* texture);
lum_Component_script*    lum_scene_add_script(lum_Ecs* ecs, lum_Entity e,
                                       lum_entity_start_fn start, lum_entity_update_fn update, void* user_data);

/* 实体名：add 拷贝 name（NULL 视为空名）；set 为 add-or-update；get 无组件返回 NULL */
lum_Component_name* lum_scene_add_name(lum_Ecs* ecs, lum_Entity e, const char* name);
void                lum_scene_set_name(lum_Ecs* ecs, lum_Entity e, const char* name);
const char*         lum_scene_name(const lum_Ecs* ecs, lum_Entity e);

/* 层级：set_parent 自动维护 hierarchy 组件与孩子链，沿祖先链查环。
   parent 传 LUM_ENTITY_NULL 即摘除为根实体 */
void       lum_scene_set_parent(lum_Ecs* ecs, lum_Entity child, lum_Entity parent);
lum_Entity lum_scene_parent(const lum_Ecs* ecs, lum_Entity e);
lum_Entity lum_scene_first_child(const lum_Ecs* ecs, lum_Entity e);
lum_Entity lum_scene_next_sibling(const lum_Ecs* ecs, lum_Entity e);
uint32_t   lum_scene_child_count(const lum_Ecs* ecs, lum_Entity e);

/* 级联销毁整棵子树并解链。层级实体请用本函数而非 lum_ecs_destroy_entity */
void lum_scene_destroy_entity(lum_Ecs* ecs, lum_Entity e);

/* 拾取查询：返回 world_point 落在实体 AABB 内的最上层（sprite layer 最大，平局取后建者）；
   无命中返回 Lum_Entity_Null。只统计 transform+sprite 实体（无 sprite 不可拾取）；
   AABB = world 平移 ± sprite.size × 层级缩放（取 world 矩阵列向量长度，忽略旋转）。
   world 矩阵是派生数据：需先跑过 lum_scene_update（编辑器在帧管线后调用即安全） */
lum_Entity lum_scene_pick(const lum_Ecs* ecs, lum_Vec2 world_point);

void lum_scene_update(lum_Ecs* ecs, float dt);
void lum_scene_render(lum_Ecs* ecs);

#endif //Lum_Scene_H
