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
*
* 两种用法：
*   rts_path_find(...)                 一次跑完（游戏侧、回归测试用）
*   search_begin / search_step / take  增量推进（demo 要手动控制循环、逐帧看搜索过程）
* 前者就是用后者实现的，两者结果必然一致。
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
* 一次搜索的"账"。工作数组仍然是实现文件里的静态量（单线程、网格有界），
* 这个结构体只记录这次搜索的参数与进度，因此可以安全地按值持有、随时推进。
*
* 还有一件更重要的：工作数组会一直保留到【下一次 begin】，
* 所以推进到一半也可以随时用下面的 last_* 查询把两张表画出来。
*/
typedef struct rts_Path_search {
  bool     active;     /* begin 成功过，可以推进 */
  bool     finished;   /* 已经结束（找到 或 确认无路） */
  bool     found;      /* 结束时是否找到；仅 finished 为真时有定义 */
  uint32_t expanded;   /* 已定案（closed list）的格子数，实时 */
  uint32_t steps;      /* 已推进的轮数；一轮 = 定案一个格子 */

  lum_Vec2 from;       /* 起点世界坐标（拉直时当第一个候选点用） */
  lum_Vec2 goal;       /* 终点世界坐标；已夹到最近的可通行格中心 */
  int32_t  scx, scy;   /* 起点格 */
  int32_t  gcx, gcy;   /* 终点格（= goal 所在格） */
  int32_t  current;    /* 最近一次 step 定案的格子序号（-1 表示还没定案过任何格）。
                          "前沿走到哪了"的读数：相机跟随、进度显示都用它。
                          格子序号 = cy * Rts_Nav_Cells_X + cx。 */
} rts_Path_search;

/* 开始一次搜索：只做好初始状态（起点入 open list），不推进任何一格。
   失败（起点在网格外 / 整张图被堵）返回 false。 */
bool rts_path_search_begin(const rts_Navgrid* nav, lum_Vec2 from, lum_Vec2 to,
                           rts_Path_search* out);

/* 推进一步。【一步 = 定案一个格子】（内部会把堆里的旧条目跳过，所以不会出现"空转一步"）。
   已结束则什么都不做。 */
void rts_path_search_step(const rts_Navgrid* nav, rts_Path_search* s);

/* 取结果：回溯格子路径 + 拉直成路点，写进 out。
   只在 finished && found 之后调用；否则返回 false 并把 out 清空。 */
bool rts_path_search_take(const rts_Navgrid* nav, const rts_Path_search* s, rts_Path* out);

/*
* 一次跑完（游戏侧用的就是这个）。语义与旧版完全一致：
* - 终点会先被夹到最近的可通行格中心；返回值里的 points[count-1] 就是那个夹后的点，
*   调用方应当把"最终目标"也记成这个点，否则会永远到不了、每帧重规划
* - 起点所在格即使被判为不可通行也允许出发（单位可能正站在被膨胀的障碍里）
* - 失败（起点在网格外 / 网格被堵死 / 无路）返回 false，out 被清空
*/
bool rts_path_find(const rts_Navgrid* nav, lum_Vec2 from, lum_Vec2 to, rts_Path* out);

/*
* 上一次搜索【定案】了多少个格子。
*
* 这是 A* 里"启发式到底有没有在省事"的直接读数：h 在起作用时，探索被压在
* "朝终点的一条带"里，这个数远小于总格数；一旦谁把 h 改成恒 0（退化成 Dijkstra），
* 探索会摊成一大片，这个数暴涨 —— 而【路径依然是最优的】，所有"路径对不对"的
* 测试都不会失败。所以必须专门盯着它。诊断与回归用。
*/
uint32_t rts_path_last_expanded(void);

/* ---------- 两张表的可视化查询 ----------
   都只对【最近一次搜索】有效，任何新的 begin / find 都会覆盖它们。
   这也就是为什么搜索跑完还能把结果画出来。 */

/* closed list：这个格子已经被定案 */
bool rts_path_last_visited(int32_t cx, int32_t cy);

/* open list：这个格子已经发现、但还没定案。
   实现上不需要额外数组 —— "被碰过（有有效 g）" 且 "没定案" 就是"在待办里"：
   第一次碰到某个格子时它的 g 还是无穷大，必然会被推进堆；之后要么还在堆里，要么已定案。 */
bool rts_path_last_open(int32_t cx, int32_t cy);

/*
* 某个格子在最近一次搜索里的两个代价读数（可视化/诊断用）：
*   g = 从起点走到该格的实际累计代价（直走 1、斜走 √2，障碍按角规则绕开）
*   h = 该格到终点的 octile 启发式，也就是堆排序键里的那一项（f = g + h）
*
* 只对【被搜索碰过】的格子（在 open 或 closed 表里）返回 true；其它格子返回 false，
* 此时两个输出都不写。h 是用 f - g 反推的（实现里只存了两个数组），
* 因此它与搜索当时用的那个 h 逐位一致，浮点上也最多差一个 ULP 级别的舍入。
*/
bool rts_path_last_costs(int32_t cx, int32_t cy, float* out_g, float* out_h);

#endif //Rts_Path_H
