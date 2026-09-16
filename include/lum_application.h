#ifndef Lum_Application_H
#define Lum_Application_H

#include <stdint.h>
#include <stdbool.h>
#include "lum_math2d.h"
#include "lum_log.h"
#include "lum_camera.h"
#include "lum_ecs.h"

struct lum_View;   /* 状态机只透传视图指针，不需要完整类型（避免与 scene 头循环包含） */

/*
* 应用层 = 一个阶段状态机。
*
* 引擎只维护两样东西：
*   ① 一个「当前阶段」（lum_Stage）—— 唯一的编排状态
*   ② 每个阶段一张按 priority 有序的系统表
*
* 驱动顺序写死在引擎里（不是注册出来的）：
*
*   Init
*   Start
*   ┌─ 每帧 ─────────────────────────────────────────────────────────┐
*   │  Frame_Begin                                                   │
*   │  Input → Pre → Transform → Post → Late → Commit   （每帧 1 步） │
*   │  ┌─ 每个视图 ──────────────────────────────┐                    │
*   │  │  View_Begin → Scene → Scene_Overlay     │                    │
*   │  └─────────────────────────────────────────┘                    │
*   │  Post_Process → Screen → Frame_End                             │
*   └────────────────────────────────────────────────────────────────┘
*   Stop
*   Shutdown
*
* 未注册任何视图时退化为「沿用调用方已设好的 VP」，Scene/Scene_Overlay 每帧各跑一次。
*
* 数据契约（单向）：
*   Pre 及之前：只能写 local（position/rotation/scale）；读 world 拿到的是上一帧的
*   Transform  ：引擎独占，local → world
*   Post 及之后：读 world 是本帧最新的；写 local 要到下一步才反映到 world
*/

typedef enum lum_Stage {
  /* ═══ ① 启动段（每次运行各一次） ═══ */
  Lum_Stage_Init = 0,    /* 引擎独占：日志→窗口→输入→时间→渲染器→特效→ECS→场景→资源 */
  Lum_Stage_Start,       /* 用户：建场景、载资源 */

  /* ═══ ② 帧循环段 ═══ */
  Lum_Stage_Frame_Begin, /* 引擎独占：begin_frame；无视图时兜底清屏 */
  Lum_Stage_Input,       /* 用户：读输入 → 写意图 */
  Lum_Stage_Pre,         /* 用户：写 local 与游戏状态（内置 script 系统在此，priority -1000） */
  Lum_Stage_Transform,   /* 引擎独占：local → world */
  Lum_Stage_Post,        /* 用户：读 world —— 物理/命中/相机/派生缓存 */
  Lum_Stage_Late,        /* 用户：收尾、生成销毁请求 */
  Lum_Stage_Commit,      /* 引擎独占：墓碑回收 */
  Lum_Stage_View_Begin,  /* 引擎独占：绑目标 / 视口 / 区域清屏 / 相机 VP（每视图一次） */
  Lum_Stage_Scene,       /* 用户：世界内容（每视图一次；内置 sprite 系统在此，priority -1000） */
  Lum_Stage_Scene_Overlay, /* 用户：世界叠加（每视图一次） */
  Lum_Stage_Post_Process,  /* 用户：全屏后处理（每帧一次） */
  Lum_Stage_Screen,        /* 用户：像素空间 UI/HUD（每帧一次） */
  Lum_Stage_Frame_End,     /* 引擎独占：end_frame + swap_buffers */

  /* ═══ ③ 关闭段（每次运行各一次） ═══ */
  Lum_Stage_Stop,        /* 用户：保存、释放自建资源 */
  Lum_Stage_Shutdown,    /* 引擎独占：按 Init 逆序拆除 */

  Lum_Stage_Count
} lum_Stage;

/* 帧上下文：回调期间有效，**不要保存指针**（单例，每次调用复用同一块内存）。
   view 仅 View_Begin / Scene / Scene_Overlay 非空 */
typedef struct lum_Context {
  lum_Stage              stage;   /* 当前阶段 —— 永远有效 */
  uint64_t               step;    /* 逻辑步序号 */
  float                  dt;      /* 本步时长；非步进/渲染阶段为 0 */
  float                  alpha;   /* 渲染插值系数（预留，当前恒为 0） */
  const struct lum_View* view;    /* 当前视图；其余阶段为 NULL */
} lum_Context;

/* 所有阶段的统一回调签名（逻辑与渲染不再分家） */
typedef void (*lum_system_fn)(lum_Ecs* ecs, lum_Context* ctx, void* user_data);

/* 注册系统：priority 升序执行（同值按注册序）。注册到引擎独占阶段会被拒绝并告警。
   注册表是全局单例（引擎默认单世界），因此可在 lum_app_run 之前注册而无需 ECS */
int lum_app_add_system(lum_Stage stage, int priority, lum_system_fn fn, void* user_data);

/* 引擎内部：注册引擎独占阶段的系统（与上者的唯一区别是不拒绝引擎独占阶段）。
   场景层等内置实现用；用户代码不应调用 */
int lum_flow_add_engine_system(lum_Stage stage, int priority, lum_system_fn fn, void* user_data);

/* 清空全部阶段系统表并重新注册引擎内置系统（供重复 run / 单测隔离用） */
void lum_app_reset_systems(void);

lum_Stage   lum_app_stage(void);                  /* 当前阶段 —— 引擎唯一的编排状态 */
const char* lum_stage_name(lum_Stage stage);      /* 日志/调试用 */
bool        lum_stage_is_registrable(lum_Stage stage);
uint32_t    lum_app_system_count(lum_Stage stage);

/* ---------- 应用配置与运行 ---------- */

typedef struct lum_App_desc {
    bool initialized;

    const char* title;
    uint32_t width;
    uint32_t height;
    bool vsync;
    bool resizable;

    lum_Vec4 clear_color;

    lum_Log_level log_level;

    uint32_t max_fps;
} lum_App_desc;

int  lum_app_init_desc(lum_App_desc* desc);        /* 只填默认配置，不收回调 */
int  lum_app_run(const lum_App_desc* desc);        /* 跑状态机直到退出 */
void lum_app_quit(void);                           /* 请求退出：本帧结束后走 Stop → Shutdown */

/* ---------- 阶段驱动（测试 / 嵌入式用，不碰窗口与 GL） ---------- */

void lum_flow_run_stage(lum_Ecs* ecs, lum_Stage stage);          /* 单阶段 */
void lum_flow_run_steps(lum_Ecs* ecs, uint32_t count, float dt); /* Input..Commit ×count */
void lum_flow_run_frame(lum_Ecs* ecs);                           /* 帧循环段（需 GL，不含 swap） */

/* ---------- 查询 ---------- */

struct lum_Camera2d* lum_app_get_camera(void);
lum_Ecs* lum_app_get_ecs(void);

float lum_app_get_delta(void);

lum_Vec2 lum_app_screen_to_world(lum_Vec2 screen);
lum_Vec2 lum_app_world_to_screen(lum_Vec2 world);

#endif //Lum_Application_H
