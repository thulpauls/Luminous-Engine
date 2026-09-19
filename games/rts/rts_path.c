#include "rts_path.h"

#include <math.h>

/*
* A* 的工作区。网格有界（64×64 = 4096 格），所以全部用静态数组，一次都不 malloc。
*
* 三个省事的地方：
*   - s_stamp / s_done 用【代次号】而不是每次查询清数组。一次查询只把 gen 加一，
*     4096 格的清零就此省掉（这正是 MicroPather 里"帧戳复用节点"的同一招）
*   - 二叉堆 + 惰性删除：重复入堆的旧条目在弹出时靠 s_done 判掉，
*     不需要 decrease-key（也就没有"节点在哪"的反查表）
*   - 每格最多被松弛 8 次（八邻居各一次），所以堆容量取 10 倍格数就绝不溢出
*/

#define Rts_Path_Heap_Cap (Rts_Nav_Max_Cells * 10)
#define RTS_SQRT2 1.41421356f

static float    s_g[Rts_Nav_Max_Cells];        /* 从起点走到该格的实际代价 */
static float    s_f[Rts_Nav_Max_Cells];        /* g + 启发式（堆的排序键） */
static int32_t  s_from[Rts_Nav_Max_Cells];     /* 前驱格，用来回溯 */
static uint32_t s_stamp[Rts_Nav_Max_Cells];    /* == s_gen 表示 g/f 有效 */
static uint32_t s_done[Rts_Nav_Max_Cells];     /* == s_gen 表示已关闭（不再松弛） */
static uint32_t s_gen = 0;

static int32_t  s_heap[Rts_Path_Heap_Cap];
static int32_t  s_heap_size = 0;

static int32_t  s_cells[Rts_Nav_Max_Cells];    /* 回溯出来的格子路径 */
static lum_Vec2 s_cand[Rts_Nav_Max_Cells + 1]; /* 拉直前的候选点 */

/* 八邻居。前 4 个正交（代价 1），后 4 个对角（代价 √2） */
static const int32_t kDX[8] = {  1, -1,  0,  0,  1,  1, -1, -1 };
static const int32_t kDY[8] = {  0,  0,  1, -1,  1, -1,  1, -1 };

static inline int32_t rts__abs(int32_t v) { return (v < 0) ? -v : v; }

/* octile 启发式：八向网格上的精确距离。
   等价于 BARb 那个写法 (dx+dy) - 0.5858*min(dx,dy)（0.5858 = 2 - √2）。
   因为对角的真实代价就是 √2，所以这个估计既不高于真实值（可采纳），
   也不会高估相邻两步的差（一致）—— 两个性质都有，A* 才保证最优。 */
static inline float rts__h(int32_t ax, int32_t ay, int32_t bx, int32_t by) {
  const int32_t dx = rts__abs(ax - bx);
  const int32_t dy = rts__abs(ay - by);
  const int32_t mn = (dx < dy) ? dx : dy;
  const int32_t mx = (dx < dy) ? dy : dx;
  return (float)(mx - mn) + RTS_SQRT2 * (float)mn;
}

/* ---------- 二叉最小堆（键 = s_f） ---------- */

static void rts__heap_push(int32_t cell) {
  if (s_heap_size >= Rts_Path_Heap_Cap) return;   /* 理论上到不了；真到了就丢，搜索仍能返回 */
  int32_t i = s_heap_size++;
  s_heap[i] = cell;
  while (i > 0) {
    const int32_t parent = (i - 1) / 2;
    if (s_f[s_heap[parent]] <= s_f[s_heap[i]]) break;
    const int32_t tmp = s_heap[parent];
    s_heap[parent] = s_heap[i];
    s_heap[i] = tmp;
    i = parent;
  }
}

static int32_t rts__heap_pop(void) {
  if (s_heap_size <= 0) return -1;
  const int32_t top = s_heap[0];
  s_heap[0] = s_heap[--s_heap_size];
  int32_t i = 0;
  for (;;) {
    const int32_t l = 2 * i + 1;
    const int32_t r = 2 * i + 2;
    int32_t best = i;
    if (l < s_heap_size && s_f[s_heap[l]] < s_f[s_heap[best]]) best = l;
    if (r < s_heap_size && s_f[s_heap[r]] < s_f[s_heap[best]]) best = r;
    if (best == i) break;
    const int32_t tmp = s_heap[best];
    s_heap[best] = s_heap[i];
    s_heap[i] = tmp;
    i = best;
  }
  return top;
}

/* ---------- 可通行判定（起点格豁免） ---------- */

static inline bool rts__passable(const rts_Navgrid* nav, int32_t cx, int32_t cy,
                                int32_t scx, int32_t scy) {
  if (cx == scx && cy == scy) return true;   /* 出发格：允许离开 */
  return !rts_navgrid_blocked(nav, cx, cy);
}

/* ---------- 搜索，结果写进 s_cells，返回格子数 ---------- */

static int32_t rts__search(const rts_Navgrid* nav, int32_t scx, int32_t scy,
                          int32_t gcx, int32_t gcy) {
  ++s_gen;
  if (s_gen == 0u) {                          /* 代次回绕：清一次再从 1 开始 */
    for (int32_t i = 0; i < Rts_Nav_Max_Cells; ++i) { s_stamp[i] = 0; s_done[i] = 0; }
    s_gen = 1u;
  }
  s_heap_size = 0;

  const int32_t start = scy * Rts_Nav_Cells_X + scx;
  const int32_t goal  = gcy * Rts_Nav_Cells_X + gcx;

  s_stamp[start] = s_gen;
  s_g[start] = 0.0f;
  s_f[start] = rts__h(scx, scy, gcx, gcy);
  s_from[start] = -1;
  rts__heap_push(start);

  while (s_heap_size > 0) {
    const int32_t cur = rts__heap_pop();
    if (s_done[cur] == s_gen) continue;       /* 惰性删除：旧条目，已经有更好的 g 了 */
    s_done[cur] = s_gen;

    if (cur == goal) break;

    const int32_t cx = cur % Rts_Nav_Cells_X;
    const int32_t cy = cur / Rts_Nav_Cells_X;

    for (int32_t k = 0; k < 8; ++k) {
      const int32_t nx = cx + kDX[k];
      const int32_t ny = cy + kDY[k];
      if (!rts__passable(nav, nx, ny, scx, scy)) continue;

      if (k >= 4) {
        /* 对角：两个正交邻居都必须可通行。
           少了这条，路径会从两个被堵格子的"角"上斜着穿过去，看着就是穿墙。 */
        if (!rts__passable(nav, cx + kDX[k], cy, scx, scy)) continue;
        if (!rts__passable(nav, cx, cy + kDY[k], scx, scy)) continue;
      }

      const int32_t n = ny * Rts_Nav_Cells_X + nx;
      if (s_done[n] == s_gen) continue;

      if (s_stamp[n] != s_gen) { s_stamp[n] = s_gen; s_g[n] = 1e30f; }

      const float ng = s_g[cur] + ((k >= 4) ? RTS_SQRT2 : 1.0f);
      if (ng < s_g[n]) {
        s_g[n] = ng;
        s_f[n] = ng + rts__h(nx, ny, gcx, gcy);
        s_from[n] = cur;
        rts__heap_push(n);
      }
    }
  }

  if (s_done[goal] != s_gen) return 0;        /* 没走到终点 = 无路 */

  int32_t n = 0;
  for (int32_t c = goal; c != -1; c = s_from[c]) {
    if (n >= Rts_Nav_Max_Cells) break;
    s_cells[n++] = c;
  }
  for (int32_t i = 0; i < n / 2; ++i) {       /* 反转成 起点 → 终点 */
    const int32_t t = s_cells[i];
    s_cells[i] = s_cells[n - 1 - i];
    s_cells[n - 1 - i] = t;
  }
  return n;
}

/* ---------- 对外 ---------- */

bool rts_path_find(const rts_Navgrid* nav, lum_Vec2 from, lum_Vec2 to, rts_Path* out) {
  if (!out) return false;
  out->count = 0;
  out->truncated = 0;
  if (!nav) return false;

  int32_t scx = 0, scy = 0;
  if (!rts_navgrid_world_to_cell(from, &scx, &scy)) return false;   /* 起点在网格外 */

  /* 终点夹到最近的可通行格。夹完的点要回传给调用方当最终目标 ——
     否则单位会永远够不到原始目标，每帧重规划。 */
  const lum_Vec2 goal = rts_navgrid_clamp_to_free(nav, to);
  int32_t gcx = 0, gcy = 0;
  if (!rts_navgrid_world_to_cell(goal, &gcx, &gcy)) return false;
  if (rts_navgrid_blocked(nav, gcx, gcy)) return false;             /* 整张图被堵死 */

  const int32_t ncell = rts__search(nav, scx, scy, gcx, gcy);
  if (ncell <= 0) return false;

  if (ncell == 1) {
    /* 起终点同格：直接一个路点（夹后的终点） */
    out->points[0] = goal;
    out->count = 1;
    return true;
  }

  /* 组装候选点。第 0 个用【单位真实位置】而不是起点格中心 ——
     否则单位会先退回去对齐格心，看着像"起步时抖一下"。 */
  int32_t m = 0;
  s_cand[m++] = from;
  for (int32_t i = 1; i < ncell; ++i) {
    s_cand[m++] = rts_navgrid_cell_center(s_cells[i] % Rts_Nav_Cells_X,
                                          s_cells[i] / Rts_Nav_Cells_X);
  }

  /* 贪心拉直：从 i 出发尽量跳到最远的、直线可达的点。
     一旦某点不可达就停 —— 再远的更不可能可达（够用且简单，不追求严格单调性证明）。 */
  int32_t i = 0;
  while (i < m - 1) {
    int32_t best = i + 1;
    for (int32_t j = i + 2; j < m; ++j) {
      if (!rts_navgrid_line_clear(nav, s_cand[i], s_cand[j])) break;
      best = j;
    }
    if (out->count >= Rts_Path_Max_Waypoints) { out->truncated = 1; break; }
    out->points[out->count++] = s_cand[best];
    i = best;
  }
  return out->count > 0;
}
