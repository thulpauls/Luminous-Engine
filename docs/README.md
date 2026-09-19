# Luminous 文档

## 阅读顺序

1. **[architecture.md](architecture.md)** —— 分层、模块依赖、启动/关闭顺序、帧流程、规模、已知架构债
2. `CONTRACT.md` —— 跨模块语义契约（线程模型 / 失败语义 / 所有权 / 坐标系与单位 / 枚举稳定性）
3. `versioning.md` —— 编号策略
4. `modules/*.md` —— 逐层参考
5. `../include/*.h` —— 逐符号契约

## 写作约定

- **语言**：中文
- **头文件注释**统一用 Doxygen 子集（`@brief` / `@param` / `@return` / `@note` / `@warning`）。
  一期不跑 Doxygen，但语法先统一 —— 二期装好工具即可直接渲染，不用回头返工。
- **行数**一律按**物理行（含空行）**计。注意 `Get-Content | Measure-Object -Line`
  只数非空行（会把 `lum_assert.h` 数成 21 而非 28），不要用它。
- **术语**中英混用会漂移，跨模块的术语在一个地方钉死（见 `CONTRACT.md` 术语表）。

## 文档状态

| 文档 | 内容 | 状态 |
|---|---|---|
| [`architecture.md`](architecture.md) | 分层与模块总览 | ✅ |
| [`CONTRACT.md`](CONTRACT.md) | 跨模块语义契约（术语表 / 线程 / 失败 / 所有权 / ECS 指针失效 / 坐标系 / 枚举稳定性） | ✅ |
| [`versioning.md`](versioning.md) | 三段式版本号、四个版本轴、依赖矩阵、tag 约定 | ✅ |
| [`modules/base.md`](modules/base.md) | 基础层（version / log / assert / math2d / collision / vector / list） | ✅ |
| [`modules/platform.md`](modules/platform.md) | 平台层（filesystem / window / input_codes / input / time） | ✅ |
| [`modules/render.md`](modules/render.md) | 渲染层（renderer2d / shader / texture / atlas / camera / transform / material / render_target） | ✅ |
| [`modules/assets.md`](modules/assets.md) | 资源层（assets 缓存） | ✅ |
| [`modules/serial.md`](modules/serial.md) | 序列化层（Archive + 字段表） | ✅ |
| [`modules/scene.md`](modules/scene.md) | 场景层（ecs / scene / scene_io） | ✅ |
| [`modules/components.md`](modules/components.md) | 内置扩展组件（glow / beam 实体组件） | ✅ |
| [`modules/effects.md`](modules/effects.md) | 特效（glow / beam 即时绘制） | ✅ |
| [`modules/subsystem.md`](modules/subsystem.md) | 子系统（text_renderer / ui） | ✅ |
| [`research/spring-recoil-skirmish-ai-contract.md`](research/spring-recoil-skirmish-ai-contract.md) | **外部调研**（非本引擎）：Recoil/Spring Skirmish AI 接口契约与"托管 AI 所需的最小引擎能力"清单 | 参考 |
| `tools/api-coverage.ps1` | 列出缺注释的公开符号 | 待写（非 markdown） |

**写作依据**：本目录所有契约均从代码中提取（`#include` 关系、`src/` 实现、
实测行为），不写推测内容。凡"代码行为已如此但头文件未写"的，标注为
**⚠ 未成文**，它们是冻结前要补进头文件的部分。

## 二期（可选）

注释铺到一定密度后：

```powershell
winget install Doxygen.Doxygen
```

再加 `docs/Doxyfile`，把本目录的 Markdown 作为 `main.md` / 附加页缝进去，
并开启 `WARN_AS_ERROR = YES` 强制覆盖。

现成范本可参考仓库内的 `glfw-3.4/docs/`（`Doxyfile.in` + `DoxygenLayout.xml` +
`main.md` + 每主题一个 `.md`）。

> ⚠ 生成文档前必须先解决 `Lum_Debug` 改变公开面的问题（见 `architecture.md` 第 8 节第 3 条），
> 否则 Doxygen 只能看到一部分 API，文档天然残缺。
