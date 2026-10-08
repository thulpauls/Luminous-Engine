/* glimmer_check —— glimmer 3D 部分的无窗口回归检查（对应 docs/design/glimmer3d.md 的 M1a）。
 *
 * 只测**纯 CPU** 的东西：3D 数学、camera3d、以及 mesh 的几何生成/包围盒两半。
 * 碰 GL 的那一半（lum_mesh_create / renderer3d）由窗口探针负责 —— 分工见 lum_mesh.h 的注释。
 *
 * 为什么要有它：3D 数学是"错了也能编过、甚至也能画出来"的那类代码
 * （投影参数传反、up 与视线共线、手性搞错，画面会以一种"看起来有点像但不对"的方式坏掉）。
 * 所以这里用可手算的判据钉住：近/远平面映射到 NDC ∓1、90° 视场下 ±1 处正好落在 NDC 边界、
 * 世界→NDC→世界往返一致、旋转矩阵的手性、转置的乘积反序、叉积的右手性。
 *
 * 运行：build/glimmer_check.exe（退出码 0 = 全过）
 */

#include "glimmer/glimmer_version.h"
#include "glimmer/lum_math2d.h"
#include "glimmer/lum_camera.h"
#include "glimmer/lum_mesh.h"
#include "glimmer/lum_material.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static int g_fail = 0;

static void check(const char* what, uint32_t got, uint32_t want) {
  const int ok = (got == want);
  if (!ok) ++g_fail;
  printf("  %-52s got %u  want %u   %s\n", what, (unsigned)got, (unsigned)want, ok ? "ok" : "★FAIL★");
}

static void checkf(const char* what, float got, float want, float tol) {
  const int ok = (fabsf(got - want) <= tol);
  if (!ok) ++g_fail;
  printf("  %-52s got %.4f  want %.4f (±%.4f)   %s\n", what, (double)got, (double)want,
         (double)tol, ok ? "ok" : "★FAIL★");
}

/* 世界点 → NDC（透视除法）。w <= 0 表示点在相机背后，返回 NaN 以便断言失败 */
static lum_Vec3 to_ndc(lum_Mat4 vp, lum_Vec3 world) {
  const lum_Vec4 clip = lum_mat4_mul_vec4(vp, lum_vec4_create(world.x, world.y, world.z, 1.0f));
  if (fabsf(clip.w) < 1e-9f) return lum_vec3_create(NAN, NAN, NAN);
  return lum_vec3_create(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w);
}

int main(void) {
  printf("[1] 向量：叉积的右手性与反交换律\n");
  {
    const lum_Vec3 x = lum_vec3_create(1.0f, 0.0f, 0.0f);
    const lum_Vec3 y = lum_vec3_create(0.0f, 1.0f, 0.0f);
    const lum_Vec3 z = lum_vec3_create(0.0f, 0.0f, 1.0f);
    checkf("x × y = +z.x", lum_vec3_cross(x, y).x, 0.0f, 1e-6f);
    checkf("x × y = +z.z", lum_vec3_cross(x, y).z, 1.0f, 1e-6f);
    checkf("y × z = +x.x", lum_vec3_cross(y, z).x, 1.0f, 1e-6f);
    checkf("z × x = +y.y", lum_vec3_cross(z, x).y, 1.0f, 1e-6f);
    const lum_Vec3 a = lum_vec3_create(1.0f, 2.0f, 3.0f);
    const lum_Vec3 b = lum_vec3_create(-4.0f, 5.0f, 0.5f);
    const lum_Vec3 ab = lum_vec3_cross(a, b);
    const lum_Vec3 ba = lum_vec3_cross(b, a);
    checkf("a × b = -(b × a) : x", ab.x, -ba.x, 1e-5f);
    checkf("a × b = -(b × a) : y", ab.y, -ba.y, 1e-5f);
    checkf("a × b = -(b × a) : z", ab.z, -ba.z, 1e-5f);
    checkf("(a × b) ⊥ a", lum_vec3_dot(ab, a), 0.0f, 1e-4f);
    checkf("(a × b) ⊥ b", lum_vec3_dot(ab, b), 0.0f, 1e-4f);
  }

  printf("\n[2] 旋转矩阵的手性（右手系、列主序）\n");
  {
    const lum_Vec4 z = lum_vec4_create(0.0f, 0.0f, 1.0f, 1.0f);
    const lum_Vec4 rx = lum_mat4_mul_vec4(lum_mat4_rotate_x(Lum_Pi * 0.5f), z);
    checkf("rotate_x(+90°) 把 +Z 转到 -Y", rx.y, -1.0f, 1e-5f);
    const lum_Vec4 ry = lum_mat4_mul_vec4(lum_mat4_rotate_y(Lum_Pi * 0.5f), z);
    checkf("rotate_y(+90°) 把 +Z 转到 +X", ry.x, 1.0f, 1e-5f);
    const lum_Vec4 rz = lum_mat4_mul_vec4(lum_mat4_rotate_z(Lum_Pi * 0.5f),
                                          lum_vec4_create(1.0f, 0.0f, 0.0f, 1.0f));
    checkf("rotate_z(+90°) 把 +X 转到 +Y", rz.y, 1.0f, 1e-5f);

    /* 转置的乘积反序：(AB)ᵀ = BᵀAᵀ */
    const lum_Mat4 a = lum_mat4_rotate_x(0.3f);
    const lum_Mat4 b = lum_mat4_rotate_y(-0.7f);
    const lum_Mat4 lhs = lum_mat4_transpose(lum_mat4_mul(a, b));
    const lum_Mat4 rhs = lum_mat4_mul(lum_mat4_transpose(b), lum_mat4_transpose(a));
    int same = 1;
    for (int i = 0; i < 16; ++i) if (fabsf(lhs.m[i] - rhs.m[i]) > 1e-5f) same = 0;
    check("(AB)ᵀ == BᵀAᵀ", (uint32_t)same, 1u);
  }

  printf("\n[3] 透视投影：可手算的判据（fov 90°、aspect 1、near 0.1、far 100）\n");
  {
    lum_Camera3d cam;
    lum_camera3d_init(&cam, 1.0f);
    cam.position = lum_vec3_create(0.0f, 0.0f, 5.0f);
    cam.target = lum_vec3_0();
    cam.fov_y_rad = Lum_Pi * 0.5f;
    cam.near_plane = 0.1f;
    cam.far_plane = 100.0f;

    const lum_Mat4 vp = lum_camera3d_view_projection(&cam);

    const lum_Vec3 near_pt = to_ndc(vp, lum_vec3_create(0.0f, 0.0f, 5.0f - cam.near_plane));
    checkf("近平面上的点 → z_ndc = -1", near_pt.z, -1.0f, 1e-3f);
    const lum_Vec3 far_pt = to_ndc(vp, lum_vec3_create(0.0f, 0.0f, 5.0f - cam.far_plane));
    checkf("远平面上的点 → z_ndc = +1", far_pt.z, 1.0f, 1e-3f);

    const lum_Vec3 centre = to_ndc(vp, lum_vec3_0());
    checkf("注视点在画面正中：x_ndc", centre.x, 0.0f, 1e-4f);
    checkf("注视点在画面正中：y_ndc", centre.y, 0.0f, 1e-4f);

    /* 90° 视场 + aspect 1 ⇒ 相机前方 1 单位处，|x| = 1 或 |y| = 1 正好落在 NDC 边界 */
    const lum_Vec3 right_edge = to_ndc(vp, lum_vec3_create(1.0f, 0.0f, 4.0f));
    checkf("前方 1 单位处 x = +1 → x_ndc = +1", right_edge.x, 1.0f, 1e-4f);
    const lum_Vec3 top_edge = to_ndc(vp, lum_vec3_create(0.0f, 1.0f, 4.0f));
    checkf("前方 1 单位处 y = +1 → y_ndc = +1", top_edge.y, 1.0f, 1e-4f);

    /* 世界 → NDC → 世界：往返一致（这是"投影/视图没有互相抵消错误"的强判据） */
    const lum_Vec3 world = lum_vec3_create(0.7f, -0.3f, 0.4f);
    const lum_Vec3 ndc = to_ndc(vp, world);
    const lum_Vec4 back = lum_mat4_mul_vec4(lum_mat4_inv(vp),
                                            lum_vec4_create(ndc.x, ndc.y, ndc.z, 1.0f));
    checkf("往返一致：x", back.x / back.w, world.x, 1e-3f);
    checkf("往返一致：y", back.y / back.w, world.y, 1e-3f);
    checkf("往返一致：z", back.z / back.w, world.z, 1e-3f);
  }

  printf("\n[4] 投影/视图的非法参数兜底（不许出 NaN）\n");
  {
    const lum_Mat4 bad_fov = lum_mat4_perspective(0.0f, 1.0f, 0.1f, 10.0f);
    int finite = 1;
    for (int i = 0; i < 16; ++i) if (!isfinite(bad_fov.m[i])) finite = 0;
    check("fov = 0 → 有限矩阵（不产生 NaN）", (uint32_t)finite, 1u);

    const lum_Mat4 bad_far = lum_mat4_perspective(1.0f, 1.0f, 10.0f, 1.0f);   /* near > far */
    finite = 1;
    for (int i = 0; i < 16; ++i) if (!isfinite(bad_far.m[i])) finite = 0;
    check("near > far → 有限矩阵", (uint32_t)finite, 1u);

    const lum_Mat4 degenerate = lum_mat4_look_at(lum_vec3_0(), lum_vec3_0(),
                                                 lum_vec3_create(0.0f, 1.0f, 0.0f));
    finite = 1;
    for (int i = 0; i < 16; ++i) if (!isfinite(degenerate.m[i])) finite = 0;
    check("eye == target → 有限矩阵", (uint32_t)finite, 1u);

    const lum_Mat4 parallel = lum_mat4_look_at(lum_vec3_create(0.0f, 5.0f, 0.0f), lum_vec3_0(),
                                               lum_vec3_create(0.0f, 1.0f, 0.0f));
    finite = 1;
    for (int i = 0; i < 16; ++i) if (!isfinite(parallel.m[i])) finite = 0;
    check("up 与视线平行 → 有限矩阵", (uint32_t)finite, 1u);
  }

  printf("\n[5] camera3d：默认值 / aspect / 轨道 / 前方向\n");
  {
    lum_Camera3d cam;
    lum_camera3d_init(&cam, 2.0f);
    checkf("默认 aspect 用参数", cam.aspect, 2.0f, 1e-6f);
    checkf("默认 near", cam.near_plane, 0.1f, 1e-6f);
    checkf("默认 far", cam.far_plane, 100.0f, 1e-6f);

    lum_camera3d_set_viewport(&cam, 1920.0f, 1080.0f);
    checkf("set_viewport(1920,1080) → 16/9", cam.aspect, 1920.0f / 1080.0f, 1e-6f);
    const float before = cam.aspect;
    lum_camera3d_set_viewport(&cam, 0.0f, 0.0f);          /* 高 DPI 首帧可能拿到 0 */
    checkf("尺寸为 0 时不动 aspect（不许出 NaN/Inf）", cam.aspect, before, 1e-6f);

    lum_camera3d_orbit(&cam, 0.0f, 0.0f, 5.0f);
    checkf("orbit(yaw=0,pitch=0,d=5) 的 z = target.z + 5", cam.position.z, 5.0f, 1e-5f);
    checkf("orbit 的 x", cam.position.x, 0.0f, 1e-5f);
    checkf("orbit 的 y", cam.position.y, 0.0f, 1e-5f);

    lum_camera3d_orbit(&cam, 0.0f, Lum_Pi, 5.0f);          /* pitch 明显越界 → 应被夹到极点之内 */
    const lum_Vec3 fwd = lum_camera3d_forward(&cam);
    /* 判据要精确：夹取后的 pitch 是 π/2 − 1e-3，于是 position.y 应当正好是 d·sin(那个角)。
       注意别写成"|fwd.y| < 0.999"——夹到 1e-3 之内时 |fwd.y| ≈ 0.9999995，那种阈值会假失败。 */
    checkf("position.y ≈ d·sin(π/2 − 1e-3)（夹取生效）",
           cam.position.y, 5.0f * sinf(Lum_Pi * 0.5f - 1e-3f), 1e-4f);
    check("夹取后 |fwd.y| 严格 < 1（没有落到退化极点）",
          (uint32_t)(fabsf(fwd.y) < 1.0f), 1u);
    const lum_Mat4 v = lum_camera3d_view(&cam);
    int not_identity = 0;
    for (int i = 0; i < 16; ++i) if (fabsf(v.m[i] - lum_mat4_identity().m[i]) > 1e-4f) not_identity = 1;
    check("夹住后视图矩阵不是退化的单位矩阵", (uint32_t)not_identity, 1u);

    checkf("前方向归一化长度 = 1", lum_vec3_len(fwd), 1.0f, 1e-5f);
  }

  printf("\n[6] mesh 的纯 CPU 一半：立方体生成与包围盒\n");
  {
    lum_Vertex3d verts[Lum_Mesh_Box_Vertex_Count];
    uint32_t idx[Lum_Mesh_Box_Index_Count];
    const lum_Vec3 half = lum_vec3_create(1.0f, 2.0f, 3.0f);
    lum_mesh_build_box(verts, idx, half);

    check("顶点数 = 24（每面独立法线，不是共享 8 个）", Lum_Mesh_Box_Vertex_Count, 24u);
    check("索引数 = 36", Lum_Mesh_Box_Index_Count, 36u);

    uint32_t out_of_range = 0;
    for (uint32_t i = 0; i < Lum_Mesh_Box_Index_Count; ++i) {
      if (idx[i] >= Lum_Mesh_Box_Vertex_Count) ++out_of_range;
    }
    check("索引全部落在顶点范围内", out_of_range, 0u);

    uint32_t bad_normal = 0;
    for (uint32_t i = 0; i < Lum_Mesh_Box_Vertex_Count; ++i) {
      if (fabsf(lum_vec3_len(verts[i].normal) - 1.0f) > 1e-5f) ++bad_normal;
    }
    check("每个法线都是单位长度", bad_normal, 0u);

    /* 每个面的 4 个顶点必须落在该面法线对应的那个平面上 */
    uint32_t off_plane = 0;
    for (uint32_t f = 0; f < 6u; ++f) {
      const lum_Vec3 n = verts[f * 4u].normal;
      for (uint32_t v = 0; v < 4u; ++v) {
        const lum_Vec3 p = verts[f * 4u + v].position;
        const float d = lum_vec3_dot(p, n);
        const float want = fabsf(n.x) * half.x + fabsf(n.y) * half.y + fabsf(n.z) * half.z;
        if (fabsf(d - want) > 1e-5f) ++off_plane;
      }
    }
    check("6 个面各自贴在正确的平面上", off_plane, 0u);

    /* 24 个顶点其实只有 8 个不同位置（每角被 3 个面共用） */
    uint32_t distinct = 0;
    for (uint32_t i = 0; i < Lum_Mesh_Box_Vertex_Count; ++i) {
      int seen = 0;
      for (uint32_t j = 0; j < i; ++j) {
        if (lum_vec3_eq(verts[i].position, verts[j].position)) { seen = 1; break; }
      }
      if (!seen) ++distinct;
    }
    check("不同位置只有 8 个角", distinct, 8u);

    lum_Vec3 mn, mx;
    lum_mesh_bounds(verts, Lum_Mesh_Box_Vertex_Count, &mn, &mx);
    checkf("包围盒 min.x", mn.x, -1.0f, 1e-6f);
    checkf("包围盒 min.y", mn.y, -2.0f, 1e-6f);
    checkf("包围盒 min.z", mn.z, -3.0f, 1e-6f);
    checkf("包围盒 max.x", mx.x, 1.0f, 1e-6f);
    checkf("包围盒 max.y", mx.y, 2.0f, 1e-6f);
    checkf("包围盒 max.z", mx.z, 3.0f, 1e-6f);

    lum_mesh_bounds(verts, 0u, &mn, &mx);
    checkf("空集合包围盒归零：min.x", mn.x, 0.0f, 0.0f);
    checkf("空集合包围盒归零：max.z", mx.z, 0.0f, 0.0f);
  }

  printf("\n[7] glimmer 版本号（与引擎分开计数，见 docs/versioning.md）\n");
  {
    check("当前阶段是 prototype", Lum_Glimmer_Version_Stage_Current,
          (uint32_t)Lum_Glimmer_Version_Stage_Prototype);
    check("迭代次数 = 2", Lum_Glimmer_Version_Iteration, 2u);
    check("显示串是 \"prototype 2\"", (uint32_t)(strcmp(Lum_Glimmer_Version_String, "prototype 2") == 0), 1u);
    /* 验证【编码规则】而不是把当前迭代次数写死：阶段在高 8 位、迭代次数在低 8 位。
       （原先写成 Encode(prototype, 1)，升级到 prototype 2 时就假失败了一次。）*/
    check("编码值 = 阶段*256 + 迭代（阶段在高位）",
          (uint32_t)(Lum_Glimmer_Version ==
                     (uint32_t)Lum_Glimmer_Version_Stage_Current * 256u + Lum_Glimmer_Version_Iteration),
          1u);
    /* 阶段是【有序数值】而不是纯标签 —— 这是本次版本规范修正的关键一点，
       它让 #if 能表达"至少 beta"这类判断（旧方案做不到）。 */
    check("阶段数值有序：prototype < alpha < beta < rc < release",
          (uint32_t)(Lum_Glimmer_Version_Stage_Prototype < Lum_Glimmer_Version_Stage_Alpha &&
                     Lum_Glimmer_Version_Stage_Alpha     < Lum_Glimmer_Version_Stage_Beta &&
                     Lum_Glimmer_Version_Stage_Beta      < Lum_Glimmer_Version_Stage_Rc &&
                     Lum_Glimmer_Version_Stage_Rc        < Lum_Glimmer_Version_Stage_Release),
          1u);
  }

  printf("\n[8] 视图矩阵本身：正交归一 / 手性 / 落位（M2 补齐的缺口）\n");
  {
    /* 取一个"斜着"的相机（三个轴都不与世界轴对齐），这样任何符号/转置错误都会现形 */
    const lum_Vec3 eye    = lum_vec3_create(3.0f, 2.0f, 5.0f);
    const lum_Vec3 target = lum_vec3_create(1.0f, 1.0f, 1.0f);
    const lum_Mat4 v = lum_mat4_look_at(eye, target, lum_vec3_create(0.0f, 1.0f, 0.0f));

    /* 视图矩阵的三"行"就是相机的 右/上/后（这正是"求逆 = 转置"的体现） */
    const lum_Vec3 r0 = lum_vec3_create(v.m[0], v.m[4], v.m[8]);
    const lum_Vec3 r1 = lum_vec3_create(v.m[1], v.m[5], v.m[9]);
    const lum_Vec3 r2 = lum_vec3_create(v.m[2], v.m[6], v.m[10]);

    checkf("行 0 单位长", lum_vec3_len(r0), 1.0f, 1e-5f);
    checkf("行 1 单位长", lum_vec3_len(r1), 1.0f, 1e-5f);
    checkf("行 2 单位长", lum_vec3_len(r2), 1.0f, 1e-5f);
    checkf("行 0 · 行 1 = 0", lum_vec3_dot(r0, r1), 0.0f, 1e-5f);
    checkf("行 0 · 行 2 = 0", lum_vec3_dot(r0, r2), 0.0f, 1e-5f);
    checkf("行 1 · 行 2 = 0", lum_vec3_dot(r1, r2), 0.0f, 1e-5f);
    /* 行列式必须是 +1：若是 -1 说明叉积顺序写反、世界被镜像了
       （镜像后画面依然"像一个场景"，肉眼几乎看不出 —— 只有这条断言能抓） */
    checkf("det(旋转部分) = +1（没有镜像）",
           lum_vec3_dot(r0, lum_vec3_cross(r1, r2)), 1.0f, 1e-4f);

    const lum_Vec4 eye_cam = lum_mat4_mul_vec4(v, lum_vec4_create(eye.x, eye.y, eye.z, 1.0f));
    checkf("眼睛落在相机原点：x", eye_cam.x, 0.0f, 1e-4f);
    checkf("眼睛落在相机原点：y", eye_cam.y, 0.0f, 1e-4f);
    checkf("眼睛落在相机原点：z", eye_cam.z, 0.0f, 1e-4f);

    const float dist = lum_vec3_len(lum_vec3_sub(target, eye));
    const lum_Vec4 tgt_cam = lum_mat4_mul_vec4(v, lum_vec4_create(target.x, target.y, target.z, 1.0f));
    checkf("注视点落在 (0,0,-距离)：x", tgt_cam.x, 0.0f, 1e-4f);
    checkf("注视点落在 (0,0,-距离)：y", tgt_cam.y, 0.0f, 1e-4f);
    checkf("注视点落在 (0,0,-距离)：z", tgt_cam.z, -dist, 1e-4f);
  }

  printf("\n[9] look_at 对 up 的加固（M2：先归一化再判退化）\n");
  {
    /* up 量级极小但方向合法 —— 不该被当成退化（旧写法会把 |f×up| 算小而误拒） */
    const lum_Mat4 tiny = lum_mat4_look_at(lum_vec3_create(0.0f, 0.0f, 5.0f), lum_vec3_0(),
                                           lum_vec3_create(0.0f, 1e-8f, 0.0f));
    check("up = (0,1e-8,0) 仍被接受（不是退化）",
          (uint32_t)(lum_vec3_dot(lum_vec3_create(tiny.m[1], tiny.m[5], tiny.m[9]),
                                  lum_vec3_create(0.0f, 1.0f, 0.0f)) > 0.999f), 1u);

    /* 夹角极小的"几乎平行"必须被拒（返回单位矩阵），否则基方向是噪声 */
    const float tilt = 1e-7f;
    const lum_Mat4 near_parallel =
      lum_mat4_look_at(lum_vec3_0(), lum_vec3_create(0.0f, 0.0f, -1.0f),
                       lum_vec3_create(0.0f, tilt, -1.0f));
    int is_identity = 1;
    const lum_Mat4 id = lum_mat4_identity();
    for (int i = 0; i < 16; ++i) if (fabsf(near_parallel.m[i] - id.m[i]) > 1e-6f) is_identity = 0;
    check("夹角 1e-7 弧度 → 判定退化并返回单位矩阵", (uint32_t)is_identity, 1u);
  }

  printf("\n[10] M3：顶点布局与 uv / lum_Material3d\n");
  {
    /* 顶点布局是 GL 属性设置的依据（步长与偏移），改错一处画面就散架 —— 用断言钉住 */
    check("顶点步长 = 32 字节（位置 12 + 法线 12 + uv 8）", (uint32_t)sizeof(lum_Vertex3d), 32u);
    check("position 偏移 = 0",  (uint32_t)offsetof(lum_Vertex3d, position), 0u);
    check("normal 偏移 = 12",   (uint32_t)offsetof(lum_Vertex3d, normal), 12u);
    check("uv 偏移 = 24",       (uint32_t)offsetof(lum_Vertex3d, uv), 24u);

    lum_Vertex3d verts[Lum_Mesh_Box_Vertex_Count];
    uint32_t idx[Lum_Mesh_Box_Index_Count];
    lum_mesh_build_box(verts, idx, lum_vec3_create(1.0f, 1.0f, 1.0f));

    /* 每个面的 4 个 uv 必须正好是单位正方形的四个角（既不漏角、也不重叠） */
    uint32_t bad_face = 0, out_of_range = 0;
    for (uint32_t f = 0; f < 6u; ++f) {
      int seen[4] = { 0, 0, 0, 0 };   /* (0,0) (1,0) (1,1) (0,1) */
      for (uint32_t v = 0; v < 4u; ++v) {
        const lum_Vec2 uv = verts[f * 4u + v].uv;
        if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) ++out_of_range;
        const int corner = ((uv.x > 0.5f) ? 1 : 0) + ((uv.y > 0.5f) ? 2 : 0);
        seen[corner] = 1;
      }
      for (int c = 0; c < 4; ++c) if (!seen[c]) ++bad_face;
    }
    check("每个面的 4 个 uv 正好覆盖单位正方形的四个角", bad_face, 0u);
    check("uv 全部落在 [0,1]", out_of_range, 0u);

    lum_Material3d mat;
    lum_material3d_init(&mat);
    check("材质 init 后已初始化", (uint32_t)mat.initialized, 1u);
    check("材质默认无贴图（albedo = NULL）", (uint32_t)(mat.albedo == NULL), 1u);
    checkf("材质默认基色为白：r", mat.color.x, 1.0f, 0.0f);
    checkf("材质默认 uv 不平铺", mat.uv_scale.x, 1.0f, 0.0f);

    lum_Material3d red = lum_material3d_create(lum_vec4_create(1.0f, 0.0f, 0.0f, 1.0f));
    checkf("create(color) 带上了颜色：r", red.color.x, 1.0f, 0.0f);
    checkf("create(color) 带上了颜色：g", red.color.y, 0.0f, 0.0f);

    lum_material3d_set_uv_scale(&red, lum_vec2_create(4.0f, 2.0f));
    checkf("set_uv_scale 生效", red.uv_scale.x, 4.0f, 0.0f);
  }

  printf("\n==== %s（失败 %d 项）====\n", g_fail == 0 ? "全部通过" : "有失败", g_fail);
  return g_fail == 0 ? 0 : 1;
}
