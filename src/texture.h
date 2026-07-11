#ifndef Lum_Texture_H
#define Lum_Texture_H

#include <stdint.h>
#include <stddef.h>

typedef struct lum_Texture2d {
  uint32_t id;
  int width, height;
  uint32_t channels;
  bool initialized;
} lum_Texture2d;

uint32_t lum_texture_create_2d_with_options(const unsigned char* pixels, int wrap_s, int wrap_t, int min_filter, int max_filter, int width, int height, uint32_t channels);
uint32_t lum_texture_create_2d(const unsigned char* pixels, int width, int height, uint32_t channels);

int lum_texture2d_create(lum_Texture2d* texture, int width, int height, uint32_t channels, const unsigned char* pixels);
int lum_texture2d_create_white(lum_Texture2d* texture);
int lum_texture2d_load_from_file(lum_Texture2d* texture, const char* path, uint32_t desired_channels);
void lum_texture2d_destroy(lum_Texture2d* texture);

void lum_texture2d_bind(const lum_Texture2d* texture, uint32_t slot);
void lum_texture2d_unbind(uint32_t slot);

#endif //Lum_Texture_H
