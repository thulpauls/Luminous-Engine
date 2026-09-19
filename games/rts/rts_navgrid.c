#include "rts_navgrid.h"

static int32_t rts__clamp_i32(int32_t v, int32_t lo, int32_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

void rts_navgrid_init(rts_Navgrid* g) {
  if (!g) return;
  for (int32_t i = 0; i < Rts_Nav_Max_Cells; ++i) g->blocked[i] = 0;
  g->obstacle_count = 0;
}

void rts_navgrid_add_obstacle(rts_Navgrid* g, int32_t cx, int32_t cy, int32_t cw, int32_t ch) {
  if (!g || cw <= 0 || ch <= 0) return;
  if (g->obstacle_count >= Rts_Nav_Max_Obstacles) return;

  /* 原始矩形记下来（画的时候想画真实边界可以用），但真正生效的是【膨胀后】的格子 */
  g->obstacles[g->obstacle_count].cx = cx;
  g->obstacles[g->obstacle_count].cy = cy;
  g->obstacles[g->obstacle_count].cw = cw;
  g->obstacles[g->obstacle_count].ch = ch;
  ++g->obstacle_count;

  const int32_t x0 = cx - Rts_Nav_Inflate;
  const int32_t y0 = cy - Rts_Nav_Inflate;
  const int32_t x1 = cx + cw - 1 + Rts_Nav_Inflate;
  const int32_t y1 = cy + ch - 1 + Rts_Nav_Inflate;
  for (int32_t y = y0; y <= y1; ++y) {
    if (y < 0 || y >= Rts_Nav_Cells_Y) continue;
    for (int32_t x = x0; x <= x1; ++x) {
      if (x < 0 || x >= Rts_Nav_Cells_X) continue;
      g->blocked[y * Rts_Nav_Cells_X + x] = 1;
    }
  }
}

void rts_navgrid_setup_default(rts_Navgrid* g) {
  if (!g) return;
  rts_navgrid_init(g);

  /* 一道竖墙（cx = 31，世界 x 约 [-32, 0]），只在 cy 45..49 留缺口。
     两军都在 cy 29..33（世界 y 约 -90..42），缺口在世界 y 约 [448, 544]，
     所以任何横穿都要北上绕一大圈 —— 绕路一眼可见。 */
  rts_navgrid_add_obstacle(g, 31,  0, 1, 45);   /* cy 0..44  */
  rts_navgrid_add_obstacle(g, 31, 50, 1, 14);   /* cy 50..63 */

  /* 两个方柱，逼路径多拐两个弯 */
  rts_navgrid_add_obstacle(g, 12, 20, 4, 4);
  rts_navgrid_add_obstacle(g, 48, 18, 5, 5);
}

bool rts_navgrid_blocked(const rts_Navgrid* g, int32_t cx, int32_t cy) {
  if (!g) return true;
  if (cx < 0 || cy < 0 || cx >= Rts_Nav_Cells_X || cy >= Rts_Nav_Cells_Y) return true;
  return g->blocked[cy * Rts_Nav_Cells_X + cx] != 0;
}

bool rts_navgrid_world_to_cell(lum_Vec2 w, int32_t* out_cx, int32_t* out_cy) {
  const float fx = (w.x - Rts_Nav_Origin_X) / Rts_Nav_Cell_Size;
  const float fy = (w.y - Rts_Nav_Origin_Y) / Rts_Nav_Cell_Size;
  if (!(fx >= 0.0f) || !(fy >= 0.0f)) return false;             /* 也挡住 NaN */
  if (fx >= (float)Rts_Nav_Cells_X || fy >= (float)Rts_Nav_Cells_Y) return false;
  if (out_cx) *out_cx = (int32_t)fx;                            /* 截断即 floor（已保证 >= 0） */
  if (out_cy) *out_cy = (int32_t)fy;
  return true;
}

lum_Vec2 rts_navgrid_cell_center(int32_t cx, int32_t cy) {
  return lum_vec2_create(Rts_Nav_Origin_X + ((float)cx + 0.5f) * Rts_Nav_Cell_Size,
                         Rts_Nav_Origin_Y + ((float)cy + 0.5f) * Rts_Nav_Cell_Size);
}

lum_Vec2 rts_navgrid_clamp_to_free(const rts_Navgrid* g, lum_Vec2 w) {
  if (!g) return w;

  int32_t cx = 0, cy = 0;
  if (!rts_navgrid_world_to_cell(w, &cx, &cy)) {
    /* 点在网格外：先把格子夹进网格，再从那里找最近的可通行格 */
    cx = rts__clamp_i32((int32_t)((w.x - Rts_Nav_Origin_X) / Rts_Nav_Cell_Size), 0, Rts_Nav_Cells_X - 1);
    cy = rts__clamp_i32((int32_t)((w.y - Rts_Nav_Origin_Y) / Rts_Nav_Cell_Size), 0, Rts_Nav_Cells_Y - 1);
  }
  if (!rts_navgrid_blocked(g, cx, cy)) return rts_navgrid_cell_center(cx, cy);

  /* 一圈一圈往外找。只扫每圈的边框，所以总代价是 O(圈数²) 而不是 O(圈数³)。
     上限取两轴之和：再远也不可能比"斜穿整张图"更远。 */
  const int32_t max_r = Rts_Nav_Cells_X + Rts_Nav_Cells_Y;
  for (int32_t r = 1; r <= max_r; ++r) {
    for (int32_t dy = -r; dy <= r; ++dy) {
      for (int32_t dx = -r; dx <= r; ++dx) {
        const bool on_border = (dx == -r || dx == r || dy == -r || dy == r);
        if (!on_border) continue;
        const int32_t x = cx + dx, y = cy + dy;
        if (x < 0 || y < 0 || x >= Rts_Nav_Cells_X || y >= Rts_Nav_Cells_Y) continue;
        if (!g->blocked[y * Rts_Nav_Cells_X + x]) return rts_navgrid_cell_center(x, y);
      }
    }
  }
  return rts_navgrid_cell_center(cx, cy);   /* 整张图都被堵：把原格丢回去，交给调用方判失败 */
}

bool rts_navgrid_line_clear(const rts_Navgrid* g, lum_Vec2 a, lum_Vec2 b) {
  if (!g) return false;
  int32_t ax = 0, ay = 0, bx = 0, by = 0;
  if (!rts_navgrid_world_to_cell(a, &ax, &ay)) return false;
  if (!rts_navgrid_world_to_cell(b, &bx, &by)) return false;

  /* Bresenham 走线。它比真实几何线段"更粗"（斜线会连带要求旁边一圈格子为空），
     而我要的就是这个方向：宁可拉直得保守，也不能出现"直线测试说通了、
     实际走的时候却蹭到墙"。 */
  int32_t x = ax, y = ay;
  const int32_t dx = (bx > ax) ? (bx - ax) : (ax - bx);
  const int32_t dy = (by > ay) ? (by - ay) : (ay - by);
  const int32_t sx = (ax < bx) ? 1 : -1;
  const int32_t sy = (ay < by) ? 1 : -1;
  int32_t err = dx - dy;

  for (;;) {
    if (rts_navgrid_blocked(g, x, y)) return false;
    if (x == bx && y == by) return true;

    const int32_t e2 = 2 * err;
    const bool step_x = (e2 > -dy);
    const bool step_y = (e2 < dx);
    if (step_x && step_y) {
      /* 对角一步：和 A* 同一条"禁止切角"规则 */
      if (rts_navgrid_blocked(g, x + sx, y)) return false;
      if (rts_navgrid_blocked(g, x, y + sy)) return false;
    }
    if (step_x) { err -= dy; x += sx; }
    if (step_y) { err += dx; y += sy; }
  }
}

void rts_navgrid_draw(const rts_Navgrid* g, int32_t layer) {
  if (!g) return;
  const lum_Vec4 fill = lum_vec4_create(0.230f, 0.270f, 0.350f, 1.0f);

  for (int32_t cy = 0; cy < Rts_Nav_Cells_Y; ++cy) {
    int32_t cx = 0;
    while (cx < Rts_Nav_Cells_X) {
      if (!g->blocked[cy * Rts_Nav_Cells_X + cx]) { ++cx; continue; }

      int32_t run = 1;
      while (cx + run < Rts_Nav_Cells_X && g->blocked[cy * Rts_Nav_Cells_X + cx + run]) ++run;

      /* origin 传 (0,0)：矩形从 position 这个角开始铺，正好对上格子边界 */
      lum_renderer2d_draw_rect(
          lum_vec2_create(Rts_Nav_Origin_X + (float)cx * Rts_Nav_Cell_Size,
                          Rts_Nav_Origin_Y + (float)cy * Rts_Nav_Cell_Size),
          lum_vec2_create((float)run * Rts_Nav_Cell_Size, Rts_Nav_Cell_Size),
          0.0f, lum_vec2_0(), layer, fill);

      cx += run;
    }
  }
}
