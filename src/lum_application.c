#include "lum_application.h"
#include "lum_window.h"
#include "lum_input.h"
#include "lum_time.h"
#include "lum_renderer2d.h"
#include "lum_render_target.h"
#include "lum_scene.h"
#include "lum_assets.h"
#include "effects/lum_effects.h"
#include "containers/lum_vector.h"
#include "lum_assert.h"
#include <string.h>

/* ============================================================================
 * 阶段状态机：注册表 + 驱动
 * ========================================================================== */

typedef struct lum_System_entry {
  lum_system_fn fn;
  void* user_data;
  int priority;
} lum_System_entry;

/* 每阶段一张有序表（全局单例：引擎默认单世界）。
   注册低频、执行每帧，所以注册时插入排序、执行时纯遍历。 */
static lum_Vector g_systems[Lum_Stage_Count];
static bool       g_flow_initialized = false;
static lum_Stage  g_current_stage = Lum_Stage_Count;
static uint64_t   g_step_counter = 0;

typedef struct lum_Application {
    bool running;
    uint32_t last_framebuffer_w, last_framebuffer_h;
    lum_Camera2d camera;
    lum_Ecs* ecs;
    float delta;
} lum_Application;

static lum_Application g_application;

/* Init 阶段已完成的步数：Shutdown 按它逆序回滚，只拆已经起来的 */
static uint32_t g_init_level = 0;

static const char* kStageName[Lum_Stage_Count] = {
  "Init", "Start",
  "Frame_Begin", "Input", "Pre", "Transform", "Post", "Late", "Commit",
  "View_Begin", "Scene", "Scene_Overlay", "Post_Process", "Screen", "Frame_End",
  "Stop", "Shutdown"
};

/* 引擎独占阶段：由引擎实现，用户注册会被拒绝 */
static bool lum__stage_is_engine_only(lum_Stage stage) {
  switch (stage) {
    case Lum_Stage_Init:
    case Lum_Stage_Frame_Begin:
    case Lum_Stage_Transform:
    case Lum_Stage_Commit:
    case Lum_Stage_View_Begin:
    case Lum_Stage_Frame_End:
    case Lum_Stage_Shutdown:
      return true;
    default:
      return false;
  }
}

/* 按 priority 升序插入；同值追加在后，保持注册序 */
static int lum__systems_insert(lum_Vector* vec, lum_System_entry entry) {
  if (!vec) return 0;
  uint32_t n = lum_vector_size(vec);
  uint32_t pos = n;
  for (uint32_t i = 0; i < n; ++i) {
    const lum_System_entry* it = (const lum_System_entry*)lum_vector_get_const(vec, i);
    if (it->priority > entry.priority) { pos = i; break; }
  }
  if (!lum_vector_reserve(vec, n + 1u)) return 0;

  unsigned char* base = vec->data;
  if (pos < n) {
    memmove(base + (size_t)(pos + 1u) * sizeof(lum_System_entry),
            base + (size_t)pos * sizeof(lum_System_entry),
            (size_t)(n - pos) * sizeof(lum_System_entry));
  }
  *(lum_System_entry*)(base + (size_t)pos * sizeof(lum_System_entry)) = entry;
  vec->size = n + 1u;
  return 1;
}

static void lum__sys_frame_begin(lum_Ecs* ecs, lum_Context* ctx, void* user_data);
static void lum__sys_frame_end(lum_Ecs* ecs, lum_Context* ctx, void* user_data);

static void lum__flow_ensure_init(void) {
  if (g_flow_initialized) return;
  g_flow_initialized = true;   /* 先置位：下面的 add_system 会回调进来 */

  for (int i = 0; i < Lum_Stage_Count; ++i) {
    if (g_systems[i].elem_size == 0) {
      if (!lum_vector_init(&g_systems[i], sizeof(lum_System_entry))) return;
    } else {
      lum_vector_clear(&g_systems[i]);
    }
  }
  g_current_stage = Lum_Stage_Count;
  g_step_counter = 0;

  /* 引擎内置系统：场景层自己的（Transform / Commit / View_Begin / sprite） */
  lum_scene_register_engine_systems();

  /* 帧边界：应用层自己的。
     必须走【引擎内部】入口 —— Frame_Begin / Frame_End 属于引擎独占阶段，
     公开的 lum_app_add_system 会直接拒绝注册（!engine 分支），
     结果是整帧不清屏、不 flush、不 swap，窗口全白且没有任何报错。 */
  lum_flow_add_engine_system(Lum_Stage_Frame_Begin, 0, lum__sys_frame_begin, NULL);
  lum_flow_add_engine_system(Lum_Stage_Frame_End,   0, lum__sys_frame_end,   NULL);
}

static int lum__add_system_impl(lum_Stage stage, int priority, lum_system_fn fn, void* user_data, bool engine) {
  if (!fn) return 0;
  if (stage < 0 || stage >= Lum_Stage_Count) return 0;
  if (!engine && lum__stage_is_engine_only(stage)) {
    Lum_Log_Warn("flow: stage '%s' is engine-owned, registration ignored.", lum_stage_name(stage));
    return 0;
  }
  lum__flow_ensure_init();
  if (g_systems[stage].elem_size == 0) return 0;

  lum_System_entry entry = { fn, user_data, priority };
  return lum__systems_insert(&g_systems[stage], entry);
}

int lum_app_add_system(lum_Stage stage, int priority, lum_system_fn fn, void* user_data) {
  return lum__add_system_impl(stage, priority, fn, user_data, false);
}

int lum_flow_add_engine_system(lum_Stage stage, int priority, lum_system_fn fn, void* user_data) {
  return lum__add_system_impl(stage, priority, fn, user_data, true);
}

void lum_app_reset_systems(void) {
  g_flow_initialized = false;
  lum__flow_ensure_init();
}

static void lum__run_stage_systems(lum_Ecs* ecs, lum_Context* ctx, lum_Stage stage) {
  g_current_stage = stage;
  ctx->stage = stage;

  /* 每轮重新取指针：系统内若再注册会触发 realloc */
  for (uint32_t i = 0; i < lum_vector_size(&g_systems[stage]); ++i) {
    const lum_System_entry* entry = (const lum_System_entry*)lum_vector_get_const(&g_systems[stage], i);
    entry->fn(ecs, ctx, entry->user_data);
  }
}

lum_Stage lum_app_stage(void) { return g_current_stage; }

const char* lum_stage_name(lum_Stage stage) {
  if (stage < 0 || stage >= Lum_Stage_Count) return "None";
  return kStageName[stage];
}

bool lum_stage_is_registrable(lum_Stage stage) {
  if (stage < 0 || stage >= Lum_Stage_Count) return false;
  return !lum__stage_is_engine_only(stage);
}

uint32_t lum_app_system_count(lum_Stage stage) {
  if (stage < 0 || stage >= Lum_Stage_Count) return 0;
  return lum_vector_size(&g_systems[stage]);
}

/* ============================================================================
 * 驱动
 * ========================================================================== */

void lum_flow_run_stage(lum_Ecs* ecs, lum_Stage stage) {
  if (!ecs || stage < 0 || stage >= Lum_Stage_Count) return;
  lum__flow_ensure_init();

  static lum_Context ctx;
  ctx.step = g_step_counter;
  ctx.dt = 0.0f;
  ctx.alpha = 0.0f;
  ctx.view = NULL;
  lum__run_stage_systems(ecs, &ctx, stage);
  g_current_stage = Lum_Stage_Count;
}

void lum_flow_run_steps(lum_Ecs* ecs, uint32_t count, float dt) {
  if (!ecs || count == 0u) return;
  lum__flow_ensure_init();

  static lum_Context ctx;
  ctx.alpha = 0.0f;
  ctx.view = NULL;

  for (uint32_t i = 0; i < count; ++i) {
    ctx.step = g_step_counter;
    ctx.dt = dt;

    lum__run_stage_systems(ecs, &ctx, Lum_Stage_Input);
    lum__run_stage_systems(ecs, &ctx, Lum_Stage_Pre);
    lum__run_stage_systems(ecs, &ctx, Lum_Stage_Transform);   /* 结构点 */
    lum__run_stage_systems(ecs, &ctx, Lum_Stage_Post);
    lum__run_stage_systems(ecs, &ctx, Lum_Stage_Late);
    lum__run_stage_systems(ecs, &ctx, Lum_Stage_Commit);      /* 结构点 */

    g_step_counter++;
  }
  g_current_stage = Lum_Stage_Count;
}

void lum_flow_run_frame(lum_Ecs* ecs) {
  if (!ecs) return;
  lum__flow_ensure_init();

  static lum_Context ctx;
  ctx.step = g_step_counter;
  ctx.dt = 0.0f;
  ctx.alpha = 0.0f;
  ctx.view = NULL;

  lum__run_stage_systems(ecs, &ctx, Lum_Stage_Frame_Begin);

  const int view_count = lum_view_count(ecs);
  if (view_count == 0) {
    /* 隐式视图：沿用调用方已设好的 VP 与视口 */
    lum__run_stage_systems(ecs, &ctx, Lum_Stage_Scene);
    lum__run_stage_systems(ecs, &ctx, Lum_Stage_Scene_Overlay);
  } else {
    int px = 0, py = 0, pw = 0, ph = 0;
    lum_renderer2d_get_viewport(&px, &py, &pw, &ph);
    lum_Mat4 saved_vp = lum_renderer2d_view_projection();

    for (int i = 0; i < view_count; ++i) {
      ctx.view = lum_view_get(ecs, i);
      lum__run_stage_systems(ecs, &ctx, Lum_Stage_View_Begin);
      lum__run_stage_systems(ecs, &ctx, Lum_Stage_Scene);
      lum__run_stage_systems(ecs, &ctx, Lum_Stage_Scene_Overlay);
    }
    ctx.view = NULL;

    /* 还原：后续阶段与调用方看到的 GL 状态与进入时一致 */
    lum_rt_bind_screen();
    lum_renderer2d_set_viewport(px, py, pw, ph);
    lum_renderer2d_set_view_projection(saved_vp);
  }

  lum__run_stage_systems(ecs, &ctx, Lum_Stage_Post_Process);

  /* Screen：像素空间、左上原点（与 UI 一致） */
  {
    int sw = 0, sh = 0;
    lum_renderer2d_get_viewport(NULL, NULL, &sw, &sh);
    lum_Mat4 saved = lum_renderer2d_view_projection();
    lum_renderer2d_set_view_projection(lum_mat4_ortho(0.0f, (float)sw, (float)sh, 0.0f, -1.0f, 1.0f));
    lum__run_stage_systems(ecs, &ctx, Lum_Stage_Screen);
    lum_renderer2d_set_view_projection(saved);
  }

  lum__run_stage_systems(ecs, &ctx, Lum_Stage_Frame_End);

  g_current_stage = Lum_Stage_Count;
}

/* ============================================================================
 * 帧边界（引擎系统）
 * ========================================================================== */

static void lum__app_sync_viewport(void) {
  uint32_t framebuffer_w = lum_window_get_framebuffer_width();
  uint32_t framebuffer_h = lum_window_get_framebuffer_height();
  if (framebuffer_w == g_application.last_framebuffer_w &&
      framebuffer_h == g_application.last_framebuffer_h) return;

  g_application.last_framebuffer_w = framebuffer_w;
  g_application.last_framebuffer_h = framebuffer_h;

  lum_renderer2d_set_viewport(0, 0, (int)framebuffer_w, (int)framebuffer_h);
  lum_camera2d_set_viewport(&g_application.camera, (float)framebuffer_w, (float)framebuffer_h);
}

static void lum__sys_frame_begin(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ctx; (void)user_data;

  lum__app_sync_viewport();
  lum_renderer2d_set_camera(&g_application.camera);   /* 环境 VP = app 相机 */
  lum_renderer2d_begin_frame();

  /* 有视图时清屏交给各视图的 View_Begin；无视图时兜底全屏清 */
  if (lum_view_count(ecs) == 0) lum_renderer2d_clear();
}

static void lum__sys_frame_end(lum_Ecs* ecs, lum_Context* ctx, void* user_data) {
  (void)ecs; (void)ctx; (void)user_data;
  lum_renderer2d_end_frame();
  lum_window_swap_buffers();
}

/* ============================================================================
 * 应用配置与运行
 * ========================================================================== */

int lum_app_init_desc(lum_App_desc* desc) {
    memset(desc, 0, sizeof(*desc));
    desc->title = "No title";
    desc->width = 800;
    desc->height = 600;
    desc->max_fps = 0;
    desc->log_level = Lum_Log_Level_Info;
    desc->vsync = true;
    desc->resizable = true;
    desc->clear_color = lum_vec4_0();
    desc->initialized = true;
    return 1;
}

/* Init 阶段：引擎子系统按序起来，g_init_level 记录进度供 Shutdown 逆序回滚 */
static int lum__stage_init(const lum_App_desc* desc) {
    g_init_level = 0;

    if (!lum_log_init()) return 0;
    lum_log_set_min_level(desc->log_level);
    g_init_level = 1;

    if (!lum_window_init(desc->title, desc->width, desc->height)) {
        Lum_Log_Fatal("app: window init failed.");
        return 0;
    }
    lum_window_set_vsync(desc->vsync);
    g_init_level = 2;

    if (!lum_input_init()) {
        Lum_Log_Fatal("app: input init failed.");
        return 0;
    }
    g_init_level = 3;

    if (!lum_time_init()) {
        Lum_Log_Fatal("app: time init failed.");
        return 0;
    }
    if (desc->max_fps > 0) lum_time_set_max_fps(desc->max_fps);
    g_init_level = 4;

    if (!lum_renderer2d_init(desc->width, desc->height)) {
        Lum_Log_Fatal("app: renderer2d init failed.");
        return 0;
    }
    lum_renderer2d_set_clear_color(desc->clear_color);
    lum_renderer2d_set_blend_mode(Lum_Blend_Alpha);
    g_init_level = 5;

    if (!lum_effects_init()) {
        Lum_Log_Fatal("app: effects init failed.");
        return 0;
    }
    g_init_level = 6;

    lum_camera2d_init(&g_application.camera, (float)desc->width, (float)desc->height);

    g_application.ecs = lum_ecs_create();
    if (!g_application.ecs || !lum_scene_init(g_application.ecs)) {
        Lum_Log_Fatal("app: ecs/scene init failed.");
        return 0;
    }
    g_init_level = 7;

    if (!lum_assets_init()) {
        Lum_Log_Fatal("app: assets init failed.");
        return 0;
    }
    g_init_level = 8;

    g_application.last_framebuffer_w = lum_window_get_framebuffer_width();
    g_application.last_framebuffer_h = lum_window_get_framebuffer_height();
    return 1;
}

/* Shutdown 阶段：只拆已经起来的（逆序） */
static void lum__stage_shutdown(void) {
    if (g_init_level >= 8) lum_assets_shutdown();      /* 资源是 GL 对象：先于渲染器拆 */
    if (g_init_level >= 7) { lum_ecs_destroy(g_application.ecs); g_application.ecs = NULL; }
    if (g_init_level >= 6) lum_effects_shutdown();
    if (g_init_level >= 5) lum_renderer2d_shutdown();
    if (g_init_level >= 4) lum_time_shutdown();
    if (g_init_level >= 3) lum_input_shutdown();
    if (g_init_level >= 2) lum_window_shutdown();
    if (g_init_level >= 1) lum_log_shutdown();
    g_init_level = 0;
}

int lum_app_run(const lum_App_desc* desc) {
    Lum_Assert(desc);
    if (!desc->initialized) return 0;

    lum_App_desc d = *desc;

    memset(&g_application, 0, sizeof(g_application));
    g_application.running = true;

    lum__flow_ensure_init();

    /* ── Init ── */
    g_current_stage = Lum_Stage_Init;
    if (!lum__stage_init(&d)) {
        g_current_stage = Lum_Stage_Shutdown;
        lum__stage_shutdown();
        g_current_stage = Lum_Stage_Count;
        return 0;
    }

    /* ── Start ── */
    {
        static lum_Context ctx;
        ctx.step = 0; ctx.dt = 0.0f; ctx.alpha = 0.0f; ctx.view = NULL;
        lum__run_stage_systems(g_application.ecs, &ctx, Lum_Stage_Start);
    }

    /* ── 帧循环 ── */
    while (lum_window_is_open() && g_application.running) {
        lum_time_begin_frame();
        lum_window_poll_events();
        lum_input_update();
        lum_time_update();
        g_application.delta = lum_time_get_delta();

        lum_flow_run_steps(g_application.ecs, 1u, g_application.delta);   /* 每帧 1 步 */
        if (!g_application.running) break;
        lum_flow_run_frame(g_application.ecs);

        lum_time_end_frame();
    }

    /* ── Stop ── */
    {
        static lum_Context ctx;
        ctx.step = g_step_counter; ctx.dt = 0.0f; ctx.alpha = 0.0f; ctx.view = NULL;
        lum__run_stage_systems(g_application.ecs, &ctx, Lum_Stage_Stop);
    }

    /* ── Shutdown ── */
    g_current_stage = Lum_Stage_Shutdown;
    lum__stage_shutdown();
    g_current_stage = Lum_Stage_Count;
    return 0;
}

void lum_app_quit(void) {
    g_application.running = false;
    lum_window_set_should_close(true);
}

struct lum_Camera2d* lum_app_get_camera(void) { return &g_application.camera; }
lum_Ecs* lum_app_get_ecs(void) { return g_application.ecs; }
float lum_app_get_delta(void) { return g_application.delta; }

lum_Vec2 lum_app_screen_to_world(lum_Vec2 screen) {
  return lum_camera2d_screen_to_world(&g_application.camera, screen,
    lum_vec2_create((float)lum_window_get_width(), (float)lum_window_get_height()));
}

lum_Vec2 lum_app_world_to_screen(lum_Vec2 world) {
  return lum_camera2d_world_to_screen(&g_application.camera, world,
    lum_vec2_create((float)lum_window_get_width(), (float)lum_window_get_height()));
}
