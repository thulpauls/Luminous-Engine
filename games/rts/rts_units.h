#ifndef Rts_Units_H
#define Rts_Units_H

#include <stdint.h>
#include "luminous.h"
#include "rts_path.h"

/*
* 游戏侧单位组件。
*
* 三处设计都是被引擎约束逼出来的，不是随便写的：
*   - selected 是 1 字节字段，而不是"零尺寸标签组件" ——
*     lum_ecs_register_component 会拒绝 size == 0
*   - 单位只挂 3 个组件（transform / sprite / unit）——
*     lum_Ecs_view 最多同时匹配 3 个组件（pools[3]）
*   - 所以【移动指令也塞在这个组件里】，而不是单开一个 order 组件：
*     再挂一个就变成 4 个组件，任何"遍历全部单位"的代码都取不到完整数据。
*     将来要做命令队列，就扩这里的字段，不要扩组件数
*/
typedef struct rts_Unit {
  uint8_t selected;     /* 0/1，供框选与高亮 */
  uint8_t team;         /* 0 = 玩家，1 = 敌方（M1 只用于配色） */
  uint8_t has_move;     /* 1 = 有一条移动指令待执行（M2） */
  uint8_t path_index;   /* 正在走向 path.points 的第几个路点（M3） */
  float   speed;        /* 移动速度（世界单位/秒） */
  lum_Vec2 move_target; /* 指令的最终目标；M3 起已被夹到可通行格中心 */
  rts_Path path;        /* A* + 拉直得到的路点。nav 传 NULL 时就只有一个"直线"路点 */
} rts_Unit;

/* 注册单位组件。幂等：重复调用只注册一次（组件名 "rts_unit"，也是持久化键） */
void rts_units_register(lum_Ecs* ecs);
lum_Component_type rts_units_type(void);

/* 建一个单位 = transform + sprite + unit。返回句柄；失败返回 Lum_Entity_Null */
lum_Entity rts_units_spawn(lum_Ecs* ecs, lum_Vec2 position, float side,
                           lum_Vec4 color, uint8_t team, const lum_Texture2d* texture);

uint32_t rts_units_count(lum_Ecs* ecs);

#endif //Rts_Units_H
