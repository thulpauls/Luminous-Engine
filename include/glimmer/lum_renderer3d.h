#ifndef Lum_Renderer3d_H
#define Lum_Renderer3d_H

#include "glimmer/lum_math2d.h"
#include "glimmer/lum_camera.h"
#include "glimmer/lum_mesh.h"
#include "glimmer/lum_material.h"
#include <stdbool.h>
#include <stdint.h>

/*
* glimmer 的 3D 渲染器（M1 最简可用版 + M2 实例化，见 docs/design/glimmer3d.md）。
*
* 与 lum_renderer2d 的关系：**两条独立的提交路径，只共享 glimmer 的 GL 包装件**
* （shader / texture / render_target）。刻意【不】复用 2D 那套延迟命令表 ——
* 它的合批键是为"CPU 烘好的四边形汤"设计的（layer → VP → 材质 → 纹理 → 混合 → 记录序），
* 而 3D 的批处理方向是实例化 + 材质排序、每物体带自己的 model 矩阵，塞进那个键只会两边都别扭。
*
* ## 只有一条绘制路径（M2 起）
*
* 模型矩阵不再走 uniform，而是**按实例存在顶点属性里**（location 2..5，每实例一个 mat4，
* 属性除数 divisor = 1）。于是：
*
*     lum_renderer3d_draw_mesh(mesh, model, color)
*         ≡ lum_renderer3d_draw_instanced(mesh, &model, 1, color)
*
* `draw_mesh` 就是"画 1 个实例"，**没有第二条代码路径** —— 实例化这条路每帧都在被使用，
* 不可能悄悄腐化（M1 的深度 A/B 示例与探针跑的也是它）。
*
* 一次 draw_instanced = **一次 glDrawElementsInstanced**，与实例个数无关。
* 实例矩阵每帧上传进渲染器自有的缓冲区（容量按需增长）。
*
* ## 批次单位（M3 起）
*
*      （网格，材质） → 一次 draw_instanced(N 个矩阵)
*
* 也就是说：**draw call 数 = 网格数 × 材质数，与实例总数无关**。同一个材质下的所有物体
* 一起走一次绘制；想减少 draw call 就要减少"网格×材质"的组合数，而不是减少物体数。
* 材质没给 albedo 时渲染器绑内置的 1×1 白纹理 —— 着色器里永远采样，不存在"有没有贴图"的分支。
*
* ## M2 的取舍（"以后再做"的清单）
*   · **朴素 uniform**：`u_view_projection` / `u_color` 仍按名字设置（内部 glGetUniformLocation）——
*     M5 换 UBO + 位置缓存
*   · **无剔除 / 无背面剔除 / 无混合**：M4 / M3
*   · **法线用 mat3(model) 变换**：只对"无缩放或等比缩放"正确；非等比缩放的
*     法线矩阵（inv-transpose）留到 M3 与材质一起做
*   · **实例缓冲整体重传**：够用；GPU 侧持久化 / 间接绘制不在 GL 3.3 的能力集内
*
* 深度：init 时打开 GL_DEPTH_TEST（GL_LESS、允许写深度），clear() 同时清颜色与深度。
* set_depth_test(false) 是给"证明深度真的在起作用"的 A/B 用的（示例里按 [D] 切换）。
*/

typedef struct lum_Renderer3d_stats {
  uint32_t draw_calls;   /* 本帧 glDrawElements* 次数 */
  uint32_t meshes;       /* 本帧网格提交次数（draw_mesh / draw_instanced 各算 1 次） */
  uint32_t instances;    /* 本帧提交的实例总数（draw_mesh 记 1） */
  uint32_t triangles;    /* 本帧提交的三角形总数 */
} lum_Renderer3d_stats;

int  lum_renderer3d_init(void);
void lum_renderer3d_shutdown(void);
bool lum_renderer3d_is_initialized(void);

/* 视口（像素、左下原点）。独立于 2D 渲染器的视口状态，直接下发 glViewport */
void lum_renderer3d_set_viewport(int x, int y, int w, int h);

/* 每帧：重置统计。**不动深度状态** —— 深度是"模式"不是"每帧瞬态"，
   帧初强行恢复默认会把调用方刚设的 set_depth_test(false) 冲掉（示例的 [D] 开关就靠它）。
   默认值（测试开、写深度）由 init 设定，之后保持到有人改它。 */
void lum_renderer3d_begin_frame(void);

/* 清颜色 + 深度（M1 不清模板，模板缓冲还没用到） */
void lum_renderer3d_clear(lum_Vec4 color);

/* 本帧的相机：绘制时用它算 VP。传 NULL 则退回单位矩阵（画面会明显不对，便于发现漏设） */
void lum_renderer3d_set_camera(const lum_Camera3d* camera);

void lum_renderer3d_set_depth_test(bool enabled);
bool lum_renderer3d_depth_test_enabled(void);

/* 画一个网格（= 画一个实例）。material 可以为 NULL（退到"白色、不贴图"的兜底材质） */
void lum_renderer3d_draw_mesh(const lum_Mesh* mesh, const lum_Material3d* material, lum_Mat4 model);

/* 实例化绘制：同一个 mesh 用同一个材质画 count 次，第 i 个实例用 models[i] 作为模型矩阵。
   · models 必须是 count 个连续排列的 lum_Mat4（列主序，与 lum_Mat4 一致）
   · count == 0 或 models == NULL 直接返回（不产生 draw call）
   · material 为 NULL 或未初始化 → 用"白色、不贴图"的兜底材质
   · 整次调用只产生【一次】draw call；实例矩阵上传进渲染器内部缓冲区，容量按需增长
   · 用法：CPU 先把各实例矩阵按材质分好组，每组调一次 */
void lum_renderer3d_draw_instanced(const lum_Mesh* mesh, const lum_Material3d* material,
                                   const lum_Mat4* models, uint32_t count);

/* 每帧收尾。立即提交模型下无事可做（统计已定格）—— 保留这个函数是给
   将来换延迟提交时的接口位置 */
void lum_renderer3d_end_frame(void);

const lum_Renderer3d_stats* lum_renderer3d_get_stats(void);
uint32_t lum_renderer3d_get_draw_call_count(void);
uint32_t lum_renderer3d_get_triangle_count(void);
uint32_t lum_renderer3d_get_instance_count(void);

#endif //Lum_Renderer3d_H
