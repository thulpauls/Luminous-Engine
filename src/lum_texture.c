#include "lum_texture.h"
#include "lum_log.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include "lum_assert.h"
#include <string.h>
#include <stdlib.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static GLenum lum__texture_pixel_format(uint32_t channels) {
  switch (channels) {
    case 1: return GL_RED;
    case 3: return GL_RGB;
    case 4: return GL_RGBA;
    default: return 0;
  }
}

/* 显式 sized internal format：unsized 格式由驱动自选，GL_R8 是 glyph atlas 的前提 */
static GLenum lum__texture_internal_format(uint32_t channels) {
  switch (channels) {
    case 1: return GL_R8;
    case 3: return GL_RGB8;
    case 4: return GL_RGBA8;
    default: return 0;
  }
}

int lum_texture2d_create(lum_Texture2d* texture, int width, int height, uint32_t channels,
                         const unsigned char* pixels, uint32_t flags) {
  Lum_Assert(texture);
  memset(texture, 0, sizeof(*texture));

  if (width <= 0 || height <= 0) {
    Lum_Log_Error("Failed to create texture: invalid size %dx%d.", width, height);
    return 0;
  }

  GLenum internal = lum__texture_internal_format(channels);
  GLenum format = lum__texture_pixel_format(channels);
  if (!internal || !format) {
    Lum_Log_Error("texture: failed to create texture: unsupported channels %u.", channels);
    return 0;
  }

  if ((flags & Lum_Texture_Flag_Clear) && pixels) {
    Lum_Log_Error("texture: failed to create texture: Flag_Clear requires pixels == NULL.");
    return 0;
  }
  if ((flags & Lum_Texture_Flag_Swizzle_R_To_RGBA) && channels != 1) {
    Lum_Log_Error("texture: failed to create texture: Flag_Swizzle_R_To_RGBA requires channels == 1.");
    return 0;
  }

  GLuint id;
  glGenTextures(1, &id);
  if (!id) return 0;

  bool linear = (flags & Lum_Texture_Flag_Filter_Linear) != 0;
  bool mipmaps = (flags & Lum_Texture_Flag_Mipmaps) != 0;
  GLenum wrap = (flags & Lum_Texture_Flag_Wrap_Repeat) ? GL_REPEAT : GL_CLAMP_TO_EDGE;
  GLenum min_filter = linear ? (mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR) : GL_NEAREST;
  GLenum mag_filter = linear ? GL_LINEAR : GL_NEAREST;

  unsigned char* zeros = NULL;
  const unsigned char* upload = pixels;
  if (flags & Lum_Texture_Flag_Clear) {
    zeros = (unsigned char*)calloc((size_t)width * (size_t)height * (size_t)channels, 1);
    if (!zeros) {
      glDeleteTextures(1, &id);
      Lum_Log_Error("texture: failed to create texture: clear buffer alloc failed (%dx%dx%u).", width, height, channels);
      return 0;
    }
    upload = zeros;
  }

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, id);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0, format, GL_UNSIGNED_BYTE, upload);
  free(zeros);

  if (mipmaps) {
    glGenerateMipmap(GL_TEXTURE_2D);
  }

  /* 单通道 swizzle 成 (r,r,r,r)：让 sprite shader 的 texture()*vColor 染色路径直接可用 */
  if (flags & Lum_Texture_Flag_Swizzle_R_To_RGBA) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
  }

  texture->id = id;
  texture->width = width;
  texture->height = height;
  texture->channels = channels;
  texture->mipmaps = mipmaps;
  texture->initialized = true;
  return 1;
}

int lum_texture2d_update_region(lum_Texture2d* texture, int x, int y, int w, int h, const unsigned char* pixels) {
  Lum_Assert(texture);
  if (!texture->initialized) return 0;
  if (!pixels || w <= 0 || h <= 0) return 0;
  if (x < 0 || y < 0 || x + w > texture->width || y + h > texture->height) {
    Lum_Log_Warn("texture region [%d, %d, %dx%d] out of bounds (texture %dx%d).",
                 x, y, w, h, texture->width, texture->height);
    return 0;
  }

  GLenum format = lum__texture_pixel_format(texture->channels);

  glBindTexture(GL_TEXTURE_2D, texture->id);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, format, GL_UNSIGNED_BYTE, pixels);
  if (texture->mipmaps) {
    glGenerateMipmap(GL_TEXTURE_2D);
  }
  glBindTexture(GL_TEXTURE_2D, 0);

  return 1;
}

lum_Vec4 lum_texture2d_pixel_rect_to_uv(const lum_Texture2d* texture, int x, int y, int w, int h) {
  Lum_Assert(texture);
  if (!texture->initialized) return lum_vec4_create(0.0f, 0.0f, 1.0f, 1.0f);

  float iw = 1.0f / (float)texture->width;
  float ih = 1.0f / (float)texture->height;
  return lum_vec4_create((float)x * iw, (float)y * ih,
                         (float)(x + w) * iw, (float)(y + h) * ih);
}

int lum_texture2d_create_white(lum_Texture2d* texture) {
  static const unsigned char white_pixel[4] = {255, 255, 255, 255};
  return lum_texture2d_create(texture, 1, 1, 4, white_pixel, Lum_Texture_Flag_None);
}

int lum_texture2d_load_from_file(lum_Texture2d* texture, const char* path, uint32_t desired_channels, uint32_t flags) {
  Lum_Assert(path);

  int width, height, channels;
  stbi_set_flip_vertically_on_load(1);
  unsigned char* pixels = stbi_load(path, &width, &height, &channels, desired_channels);
  if (!pixels) return 0;

  int upload_channels = desired_channels ? (int)desired_channels : channels;
  int success = lum_texture2d_create(texture, width, height, upload_channels, pixels, flags);
  stbi_image_free(pixels);

  return success;
}

void lum_texture2d_destroy(lum_Texture2d* texture) {
  Lum_Assert(texture);
  if (!texture->initialized) return;

  glDeleteTextures(1, &texture->id);
  memset(texture, 0, sizeof(*texture));
}

void lum_texture2d_bind(const lum_Texture2d* texture, uint32_t slot) {
  Lum_Assert(texture);
  if (!texture->initialized) return;

  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_2D, texture->id);
}

void lum_texture2d_unbind(uint32_t slot) {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_2D, 0);
}
