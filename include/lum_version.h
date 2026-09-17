#ifndef Lum_Version_H
#define Lum_Version_H

/*
* 版本号：三段式 ——  <开发阶段> <主要版本号>.<patch 版本号>
*
*   例：prototype 0.1
*        ^^^^^^^^^   开发阶段标记（纯标签）
*                    ^ 主要版本号
*                      ^ patch 版本号
*
* 本文件是版本号的【唯一来源】：CMake 在配置阶段读取它并据此填 project(VERSION)，
* 不反向生成头文件 —— 因此不存在"构建脚本与头文件各写一份、悄悄跑偏"的情况。
*
* 比较规则（目前唯一一条）：
*   - 只比较两段数字。**开发阶段标记完全不参与比较**：它只是给人看的成熟度标签，
*     没有顺序语义，也无法用于 #if 的大小判断
*   - 两段数字各自取 0..255（各占 8 位）
*
* ⚠ 编号规则尚未定案：什么时候该动哪一段、以及"加功能但不破坏兼容"与"只修 bug"
*   这两件事目前无法区分（都表现为 patch +1）。冻结时再补正式约定。
*
* 注：CMake 的 project(VERSION) 只接受数字，塞不进阶段词，因此喂给 CMake 的是
*     `主要.patch` 两段（PROJECT_VERSION 形如 "0.1"），阶段标记单独作为一个变量。
*/

/* ---------- 三段 ---------- */

/* 开发阶段标记：纯标签，不参与比较、无顺序语义。
   词表（仅文档约定，代码不校验）：prototype / alpha / beta / rc / release */
#define Lum_Version_Stage "prototype"

/* 主要版本号：代际 */
#define Lum_Version_Major 0

/* patch 版本号：本代际内的迭代次数 */
#define Lum_Version_Patch 1

/* ---------- 比较与显示 ---------- */

/* 两段数字打包成单个整数，便于编译期比较：
     #if Lum_Version < Lum_Version_Encode(1, 0)
       #error "需要 Luminous 1.0 或更高"
     #endif
   注意：这里刻意只用整数乘加、不做任何类型转换。#if 的预处理器不认
   `(unsigned long)` 这类多词类型转型（GCC 会报 "missing binary operator
   before token 'long'"）；不带转型的写法在 #if 和普通 C 表达式里都成立。
   每段上限 255（8 位） */
#define Lum_Version_Encode(major, patch) ((major) * 256 + (patch))

#define Lum_Version \
  Lum_Version_Encode(Lum_Version_Major, Lum_Version_Patch)

/* 字符串字面量版本号，形如 "prototype 0.1"。相邻字面量拼接，无运行时代价。
   两级宏是必需的：只有经过一层展开，参数才会先被替换、再被字符串化。
   Impl 那层只在本文件内使用；带下划线后缀会违反宏命名约定（每段首字母大写） */
#define Lum_Version_Stringize_Impl(x) #x
#define Lum_Version_Stringize(x)  Lum_Version_Stringize_Impl(x)
#define Lum_Version_String                      \
  Lum_Version_Stage " "                         \
  Lum_Version_Stringize(Lum_Version_Major) "."  \
  Lum_Version_Stringize(Lum_Version_Patch)

#endif //Lum_Version_H
