#include "glimmer/lum_atlas.h"
#include "glimmer/lum_assert.h"
#include <string.h>

int lum_atlas_init(lum_Atlas* atlas, int width, int height, int padding) {
  Lum_Assert(atlas);
  memset(atlas, 0, sizeof(*atlas));

  if (width <= 0 || height <= 0 || padding < 0) return 0;

  atlas->width = width;
  atlas->height = height;
  atlas->padding = padding;
  atlas->initialized = true;
  return 1;
}

void lum_atlas_shutdown(lum_Atlas* atlas) {
  Lum_Assert(atlas);
  memset(atlas, 0, sizeof(*atlas));
}

void lum_atlas_reset(lum_Atlas* atlas) {
  Lum_Assert(atlas);
  if (!atlas->initialized) return;
  atlas->cursor_x = 0;
  atlas->row_y = 0;
  atlas->row_height = 0;
}

int lum_atlas_alloc(lum_Atlas* atlas, int w, int h, lum_Atlas_slot* out_slot) {
  Lum_Assert(atlas && out_slot);
  Lum_Assert(w > 0 && h > 0);
  if (!atlas->initialized) return 0;
  if (w <= 0 || h <= 0) return 0;
  /* 单项超页在任何布局下都放不下，直接判满（否则开新行后仍会越出右边界） */
  if (w > atlas->width || h > atlas->height) return 0;

  if (atlas->cursor_x + w > atlas->width) {
    /* 行满 → 开新行；新行底边在当前行顶 + padding */
    atlas->row_y += atlas->row_height + atlas->padding;
    atlas->row_height = 0;
    atlas->cursor_x = 0;
    if (atlas->row_y + h > atlas->height) return 0;   /* 页满 */
  }

  out_slot->x = atlas->cursor_x;
  out_slot->y = atlas->row_y;

  if (h > atlas->row_height) atlas->row_height = h;
  atlas->cursor_x += w + atlas->padding;

  return 1;
}
