#include "lum_texture.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

uint32_t lum_texture_create_2d_with_options(const unsigned char* pixels, int wrap_s, int wrap_t, int min_filter, int max_filter, int width, int height, uint32_t channels) {
  assert(pixels);
  
  GLuint texture;
  glGenTextures(1, &texture);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, max_filter);

  if (channels == 1 || channels == 3 || channels == 4) {
    GLenum format = channels == 1 ? GL_RED : channels == 3 ? GL_RGB : GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
  } else {
    glDeleteTextures(1, &texture);
    fprintf(stderr, "Failed to load texture.");
    
    return 0;
  }
  
  return texture;
}

uint32_t lum_texture_create_2d(const unsigned char* pixels, int width, int height, uint32_t channels) {
  return lum_texture_create_2d_with_options(pixels, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_NEAREST, GL_NEAREST, width, height, channels);
}

int lum_texture2d_create(lum_Texture2d* texture, int width, int height, uint32_t channels, const unsigned char* pixels) {
  assert(texture);

  uint32_t id = lum_texture_create_2d(pixels, width, height, channels);
  if (!id) return 0;
  texture->id = id;
  texture->width = width;
  texture->height = height;
  texture->channels = channels;
  texture->initialized = true;

  return 1;
}

int lum_texture2d_create_white(lum_Texture2d* texture) {
  static const unsigned char white_pixel[4] = {255, 255, 255, 255};
  return lum_texture2d_create(texture, 1, 1, 4, white_pixel);
}

int lum_texture2d_load_from_file(lum_Texture2d* texture, const char* path, uint32_t desired_channels) {
  assert(path);

  int width, height, channels;
  stbi_set_flip_vertically_on_load(1);
  unsigned char* pixels = stbi_load(path, &width, &height, &channels, desired_channels);
  if (!pixels) return 0;

  int upload_channels = desired_channels ? desired_channels : channels;
  int success = lum_texture2d_create(texture, width, height, upload_channels, pixels);
  stbi_image_free(pixels);

  return success;
}

void lum_texture2d_destroy(lum_Texture2d* texture) {
  assert(texture);
  if (!texture->initialized) return;

  glDeleteTextures(1, &texture->id);
  memset(texture, 0, sizeof(*texture));
}

void lum_texture2d_bind(const lum_Texture2d* texture, uint32_t slot) {
  assert(texture);
  if (!texture->initialized) return;

  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_2D, texture->id);
}

void lum_texture2d_unbind(uint32_t slot) {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_2D, 0);
}
