#ifndef Lum_Render_Target_H
#define Lum_Render_Target_H

#include "glimmer/lum_math2d.h"
#include "glimmer/lum_texture.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct lum_Render_target {
    bool initialized;
    uint32_t FBO;
    lum_Texture2d color;
    int width, height;
    bool has_depth;
    uint32_t depth_RBO;
} lum_Render_target;

int lum_rt_create(lum_Render_target* rt, int width, int height, bool depth);
void lum_rt_destroy(lum_Render_target* rt);

int lum_rt_resize(lum_Render_target* rt, int width, int height);

void lum_rt_bind(const lum_Render_target* rt);
void lum_rt_bind_screen(void);

const lum_Texture2d* lum_rt_get_color(const lum_Render_target* rt);

#endif //Lum_Render_Target_H