#ifndef Lum_Assets_H
#define Lum_Assets_H

#include "lum_texture.h"
#include "lum_text_renderer.h"

int  lum_assets_init(void);
void lum_assets_shutdown(void);

const lum_Texture2d* lum_assets_texture(const char* path, uint32_t flags);
lum_Font*            lum_assets_font(const char* path, float height_pixel);

/* 纹理 → 加载路径反查（序列化引用用）；非本仓库来源的纹理返回 NULL */
const char* lum_assets_texture_path(const lum_Texture2d* texture);

uint32_t lum_assets_count(void);

#endif //Lum_Assets_H
