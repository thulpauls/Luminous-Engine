#ifndef Rts_Navgrid_H
#define Rts_Navgrid_H

#include <stdbool.h>
#include <stdint.h>
#include "luminous.h"

/*
* 导航网格：寻路的"可通行地图"。
*
* 三个刻意的设计选择：
*
* ① 障碍用【整数格子矩形】表示，不用世界坐标矩形。
*    这样烘焙（矩形 → 哪些格子被堵）没有任何浮点误差，而且"画出来的障碍"
*    和"寻路看到的障碍"是同一份数据 —— 只有单一事实来源。
*
* ② 障碍在烘焙时【向四周膨胀 Rts_Nav_Inflate 格】。
*    格子 32 世界单位、单位身宽 22：膨胀 1 格之后，"某个格子可通行"
*    就意味着单位站在这格中心时整个身子都在可通行区域里。
*    于是寻路之外【不需要任何几何碰撞检测】，绕行是走出来的而不是挤出来的。
*
* ③ 网格是有界的静态数组，不 malloc。
*    越界的格子一律算"被堵"，所以网格边界天然就是世界边界。
*/

#define Rts_Nav_Cell_Size   32.0f    /* 一格多少世界单位 */
#define Rts_Nav_Cells_X     64
#define Rts_Nav_Cells_Y     64
#define Rts_Nav_Max_Cells   (Rts_Nav_Cells_X * Rts_Nav_Cells_Y)

/* 格子 (0,0) 的左下角在世界坐标的位置。网格覆盖
   [-1024, 1024] × [-1024, 1024]，够两支军队（都在 ±400 内）用很久。 */
#define Rts_Nav_Origin_X    (-1024.0f)
#define Rts_Nav_Origin_Y    (-1024.0f)

#define Rts_Nav_Max_Obstacles 16
#define Rts_Nav_Inflate       1      /* 障碍向四周膨胀的格数 */

typedef struct rts_Navgrid {
  uint8_t blocked[Rts_Nav_Max_Cells];             /* 1 = 不可通行（已含膨胀） */
  uint32_t obstacle_count;
  struct {                                        /* 原始（未膨胀）的障碍矩形，格子单位 */
    int32_t cx, cy, cw, ch;
  } obstacles[Rts_Nav_Max_Obstacles];
} rts_Navgrid;

/* 清空成"全可通行" */
void rts_navgrid_init(rts_Navgrid* g);

/* 加一个障碍矩形（格子单位，未膨胀）。超出网格或数量超限会被裁掉/忽略 */
void rts_navgrid_add_obstacle(rts_Navgrid* g, int32_t cx, int32_t cy, int32_t cw, int32_t ch);

/* 摆一个能一眼看出"必须绕路"的默认布局：一道带缺口的竖墙 + 两个方柱。
   缺口故意开在两军 y 范围之外，所以要北上绕一大圈。 */
void rts_navgrid_setup_default(rts_Navgrid* g);

/* 越界一律返回 true（网格外 = 不可通行） */
bool     rts_navgrid_blocked(const rts_Navgrid* g, int32_t cx, int32_t cy);

/* 世界坐标 → 格子。越界返回 false（不夹取，让调用方自己决定怎么办） */
bool     rts_navgrid_world_to_cell(lum_Vec2 w, int32_t* out_cx, int32_t* out_cy);
lum_Vec2 rts_navgrid_cell_center(int32_t cx, int32_t cy);

/* 把一个点夹到最近的可通行格子中心。
   用途：玩家点在墙上时，指令落到墙边最近的可站位置，而不是"没有反应"。
   网格全被堵时返回原格中心（调用方会因寻路失败而拒绝这条指令）。 */
lum_Vec2 rts_navgrid_clamp_to_free(const rts_Navgrid* g, lum_Vec2 w);

/* 直线可视判定：线经过的每一格都必须可通行。
   对角步子要求两个正交邻居都可通行 —— 和 A* 用的是【同一条规则】，
   否则会出现"寻路不走、拉直却切过去了"。比真实几何更保守，保守这一侧才安全。 */
bool rts_navgrid_line_clear(const rts_Navgrid* g, lum_Vec2 a, lum_Vec2 b);

/* Scene 阶段画障碍。同一行的连续被堵格子会合并成一个矩形，
   不然 800 个格子会变成 800 条绘制命令。 */
void rts_navgrid_draw(const rts_Navgrid* g, int32_t layer);

#endif //Rts_Navgrid_H
