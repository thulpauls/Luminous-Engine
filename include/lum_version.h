#ifndef Lum_Version_H
#define Lum_Version_H

/*
* 版本号。
*
* 本文件是版本号的【唯一来源】：CMake 在配置阶段读这三行填进 project(VERSION)，
* 不反向生成头文件 —— 因此不存在"构建脚本与头文件各写一份、悄悄漂移"的情况。
*
* ⚠ 编号规则尚未定案：什么时候该动哪一位、0.x 与 1.x 各自承诺什么兼容性，
*   都还没定。本文件目前只提供【值】和【比较】机制，不承诺任何兼容性语义。
*   冻结时再补规则（届时这张注记换成正式约定）。
*
* 约定：
*   - 三个分量都取 0..255：LUM_VERSION_ENCODE 每个分量只留 8 位
*   - 预发布/开发标识只进 LUM_VERSION_STRING，不进编码值，因此不参与
*     LUM_VERSION 的大小比较（等价于 SemVer "预发布不参与优先级"的精神）
*/

#define LUM_VERSION_MAJOR 0
#define LUM_VERSION_MINOR 1
#define LUM_VERSION_PATCH 0

/* 附加标识：正式发布留空；开发/预发布填 "-dev"、"-rc.1" 之类。只影响字符串 */
#define LUM_VERSION_SUFFIX ""

/* 三分量打包成单个整数，便于编译期比较：
     #if LUM_VERSION < LUM_VERSION_ENCODE(0, 2, 0)
       #error "需要 Luminous 0.2.0 或更高"
     #endif
   注意：这里刻意只用整数乘加、不做任何类型转换。#if 的预处理器不认
   `(unsigned long)` 这类多词类型转型（GCC 会报 "missing binary operator
   before token 'long'"）；不带转型的写法在 #if 和普通 C 表达式里都成立。
   每个分量上限 255（8 位） */
#define LUM_VERSION_ENCODE(major, minor, patch) \
  ((major) * 65536 + (minor) * 256 + (patch))

#define LUM_VERSION \
  LUM_VERSION_ENCODE(LUM_VERSION_MAJOR, LUM_VERSION_MINOR, LUM_VERSION_PATCH)

/* 字符串字面量版本号，如 "0.1.0"。相邻字面量拼接，无运行时代价 */
#define LUM_VERSION_STR_(x) #x
#define LUM_VERSION_STR(x)  LUM_VERSION_STR_(x)
#define LUM_VERSION_STRING                        \
  LUM_VERSION_STR(LUM_VERSION_MAJOR) "."          \
  LUM_VERSION_STR(LUM_VERSION_MINOR) "."          \
  LUM_VERSION_STR(LUM_VERSION_PATCH) LUM_VERSION_SUFFIX

#endif //Lum_Version_H
