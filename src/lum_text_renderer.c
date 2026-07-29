#include "lum_text_renderer.h"
#include "lum_texture.h"
#include "lum_filesystem.h"
#include "lum_log.h"
#include "lum_math2d.h"
#include "lum_renderer2d.h"
#include <stb_image.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include <string.h>
#include <glad/glad.h>

typedef struct lum_Glyph {
  lum_Texture2d texture;
  int width, height;
  int bearingX, bearingY;
  int advance;
  bool loaded;
} lum_Glyph;

typedef struct lum_Text_renderer {
  stbtt_fontinfo font;
  unsigned char* font_buffer;

  float scale;
  int ascent, descent;
  int line_gap;

  lum_Glyph glyphs[128];
} lum_Text_renderer;

static void lum_font_reset(lum_Font* font) {
  memset(font, 0, sizeof(*font));
}

static void lum_text_renderer_reset(lum_Text_renderer* tr) {
  memset(tr, 0, sizeof(*tr));
}

static int lum_tr_initialize_renderer(lum_Text_renderer* tr, const char* font, float height_pixel) {
  lum_text_renderer_reset(tr);
    
  tr->font_buffer = lum_fs_read_binary(font).data;
  if (!tr->font_buffer) return 0;

  if (!stbtt_InitFont(&tr->font, tr->font_buffer, stbtt_GetFontOffsetForIndex(tr->font_buffer, 0))) {
    free(tr->font_buffer);
    tr->font_buffer = NULL;
    Lum_Log_Error("Failed to initialize stb_tt font: %s", font);
    return 0;
  }

  tr->scale = stbtt_ScaleForPixelHeight(&tr->font, height_pixel);
  stbtt_GetFontVMetrics(&tr->font, &tr->ascent, &tr->descent, &tr->line_gap);
  return 1;
}

static void lum_tr_destroy_renderer(lum_Text_renderer* tr) {
  if (!tr) return;

  for (size_t i = 0; i < 128; ++i) {
    if (tr->glyphs[i].texture.initialized) {
      lum_texture2d_destroy(&tr->glyphs[i].texture);
    }
  }

  if (tr->font_buffer) {
    free(tr->font_buffer);
    tr->font_buffer = NULL;
  }
}

int lum_tr_load_font(lum_Font* font, const char* text, float height_pixel) {
    lum_font_reset(font);
    lum_Text_renderer* tr = malloc(sizeof(*tr));
    if (!tr) return 0;
    if (!lum_tr_initialize_renderer(tr, text, height_pixel)) {
        free(tr);
        return 0;
    }
    font->renderer = tr;
    font->initialized = true;
    return 1;
}

void lum_tr_destroy_font(lum_Font* font) {
    if(!font->initialized) return;
    lum_tr_destroy_renderer((lum_Text_renderer*)font->renderer);
    free((lum_Text_renderer*)font->renderer);
    font->renderer = NULL;
    font->initialized = false;
}

static int lum_tr_load_glyph(lum_Text_renderer* tr, unsigned int codepoint) {
  assert(codepoint < 128);

  lum_Glyph* glyph = &tr->glyphs[codepoint];
  if(glyph->loaded) return 1;

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
    if (!lum_texture2d_create(&glyph->texture, w, h, 1, bitmap)) {
      free(bitmap);
      return 0;
    }
    free(bitmap);

    glBindTexture(GL_TEXTURE_2D, glyph->texture.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  glyph->width = w;
  glyph->height = h;
  glyph->bearingX = x0;
  glyph->bearingY = y0;
  glyph->advance = advance_w;
  glyph->loaded = true;

  return 1;
}

void lum_tr_draw_layer(lum_Font* font, const char* text, lum_Vec2 position, int32_t layer, lum_Vec4 color) {
  assert(font && text);
  if (!font->initialized) return;
  lum_Text_renderer* tr = (lum_Text_renderer*)font->renderer;

  float x = position.x, y = position.y;
  float pen_x = x;
  float baseline = y;

  for(const char* p = text; *p; ++p) {
    unsigned char c = (unsigned char)(*p);
    assert(c < 128);

    if (c == '\n') {
      pen_x = x;
      baseline -= (float)(tr->ascent - tr->descent + tr->line_gap) * tr->scale;
      continue;
    }

    if (!lum_tr_load_glyph(tr, c)) continue;

    lum_Glyph* glyph = &tr->glyphs[c];

    if (glyph->width > 0 && glyph->height > 0) {
      float xpos = pen_x + (float)glyph->bearingX;
      float ypos = baseline - (float)glyph->bearingY - (float)glyph->height;
      float w = (float)glyph->width;
      float h = (float)glyph->height;

      lum_renderer2d_draw_sprite_ex_layer(&glyph->texture, lum_vec2_create(xpos, ypos), lum_vec2_create(w, h), 0.0f, lum_vec2_0(), layer, color);
    }

    int kerning = 0;
    if (*(p + 1)) {
      int glyph1 = stbtt_FindGlyphIndex(&tr->font, c);
      int glyph2 = stbtt_FindGlyphIndex(&tr->font, (unsigned char)*(p + 1));
      kerning = stbtt_GetGlyphKernAdvance(&tr->font, glyph1, glyph2);
    }
    pen_x += (float)(glyph->advance + kerning) * tr->scale;
  }
}

void lum_tr_draw(lum_Font* font, const char* text, lum_Vec2 position, lum_Vec4 color) {
    lum_tr_draw_layer(font, text, position, 0, color);
}
