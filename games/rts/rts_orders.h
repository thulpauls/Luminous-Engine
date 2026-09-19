#ifndef Rts_Orders_H
#define Rts_Orders_H

#include <stdbool.h>
#include <stdint.h>
#include "luminous.h"
#include "rts_navgrid.h"

/*
* M2：单位移动指令；M3：网格 A* 寻路。
*
* 这一层的定位是【将来 AI 唯一的输出口】—— AI 能做的事就是下命令，和玩家走
* 同一条路径。所以入口是 rts_orders_move_selected(ecs, nav, target) 这种形状：
* 显式收 ecs 和导航网格、显式收目标点，内部【不取任何全局单例】（没有 lum_app_*、
* 没有 lum_window_*），因此不需要窗口就能回归测试。
*
* 阶段契约（引擎要求，见 include/lum_application.h）：
*   - 下令放 Input：屏幕→世界只需要 camera + window_size，不依赖 transform.world，
*     所以本步 Pre 就能开始走，没有一帧延迟
*   - 推进放 Pre：它写 transform.position（local），本步 Transform 阶段就会用上
* 两者都不许放 Post 之后 —— 那时写 local 要到下一步才反映到 world。
*
* nav 传 NULL 时退化成 M2 的"直线趋近"（一个路点），这条路径一直保留着：
* 它既是 [P] 键切换的对照，也是回归测试里"没有网格也能动"的基线。
*/

/* 编队格距系数：围绕目标点铺网格，格距 = 单位边长 × 这个系数。
   若让所有单位奔同一个点，它们会完全重叠，看起来像"只剩一个单位"。
   参考 BARb：它对小队用同心弧站位（SquadTask.cpp），M3 仍用更简单的网格。 */
#define Rts_Orders_Spread 1.30f

/* 距终点小于这个距离即算到达（世界单位）。
   必须 > 0：浮点逼近永远到不了精确相等，没有容差就会在终点附近抖动。 */
#define Rts_Orders_Arrive 1.0f

/* 中途路点的到达容差。比格子（32）小得多，所以不会为了赶下一个点而切角。 */
#define Rts_Orders_Waypoint_Tol 4.0f

/* 给【已选中】的单位下发移动指令：在目标点周围铺成网格编队，
   再给每个单位算一条路（nav 为 NULL 时退化成直线）。
   返回实际接到指令的单位数 —— 寻路失败的单位【不接指令】，
   免得它一头直线撞进墙里。0 表示没有选中单位、或全部没路可走。 */
uint32_t rts_orders_move_selected(lum_Ecs* ecs, const rts_Navgrid* nav, lum_Vec2 target);

/* 推进所有有移动指令的单位：沿路点走，到点即停。
   dt 由调用方给（引擎在 Pre 阶段通过 ctx->dt 提供）。
   长路径被截断时，走到最后一个路点会就地重新规划一段。 */
void rts_orders_update(lum_Ecs* ecs, const rts_Navgrid* nav, float dt);

/* 当前有多少单位正在执行移动指令（HUD 与回归测试用） */
uint32_t rts_orders_active_count(lum_Ecs* ecs);

/* 所有在执行的单位的【剩余路点总数】（HUD / 测试用，能看出拉直的效果） */
uint32_t rts_orders_remaining_waypoints(lum_Ecs* ecs);

/* Scene 阶段：在下令点画一个方框标记。"上一次下令点"是 UI 状态，
   由调用方（rts_main.c）自己存，这里只提供无状态的画法。 */
void rts_orders_draw_mark(lum_Vec2 world, int32_t layer);

/* Scene 阶段：给【选中的】单位画出它还要走的折线。纯调试可视化，
   但它是"寻路到底绕没绕"最直接的证据。 */
void rts_orders_draw_paths(lum_Ecs* ecs, int32_t layer);

#endif //Rts_Orders_H
