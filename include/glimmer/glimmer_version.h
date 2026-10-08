#ifndef Lum_Glimmer_Version_H
#define Lum_Glimmer_Version_H

/*
* glimmer（基础平台）的版本号 —— 与引擎【分开计数】。
*
* 格式：  <开发阶段> <迭代次数>          例：prototype 2
*          ^^^^^^^^^   ^^^^^^^^
*          阶段词       本阶段内的第几次迭代
*
* 为什么与引擎分开：两者是独立的东西 —— 引擎可以升到 alpha 3，而 glimmer 还停在
* prototype 7；把两条线绑成一个号，就没法表达"平台没动、引擎动了"这种最常见的状态。
* 代价是必须显式声明兼容关系：引擎在 include/lum_version.h 里写明它针对哪个 glimmer 版本，
* 两边由编译期 #if 与 CMake 配置期检查强制成对（见 docs/versioning.md）。
*
* 阶段词是【有序数值】（1..5），不是纯标签：这样可以写
*     #if Lum_Glimmer_Version < Lum_Glimmer_Version_Encode(Lum_Glimmer_Version_Stage_Beta, 1)
* 旧方案把阶段当纯字符串标签、不参与比较，因此无法表达"至少 beta"—— 这是本次修正的一点。
*
* 迭代次数的规则：本阶段内每迭代一次 +1；**阶段推进时从 1 重新开始**。
* 因此跨阶段比较迭代次数没有意义，比较必须带上阶段（Encode 把阶段编码在高位，天然满足）。
*/

/* ---------- 阶段词表（数值有序；改这里要同步改阶段名串） ---------- */

#define Lum_Glimmer_Version_Stage_Prototype 1
#define Lum_Glimmer_Version_Stage_Alpha     2
#define Lum_Glimmer_Version_Stage_Beta      3
#define Lum_Glimmer_Version_Stage_Rc        4
#define Lum_Glimmer_Version_Stage_Release   5

/* ---------- 本模块当前版本 ---------- */

#define Lum_Glimmer_Version_Stage_Current Lum_Glimmer_Version_Stage_Prototype
#define Lum_Glimmer_Version_Stage_Name    "prototype"
#define Lum_Glimmer_Version_Iteration     2

/* ---------- 编码与显示 ---------- */

/* 阶段在高位、迭代次数在低位，便于在 #if 里比较（每段上限 255，即 8 位）。
   刻意只用整数乘加、不做类型转换 —— #if 的预处理器不认 `(unsigned long)` 这类多词转型
   （GCC 报 "missing binary operator before token 'long'"）。 */
#define Lum_Glimmer_Version_Encode(stage, iteration) ((stage) * 256 + (iteration))

#define Lum_Glimmer_Version \
  Lum_Glimmer_Version_Encode(Lum_Glimmer_Version_Stage_Current, Lum_Glimmer_Version_Iteration)

/* 字符串字面量，形如 "prototype 2"。两级宏是必需的：参数要先展开再字符串化。 */
#define Lum_Glimmer_Version_Stringize_Impl(x) #x
#define Lum_Glimmer_Version_Stringize(x)  Lum_Glimmer_Version_Stringize_Impl(x)
#define Lum_Glimmer_Version_String \
  Lum_Glimmer_Version_Stage_Name " " Lum_Glimmer_Version_Stringize(Lum_Glimmer_Version_Iteration)

#endif //Lum_Glimmer_Version_H
