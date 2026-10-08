#ifndef Rts_Collision_H
#define Rts_Collision_H

#include <stdint.h>
#include "luminous.h"
#include "rts_navgrid.h"

/*
* 单位互斥（M3 后半）：把互相重叠的单位推开。
*
* 自己只做"调度"，几何全交给引擎的碰撞模块：
*   - 每个单位一个【方形碰撞箱】，边长取 sprite.size（和精灵同尺寸），
*     于是"两个箱子刚好接触"就等价于"两个精灵刚好贴边"。
*   - 判交与最小平移量（MTV）直接用 lum_collide_box_box（内部是多边形 SAT），
*     拿到的 lum_Contact 里有 normal（A→B）和 depth（整段平移量）。
*   - 两两之间先做一次便宜的宽相排除（外接圆），只有真可能相交才进 SAT。
*
* 放置阶段：它写 transform.position（local），所以必须挂在 Pre 或更早 ——
* 而且必须排在"沿路点推进"之后（推进会把单位挤到一起，这里负责推开）。
*/

/* 每步迭代几轮。一轮只能解决"两两"，挤成一团时需要多轮把连锁传开。 */
#define Rts_Collision_Iterations 4

/* 单轮单体位移上限。密集时累积位移可能很大，一次弹开看着像"炸了"。 */
#define Rts_Collision_Max_Push   8.0f

/* 穿透小于它就当作"已经分开"，不推。
   这一条不是为了省算力，而是为了让系统有【真正的定点】：
   浮点上的推离永远差最后一点点，没有死区就会每帧微推、永远抖。 */
#define Rts_Collision_Slop       0.05f

/*
* 解开所有单位的互相重叠。
*
* nav 可以传 NULL。传了就额外做一件事：不把单位推进导航网格里被判为不可通行的格子
* （整段推不进去就退化成只推一个轴，让贴墙的单位沿墙滑，而不是嵌进墙里）。
*
* 返回累计【应用了多少次位移】（同一个单位在多轮/多帧里会被重复计入），
* 所以它只适合用来判断"这一帧有没有动过"，不是"动了几个单位"。
*/
uint32_t rts_collision_resolve(lum_Ecs* ecs, const rts_Navgrid* nav);

#endif //Rts_Collision_H
