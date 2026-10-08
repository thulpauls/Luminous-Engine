/* a_star_demo —— 网格 A* 寻路的可视化演示（手动控制算法循环）。
 *
 * 演示内容：
 *   - A* 的【每一步】都能看见：closed list 画成半透明蓝色纯色填充，open list 画成
 *     每个格心上的琥珀色小圆点，每按一次空格，恰好定案一个格子
 *   - closed 的每一格里还写着它当时的【两个代价】：左上角 g（从起点的实际代价）、
 *     右上角 h（到终点的 octile 启发式），都是搜索当时的真实读数、一位小数 ——
 *     直走 1.0 与斜走 1.4 的差别就是 A* 代价的样子，抹成整数就看不出来了。
 *     只画视野内 closed 的格子（视野外画了也看不见，白白堆命令）
 *   - 左键设起点、右键画/擦障碍、中键设终点，任何改动都会把搜索重置到起点入队的初始态
 *   - 搜索跑完后继续显示两张表（工作数组会保留到下一次 begin），并画出拉直后的路径
 *
 * 视野：一屏 12×12 格（每格 80 px，zoom = 2.5）。整张图是 64×64，所以相机默认
 * 【跟随刚刚定案的那一格】，按 WASD/方向键就接管成手动平移（[F] 再切回跟随）。
 * 为什么是 80 px 这么大的格子：格内要并排放下 g 和 h 两个一位小数（各 4 字符），
 * 而实测字号小于 10 px 的文字墨迹覆盖会掉到 0.2 以下、等于没画 —— 详细推导见下面
 * Demo_Cell_Px 那段注释。24×24 的视野与"看得清的 g/h"不可兼得，这里选了后者。
 *
 * 操作：
 *   左键 = 设起点      右键 = 画障碍（单击已堵的格子则擦掉）      中键 = 设终点
 *   [空格] 单步推进   [回车] 一次跑完   [B] 重置搜索   [P] 自动播放开关
 *   [C] 清空障碍   [M] 随机障碍   [V] 两张表开关   [G] 网格线开关   [Q] 退出
 *   [WASD/方向键] 平移视野（一按就接管）   [F] 跟随前沿开关
 *
 * 重要：本演示【复用 games/rts 下的 rts_navgrid + rts_path】—— 也就是游戏真正在跑的
 * 那一份实现，而不是另写一个教学版。demo 里看到的搜索过程就是游戏的搜索过程，
 * 不存在"两份 A* 慢慢漂移"。所以这里只做"编辑"和"画"，算法一行都不重写。
 *
 * 另一处刻意的差异：障碍是【直接写 navgrid 的 blocked 数组】，不走 rts_navgrid_add_obstacle ——
 * 那个接口会按 RTS 的需要把障碍向四周膨胀一格（因为单位有体积），
 * 对逐格编辑的 demo 会变成"点一下出 3×3"，反而看不清网格语义。
 */

#include "luminous.h"
#include "rts_navgrid.h"
#include "rts_path.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define Demo_Win_W 960
#define Demo_Win_H 960

/* 网格在世界空间的总跨度：64 格 × 32 单位 = 2048，且以世界原点为中心 */
#define Demo_Grid_Span ((float)Rts_Nav_Cells_X * Rts_Nav_Cell_Size)

/* 自动播放时每帧推进几格 */
#define Demo_Auto_Steps 4

/* ---------- 视野：一屏看多少格 ----------
   每格 80 px（zoom = 2.5，一屏 12×12 格）。这两个数是被"格内要放下 g 和 h 两个一位小数"
   反推出来的，而且推法很容易错 —— 我第一版按世界单位算，算对了却仍然看不见，实测才明白：

     格内文字现在走【屏幕空间】的 lum_tr_draw_screen（1:1 光栅化），字号是像素。
     而字形位图是按字号【像素】光栅化的，再把字形四边形放到屏幕上：
       · 世界空间写法（lum_tr_draw_layer）的字号是世界单位，位图也按这个数光栅化，
         于是屏幕上真正的字号 = 世界字号 × zoom，位图却被放大 zoom 倍 → 小字被糊掉；
       · 实测墨迹峰值覆盖（字号 = 位图光栅化高度）：
           7 px → 0.00（完全看不见）  8 px → 0.22   10 px → 0.66
          12 px → 0.69              15 px → 0.85   20 px → 0.95
        也就是"字号小于 10 px 的数字等于没画"—— 细小笔画的覆盖被线性过滤摊没了。
     所以一位小数的 4 字符数字（如 72.1）在 14 px 下要 ~34 px 宽，两个并排 ≈ 76 px：
     格子必须 ~80 px，一屏就是 12×12。这与"24×24 的视野"不可兼得。 */
#define Demo_Cell_Px     80.0f
#define Demo_Zoom        ((float)Demo_Cell_Px / Rts_Nav_Cell_Size)
#define Demo_View_Span   ((float)Demo_Win_W / Demo_Zoom)                   /* 可见世界边长 384 */
#define Demo_View_Cells  ((int32_t)(Demo_View_Span / Rts_Nav_Cell_Size))   /* 可见格数 12 */
#define Demo_Half        (Demo_View_Span * 0.5f)

/* 格内 g/h：字号与留白都是【屏幕像素】（走 Screen 阶段的文字 API） */
#define Demo_Label_Font_Px 14.0f
#define Demo_Label_Pad_Px  3.0f

/* 手动平移速度（世界单位/秒）：32 单位/格 ⇒ 160 就是 5 格/秒（≈ 400 px/秒） */
#define Demo_Pan_Speed   160.0f

/* open list 小圆点的半径（像素）。一格 80 px，点取 3.5 px 半径（直径 7 px）。 */
#define Demo_Open_Dot_Px 3.5f

/*
* 世界单位 ↔ 像素的换算，只为一件事：线宽/点半径。
*
* lum_renderer2d_draw_line / draw_rect 的 thickness 与 size 都是【世界单位】。
* 现在 zoom = 1.25（放大了），"thickness = 1"其实有 1.25 px —— 但结论不变：
* 只要有人把 zoom 调回 < 1，按"像素直觉"写的细线就会掉到亚像素，
* **光栅化不出任何片元，整条线静默消失**（0 像素、无警告、无报错）。
* 我为这个查了四轮：一开始怀疑 layer，把 9 个 layer 各画一条线做 A/B，
* 结果 9 个全部正常，才意识到变量是粗细而不是图层。
* 所以下面所有"线宽/圆点半径"一律过 demo_px()，按像素写、按世界单位传。
*/
static float demo_px(float pixels) { return pixels / Demo_Zoom; }

/* 绘制层级：渲染器按 layer 优先排序，小的先画。
   每个视觉元素各占一层，不只是为了好看 —— 同一层里 VP / 材质不同的命令，
   绘制顺序由状态字节决定而不是记录顺序（背板就是这么被压到文字上面的，
   完整说明见 sys_screen 里画背板处）。

   两张表的画法：
     closed = 一整片半透明纯色（按行合并成矩形，见 demo_draw_runs），垫在网格线【之下】，
              这样网格依然清晰，填充只表达"这些格子已定案"；
     open   = 每个格心一个小圆点，放在网格线【之上】，是当前的前沿。
   一开始用的是"只画集合边界"的边框，同心方框叠几层之后糊成一片，
   不如纯色填充 + 点直观。 */
enum {
  Demo_Layer_Cell        = 0,   /* 可通行底色 */
  Demo_Layer_Wall        = 1,   /* 障碍 */
  Demo_Layer_Closed      = 2,   /* closed list：半透明纯色填充 */
  Demo_Layer_Grid        = 3,   /* 网格线 */
  Demo_Layer_Open        = 4,   /* open list：格心小圆点 */
  Demo_Layer_Path        = 5,   /* 路径折线 */
  Demo_Layer_Mark        = 6,   /* 起点 / 终点 */
  Demo_Layer_Cost        = 7,   /* 格内的 g / h 数字（压在路径与起终点标记之上，才读得到） */
  Demo_Layer_Panel       = 9,   /* HUD 背板（Screen 阶段，必须低于 HUD 文字） */
  Demo_Layer_Hud         = 10   /* HUD 文字（Screen 阶段） */
};

static rts_Navgrid     g_nav;
static rts_Path_search g_search;
static rts_Path        g_path;
static bool            g_have_path;

static int32_t         g_start_cx = 6,  g_start_cy = 6;
static int32_t         g_goal_cx  = 57, g_goal_cy  = 57;

static bool            g_auto;
static bool            g_show_lists = true;
static bool            g_show_grid = true;

static uint32_t        g_closed_count;                /* closed 表的格子数 */
static uint32_t        g_open_dots, g_closed_runs;    /* 实际发出的几何量：open 画了几个点、closed 合并成几个矩形 */
static uint32_t        g_cost_labels;                 /* 实际画出的 g/h 数字对数（视野内） */
static float           g_path_len, g_straight_len;

static lum_Font*       g_font;                        /* HUD 字体（Screen 阶段，字号 = 像素） */
static lum_Font*       g_label_font;                  /* 格内 g/h 字体（Screen 阶段，字号 = 像素） */
static float           g_label_char_w;                /* 等宽字体一个字符的进给（像素），右对齐用 */

/* 本帧渲染真正用的相机。必须在 sys_pre 之前抓：引擎在 Frame_Begin 就把相机灌给渲染器，
   而 sys_pre 会把相机挪到"下一帧"的位置，所以 Scene 阶段的几何用的是【抓之前】那个值。
   格内文字在 Screen 阶段才画，如果那时读实时相机会比网格晚一帧 —— 平移时文字会甩出去。 */
static lum_Camera2d    g_frame_cam;

static bool            g_follow = true;               /* 相机跟随"刚刚定案的那一格" */

/* ---------------- 小工具 ---------------- */

static lum_Vec2 demo_cell_min(int32_t cx, int32_t cy) {
  return lum_vec2_create(Rts_Nav_Origin_X + (float)cx * Rts_Nav_Cell_Size,
                         Rts_Nav_Origin_Y + (float)cy * Rts_Nav_Cell_Size);
}

/* 把相机夹到"可见矩形始终落在网格内"。夹完就没有"网格外的空白"，
   于是鼠标点在任何地方都必然落在某个真实格子里（编辑逻辑不用再判边界）。 */
static lum_Vec2 demo_clamp_cam(lum_Vec2 p) {
  const float x0 = Rts_Nav_Origin_X + Demo_Half;
  const float x1 = Rts_Nav_Origin_X + Demo_Grid_Span - Demo_Half;
  const float y0 = Rts_Nav_Origin_Y + Demo_Half;
  const float y1 = Rts_Nav_Origin_Y + Demo_Grid_Span - Demo_Half;

  if (x0 > x1) p.x = 0.5f * (x0 + x1);      /* 视野比网格还大：居中（正常配置到不了） */
  else if (p.x < x0) p.x = x0;
  else if (p.x > x1) p.x = x1;

  if (y0 > y1) p.y = 0.5f * (y0 + y1);
  else if (p.y < y0) p.y = y0;
  else if (p.y > y1) p.y = y1;

  return p;
}

/* 视野覆盖的格子范围（闭区间，已夹到网格内）。相机显式传参，好让自检也能用。
   下界取 floor、上界取 ceil-1：两端【只露出一部分】的边缘格也要算进来，
   否则贴着视野边缘的那一列/行会没有 g/h 标签。

   两端都先减一个 eps（单位是格，1e-3 格 = 0.032 世界单位，远小于一个像素）：
   相机正好落在格边界上时，浮点噪声会让 ceil(12.0000006) = 13，凭空多出一整列
   （自检里就是这么发现"一屏 13 格"的）。减 eps 之后边界值落在哪边都不再多算。 */
#define Demo_Range_Eps 1e-3f
static void demo_visible_cells(const lum_Camera2d* cam,
                              int32_t* out_x0, int32_t* out_y0, int32_t* out_x1, int32_t* out_y1) {
  const float half = Demo_Half;
  const float inv = 1.0f / Rts_Nav_Cell_Size;
  int32_t x0 = (int32_t)floorf((cam->position.x - half - Rts_Nav_Origin_X) * inv + Demo_Range_Eps);
  int32_t x1 = (int32_t)ceilf((cam->position.x + half - Rts_Nav_Origin_X) * inv - Demo_Range_Eps) - 1;
  int32_t y0 = (int32_t)floorf((cam->position.y - half - Rts_Nav_Origin_Y) * inv + Demo_Range_Eps);
  int32_t y1 = (int32_t)ceilf((cam->position.y + half - Rts_Nav_Origin_Y) * inv - Demo_Range_Eps) - 1;

  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > Rts_Nav_Cells_X - 1) x1 = Rts_Nav_Cells_X - 1;
  if (y1 > Rts_Nav_Cells_Y - 1) y1 = Rts_Nav_Cells_Y - 1;

  *out_x0 = x0; *out_y0 = y0; *out_x1 = x1; *out_y1 = y1;
}

static bool demo_is_wall(int32_t cx, int32_t cy) {
  return rts_navgrid_blocked(&g_nav, cx, cy);
}

/* 鼠标当前在哪个格子。指针在网格外返回 false */
static bool demo_mouse_cell(int32_t* out_cx, int32_t* out_cy) {
  const lum_Vec2 w = lum_app_screen_to_world(lum_input_get_mouse_position());
  int32_t cx = 0, cy = 0;
  if (!rts_navgrid_world_to_cell(w, &cx, &cy)) return false;
  if (out_cx) *out_cx = cx;
  if (out_cy) *out_cy = cy;
  return true;
}

static bool demo_mouse_inside(void) {
  int32_t cx = 0, cy = 0;
  return demo_mouse_cell(&cx, &cy);
}

/* ---------------- 搜索控制 ---------------- */

static void demo_refresh_counts(void) {
  /* open 的格子数不在这里数：sys_scene 画点时必须逐格走一遍同样的谓词，
     顺手就把它记进 g_open_dots 了（同一帧、同一谓词，重复扫描 4096 格没意义）。 */
  g_closed_count = g_search.expanded;
}

/* 把搜索退回到"只有起点进了 open list"的初始态。任何编辑之后都调它 ——
   手动控制循环的语义就是：改完重新开始按步走。 */
static void demo_reset_search(void) {
  g_have_path = false;
  g_path.count = 0;
  g_path.truncated = 0;
  g_path_len = 0.0f;
  g_search = (rts_Path_search){0};

  const lum_Vec2 from = rts_navgrid_cell_center(g_start_cx, g_start_cy);
  const lum_Vec2 to   = rts_navgrid_cell_center(g_goal_cx, g_goal_cy);
  if (rts_path_search_begin(&g_nav, from, to, &g_search)) {
    const float dx = g_search.goal.x - g_search.from.x;
    const float dy = g_search.goal.y - g_search.from.y;
    g_straight_len = sqrtf(dx * dx + dy * dy);
  } else {
    g_straight_len = 0.0f;
  }
  demo_refresh_counts();
}

/* 推进一步。【一步 = 定案一个格子】。跑完的那一刻顺手把路径取出来。 */
static void demo_step(void) {
  if (!g_search.active || g_search.finished) return;
  rts_path_search_step(&g_nav, &g_search);

  if (g_search.finished && g_search.found) {
    g_have_path = rts_path_search_take(&g_nav, &g_search, &g_path);
    if (g_have_path) {
      g_path_len = 0.0f;
      lum_Vec2 prev = g_search.from;
      for (uint8_t i = 0; i < g_path.count; ++i) {
        const float dx = g_path.points[i].x - prev.x;
        const float dy = g_path.points[i].y - prev.y;
        g_path_len += sqrtf(dx * dx + dy * dy);
        prev = g_path.points[i];
      }
    }
  }
}

/* ---------------- 编辑 ---------------- */

static void demo_clear_walls(void) {
  for (int32_t i = 0; i < Rts_Nav_Max_Cells; ++i) g_nav.blocked[i] = 0;
  demo_reset_search();
}

static uint32_t g_rng = 20260101u;
static float demo_frand(void) {
  g_rng = g_rng * 1664525u + 1013904223u;
  return (float)((g_rng >> 8) & 0xFFFFFFu) / 16777216.0f;
}

static void demo_random_walls(void) {
  for (int32_t cy = 0; cy < Rts_Nav_Cells_Y; ++cy) {
    for (int32_t cx = 0; cx < Rts_Nav_Cells_X; ++cx) {
      g_nav.blocked[cy * Rts_Nav_Cells_X + cx] = (demo_frand() < 0.22f) ? 1u : 0u;
    }
  }
  /* 起点终点所在格必须是空的，否则一开局就是"起点被埋着"，看不出东西 */
  g_nav.blocked[g_start_cy * Rts_Nav_Cells_X + g_start_cx] = 0;
  g_nav.blocked[g_goal_cy  * Rts_Nav_Cells_X + g_goal_cx]  = 0;
  demo_reset_search();
}

static void demo_set_start(void) {
  int32_t cx = 0, cy = 0;
  if (!demo_mouse_cell(&cx, &cy)) return;
  if (cx == g_start_cx && cy == g_start_cy) return;
  g_start_cx = cx;
  g_start_cy = cy;
  g_nav.blocked[cy * Rts_Nav_Cells_X + cx] = 0;   /* 起点不该被障碍埋着 */
  demo_reset_search();
}

static void demo_set_goal(void) {
  int32_t cx = 0, cy = 0;
  if (!demo_mouse_cell(&cx, &cy)) return;
  if (cx == g_goal_cx && cy == g_goal_cy) return;
  g_goal_cx = cx;
  g_goal_cy = cy;
  g_nav.blocked[cy * Rts_Nav_Cells_X + cx] = 0;
  demo_reset_search();
}

/* 右键：单击切换（画 / 擦），拖动只画。
   之所以分开：拖动时如果每个格子都切换，手指一抖就会画一片再擦一片。 */
static void demo_toggle_wall(void) {
  int32_t cx = 0, cy = 0;
  if (!demo_mouse_cell(&cx, &cy)) return;
  const int32_t i = cy * Rts_Nav_Cells_X + cx;
  if (cx == g_start_cx && cy == g_start_cy) return;   /* 不许把起点/终点堵死 */
  if (cx == g_goal_cx  && cy == g_goal_cy)  return;
  g_nav.blocked[i] = g_nav.blocked[i] ? 0u : 1u;
  demo_reset_search();
}

static void demo_paint_wall(void) {
  int32_t cx = 0, cy = 0;
  if (!demo_mouse_cell(&cx, &cy)) return;
  if ((cx == g_start_cx && cy == g_start_cy) || (cx == g_goal_cx && cy == g_goal_cy)) return;
  const int32_t i = cy * Rts_Nav_Cells_X + cx;
  if (g_nav.blocked[i]) return;
  g_nav.blocked[i] = 1u;
  demo_reset_search();
}

/* ---------------- 自检（写日志，供冒烟验证） ---------------- */

/* 正确性靠复用已测试的模块保证；demo 自己最容易错的是三件事：
   相机缩放、点击往返（屏幕→世界→格子）、以及【增量推进与一次跑完必须等价】。
   三个都做成纯函数，冒烟时从 stderr 就能读到结论。 */
static void demo_self_check(void) {
  lum_Camera2d cam;
  lum_camera2d_init(&cam, (float)Demo_Win_W, (float)Demo_Win_H);
  cam.zoom = Demo_Zoom;
  cam.position = demo_clamp_cam(rts_navgrid_cell_center(g_start_cx, g_start_cy));

  const lum_Vec2 win = lum_vec2_create((float)Demo_Win_W, (float)Demo_Win_H);

  /* ① 视野：可见【跨度】必须正好 12 格 —— 这是"格内放得下 g/h"的前提，写死在这里，
        谁改了 Demo_Cell_Px 都会在这一行被拦下来。
        注意"碰到的格子数"会是 12 或 13：相机没落在格边界上时，两侧各有一列只露出一部分，
        它们也要算进来（不然贴边的那一列没有 g/h 标签），所以 touched 允许 +1。 */
  int32_t vx0 = 0, vy0 = 0, vx1 = 0, vy1 = 0;
  demo_visible_cells(&cam, &vx0, &vy0, &vx1, &vy1);
  const int32_t touched_w = vx1 - vx0 + 1, touched_h = vy1 - vy0 + 1;
  const float span_cells = Demo_View_Span / Rts_Nav_Cell_Size;
  const bool span_ok = (span_cells > (float)Demo_View_Cells - 0.001f) &&
                       (span_cells < (float)Demo_View_Cells + 0.001f);
  const bool touched_ok = (touched_w == Demo_View_Cells || touched_w == Demo_View_Cells + 1) &&
                          (touched_h == Demo_View_Cells || touched_h == Demo_View_Cells + 1);

  /* ② 夹取：视野必须始终落在网格内（否则会出现"网格外的空白"，点击也会落到格子外） */
  const lum_Vec2 lo = demo_clamp_cam(lum_vec2_create(-1.0e6f, -1.0e6f));
  const lum_Vec2 hi = demo_clamp_cam(lum_vec2_create( 1.0e6f,  1.0e6f));
  const bool clamp_ok = (lo.x == Rts_Nav_Origin_X + Demo_Half) &&
                        (lo.y == Rts_Nav_Origin_Y + Demo_Half) &&
                        (hi.x == Rts_Nav_Origin_X + Demo_Grid_Span - Demo_Half) &&
                        (hi.y == Rts_Nav_Origin_Y + Demo_Grid_Span - Demo_Half);

  /* ③ 点击往返：可见区域里的【每一格】格心 → 像素 → 世界 → 格子，必须原样问回来。
        放大之后这一步更容易错（zoom 变了、相机还可能被夹过），所以逐格查而不是抽查一格 */
  uint32_t rt_bad = 0, rt_n = 0;
  for (int32_t cy = vy0; cy <= vy1; ++cy) {
    for (int32_t cx = vx0; cx <= vx1; ++cx) {
      const lum_Vec2 px = lum_camera2d_world_to_screen(&cam, rts_navgrid_cell_center(cx, cy), win);
      const lum_Vec2 back = lum_camera2d_screen_to_world(&cam, px, win);
      int32_t rx = -1, ry = -1;
      ++rt_n;
      if (!rts_navgrid_world_to_cell(back, &rx, &ry) || rx != cx || ry != cy) ++rt_bad;
    }
  }

  Lum_Log_Info("astar_demo camera: zoom=%.5f (want %.5f)  span=%.2f cells (want %d) ok=%d"
               "  touched=%dx%d (want %d or %d) ok=%d  clamp=[%.0f,%.0f] (want [+/-%.0f]) ok=%d"
               "  click_roundtrip_bad=%u/%u",
               (double)cam.zoom, (double)Demo_Zoom, (double)span_cells, (int)Demo_View_Cells,
               (int)span_ok, (int)touched_w, (int)touched_h, (int)Demo_View_Cells,
               (int)Demo_View_Cells + 1, (int)touched_ok,
               (double)lo.x, (double)hi.x, (double)(Demo_Grid_Span * 0.5f - Demo_Half),
               (int)clamp_ok, (unsigned)rt_bad, (unsigned)rt_n);

  /* ① 一条不留缺口的竖墙：必须报"没有路"；② 开一个口：又通，且路径真的穿过那个口 */
  rts_Navgrid t;
  rts_navgrid_init(&t);
  for (int32_t y = 0; y < Rts_Nav_Cells_Y; ++y) t.blocked[y * Rts_Nav_Cells_X + 30] = 1;

  rts_Path p;
  const lum_Vec2 a = rts_navgrid_cell_center(2, 2);
  const lum_Vec2 b = rts_navgrid_cell_center(60, 60);
  const bool sealed_ok = rts_path_find(&t, a, b, &p);

  t.blocked[40 * Rts_Nav_Cells_X + 30] = 0;
  const bool opened_ok = rts_path_find(&t, a, b, &p);
  uint32_t through_gap = 0;
  {
    lum_Vec2 prev = a;
    for (uint8_t i = 0; i < p.count; ++i) {
      const float dx = p.points[i].x - prev.x, dy = p.points[i].y - prev.y;
      const int32_t steps = (int32_t)sqrtf(dx * dx + dy * dy) + 1;
      for (int32_t k = 0; k <= steps; ++k) {
        const float tt = (float)k / (float)steps;
        int32_t cx = -1, cy = -1;
        if (rts_navgrid_world_to_cell(
                lum_vec2_create(prev.x + dx * tt, prev.y + dy * tt), &cx, &cy) &&
            cx == 30 && cy == 40) {
          through_gap = 1;
        }
      }
      prev = p.points[i];
    }
  }
  Lum_Log_Info("astar_demo self-check: sealed=%d opened=%d gap_used=%d (want 0/1/1)",
               (int)sealed_ok, (int)opened_ok, (int)through_gap);

  /* ③ 增量推进必须与一次跑完【逐位一致】—— 这是"手动控制循环"能成立的前提，
        也是本次改动最该被盯住的地方。同时顺便检查 open/closed 两表在中途确实非空。 */
  {
    rts_Navgrid g;
    rts_navgrid_init(&g);
    for (int32_t y = 8; y < 56; ++y) g.blocked[y * Rts_Nav_Cells_X + 30] = 1;
    g.blocked[30 * Rts_Nav_Cells_X + 30] = 0;   /* 留一个口，逼它绕 */

    const lum_Vec2 s0 = rts_navgrid_cell_center(5, 20);
    const lum_Vec2 s1 = rts_navgrid_cell_center(58, 44);

    rts_Path once;
    const bool ok_once = rts_path_find(&g, s0, s1, &once);

    rts_Path_search sh;
    const bool ok_begin = rts_path_search_begin(&g, s0, s1, &sh);
    uint32_t mid_open = 0, mid_closed = 0, steps = 0;
    while (!sh.finished && steps < 100000u) {
      rts_path_search_step(&g, &sh);
      ++steps;
      if (steps == 40u) {          /* 中途取一次两张表的读数 */
        for (int32_t cy = 0; cy < Rts_Nav_Cells_Y; ++cy) {
          for (int32_t cx = 0; cx < Rts_Nav_Cells_X; ++cx) {
            if (rts_path_last_open(cx, cy)) ++mid_open;
            if (rts_path_last_visited(cx, cy)) ++mid_closed;
          }
        }
      }
    }
    rts_Path inc;
    const bool ok_take = rts_path_search_take(&g, &sh, &inc);

    uint32_t same = 0;
    if (ok_once && ok_take && once.count == inc.count) {
      same = 1;
      for (uint8_t i = 0; i < once.count; ++i) {
        if (once.points[i].x != inc.points[i].x || once.points[i].y != inc.points[i].y) { same = 0; break; }
      }
    }
    Lum_Log_Info("astar_demo self-check: incremental_begin=%d steps=%u closed=%u take=%d "
                 "identical_to_oneshot=%d (want 1 / >0 / >0 / 1 / 1)  mid_open=%u mid_closed=%u",
                 (int)ok_begin, (unsigned)steps, (unsigned)sh.expanded, (int)ok_take,
                 (unsigned)same, (unsigned)mid_open, (unsigned)mid_closed);
  }
}

/* ---------------- 绘制 ---------------- */

static void demo_draw_text(const char* text, float px, float py, lum_Vec4 color) {
  if (!g_font) return;
  lum_tr_draw_screen(g_font, text, lum_vec2_create(px, py), Demo_Layer_Hud, color);
}

/* 把满足条件的格子按【同行的连续段】合并成矩形再画，返回画出的矩形数。
   逐格画 4096 个矩形会让绘制命令表爆掉；合并之后通常只剩几十条 ——
   closed list 就是靠这个才能"整片纯色填充"而不炸掉命令表。 */
static uint32_t demo_draw_runs(bool (*pred)(int32_t, int32_t), lum_Vec4 color, int32_t layer) {
  uint32_t rects = 0;
  for (int32_t cy = 0; cy < Rts_Nav_Cells_Y; ++cy) {
    int32_t cx = 0;
    while (cx < Rts_Nav_Cells_X) {
      if (!pred(cx, cy)) { ++cx; continue; }
      int32_t run = 1;
      while (cx + run < Rts_Nav_Cells_X && pred(cx + run, cy)) ++run;
      lum_renderer2d_draw_rect(demo_cell_min(cx, cy),
                               lum_vec2_create((float)run * Rts_Nav_Cell_Size, Rts_Nav_Cell_Size),
                               0.0f, lum_vec2_0(), layer, color);
      ++rects;
      cx += run;
    }
  }
  return rects;
}

/* open list 的表示法：每个 open 格子的【格心】点一个小圆点，返回实际画出的点数。
   半径按像素写、过 demo_px() —— 和线宽一样，draw_circle 的半径也是【世界单位】，
   直接写个"看起来像 2 px"的小数在某些 zoom 下会掉到亚像素、整颗点消失。
   一个点一条命令，但它们的 layer / 材质 / 纹理 / 图元 / VP 全同 → 合批后仍是一次 draw call。 */
static uint32_t demo_draw_open_dots(lum_Vec4 color) {
  const float r = demo_px(Demo_Open_Dot_Px);
  uint32_t dots = 0;
  for (int32_t cy = 0; cy < Rts_Nav_Cells_Y; ++cy) {
    for (int32_t cx = 0; cx < Rts_Nav_Cells_X; ++cx) {
      if (!rts_path_last_open(cx, cy)) continue;
      lum_renderer2d_draw_circle(rts_navgrid_cell_center(cx, cy), r, Demo_Layer_Open, color);
      ++dots;
    }
  }
  return dots;
}

/* closed 格里的两个代价数字：g 贴格子左上角、h 贴右上角，只画视野内的格子。
   两个数都是搜索当时的真实读数（rts_path_last_costs）、一位小数 ——
   直走 1.0 与斜走 1.4 的区别就是 A* 代价的样子，抹成整数就看不出来了。

   走【屏幕空间】的 lum_tr_draw_screen（1:1 光栅化）：世界空间那条路（lum_tr_draw_layer）
   的字号是世界单位、位图也按那个数光栅化，屏幕上再乘 zoom —— 等于把一张小位图放大，
   实测 7 px 字号只剩 0.00 的墨迹覆盖，整串数字等于没画。屏幕空间没有这个放大。
   代价是屏幕坐标要自己算：这里用 g_frame_cam（本帧渲染真正用的那个相机）做世界→屏幕，
   并用等宽字体的字符宽把 h 右对齐。 */
static uint32_t demo_draw_costs(lum_Vec4 cg, lum_Vec4 ch) {
  if (!g_label_font || g_label_char_w <= 0.0f) return 0;

  const lum_Vec2 win = lum_vec2_create((float)Demo_Win_W, (float)Demo_Win_H);
  const float cell_px = Rts_Nav_Cell_Size * Demo_Zoom;

  int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  demo_visible_cells(&g_frame_cam, &x0, &y0, &x1, &y1);

  uint32_t labels = 0;
  char buf[16];
  for (int32_t cy = y0; cy <= y1; ++cy) {
    for (int32_t cx = x0; cx <= x1; ++cx) {
      if (!rts_path_last_visited(cx, cy)) continue;      /* 只标 closed 的格子 */

      float g = 0.0f, h = 0.0f;
      if (!rts_path_last_costs(cx, cy, &g, &h)) continue;

      /* 格子的【左上角】：世界坐标取格子的上边（mn.y + Cell_Size），屏幕 y 向下 */
      const lum_Vec2 mn = demo_cell_min(cx, cy);
      const lum_Vec2 tl = lum_camera2d_world_to_screen(
          &g_frame_cam, lum_vec2_create(mn.x, mn.y + Rts_Nav_Cell_Size), win);
      const float px = tl.x + Demo_Label_Pad_Px;
      const float py = tl.y + Demo_Label_Pad_Px;

      snprintf(buf, sizeof(buf), "%.1f", (double)g);
      lum_tr_draw_screen(g_label_font, buf, lum_vec2_create(px, py), Demo_Layer_Cost, cg);

      snprintf(buf, sizeof(buf), "%.1f", (double)h);
      const float w = g_label_char_w * (float)strlen(buf);
      lum_tr_draw_screen(g_label_font, buf,
                         lum_vec2_create(px + cell_px - 2.0f * Demo_Label_Pad_Px - w, py),
                         Demo_Layer_Cost, ch);
      ++labels;
    }
  }
  return labels;
}

static void demo_draw_cell(int32_t cx, int32_t cy, lum_Vec4 fill, int32_t layer) {
  lum_renderer2d_draw_rect(demo_cell_min(cx, cy),
                           lum_vec2_create(Rts_Nav_Cell_Size, Rts_Nav_Cell_Size),
                           0.0f, lum_vec2_0(), layer, fill);
}

static void sys_start(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ecs; (void)ctx; (void)user_data;
  lum_renderer2d_set_clear_color(lum_vec4_create(0.045f, 0.050f, 0.065f, 1.0f));
  g_font = lum_assets_font("resources/FiraCode-Regular.ttf", 15);
  if (!g_font) Lum_Log_Warn("astar_demo: font load failed, HUD disabled.");

  /* 格内 g/h 用【另一个字号】的同款字体：同一路径不同高度会被资产表按 (path,height) 分别缓存，
     两个字体对象互不影响。这里是【像素】字号（Screen 阶段画），14 px 实测墨迹覆盖 ~0.8 清楚。 */
  g_label_font = lum_assets_font("resources/FiraCode-Regular.ttf", Demo_Label_Font_Px);
  if (!g_label_font) Lum_Log_Warn("astar_demo: label font load failed, g/h labels disabled.");
  /* 等宽字体：量一次"0"的宽度就是每字符进给，右对齐用它乘字符数即可 */
  g_label_char_w = g_label_font ? lum_tr_measure_text(g_label_font, "0").x : 0.0f;

  /* 相机：一屏 12×12 格（80 px/格），开局把视野夹到起点附近；
     之后由 sys_pre 负责跟随/平移。 */
  lum_Camera2d* cam = lum_app_get_camera();
  cam->zoom = Demo_Zoom;
  cam->position = demo_clamp_cam(rts_navgrid_cell_center(g_start_cx, g_start_cy));
  g_frame_cam = *cam;

  demo_self_check();

  rts_navgrid_init(&g_nav);
  demo_random_walls();     /* 开局一片随机障碍，直接能看到绕路 */
  /* 不自动跑：手动控制循环是本 demo 的重点，交给用户按空格/回车 */
}

static void sys_input(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ecs; (void)ctx; (void)user_data;

  if (lum_input_is_key_pressed(Lum_Key_Q) || lum_input_is_key_pressed(Lum_Key_Escape)) {
    lum_app_quit();
    return;
  }
  if (lum_input_is_key_pressed(Lum_Key_Space))   demo_step();
  if (lum_input_is_key_pressed(Lum_Key_Enter)) {
    uint32_t guard = 0;
    while (!g_search.finished && guard < 100000u) { demo_step(); ++guard; }
  }
  if (lum_input_is_key_pressed(Lum_Key_B))       demo_reset_search();
  /* 注意：[A] 已经被"向左平移"占用（WASD），所以自动播放挪到 [P]（play/pause） */
  if (lum_input_is_key_pressed(Lum_Key_P))       g_auto = !g_auto;
  if (lum_input_is_key_pressed(Lum_Key_C))       demo_clear_walls();
  if (lum_input_is_key_pressed(Lum_Key_M))       demo_random_walls();
  if (lum_input_is_key_pressed(Lum_Key_V))       g_show_lists = !g_show_lists;
  if (lum_input_is_key_pressed(Lum_Key_G))       g_show_grid = !g_show_grid;
  if (lum_input_is_key_pressed(Lum_Key_F))       g_follow = !g_follow;

  /* 三个键各管一件事，没有模式：
       左键 = 起点（按下或拖动都跟着走）
       中键 = 终点
       右键 = 障碍：单击切换（画/擦），拖动只画
     "按下"和"按住"都处理，才能既单击又拖动连续编辑。 */
  const bool lmb = lum_input_is_mouse_pressed(Lum_Mouse_Button_Left) ||
                   lum_input_is_mouse_down(Lum_Mouse_Button_Left);
  const bool mmb = lum_input_is_mouse_pressed(Lum_Mouse_Button_Middle) ||
                   lum_input_is_mouse_down(Lum_Mouse_Button_Middle);
  const bool rmb_press = lum_input_is_mouse_pressed(Lum_Mouse_Button_Right);
  const bool rmb_down  = lum_input_is_mouse_down(Lum_Mouse_Button_Right);

  if (demo_mouse_inside()) {
    if (lmb) demo_set_start();
    else if (mmb) demo_set_goal();
    else if (rmb_press) demo_toggle_wall();
    else if (rmb_down) demo_paint_wall();
  }

  /* 自动播放：每帧推进几格，方便看"前沿是怎么扩开的" */
  if (g_auto && !g_search.finished) {
    for (int k = 0; k < Demo_Auto_Steps && !g_search.finished; ++k) demo_step();
  }

  /* 抓本帧渲染真正用的相机：引擎在 Frame_Begin 已经把当时那个值灌给渲染器了，
     而 sys_pre 紧接着就会改它（下一帧才生效）。格内文字在 Screen 阶段画，
     必须在 Pre 之前抓，才能和网格用同一个 VP —— 否则平移时文字会甩出去一帧。 */
  g_frame_cam = *lum_app_get_camera();

  demo_refresh_counts();
}

/*
* 相机：跟随"刚刚定案的那一格"，或者被键盘平移接管。
*
* 为什么放在 Pre 而不是渲染阶段：
*   · 引擎在 Frame_Begin 里把 camera 灌给渲染器，所以这里改的其实是【下一帧】的 VP
*     （一帧延迟，肉眼看不出）；
*   · 渲染阶段的 ctx->dt 恒为 0（数据契约），只有 Input..Commit 里 dt 才是真实帧间隔，
*     平移速度必须按真实 dt 积分才不会随帧率变快变慢。
*
* 平移一旦按下就自动接管（把跟随关掉），[F] 再切回跟随 ——
* 否则"跟随"每帧把相机拽回前沿，手动平移等于没效果。
*/
static void sys_pre(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ecs; (void)user_data;

  lum_Camera2d* cam = lum_app_get_camera();

  float px = 0.0f, py = 0.0f;
  if (lum_input_is_key_down(Lum_Key_A) || lum_input_is_key_down(Lum_Key_Left))  px -= 1.0f;
  if (lum_input_is_key_down(Lum_Key_D) || lum_input_is_key_down(Lum_Key_Right)) px += 1.0f;
  if (lum_input_is_key_down(Lum_Key_W) || lum_input_is_key_down(Lum_Key_Up))    py += 1.0f;  /* 屏幕上方 = 世界 +y */
  if (lum_input_is_key_down(Lum_Key_S) || lum_input_is_key_down(Lum_Key_Down))  py -= 1.0f;

  if (px != 0.0f || py != 0.0f) {
    g_follow = false;
    const float len = sqrtf(px * px + py * py);       /* 斜向不能更快 */
    cam->position.x += (px / len) * Demo_Pan_Speed * ctx->dt;
    cam->position.y += (py / len) * Demo_Pan_Speed * ctx->dt;
  }

  if (g_follow) {
    int32_t cx = g_start_cx, cy = g_start_cy;
    if (g_search.current >= 0) {                     /* 有定案过的格子：盯住它 */
      cx = g_search.current % Rts_Nav_Cells_X;
      cy = g_search.current / Rts_Nav_Cells_X;
    }
    cam->position = demo_clamp_cam(rts_navgrid_cell_center(cx, cy));
  } else {
    cam->position = demo_clamp_cam(cam->position);
  }
  cam->zoom = Demo_Zoom;                             /* 缩放恒定；平移/跟随都不改它 */
}

static void sys_scene(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ecs; (void)ctx; (void)user_data;
  lum_renderer2d_set_blend_mode(Lum_Blend_Alpha);



  /* ① 可通行区底色：整张网格一个大矩形 */
  lum_renderer2d_draw_rect(lum_vec2_create(Rts_Nav_Origin_X, Rts_Nav_Origin_Y),
                           lum_vec2_create(Demo_Grid_Span, Demo_Grid_Span),
                           0.0f, lum_vec2_0(), Demo_Layer_Cell,
                           lum_vec4_create(0.085f, 0.095f, 0.120f, 1.0f));

  /* ② 障碍 */
  demo_draw_runs(demo_is_wall, lum_vec4_create(0.30f, 0.34f, 0.43f, 1.0f), Demo_Layer_Wall);

  /* ③ 网格线 */
  if (g_show_grid) {
    const lum_Vec4 line = lum_vec4_create(0.20f, 0.22f, 0.28f, 0.55f);
    for (int32_t i = 0; i <= Rts_Nav_Cells_X; ++i) {
      const float x = Rts_Nav_Origin_X + (float)i * Rts_Nav_Cell_Size;
      lum_renderer2d_draw_line(lum_vec2_create(x, Rts_Nav_Origin_Y),
                               lum_vec2_create(x, Rts_Nav_Origin_Y + Demo_Grid_Span),
                               demo_px(1.0f), Demo_Layer_Grid, line);
    }
    for (int32_t i = 0; i <= Rts_Nav_Cells_Y; ++i) {
      const float y = Rts_Nav_Origin_Y + (float)i * Rts_Nav_Cell_Size;
      lum_renderer2d_draw_line(lum_vec2_create(Rts_Nav_Origin_X, y),
                               lum_vec2_create(Rts_Nav_Origin_X + Demo_Grid_Span, y),
                               demo_px(1.0f), Demo_Layer_Grid, line);
    }
  }

  /* ④ 两张表：closed = 一整片半透明蓝色纯色填充，open = 每个格心一颗琥珀色小圆点。
        画的是搜索的真实状态（只读查询），不是示意。
        两个计数器是"可视化真的发出了几何"的直接读数（0 就说明哪里不对）：
        closed 记【合并后的矩形数】（远小于格子数才说明按行合并生效了），
        open 记【实际画出的点数】。 */
  if (g_show_lists) {
    g_closed_runs = demo_draw_runs(rts_path_last_visited,
                                   lum_vec4_create(0.30f, 0.62f, 1.00f, 0.30f),
                                   Demo_Layer_Closed);
    g_open_dots = demo_draw_open_dots(lum_vec4_create(1.00f, 0.80f, 0.25f, 1.00f));
  } else {
    g_closed_runs = 0;
    g_open_dots = 0;
  }

  /* ⑤ 路径：拉直之后的路点折线 + 路点上的小圆点。
        路点少（通常 2~5 个）正是"拉直"在起作用。只有搜索真的找到路才画。 */
  if (g_have_path && g_path.count > 0) {
    const lum_Vec4 pc = lum_vec4_create(0.45f, 1.00f, 0.55f, 0.95f);
    lum_Vec2 prev = g_search.from;
    for (uint8_t i = 0; i < g_path.count; ++i) {
      lum_renderer2d_draw_line(prev, g_path.points[i], demo_px(3.0f), Demo_Layer_Path, pc);
      prev = g_path.points[i];
    }
    for (uint8_t i = 0; i + 1u < g_path.count; ++i) {
      lum_renderer2d_draw_circle(g_path.points[i], demo_px(4.0f), Demo_Layer_Path,
                                 lum_vec4_create(1.00f, 0.85f, 0.35f, 0.95f));
    }
  }

  /* ⑥ 起点 / 终点标记 */
  demo_draw_cell(g_start_cx, g_start_cy, lum_vec4_create(0.25f, 0.90f, 0.45f, 1.0f), Demo_Layer_Mark);
  demo_draw_cell(g_goal_cx,  g_goal_cy,  lum_vec4_create(1.00f, 0.30f, 0.45f, 1.0f), Demo_Layer_Mark);
}

/* HUD 行收集器：先拼好全部行，再按【实测文字宽度】画背板。
   之前背板是写死的 452×206，结果行一变长就超出背板、行一变少就比内容高 40 px；
   改成实测之后，改文案/改字号都不用再回来量一遍。 */
#define Demo_Hud_Max_Lines 12
typedef struct demo_Hud {
  char     text[Demo_Hud_Max_Lines][200];
  lum_Vec4 color[Demo_Hud_Max_Lines];
  float    step[Demo_Hud_Max_Lines];      /* 到下一行的行距 */
  int      count;
} demo_Hud;

static void demo_hud_add(demo_Hud* hud, const char* text, float step, lum_Vec4 color) {
  if (hud->count >= Demo_Hud_Max_Lines) return;
  snprintf(hud->text[hud->count], sizeof(hud->text[0]), "%s", text);
  hud->step[hud->count] = step;
  hud->color[hud->count] = color;
  ++hud->count;
}

static void sys_screen(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ecs; (void)ctx; (void)user_data;
  if (!g_font) return;

  const lum_Vec4 white = lum_vec4_create(0.92f, 0.94f, 0.98f, 1.0f);
  const lum_Vec4 dim   = lum_vec4_create(0.62f, 0.67f, 0.76f, 1.0f);
  const lum_Vec4 hot   = lum_vec4_create(1.00f, 0.78f, 0.35f, 1.0f);
  const lum_Vec4 good  = lum_vec4_create(0.45f, 1.00f, 0.55f, 1.0f);
  const lum_Vec4 bad   = lum_vec4_create(1.00f, 0.42f, 0.42f, 1.0f);
  const lum_Vec4 blue  = lum_vec4_create(0.55f, 0.75f, 1.00f, 1.0f);

  demo_Hud hud;
  memset(&hud, 0, sizeof(hud));

  /* 格内 g/h 数字：画在 closed 格子的左上/右上角（图层低于背板，被背板压住是应该的）。
     放在 HUD 之前先把计数刷出来，好让下面那行读数显示的是本帧真实值。 */
  if (g_show_lists) {
    g_cost_labels = demo_draw_costs(lum_vec4_create(0.88f, 0.94f, 1.00f, 1.0f),   /* g：冷白 */
                                    lum_vec4_create(1.00f, 0.86f, 0.55f, 1.0f));  /* h：暖黄 */
  } else {
    g_cost_labels = 0;
  }

  char line[200];

  demo_hud_add(&hud, "A* grid pathfinding demo   (manual loop control)", 22.0f, white);

  snprintf(line, sizeof(line), "start (%d,%d)   goal (%d,%d)",
           (int)g_start_cx, (int)g_start_cy, (int)g_goal_cx, (int)g_goal_cy);
  demo_hud_add(&hud, line, 20.0f, hot);

  if (!g_search.active) {
    demo_hud_add(&hud, "search not started (start cell outside grid?)", 20.0f, bad);
  } else if (!g_search.finished) {
    snprintf(line, sizeof(line), "state: SEARCHING   steps %u", (unsigned)g_search.steps);
    demo_hud_add(&hud, line, 20.0f, hot);
  } else if (g_search.found) {
    snprintf(line, sizeof(line), "state: FOUND   steps %u", (unsigned)g_search.steps);
    demo_hud_add(&hud, line, 20.0f, good);
  } else {
    snprintf(line, sizeof(line), "state: NO PATH   steps %u (open list drained)", (unsigned)g_search.steps);
    demo_hud_add(&hud, line, 20.0f, bad);
  }

  /* 两张表是重点，颜色和画面上的填充/点一致。括号里的 runs 是"按行合并后真正发出去
     几个矩形"（远小于格子数才对）；labels 是视野内真正画了 g/h 的格数。
     刻意拆成两行：一行塞下四个数会超出背板宽度（背板虽会自动跟着变宽，但越宽越挡格子）。 */
  snprintf(line, sizeof(line), "closed %u cells (%u runs)", (unsigned)g_closed_count,
           (unsigned)g_closed_runs);
  demo_hud_add(&hud, line, 18.0f, blue);
  snprintf(line, sizeof(line), "open %u dots   g/h in %u cells", (unsigned)g_open_dots,
           (unsigned)g_cost_labels);
  demo_hud_add(&hud, line, 20.0f, blue);

  if (g_have_path) {
    snprintf(line, sizeof(line), "path: %u wp   len %.0f / straight %.0f (x%.2f)",
             (unsigned)g_path.count, (double)g_path_len, (double)g_straight_len,
             (g_straight_len > 0.0f) ? (double)(g_path_len / g_straight_len) : 0.0);
  } else {
    snprintf(line, sizeof(line), "path: not found yet");
  }
  demo_hud_add(&hud, line, 20.0f, good);

  demo_hud_add(&hud, "LMB start   RMB wall (toggles)   MMB goal", 18.0f, dim);
  snprintf(line, sizeof(line), "[SPACE]step  [ENTER]run  [B]reset  [P]auto=%s",
           g_auto ? "on " : "off");
  demo_hud_add(&hud, line, 18.0f, dim);
  snprintf(line, sizeof(line), "[C]clear [M]random [V]lists=%s [G]grid=%s [Q]quit",
           g_show_lists ? "on " : "off", g_show_grid ? "on " : "off");
  demo_hud_add(&hud, line, 18.0f, dim);
  snprintf(line, sizeof(line), "[WASD/arrows]pan  [F]follow=%s   g TL / h TR",
           g_follow ? "on " : "off");
  demo_hud_add(&hud, line, 18.0f, dim);

  /* 背板：宽 = 最宽一行 + 左右各 10 px；高 = 最后一行基线 + 下留白。
     它必须独占一层（Demo_Layer_Panel < Demo_Layer_Hud），哪怕"先记录背板、再记录文字"
     看起来就够了：渲染器的排序键是
       layer → view_projection → has_scissor → scissor → material → texture → primitive
             → blend → sequence
     sequence（记录顺序）是【最后一项】；lum_tr_draw_screen() 画字时会临时把 VP 换成
     Y 向上的像素 VP，与背板用的 Y 向下 VP 不同 → 这些状态字节先把顺序定死了，
     sequence 根本轮不到。实测：同层时先记录的背板反而最后画、盖在文字上（0.88 的 alpha
     把整段 HUD 压成灰影），改用两层后文字恢复正常。 */
  float y_end = 22.0f, panel_w = 0.0f;
  for (int i = 0; i < hud.count; ++i) {
    const float w = lum_tr_measure_text(g_font, hud.text[i]).x;
    if (w > panel_w) panel_w = w;
    y_end += hud.step[i];
  }
  lum_renderer2d_draw_rect(lum_vec2_create(10.0f, 10.0f),
                           lum_vec2_create(panel_w + 20.0f, y_end + 6.0f),
                           0.0f, lum_vec2_0(), Demo_Layer_Panel,
                           lum_vec4_create(0.03f, 0.04f, 0.06f, 0.88f));

  float y = 22.0f;
  for (int i = 0; i < hud.count; ++i) {
    demo_draw_text(hud.text[i], 20.0f, y, hud.color[i]);
    y += hud.step[i];
  }
}

int main(void) {
  lum_App_desc desc;
  lum_app_init_desc(&desc);
  desc.log_level = Lum_Log_Level_Info;
  desc.title = "Luminous A* Demo";
  desc.width = Demo_Win_W;
  desc.height = Demo_Win_H;

  lum_app_add_system(Lum_Stage_Start,  0, sys_start,  NULL);
  lum_app_add_system(Lum_Stage_Input,  0, sys_input,  NULL);
  lum_app_add_system(Lum_Stage_Pre,    0, sys_pre,    NULL);   /* 相机跟随/平移：要真实 dt */
  lum_app_add_system(Lum_Stage_Scene,  0, sys_scene,  NULL);
  lum_app_add_system(Lum_Stage_Screen, 0, sys_screen, NULL);

  lum_app_run(&desc);
  return 0;
}
