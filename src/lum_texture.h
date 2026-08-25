#ifndef Lum_Texture_H
#define Lum_Texture_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "lum_math2d.h"

typedef struct lum_Texture2d {
  uint32_t id;
  int width, height;
  uint32_t channels;
  bool initialized;
  bool mipmaps;
} lum_Texture2d;

/* 创建标志，可按位或组合；非法组合（Clear 配非空 pixels、Swizzle 配多通道）创建失败并记日志 */
typedef enum lum_Texture_flag {
  Lum_Texture_Flag_None              = 0,
  Lum_Texture_Flag_Mipmaps           = (1u << 0),  /* 上传/清零后生成完整 mipmap 链 */
  Lum_Texture_Flag_Clear             = (1u << 1),  /* 分配的存储整体清零；仅 pixels == NULL 时合法 */
  Lum_Texture_Flag_Filter_Linear     = (1u << 2),  /* LINEAR 过滤（默认 NEAREST）；与 Mipmaps 叠加时 min 自动用 LINEAR_MIPMAP_LINEAR */
  Lum_Texture_Flag_Wrap_Repeat       = (1u << 3),  /* REPEAT 寻址（默认 CLAMP_TO_EDGE） */
  Lum_Texture_Flag_Swizzle_R_To_RGBA = (1u << 4),  /* 单通道采样结果复制为 (r,r,r,r)；仅 channels == 1 时合法 */
} lum_Texture_flag;

/*
* @brief 唯一的纹理创建入口。pixels 为 NULL 时只分配存储、内容未定义（GL 规范行为），
*        适用于写入前必有完整覆盖的用途（如 FBO color 附件）；需要确定的安全内容时
*        显式传 Lum_Texture_Flag_Clear。NULL 在全库只有这一种解释，无隐藏行为。
*
* 典型用法：
*   FBO 附件      create(&t, w, h, 4, NULL, Lum_Texture_Flag_Filter_Linear)
*   glyph atlas   create(&t, 1024, 1024, 1, NULL, Lum_Texture_Flag_Clear
*                        | Lum_Texture_Flag_Filter_Linear | Lum_Texture_Flag_Swizzle_R_To_RGBA)
*   像素风贴图    create(&t, w, h, 4, pixels, Lum_Texture_Flag_None)
*/
int lum_texture2d_create(lum_Texture2d* texture, int width, int height, uint32_t channels,
                         const unsigned char* pixels, uint32_t flags);

/*
* @brief 向已创建纹理写入子矩形像素（glTexSubImage2D）。pixels 按纹理自身通道数逐行紧凑排列，
*/
int lum_texture2d_update_region(lum_Texture2d* texture, int x, int y, int w, int h, const unsigned char* pixels);

/*
* @brief 像素子矩形 → 归一化 UV 矩形 (u0, v0, u1, v1)。原点约定与 GL 纹理坐标一致：
*        (x, y) 为子矩形左下角。做的是精确换算；LINEAR 过滤下的边缘渗色由调用方
*        通过 atlas 打包预留的 1px padding 解决（需要更锐利边缘时按半 texel 内缩）。
*/
lum_Vec4 lum_texture2d_pixel_rect_to_uv(const lum_Texture2d* texture, int x, int y, int w, int h);

int lum_texture2d_create_white(lum_Texture2d* texture);
int lum_texture2d_load_from_file(lum_Texture2d* texture, const char* path, uint32_t desired_channels, uint32_t flags);
void lum_texture2d_destroy(lum_Texture2d* texture);

void lum_texture2d_bind(const lum_Texture2d* texture, uint32_t slot);
void lum_texture2d_unbind(uint32_t slot);

#endif //Lum_Texture_H
