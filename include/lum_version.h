#ifndef Lum_Version_H
#define Lum_Version_H

#include "glimmer/glimmer_version.h"   /* 兼容性声明要引用 glimmer 的版本常量，并在编译期校验 */

/*
* 引擎版本号 —— 格式：  <开发阶段> <迭代次数>          例：prototype 1
*
* 引擎与 glimmer 【分开计数】：两条版本线各自迭代。本文件是引擎版本的唯一来源，
* 同时声明【本引擎版本针对哪个 glimmer 版本开发】（见下方"兼容性声明"）。
*
* CMake 在 project() 之前读取本文件与 include/glimmer/glimmer_version.h，据此填
* project(VERSION) 并做一次配置期兼容检查 —— 不反向生成头文件，所以构建脚本与公共头
* 不可能各写一份而悄悄跑偏。
*
* 比较规则：阶段在高位、迭代次数在低位，用 Lum_Version_Encode 打包后可在 #if 里比较。
* 迭代次数在本阶段内递增；**阶段推进时从 1 重新开始**，因此跨阶段比较必须带上阶段。
*
* ⚠ 编号规则仍是暂定的：什么算"一次迭代"、何时该推进阶段，见 docs/versioning.md
*   的"已定 / 未定"表；冻结前会正式化。
*/

/* ---------- 阶段词表（数值有序；改这里要同步改阶段名串） ---------- */

#define Lum_Version_Stage_Prototype 1
#define Lum_Version_Stage_Alpha     2
#define Lum_Version_Stage_Beta      3
#define Lum_Version_Stage_Rc        4
#define Lum_Version_Stage_Release   5

/* ---------- 引擎当前版本 ---------- */

#define Lum_Version_Stage_Current Lum_Version_Stage_Prototype
#define Lum_Version_Stage_Name    "prototype"
#define Lum_Version_Iteration     1

/* ---------- 编码与显示 ---------- */

/* 阶段在高位、迭代次数在低位（各 8 位）。只用整数乘加、不做类型转换：
   #if 的预处理器不认 `(unsigned long)` 这类多词转型（GCC 报
   "missing binary operator before token 'long'"）。 */
#define Lum_Version_Encode(stage, iteration) ((stage) * 256 + (iteration))

#define Lum_Version \
  Lum_Version_Encode(Lum_Version_Stage_Current, Lum_Version_Iteration)

/* 字符串字面量版本号，形如 "prototype 1"。相邻字面量拼接，无运行时代价。
   两级宏是必需的：只有经过一层展开，参数才会先被替换、再被字符串化。 */
#define Lum_Version_Stringize_Impl(x) #x
#define Lum_Version_Stringize(x)  Lum_Version_Stringize_Impl(x)
#define Lum_Version_String \
  Lum_Version_Stage_Name " " Lum_Version_Stringize(Lum_Version_Iteration)

/* ---------- 兼容性声明：本引擎版本针对的 glimmer 版本 ----------
*
* 改这里 = 声明"引擎改到能配一个新 glimmer 版本了"。它与 glimmer 的实际版本必须一致，
* 否则下面的 #if 会直接编译失败 —— 三道门里最早的一道（另两道在 CMake 配置期与
* docs/versioning.md 的兼容性矩阵）。
*
* 为什么要求严格相等、而不是"大于等于"：prototype 阶段没有向前兼容承诺（见
* docs/versioning.md），"针对哪个版本开发"就是全部事实。将来进入 alpha/beta 后，
* 这里的判据会改成"引擎要求的最低 glimmer 版本"，届时 #if 的比较就有了顺序语义。
*/
#define Lum_Version_Glimmer_Stage     1          /* = Lum_Glimmer_Version_Stage_Prototype */
#define Lum_Version_Glimmer_Stage_Name "prototype"
#define Lum_Version_Glimmer_Iteration  2
#define Lum_Version_Glimmer_String \
  Lum_Version_Glimmer_Stage_Name " " Lum_Version_Stringize(Lum_Version_Glimmer_Iteration)

#if Lum_Version_Glimmer_Stage != Lum_Glimmer_Version_Stage_Current
#error "引擎声明的 glimmer 阶段与 include/glimmer/glimmer_version.h 不一致：两者必须成对修改（见 docs/versioning.md）"
#endif

#if Lum_Version_Glimmer_Iteration != Lum_Glimmer_Version_Iteration
#error "引擎声明的 glimmer 迭代次数与 include/glimmer/glimmer_version.h 不一致：两者必须成对修改（见 docs/versioning.md）"
#endif

#endif //Lum_Version_H
