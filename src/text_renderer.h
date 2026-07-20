#if 0
typedef struct lum_Glyph {
  GLuint texture;
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
  GLuint VAO, VBO, shader;
} lum_Text_renderer;

int lum_text_initialize_renderer(lum_Text_renderer* tr, const char* font, float height_pixel) {
  memset(tr, 0, sizeof(*tr));
  
  const char *text_vertex_source = lum_file_read("C:/C Projects/Luminous/src/shaders/text_renderer_vertex.shader");
  const char *text_fragment_source = lum_file_read("C:/C Projects/Luminous/src/shaders/text_renderer_fragment.shader");
  GLuint text_renderer_shader_program = lum_shader_create_program_from_source(text_vertex_source, text_fragment_source);

  tr->font_buffer = lum_file_read_binary(font).data;
  if (!tr->font_buffer) return 0;

  if (!stbtt_InitFont(&tr->font, tr->font_buffer, stbtt_GetFontOffsetForIndex(tr->font_buffer, 0))) {
    tr->font_buffer = NULL;
    fprintf(stderr, "Failed to initialize stb_tt font.");
    return 0;
  }

  tr->scale = stbtt_ScaleForPixelHeight(&tr->font, height_pixel);
  stbtt_GetFontVMetrics(&tr->font, &tr->ascent, &tr->descent, &tr->line_gap);

  tr->shader = text_renderer_shader_program;

  glGenVertexArrays(1, &tr->VAO);
  glGenBuffers(1, &tr->VBO);
  glBindVertexArray(tr->VAO);
  glBindBuffer(GL_ARRAY_BUFFER, tr->VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  return 1;
}

int lum_text_load_glyph(lum_Text_renderer* tr, unsigned int codepoint) {
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

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  }

  glGenTextures(1, &glyph->texture);
  glBindTexture(GL_TEXTURE_2D, glyph->texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w > 0 ? w : 1, h > 0 ? h : 1, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  if (bitmap) free(bitmap);

  glyph->width = w;
  glyph->height = h;
  glyph->bearingX = x0;
  glyph->bearingY = y0;
  glyph->advance = advance_w;
  glyph->loaded = true;

  return 1;
}

void lum_text_draw(lum_Text_renderer* tr, const char* text, float x, float y, float r, float g, float b, int screen_w, int screen_h) {
  assert(tr && text);

  glUseProgram(tr->shader);

  float projection[16] = {
    2.0f / (float)screen_w, 0, 0, 0,
    0, -2.0f / (float)screen_h, 0, 0,
    0, 0, -1.0f, 0,
    -1.0f, 1.0f, 0, 1.0f
  };

  glUniformMatrix4fv(glGetUniformLocation(tr->shader, "projection"), 1, GL_FALSE, projection);
  glUniform3f(glGetUniformLocation(tr->shader, "textColor"), r, g, b);
  glUniform1i(glGetUniformLocation(tr->shader, "glyphTexture"), 0);

  glActiveTexture(GL_TEXTURE0);
  glBindVertexArray(tr->VAO);

  float pen_x = x;
  float baseline = y + (float)tr->ascent * tr->scale;

  for(const char* p = text; *p; ++p) {
    unsigned char c = (unsigned char)(*p);
    assert(c < 128);
    
    if (c == '\n') {
      pen_x = x;
      baseline += (float)(tr->ascent - tr->descent + tr->line_gap) * tr->scale;
      continue;
    }

    if (!lum_text_load_glyph(tr, c)) continue;

    lum_Glyph* glyph = &tr->glyphs[c];

    float xpos = pen_x + (float)glyph->bearingX;
    float ypos = baseline + (float)glyph->bearingY;
    float w = (float)glyph->width;
    float h = (float)glyph->height;

    float vertices[6][4] = {
      { xpos,     ypos,    0.0f, 0.0f },
      { xpos + w, ypos,     1.0f, 0.0f },
      { xpos + w, ypos + h, 1.0f, 1.0f },

      { xpos,     ypos,     0.0f, 0.0f },
      { xpos + w, ypos + h, 1.0f, 1.0f },
      { xpos,     ypos + h, 0.0f, 1.0f }
    };

    glBindTexture(GL_TEXTURE_2D, glyph->texture);
    glBindBuffer(GL_ARRAY_BUFFER, tr->VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    int kerning = 0;
    if (*(p + 1)) {
      int glyph1 = stbtt_FindGlyphIndex(&tr->font, c);
      int glyph2 = stbtt_FindGlyphIndex(&tr->font, (unsigned char)*(p + 1));
      kerning = stbtt_GetGlyphKernAdvance(&tr->font, glyph1, glyph2);
    }
    pen_x += (float)(glyph->advance + kerning) * tr->scale;
  }
  
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void lum_text_destroy_renderer(lum_Text_renderer* tr) {
  if (!tr) return;

  for (size_t i = 0; i < 128; ++i) {
    if (tr->glyphs[i].texture) {
      glDeleteTextures(1, &tr->glyphs[i].texture);
      tr->glyphs[i].texture = 0;
    }
  }

  if (tr->VBO) {
    glDeleteBuffers(1, &tr->VBO);
    tr->VBO = 0;
  }

  if (tr->VAO) {
    glDeleteVertexArrays(1, & tr->VAO);
    tr->VAO = 0;
  }

  if (tr->font_buffer) {
    free(tr->font_buffer);
    tr->font_buffer = NULL;
  }
}
#endif
