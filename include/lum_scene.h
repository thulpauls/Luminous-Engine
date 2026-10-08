#ifndef Lum_Scene_H
#define Lum_Scene_H

#include "lum_ecs.h"
#include "glimmer/lum_math2d.h"
#include "glimmer/lum_texture.h"
#include "glimmer/lum_camera.h"
#include "glimmer/lum_render_target.h"
#include <stdbool.h>

struct lum_Context;             /* 引擎系统签名需要；完整定义在 lum_application.h（避免循环包含） */
typedef struct lum_Context lum_Context;   /* 同名 typedef：C11 允许与定义处重复（同类型） */

/*
* 场景层：内置组件 + 层级管理 + 视图。
*
* 本层不再拥有管线 —— 阶段状态机在 lum_application 里。场景提供给引擎的只有
* 「内置系统的实现」（lum_scene_register_engine_systems 一次注册到位）：
*
*   Lum_Stage_Pre        ← 实体脚本系统（priority -1000）
*   Lum_Stage_Transform  ← local → world（引擎独占阶段）
*   Lum_Stage_Commit     ← 墓碑回收
*   Lum_Stage_View_Begin ← 绑目标 / 视口 / 区域清屏 / 相机 VP（引擎独占阶段，每视图一次）
*   Lum_Stage_Scene      ← 精灵绘制（priority -1000）
*
* 用户系统用 lum_app_add_system(stage, priority, fn, ud) 注册（见 lum_application.h）。
* 数据契约：Pre 及之前只能写 local；Post 及之后才能读 world。
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

/* sprite 可见性分类位掩码：View 的 category_mask 按位筛选，0 掩码 = 不筛选（全可见）。
   默认属于第 0 类；需要"某个视图只看一部分实体"（小地图/分屏/编辑视图）时给实体打类 */
#define Lum_Sprite_Category_Default (1u << 0)

/* 渲染。uv 默认 (0,0,1,1) 整图；atlas 子区域用 lum_texture2d_pixel_rect_to_uv 预算。
   size 为自身绘制尺寸（本地单位）：不进层级矩阵，不受父级缩放影响，也不影响子实体。
   锚点：sprite 以实体的 world 平移点为中心（即绘制矩形 = 平移 ± size×层级缩放/2），
   与 lum_scene_pick 的命中 AABB 同一套判定。
   layer 是排序键（越小越先画）；category 是可见性分类，与排序无关 */
typedef struct lum_Component_sprite {
  const lum_Texture2d* texture;
  lum_Vec2 size;
  lum_Vec4 uv;
  lum_Vec4 color;
  int32_t  layer;
  uint32_t category;
  bool     flip_x, flip_y;
} lum_Component_sprite;

/* 实体名：大纲树、属性面板标题、调试显示的唯一依据。
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

/* ---------- 生命周期 ---------- */

/* 注册内置组件并让引擎系统可用。world 创建后调用一次；
   对同一 world 幂等（重复调用不会重复注册组件），但会重置引擎系统注册 */
int lum_scene_init(lum_Ecs* ecs);

/* 把场景层的引擎系统注册进阶段状态机。由 lum_application 在流程初始化时调用一次，
   用户不应直接调用（重复注册会造成同一系统跑多次） */
void lum_scene_register_engine_systems(void);

/* ---------- 视图 ---------- */

/* 视口：世界上的一块可视区域。分屏 / 小地图 / 编辑器视口 / 画中画都是同一个概念。
   viewport 单位为渲染目标像素、左下原点；w 或 h <= 0 表示"沿用当前视口"（全屏）。
   target 为 NULL 表示默认帧缓冲。category_mask == 0 表示不筛选实体分类。 */
typedef struct lum_View {
  const char*        name;
  lum_Camera2d       camera;
  int32_t            viewport[4];
  lum_Render_target* target;
  uint32_t           category_mask;
  int                priority;        /* 小的先绘制 */
  bool               clear_enabled;
  lum_Vec4           clear;
} lum_View;

/* 便捷构造：填一套可用默认值（相机 position 0 / zoom 1、viewport 全屏、mask 0 不筛选、
   不清屏、priority 0）。手写 lum_View 时务必至少把 camera 初始化，否则 zoom 为 0 会算出
   退化投影矩阵（引擎对 zoom<=0 有兜底，但那是保险不是用法） */
lum_View lum_view_default(const char* name);

/* add 拷贝 desc（name 只存指针，需长命内存），返回 1 成功 / 0 失败，按 priority 插入排序；
   索引访问用 lum_view_get（索引是"绘制序"，后续 add 会让它漂移）。
   get 返回可写指针，供每帧更新相机（相机跟随等） */
int        lum_view_add(lum_Ecs* ecs, const lum_View* desc);
lum_View*  lum_view_get(lum_Ecs* ecs, int index);
int        lum_view_count(const lum_Ecs* ecs);
void       lum_view_clear_all(lum_Ecs* ecs);

/* ---------- 组件便捷添加 ---------- */

/* 设默认值（transform scale=1；sprite size=(1,1)/全图/白色/layer 0/默认分类） */
lum_Component_transform* lum_scene_add_transform(lum_Ecs* ecs, lum_Entity e);
lum_Component_sprite*    lum_scene_add_sprite(lum_Ecs* ecs, lum_Entity e, const lum_Texture2d* texture);
lum_Component_script*    lum_scene_add_script(lum_Ecs* ecs, lum_Entity e,
                                       lum_entity_start_fn start, lum_entity_update_fn update, void* user_data);

/* 实体名：add 拷贝 name（NULL 视为空名）；set 为 add-or-update；get 无组件返回 NULL */
lum_Component_name* lum_scene_add_name(lum_Ecs* ecs, lum_Entity e, const char* name);
void                lum_scene_set_name(lum_Ecs* ecs, lum_Entity e, const char* name);
const char*         lum_scene_name(const lum_Ecs* ecs, lum_Entity e);

/* ---------- 层级 ---------- */

/* set_parent 自动维护 hierarchy 组件与孩子链，沿祖先链查环。
   parent 传 LUM_ENTITY_NULL 即摘除为根实体 */
void       lum_scene_set_parent(lum_Ecs* ecs, lum_Entity child, lum_Entity parent);
lum_Entity lum_scene_parent(const lum_Ecs* ecs, lum_Entity e);
lum_Entity lum_scene_first_child(const lum_Ecs* ecs, lum_Entity e);
lum_Entity lum_scene_next_sibling(const lum_Ecs* ecs, lum_Entity e);
uint32_t   lum_scene_child_count(const lum_Ecs* ecs, lum_Entity e);

/* 级联销毁整棵子树并解链。层级实体请用本函数而非 lum_ecs_destroy_entity */
void lum_scene_destroy_entity(lum_Ecs* ecs, lum_Entity e);

/* ---------- 查询 ---------- */

/* 拾取：返回 world_point 落在实体 AABB 内的最上层（sprite layer 最大，平局取后建者）；
   无命中返回 Lum_Entity_Null。只统计 transform+sprite 实体（无 sprite 不可拾取）；
   AABB = world 平移 ± sprite.size × 层级缩放（取 world 矩阵列向量长度，忽略旋转）。
   world 矩阵是派生数据：需先跑过包含 Transform 阶段的步进（管线后调用即安全） */
lum_Entity lum_scene_pick(const lum_Ecs* ecs, lum_Vec2 world_point);

#endif //Lum_Scene_H
