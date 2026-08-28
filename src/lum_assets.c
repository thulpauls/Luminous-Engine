#include "lum_assets.h"
#include "containers/lum_vector.h"
#include "lum_assert.h"
#include "lum_log.h"
#include <stdlib.h>
#include <string.h>

typedef enum lum_Asset_type {
  Lum_Asset_Texture,
  Lum_Asset_Font
} lum_Asset_type;

typedef struct lum_Asset_entry {
  lum_Asset_type type;
  char* path;            /* 拥有的拷贝，去重键的一部分 */
  union {
    uint32_t texture_flags;
    float    font_height;
  } params;
  void* resource;        /* 对象本体一次分配、永不搬移，指针返回后永久有效 */
  bool  failed;
} lum_Asset_entry;

static lum_Vector g_assets;

static char* lum__assets_copy_string(const char* src) {
  size_t len = strlen(src) + 1;
  char* copy = (char*)malloc(len);
  if (copy) memcpy(copy, src, len);
  return copy;
}

static lum_Asset_entry* lum__assets_find_texture(const char* path, uint32_t flags) {
  for (uint32_t i = 0; i < lum_vector_size(&g_assets); ++i) {
    lum_Asset_entry* entry = (lum_Asset_entry*)lum_vector_get(&g_assets, i);
    if (entry->type == Lum_Asset_Texture &&
        entry->params.texture_flags == flags &&
        strcmp(entry->path, path) == 0) {
      return entry;
    }
  }
  return NULL;
}

static lum_Asset_entry* lum__assets_find_font(const char* path, float height) {
  for (uint32_t i = 0; i < lum_vector_size(&g_assets); ++i) {
    lum_Asset_entry* entry = (lum_Asset_entry*)lum_vector_get(&g_assets, i);
    if (entry->type == Lum_Asset_Font &&
        entry->params.font_height == height &&
        strcmp(entry->path, path) == 0) {
      return entry;
    }
  }
  return NULL;
}

static void lum__assets_release_all(void) {
  for (uint32_t i = 0; i < lum_vector_size(&g_assets); ++i) {
    lum_Asset_entry* entry = (lum_Asset_entry*)lum_vector_get(&g_assets, i);
    if (entry->resource) {
      switch (entry->type) {
        case Lum_Asset_Texture: {
          lum_Texture2d* texture = (lum_Texture2d*)entry->resource;
          lum_texture2d_destroy(texture);
          free(texture);
          break;
        }
        case Lum_Asset_Font: {
          lum_Font* font = (lum_Font*)entry->resource;
          lum_tr_destroy_font(font);
          free(font);
          break;
        }
      }
      entry->resource = NULL;
    }
    free(entry->path);
    entry->path = NULL;
  }
}

int lum_assets_init(void) {
  if (g_assets.elem_size == 0) {
    if (!lum_vector_init(&g_assets, sizeof(lum_Asset_entry))) return 0;
  } else {
    lum__assets_release_all();   /* 进程内重复 init：释放旧条目后重置 */
    lum_vector_clear(&g_assets);
  }
  return 1;
}

void lum_assets_shutdown(void) {
  if (g_assets.elem_size == 0) return;
  lum__assets_release_all();
  lum_vector_destroy(&g_assets);
}

const lum_Texture2d* lum_assets_texture(const char* path, uint32_t flags) {
  if (g_assets.elem_size == 0 || !path) return NULL;

  lum_Asset_entry* entry = lum__assets_find_texture(path, flags);
  if (entry) return entry->failed ? NULL : (const lum_Texture2d*)entry->resource;

  entry = (lum_Asset_entry*)lum_vector_emplace(&g_assets);
  if (!entry) return NULL;
  entry->type = Lum_Asset_Texture;
  entry->params.texture_flags = flags;
  entry->resource = NULL;
  entry->failed = false;
  entry->path = lum__assets_copy_string(path);
  if (!entry->path) { entry->failed = true; return NULL; }

  lum_Texture2d* texture = (lum_Texture2d*)malloc(sizeof(*texture));
  if (!texture) { entry->failed = true; return NULL; }
  if (!lum_texture2d_load_from_file(texture, path, 0, flags)) {   /* 0 = 保留源格式 */
    free(texture);
    entry->failed = true;
    Lum_Log_Error("assets: texture load failed: %s", path);
    return NULL;
  }
  entry->resource = texture;
  return texture;
}

lum_Font* lum_assets_font(const char* path, float height_pixel) {
  if (g_assets.elem_size == 0 || !path) return NULL;

  lum_Asset_entry* entry = lum__assets_find_font(path, height_pixel);
  if (entry) return entry->failed ? NULL : (lum_Font*)entry->resource;

  entry = (lum_Asset_entry*)lum_vector_emplace(&g_assets);
  if (!entry) return NULL;
  entry->type = Lum_Asset_Font;
  entry->params.font_height = height_pixel;
  entry->resource = NULL;
  entry->failed = false;
  entry->path = lum__assets_copy_string(path);
  if (!entry->path) { entry->failed = true; return NULL; }

  lum_Font* font = (lum_Font*)malloc(sizeof(*font));
  if (!font) { entry->failed = true; return NULL; }
  if (!lum_tr_load_font(font, path, height_pixel)) {
    free(font);
    entry->failed = true;
    Lum_Log_Error("assets: font load failed: %s", path);
    return NULL;
  }
  entry->resource = font;
  return font;
}

uint32_t lum_assets_count(void) {
  return g_assets.elem_size == 0 ? 0u : lum_vector_size(&g_assets);
}

const char* lum_assets_texture_path(const lum_Texture2d* texture) {
  if (g_assets.elem_size == 0 || !texture) return NULL;
  for (uint32_t i = 0; i < lum_vector_size(&g_assets); ++i) {
    const lum_Asset_entry* entry = (const lum_Asset_entry*)lum_vector_get_const(&g_assets, i);
    if (entry->type == Lum_Asset_Texture && !entry->failed &&
        entry->resource == (const void*)texture) {
      return entry->path;
    }
  }
  return NULL;
}
