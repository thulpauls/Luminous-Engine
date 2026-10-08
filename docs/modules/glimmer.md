# glimmer（基础平台）

**glimmer = 数学层 + 平台层 + 渲染器**，合成一个**能独立运行的基础平台**；
Luminous 的其余模块（ecs / scene / serial / assets / collision / ui / effects /
components / application）全部建在它上面。

两段式分层与三个构建目标：

```
glad ──> glimmer ──> Luminous
         （基础平台）  （引擎高层）
```

| 组 | 模块 | 头文件 | 职责 |
|---|---|---|---|
| 数学 | `lum_math2d` | `glimmer/lum_math2d.h` | Vec2/3/4、Mat4、Rect、浮点谓词（引擎头闭包为**空**） |
| 版本 | `Lum_Glimmer_Version` | `glimmer/glimmer_version.h` | **平台自身的版本号**（`<阶段> <迭代次数>`，当前 `prototype 2`），**与引擎分开计数**；引擎兼容哪个 glimmer 版本由 `include/lum_version.h` 声明。见 [versioning.md](../versioning.md) |
| 数学 | `lum_transform` | `glimmer/lum_transform.h` | `lum_Transform2d` → 矩阵 |
| 平台 | `lum_log` | `glimmer/lum_log.h` | 分级日志（级别 / 颜色 / 文件落盘） |
| 平台 | `lum_assert` | `glimmer/lum_assert.h` | `Lum_Assert`（仅头，Debug 构建真断言） |
| 平台 | `lum_filesystem` | `glimmer/lum_filesystem.h` | 读文件（着色器 / 字体 / 像素） |
| 平台 | `lum_window` | `glimmer/lum_window.h` | GLFW 窗口 + GL 上下文 + 帧缓冲尺寸 |
| 平台 | `lum_input` | `glimmer/lum_input.h` | 键盘/鼠标/滚轮状态与字符回调 |
| 平台 | `lum_input_codes` | `glimmer/lum_input_codes.h` | 键码枚举 |
| 平台 | `lum_time` | `glimmer/lum_time.h` | 帧计时、delta、max_fps |
| 渲染 | `lum_shader` | `glimmer/lum_shader.h` | GLSL 程序编译与 uniform 设置 |
| 渲染 | `lum_texture` | `glimmer/lum_texture.h` | 纹理创建与子矩形上传；**sRGB 标志（M3）**：颜色贴图标它，数据贴图**绝不能**标 |
| 渲染 | `lum_atlas` | `glimmer/lum_atlas.h` | 货架式图集分配器（只算位置） |
| 渲染 | `lum_camera` | `glimmer/lum_camera.h` | **两个相机同住一个文件**：`lum_Camera2d`（正交，精灵/HUD/屏幕↔世界换算）+ `lum_Camera3d`（透视，3D 渲染用） |
| 渲染 | `lum_material` | `glimmer/lum_material.h` | 两种材质同住一个文件：`lum_Material`（通用容器：shader + 4 贴图 + 16 命名参数）+ **`lum_Material3d`（M3：albedo + 基色 + uv 平铺，3D 渲染器的固定语义材质）** |
| 渲染 | `lum_render_target` | `glimmer/lum_render_target.h` | 离屏渲染目标（FBO） |
| 渲染 | `lum_renderer2d` | `glimmer/lum_renderer2d.h` | **延迟批处理渲染器**（单例，唯一 2D 绘制入口） |
| 渲染 | `lum_text_renderer` | `glimmer/lum_text_renderer.h` | 位图字体栅格化 + 图集（**仅 ASCII**） |
| **3D** | `lum_renderer3d` | `glimmer/lum_renderer3d.h` | **3D 渲染器（M1/M2）**：立即提交、深度测试、内置方向光着色器；**实例化**（模型矩阵走实例属性）+ **材质与贴图（M3）**：批次单位 = （网格，材质），draw call 数 = 网格数 × 材质数、与实例总数无关 |
| 3D | `lum_mesh` | `glimmer/lum_mesh.h` | 3D 网格：**位置+法线+uv（32 字节固定布局，M3）**、VAO/VBO/EBO；**几何生成与 GPU 上传分成两半**（前者可 headless 测试） |
> **3D 施工路线见 [`../design/glimmer3d.md`](../design/glimmer3d.md)**（分期、规模、验收判据、进度台账）。
> 当前进度：**M1 已完成**（最简可用：带深度测试的立方体；`glimmer_check` 52 条 headless 断言、
> `glimmer_hello3d` 深度 A/B 读回）。M1 的取舍写在 `lum_renderer3d.h` 的文件头注释里
> （立即提交、朴素 uniform、无剔除/混合、法线用 `mat3(u_model)`）。

统一入口是 `glimmer/glimmer.h`；`include/luminous.h` 的 glimmer 段就只包含它。

## 边界（本模块的存在意义）

- **glimmer 自带一切它能用到的东西**：数学、日志、断言、文件、窗口、输入、时间、渲染器。
  因此它**不需要引擎就能跑起来**。
- **glimmer 禁止依赖**：`application` / `ecs` / `scene` / `scene_io` / `serial` / `assets` /
  `collision` / `ui` / `effects` / `components` / `containers` / `lum_version` / `luminous.h`。
- **Luminous 只许单向依赖 glimmer**：`lum_scene` 的 sprite 系统、`lum_ui`、
  `effects/*`、`components/*`、`lum_application` 都是它的客户。

### "能独立运行"是可验证的，不是口号

`glimmer_hello.c`（构建目标 `glimmer_hello`）是一个**只链接 `libglimmer.a`** 的完整程序：
自己 `lum_log_init` → `lum_window_init` → `lum_input_init` → `lum_time_init` →
`lum_renderer2d_init`，自己跑帧循环（`time_begin_frame` → `poll_events` → `input_update` →
`time_update` → `begin_frame` → 画 → `end_frame` → `swap_buffers` → `time_end_frame`），
自己按逆序拆除。注意它**不能用 `lum_assets`**（那是引擎高层的资源缓存）——
字体走 `lum_tr_load_font`、纹理走 `lum_texture2d_create*`，都是 glimmer 自己的能力。

验证方式（两条，都是机器判据）：

```powershell
cmake --build build --target glimmer_hello
# ① 链接行里不含 Luminous；把 build/libLuminous.a 改名藏起来，这个目标照样链得出来
# ② 真跑一遍：glimmer_hello.exe 跑满 240 帧自动退出，stderr 只有 2 条 Info
```

### 机械校验（三条规则）

`tools/check-glimmer-boundary.ps1`（**UTF-8 带 BOM** 保存，否则 Windows PowerShell 按
CP936 解码会语法报错）：

| 规则 | 内容 |
|---|---|
| 1 | glimmer 源码与公共头只允许包含 `glimmer/*`、third_party（glad/stb/GLFW）、标准库 |
| 2 | glimmer 公共头不许暴露 third_party（消费者不该被迫看到 glad/stb/GLFW） |
| 3 | `glimmer_hello` 的链接行里不许出现 `Luminous` |

三条都用"故意插入违规"的 A/B 验证过会真的报错（退出码 1）。
另一道门是公共头自包含：`include/glimmer/*.h` 必须**只给 `-Iinclude`** 就能编过。

**历史**：第一次隔离（渲染器单独成模块）时，是**链接器**发现 `lum_shader.c` /
`lum_text_renderer.c` 直接调了引擎的 `lum_fs_read*` —— 于是补了一层 `luminous_base`；
第二次重组（本次）把数学与平台层并入 glimmer，`luminous_base` 随之消失，
`glimmer` 自己成为那块地基。两次都说明：**隐藏依赖只有链接器/边界脚本能发现，挪目录发现不了**。

**命名现状**：符号仍是 `lum_*` / `Lum_*`；改成 `glimmer_*` / `Glimmer_*` 是独立的阶段 2
（触及引擎 + 游戏 + demo 约 170 处调用点，属破坏性变更）。

---

## `lum_renderer2d`（先看这个，其余都服务于它）

**职责**：延迟（deferred）批处理渲染器。**`draw_*` 不立即画** ——
它们只追加顶点/索引并记录一条 `lum_Mesh_command`。

**关键契约**

- 真正的 `glDrawElements` 发生在 **`lum_renderer2d_end_frame()`**：
  先排序命令 → 合并同批 → 上传 → 绘制
- `lum_renderer2d_begin_frame()` 清空本帧的命令/顶点/索引计数
- **排序键顺序**（`src/glimmer/lum_renderer2d.c:367-388`）：

  ```
  layer → view_projection → has_scissor → scissor
        → material → texture->id → primitive → blend → sequence
  ```

  最后一项 `sequence` 保证同批内保持提交顺序（稳定排序）
- 混合模式：`Lum_Blend_None / Alpha / Additive / Premul`；app Init 里设为 **Alpha**
- 剪裁：`lum_renderer2d_set_scissor(x,y,w,h)` / `clear_scissor()` **成对**使用；
  剪裁是**命令级快照** → 不同剪裁的命令不会进同一批
- 区域清屏：`lum_renderer2d_clear_region(x,y,w,h,color)` ——
  必须走这条，因为 **`glClear` 只受 scissor 限制、不受 viewport 限制**
- **位置型图元在记录时就展开**：`Triangle_Fan` / `Triangle_Strip` /
  `Line_Strip` / `Line_Loop` 会被改写成显式 `Triangles` / `Lines` 索引
  （否则合批会连出垃圾三角形）
- `lum_renderer2d_draw_fullscreen(material, src, layer)` 画全屏四边形（后处理用）
- `view_projection()` 读当前 VP，供 UI 等子系统暂存/还原

**易错点**

- 忘了 `begin_frame` / `end_frame` → **什么都不画**（命令被下一帧清掉）
- **`layer` 是第一排序键**：给每个实体唯一 layer 做精确 Y 排序，会让每条命令的
  layer 都不同 → **批处理彻底失效**。需要精确排序时用顶点 z + 深度测试
- **`sequence` 只在"状态完全一致"时才决定顺序**：排序键里 `view_projection` /
  `material` / `texture` / `scissor` 都排在 `sequence` **之前**，所以**同一 `layer`
  内、VP 或纹理不同的命令，绘制顺序由状态字节决定，而不是记录顺序**。
  最典型的受害者是 Screen 阶段的 HUD：背板用阶段自带的 Y 向下 VP，
  文字经 `lum_tr_draw_screen` 换成 Y 向上 VP → 同层时**先记录的背板反而画在文字之上**
  （实测：背板矩形内 93112 像素全被压暗、最亮 `(31,33,39)`）。
  **背板与文字必须各占一层（背板层 < 文字层）。**
- 视口是**左下原点**，屏幕空间是**左上原点**（[CONTRACT.md](../CONTRACT.md) §8）
- 位置型图元会被展开 → **索引数与输入不是 1:1**，别按输入长度反推

---

## `lum_shader`

**职责**：GLSL 程序编译/链接与 uniform 设置。

**关键契约**

- `lum_shader_create_from_source(shader, vs_src, fs_src)` /
  `lum_shader_create_from_file(shader, vs_path, fs_path)` → **1/0**
- 底层：`lum_shader_compile_from_source` / `lum_shader_link_shaders` /
  `lum_shader_create_program_from_source` 返回 **GL 句柄**（`uint32_t`，**0 = 失败**）
- `lum_shader_get_uniform_location(shader, name)` → **-1 表示失败**（GL 约定）
- `lum_shader_use`、`lum_shader_uniform_set1i / 1f / 2v / 3v / 4v / 4m`
- `lum_Shader{program, initialized}` 是**公开结构体**

**易错点**

- 返回值约定在这里是**第三种风格**（哨兵），与多数模块的 `int` 1/0 不同
  （[CONTRACT.md](../CONTRACT.md) §3）
- 本引擎的着色器写死 `#version 330 core`，与 GL 3.3 core 上下文绑定

---

## `lum_texture`

**职责**：**唯一的纹理创建入口** + 子矩形上传。头文件里已有较完整的 `@brief`。

**关键契约**

- `lum_texture2d_create(t, width, height, channels, pixels, flags)`
- **`pixels == NULL` 只分配存储、内容未定义**（GL 规范行为）——
  NULL 在全库只有这一种解释，没有隐藏的"清零"行为
- 非法组合会失败并记日志：
  - `Flag_Clear` 配**非空** `pixels`
  - `Flag_Swizzle_R_To_RGBA` 配 `channels != 1`
- flags：`Mipmaps` / `Clear` / `Filter_Linear` / `Wrap_Repeat` / `Swizzle_R_To_RGBA`
- `lum_texture2d_pixel_rect_to_uv(t, x, y, w, h)`：**(x, y) 是子矩形左下角**，
  与 GL 纹理坐标一致；LINEAR 下的边缘渗色由 atlas 的 1px padding 解决
- `lum_texture2d_update_region` → `glTexSubImage2D`（像素按纹理自身通道数逐行紧凑排列）
- `lum_texture2d_create_white` 生成 1×1 白纹理（渲染器的默认贴图）
- `lum_texture2d_bind` / `unbind(slot)`

**易错点**

- 要"确定的安全内容"必须显式加 `Flag_Clear`，不能指望 NULL
- **uv 矩形是左下原点，UI 与屏幕是左上原点** —— 直接拿 UI 坐标算 uv 会上下翻转

---

## `lum_atlas`

**职责**：把多张小图打进一张大图的**货架式（shelf）**分配器。

**关键契约**

- `lum_atlas_init(atlas, width, height, padding)`；`padding` 防 LINEAR 渗色，**通常 1**
- `lum_atlas_alloc(atlas, w, h, out_slot)` → **1 成功 / 0 放不下**
  - 单项尺寸超过整页（`w > width` 或 `h > height`）**直接判满**，不尝试开新行
  - 当前行放不下就开新行（新行底边 = 当前行顶 + padding）
- `lum_atlas_slot.x/y` 是**内容区左下角**（像素）
- `lum_atlas_reset` 回到空，**不重新分配**
- 它**只算位置，不碰像素** —— 上传是调用方的事

**易错点**

- 返回 `0` 只表示"这次放不下"，**不区分**是页面满了还是这项本身太大
- 忘了给相邻项留 `padding` → LINEAR 过滤时边缘互相渗色

---

## `lum_camera`

**职责**：2D 相机（位置 / 旋转 / 缩放 / 视口 / 近远平面）。

**关键契约**

- `lum_camera2d_init(cam, viewport_w, viewport_h)`
- 投影 = `ortho(-w/2, w/2, -h/2, h/2, near, far)`，其中 `w = viewport_w / zoom`
  → **世界空间 Y 向上**
- `lum_camera2d_screen_to_world(cam, screen_pos, window_size)`：
  **`screen_pos` 是左上原点、Y 向下**（内部 `ndc.y = 1 - 2*y/h`），
  `window_size` 要用**窗口/帧缓冲像素尺寸**
- `lum_camera2d_get_view_matrix` / `get_projection_matrix` / `get_view_projection_matrix`
- 也提供 `lum_camera2d_world_to_screen`、`set_rotation(rad)`、`translate`、`rotate`

**易错点**

- **`zoom <= 0` 会算出退化投影矩阵**。引擎对 `lum_View` 有 `zoom <= 0 → 1.0` 的兜底，
  但独立使用相机时要自己保证
- `screen_to_world` 需要**像素尺寸**；传世界尺寸会**静默算错**（不报错）
- `lum_camera2d_screen_to_world` 用 `Lum_Assert` 要求 `window_size` 分量为正

---

## `lum_transform`

**职责**：`lum_Transform2d`（position / **size** / scale / rotation / origin）→ 4×4 矩阵。

**关键契约**

- `lum_transform2d_get_matrix(t)` 生成 model 矩阵
- `size` 参与矩阵（`lum_mat4_trans_2d_with_size` / `..._and_origin` 系列）
- `origin` 是**缩放/旋转的锚点**（本地单位）；`lum_transform2d_set_center_origin(t)`
  等价于 `origin = size / 2`
- 提供 `set_*` 系列与 `translate` / `rotate`

**易错点**

- `rotation` 与 `delta_rad` **都是弧度**（前者没有 `_rad` 后缀，别被误导）
- `lum_Transform2d.size` 与场景里 `lum_Component_sprite.size` **是两个不同的东西**：
  前者进矩阵，后者是"自身绘制尺寸"（见 [scene.md](scene.md)）。
  `lum__sys_sprite` 用前者给后者套了一层 `origin = size/2`，把 sprite 摆成**以实体为中心**

---

## `lum_material`

**职责**：着色器 + 纹理 + 参数的组合体；**变体池**用于按参数复用材质实例。

**关键契约**

- `lum_Material{initialized, shader, textures[4], params[16], param_count}`（公开结构体）
- `lum_material_create(mat, shader)` / `lum_material_destroy` / `set_shader` /
  `set_texture(mat, slot, tex)`
- 参数按**名字**设置：`set_parameter_1i / 1f / 2v / 3v / 4v / 4m`；
  也可先 `lum_material_construct_parameter_*` 构造，再批量提交
- 上限：参数名 `Lum_Material_Max_Name`(31)、参数槽 `Max_Params`(16)、纹理槽 `Max_Textures`(4)
- **变体池**：`lum_material_variant_pool_init(pool, shader, capacity, key_count)`；
  `lum_material_variant_pool_acquire(pool, key)` 内部**线性查表** ——
  命中就返回已有材质，未命中则取 `variants[next]` **销毁重建**
  → 固定容量缓存 + **轮转复用**，**池满不报错**

**易错点**

- `acquire` 返回的 `lum_Material*` **由池拥有**，且会被后续 `acquire` 轮转覆盖 ——
  **不要长期持有**，更不要释放
- `key` 数组长度必须与 init 时的 `key_count` 一致（只比对前 `key_count` 个）
- 参数名超过 31 字符会被截断

---

## `lum_render_target`

**职责**：离屏渲染目标（FBO + 颜色纹理 [+ 深度 RBO]）。

**关键契约**

- `lum_rt_create(rt, width, height, depth)` / `lum_rt_destroy` / `lum_rt_resize`
- `lum_rt_bind(rt)` / `lum_rt_bind_screen()`（回到默认帧缓冲）
- `lum_rt_get_color(rt)` → 颜色纹理（**借用**，不要释放）
- `lum_Render_target{initialized, FBO, color, width, height, has_depth, depth_RBO}`
  是**公开结构体**

**易错点**

- 画完忘了 `lum_rt_bind_screen()` → 后续绘制继续进离屏目标
- `lum_rt_resize` 会**重建 GL 对象**（FBO / 纹理 / RBO），
  之前缓存过的 GL id 全部失效
- 视口不会自动跟随：绑定目标后仍需设置匹配的视口
  （走引擎流程时 `Lum_Stage_View_Begin` 会按 `lum_View.viewport` 处理）
