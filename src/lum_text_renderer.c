#include "lum_text_renderer.h"
#include "lum_texture.h"
#include "lum_atlas.h"
#include "lum_filesystem.h"
#include "lum_log.h"
#include "lum_assert.h"
#include "lum_math2d.h"
#include "lum_renderer2d.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include <string.h>
#include <stdlib.h>

#define Lum_Text_Atlas_Size    512   /* R8 单页 256KB；ASCII 128 字形按 24px 算只用约 1/3 */
#define Lum_Text_Atlas_Padding 1

typedef struct lum_Glyph {
  lum_Vec4 uv;          /* atlas 内归一化 UV 矩形（已含半 texel 内缩） */
  int width, height;
  int bearingX, bearingY;
  int advance;
  int index;            /* 缓存的 stbtt glyph index，kerning 免每帧 FindGlyphIndex */
  bool loaded;
} lum_Glyph;

typedef struct lum_Text_renderer {
  stbtt_fontinfo font;
  unsigned char* font_buffer;

  float scale;
  int ascent, descent;
  int line_gap;

  lum_Texture2d atlas_tex;   /* 整个字体共享一张：R8 + LINEAR + swizzle + 清零 */
  lum_Atlas atlas;           /* 槽位账本，尺寸与 atlas_tex 一致 */

  lum_Glyph glyphs[128];
} lum_Text_renderer;

static void lum__font_reset(lum_Font* font) {
  memset(font, 0, sizeof(*font));
}

static void lum__text_renderer_reset(lum_Text_renderer* tr) {
  memset(tr, 0, sizeof(*tr));
}

static int lum__tr_initialize_renderer(lum_Text_renderer* tr, const char* font, float height_pixel) {
  lum__text_renderer_reset(tr);

  tr->font_buffer = lum_fs_read_binary(font).data;
  if (!tr->font_buffer) return 0;

  if (!stbtt_InitFont(&tr->font, tr->font_buffer, stbtt_GetFontOffsetForIndex(tr->font_buffer, 0))) {
    free(tr->font_buffer);
    tr->font_buffer = NULL;
    Lum_Log_Error("Failed to initialize stb_tt font: %s", font);
    return 0;
  }

  if (!lum_texture2d_create(&tr->atlas_tex, Lum_Text_Atlas_Size, Lum_Text_Atlas_Size, 1, NULL,
                            Lum_Texture_Flag_Clear | Lum_Texture_Flag_Filter_Linear
                            | Lum_Texture_Flag_Swizzle_R_To_RGBA)) {
    free(tr->font_buffer);
    tr->font_buffer = NULL;
    Lum_Log_Error("text: failed to create glyph atlas.");
    return 0;
  }
  if (!lum_atlas_init(&tr->atlas, Lum_Text_Atlas_Size, Lum_Text_Atlas_Size, Lum_Text_Atlas_Padding)) {
    lum_texture2d_destroy(&tr->atlas_tex);
    free(tr->font_buffer);
    tr->font_buffer = NULL;
    return 0;
  }

  tr->scale = stbtt_ScaleForPixelHeight(&tr->font, height_pixel);
  stbtt_GetFontVMetrics(&tr->font, &tr->ascent, &tr->descent, &tr->line_gap);
  return 1;
}

static void lum__tr_destroy_renderer(lum_Text_renderer* tr) {
  if (!tr) return;

  lum_texture2d_destroy(&tr->atlas_tex);
  lum_atlas_shutdown(&tr->atlas);

  if (tr->font_buffer) {
    free(tr->font_buffer);
    tr->font_buffer = NULL;
  }
}

int lum_tr_load_font(lum_Font* font, const char* text, float height_pixel) {
    lum__font_reset(font);
    lum_Text_renderer* tr = malloc(sizeof(*tr));
    if (!tr) return 0;
    if (!lum__tr_initialize_renderer(tr, text, height_pixel)) {
        free(tr);
        return 0;
    }
    font->renderer = tr;
    font->initialized = true;
    return 1;
}

void lum_tr_destroy_font(lum_Font* font) {
    if(!font->initialized) return;
    lum__tr_destroy_renderer((lum_Text_renderer*)font->renderer);
    free((lum_Text_renderer*)font->renderer);
    font->renderer = NULL;
    font->initialized = false;
}

static int lum__tr_load_glyph(lum_Text_renderer* tr, unsigned int codepoint) {
  Lum_Assert(codepoint < 128);

  lum_Glyph* glyph = &tr->glyphs[codepoint];
  if (glyph->loaded) return 1;

  int glyph_index = stbtt_FindGlyphIndex(&tr->font, codepoint);

  int advance_w = 0, bearing_left = 0;
  stbtt_GetGlyphHMetrics(&tr->font, glyph_index, &advance_w, &bearing_left);
  int x0, y0, x1, y1;
  stbtt_GetGlyphBitmapBox(&tr->font, glyph_index, tr->scale, tr->scale, &x0, &y0, &x1, &y1);

  int w = x1 - x0, h = y1 - y0;
  unsigned char* bitmap = NULL;
  if (w > 0 && h > 0) {
    bitmap = (unsigned char*)calloc((size_t)w * (size_t)h, 1);
    if(!bitmap) return 0;

    stbtt_MakeGlyphBitmap(&tr->font, bitmap, w, h, w, tr->scale, tr->scale, glyph_index);

    /* stbtt 输出顶朝下行序，翻成 GL 的底朝向，pixel_rect_to_uv 才能直接使用 */
    for (int row = 0; row < h / 2; ++row) {
      unsigned char* top_row = bitmap + (size_t)row * w;
      unsigned char* bot_row = bitmap + (size_t)(h - 1 - row) * w;
      for (int col = 0; col < w; ++col) {
        unsigned char tmp = top_row[col];
        top_row[col] = bot_row[col];
        bot_row[col] = tmp;
      }
    }
  }

  if (bitmap) {
    lum_Atlas_slot slot;
    if (!lum_atlas_alloc(&tr->atlas, w, h, &slot)) {
      free(bitmap);
      Lum_Log_Warn("text: atlas page full, glyph %u dropped.", codepoint);
      return 0;
    }

    lum_texture2d_update_region(&tr->atlas_tex, slot.x, slot.y, w, h, bitmap);
    free(bitmap);

    lum_Vec4 uv = lum_texture2d_pixel_rect_to_uv(&tr->atlas_tex, slot.x, slot.y, w, h);
    /* 半 texel 内缩：采样落在边缘 texel 中心，LINEAR 下字形边缘更实；
       w/h <= 1 不缩，避免 UV 区间退化成零宽 */
    if (w > 1 && h > 1) {
      float hu = 0.5f / (float)tr->atlas_tex.width;
      float hv = 0.5f / (float)tr->atlas_tex.height;
      uv = lum_vec4_create(uv.x + hu, uv.y + hv, uv.z - hu, uv.w - hv);
    }
    glyph->uv = uv;
  }
  /* 空 bitmap（空格等）：不占槽位，只存度量 */

  glyph->width = w;
  glyph->height = h;
  glyph->bearingX = x0;
  glyph->bearingY = y0;
  glyph->advance = advance_w;
  glyph->index = glyph_index;
  glyph->loaded = true;

  return 1;
}

void lum_tr_draw_layer(lum_Font* font, const char* text, lum_Vec2 position, int32_t layer, lum_Vec4 color) {
  Lum_Assert(font && text);
  if (!font->initialized) return;
  lum_Text_renderer* tr = (lum_Text_renderer*)font->renderer;

  float x = position.x, y = position.y;
  float pen_x = x;
  float baseline = y;
  float line_step = (float)(tr->ascent - tr->descent + tr->line_gap) * tr->scale;

  for (const char* p = text; *p; ++p) {
    unsigned char c = (unsigned char)(*p);
    Lum_Assert(c < 128);

    if (c == '\n') {
      pen_x = x;
      baseline -= line_step;
      continue;
    }

    if (!lum__tr_load_glyph(tr, c)) continue;

    lum_Glyph* glyph = &tr->glyphs[c];

    if (glyph->width > 0 && glyph->height > 0) {
      float xpos = pen_x + (float)glyph->bearingX;
      float ypos = baseline - (float)glyph->bearingY - (float)glyph->height;
      float w = (float)glyph->width;
      float h = (float)glyph->height;

      lum_renderer2d_draw_sprite_uv(&tr->atlas_tex, lum_vec2_create(xpos, ypos),
                                          lum_vec2_create(w, h), glyph->uv, layer, color);
    }

    int kerning = 0;
    if (*(p + 1)) {
      unsigned char nc = (unsigned char)*(p + 1);
      int gi2 = tr->glyphs[nc].loaded ? tr->glyphs[nc].index
                                      : stbtt_FindGlyphIndex(&tr->font, nc);
      kerning = stbtt_GetGlyphKernAdvance(&tr->font, glyph->index, gi2);
    }
    pen_x += (float)(glyph->advance + kerning) * tr->scale;
  }
}

void lum_tr_preload_ascii(lum_Font* font) {
  if (!font->initialized) return;
  lum_Text_renderer* tr = (lum_Text_renderer*)font->renderer;
  for (unsigned int c = 32; c < 127; ++c) {
    lum__tr_load_glyph(tr, c);
  }
}

void lum_tr_draw(lum_Font* font, const char* text, lum_Vec2 position, lum_Vec4 color) {
    lum_tr_draw_layer(font, text, position, 0, color);
}

void lum_tr_draw_screen(lum_Font* font, const char* text, lum_Vec2 top_left, int32_t layer, lum_Vec4 color) {
  if (!font || !font->initialized) return;
  /* Screen 阶段的 VP 是 ortho(0, sw, sh, 0)（Y 向下），而 lum_tr_draw_layer 画的是
     Y 向上的世界空间文字 —— 必须把"左上角"换算成翻转后的基线位置。
     用视口高而不是窗口高：Screen 的 VP 用的就是当前视口尺寸。 */
  int sw = 0, sh = 0;
  lum_renderer2d_get_viewport(NULL, NULL, &sw, &sh);
  float baseline = (float)sh - top_left.y - lum_tr_ascent(font);
  lum_tr_draw_layer(font, text, lum_vec2_create(top_left.x, baseline), layer, color);
}

float lum_tr_ascent(lum_Font* font) {
  if (!font || !font->initialized) return 0.0f;
  lum_Text_renderer* tr = (lum_Text_renderer*)font->renderer;
  return (float)tr->ascent * tr->scale;
}

lum_Vec2 lum_tr_measure_text(lum_Font* font, const char* text) {
  lum_Vec2 size = lum_vec2_create(0.0f, 0.0f);
  Lum_Assert(font && text);
  if (!font->initialized || !text[0]) return size;

  lum_Text_renderer* tr = (lum_Text_renderer*)font->renderer;
  float line_step = (float)(tr->ascent - tr->descent + tr->line_gap) * tr->scale;
  float pen_x = 0.0f;
  float max_x = 0.0f;
  int lines = 1;

  for (const char* p = text; *p; ++p) {
    unsigned char c = (unsigned char)(*p);
    Lum_Assert(c < 128);

    if (c == '\n') {
      if (pen_x > max_x) max_x = pen_x;
      pen_x = 0.0f;
      ++lines;
      continue;
    }

    if (!lum__tr_load_glyph(tr, c)) continue;

    lum_Glyph* glyph = &tr->glyphs[c];

    int kerning = 0;
    if (*(p + 1)) {
      unsigned char nc = (unsigned char)*(p + 1);
      int gi2 = tr->glyphs[nc].loaded ? tr->glyphs[nc].index
                                      : stbtt_FindGlyphIndex(&tr->font, nc);
      kerning = stbtt_GetGlyphKernAdvance(&tr->font, glyph->index, gi2);
    }
    pen_x += (float)(glyph->advance + kerning) * tr->scale;
  }
  if (pen_x > max_x) max_x = pen_x;

  size.x = max_x;
  size.y = (float)lines * line_step;
  return size;
}
