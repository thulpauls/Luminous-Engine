#ifndef Lum_Atlas_H
#define Lum_Atlas_H

#include <stdint.h>
#include <stdbool.h>

typedef struct lum_Atlas {
  bool initialized;
  int width, height;
  int padding;        /* 相邻项之间的像素间隔，LINEAR 过滤防渗色，通常 1 */
  int cursor_x;       /* 当前行下一个可用位置的 x */
  int row_y;          /* 当前行底边的 y */
  int row_height;     /* 当前行已放项的最大高度 */
} lum_Atlas;

typedef struct lum_Atlas_slot {
  int x, y;           /* 内容区域左下角（像素）；尺寸即 alloc 时传入的 w/h */
} lum_Atlas_slot;

int lum_atlas_init(lum_Atlas* atlas, int width, int height, int padding);
void lum_atlas_shutdown(lum_Atlas* atlas);

int lum_atlas_alloc(lum_Atlas* atlas, int w, int h, lum_Atlas_slot* out_slot);
void lum_atlas_reset(lum_Atlas* atlas);

#endif //Lum_Atlas_H
