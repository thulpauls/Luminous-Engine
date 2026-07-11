#include <glad/glad.h>   // 必须在 GLFW 之前包含
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "quad.h"
#include "math2d.h"
#include "file_read.h"
#include "shader.h"

// 窗口尺寸变化时的回调
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height);
}

// 处理输入
void processInput(GLFWwindow *window) {
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, 1);
}

GLuint lum_texture_create_2D_from_dir_with_options(const char* path, GLint wrap_s, GLint wrap_t, GLint min_filter, GLint max_filter, bool flip_vertically) {
  GLuint texture;
  glGenTextures(1, &texture);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, max_filter);

  stbi_set_flip_vertically_on_load(flip_vertically);
  
  int width, height, channels;
  unsigned char* data = stbi_load(path, &width, &height, &channels, 0);
  if (data && (channels == 1 || channels == 3 || channels == 4)) {
    GLenum format = channels == 1 ? GL_RED : channels == 3 ? GL_RGB : GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
  } else {
    glDeleteTextures(1, &texture);
    fprintf(stderr, "Failed to load texture.");
    
    return 0;
  }
  stbi_image_free(data);
  
  return texture;
}

GLuint lum_texture_create_2D_from_dir(const char* path, bool flip_vertically) {
  return lum_texture_create_2D_from_dir_with_options(path, GL_REPEAT, GL_REPEAT, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, flip_vertically);
}

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

int main() {
  const char *vertexShaderSource = lum_file_read("C:/C Projects/Luminous/src/shaders/vertex.shader");
  const char *fragmentShaderSource = lum_file_read("C:/C Projects/Luminous/src/shaders/fragment.shader");
  const char *text_vertex_source = lum_file_read("C:/C Projects/Luminous/src/shaders/text_renderer_vertex.shader");
  const char *text_fragment_source = lum_file_read("C:/C Projects/Luminous/src/shaders/text_renderer_fragment.shader");

  // 1. 初始化 GLFW
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }
    // 指定使用 OpenGL 3.3 核心模式
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 2. 创建窗口
    GLFWwindow* window = glfwCreateWindow(800, 600, "Luminous Playground", NULL, NULL);
    if (window == NULL) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    // 设置视口回调
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // 3. 初始化 GLAD（加载 OpenGL 函数）
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "Failed to initialize GLAD\n");
        return -1;
    }

    // 4. 设置视口（初始值）
    glViewport(0, 0, 800, 600);

    lum_Shader shader;
    lum_shader_create_from_source(&shader, vertexShaderSource, fragmentShaderSource);

    // 6. 定义三角形顶点数据
    float vertices[] = {
        // positions        // texCoords
         0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
         0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
        -0.5f,  0.5f, 0.0f, 0.0f, 1.0f
    };

    unsigned int indices[] = {
        0, 1, 3,
        1, 2, 3
    };

    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 解绑（可选，但保持整洁）
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    GLuint texture = lum_texture_create_2D_from_dir("texture.jpg", true);
    glBindTexture(GL_TEXTURE_2D, texture);

    shader.initialized = true;
    lum_shader_uniform_set1i(&shader, "texture1", 0);

    lum_Text_renderer tr;
    if (!lum_text_initialize_renderer(&tr, "FiraCode-Regular.ttf", 48.0f)) {
      fprintf(stderr, "Failed to initialize text renderer.");
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    
  lum_Quad quad;
  assert(lum_quad_init(&quad));
  assert(lum_float_nearly_equal(1.0f, 1.1f, 0.000001f, 0.1f));

  lum_Mat4 m = lum_mat4_translate(5.0f, 0.0f, 2.0f), m1 = lum_mat4_rotate_z(lum_deg_to_rad(lum_rad_to_deg(Lum_Pi / 2)));
  lum_mat4_print(m);
  lum_Vec4 v = lum_vec4_create(90.0f, 20.0f, 35.0f, 1.0f);
  printf("\n");
  lum_mat4_print(m1);
  printf("\n");
  lum_vec4_print(v);
  printf("\n");
  lum_vec4_print(lum_mat4_mul_vec4(m, v));
  printf("\n");
  lum_vec4_print(lum_mat4_mul_vec4(m1, lum_mat4_mul_vec4(m, v)));
        // 7. 渲染循环
    while (!glfwWindowShouldClose(window)) {      
        // 输入处理
        processInput(window);

        // 清屏（用深蓝灰色背景）
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

       	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, texture);
	
        // 使用着色器程序并绘制三角形
	lum_shader_use(&shader);

	lum_quad_draw(&quad);

	lum_text_draw(&tr, "Hello Luminous!", 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 800, 600);
	
        // 交换缓冲，轮询事件
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 8. 清理资源
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    lum_shader_destroy(&shader);

    lum_text_destroy_renderer(&tr);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
