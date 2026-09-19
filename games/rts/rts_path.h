#ifndef Rts_Path_H
#define Rts_Path_H

#include <stdbool.h>
#include <stdint.h>
#include "luminous.h"
#include "rts_navgrid.h"

/*
* 网格 A* + 拉直（string pulling）。
*
* 为什么是 A*：网格是有界的静态图，"起点到终点"是单源单汇最短路，
* 启发式又能给方向感 —— A* 在这里是最省事且够快的选择。
* 启发式用 octile（八向网格的精确距离），保证可采纳、也就保证最优。
*
* 为什么要拉直：A* 给的是【逐格】路径，几十个格子中心连起来是锯齿状。
* 拉直把"中间能一眼看穿的点"全删掉，结果通常只剩 2~5 个拐点。
* 这既让移动看起来自然，也是能让路点塞进【定长缓冲】的前提。
*/

/* 每个单位能存的路点数。拉直之后通常 2~5 个，16 是很大的余量。
   万一真超了，就截断并置 truncated，单位走到头会重新规划一段。
   —— 定长缓冲 + 走到头重规划，比"每单位动态数组"简单得多，
      也不需要为了路径去开新组件（单位已经用满 3 个组件槽）。 */
#define Rts_Path_Max_Waypoints 16

typedef struct rts_Path {
  lum_Vec2 points[Rts_Path_Max_Waypoints];   /* 路点，最后一个就是最终目标 */
  uint8_t  count;
  uint8_t  truncated;                        /* 1 = 太长被截断，走到头要重新规划 */
} rts_Path;

/*
* 求一条从 from 到 to 的路点序列，写进 out。
*
* - 终点会先被夹到最近的可通行格中心；返回值里的 points[count-1] 就是那个夹后的点，
*   调用方应当把"最终目标"也记成这个点，否则会永远到不了、每帧重规划
* - 起点所在格即使被判为不可通行也允许出发（单位可能正站在被膨胀的障碍里）
* - 失败（起点在网格外 / 网格被堵死 / 无路）返回 false，out 被清空
*/
bool rts_path_find(const rts_Navgrid* nav, lum_Vec2 from, lum_Vec2 to, rts_Path* out);

#endif //Rts_Path_H
