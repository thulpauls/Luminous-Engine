#ifndef Lum_Application_H
#define Lum_Application_H

#include <stdint.h>
#include "lum_math2d.h"
#include "lum_log.h"
#include "lum_camera.h"

typedef struct lum_App_desc {
    bool initialized;

    const char* title;
    uint32_t width;
    uint32_t height;
    bool vsync;
    bool resizable;

    lum_Vec4 clear_color;

    lum_Log_level log_level;

    uint32_t max_fps;

    void (*on_start)(void);
    void (*on_update)(float dt);
    void (*on_render)(void);
    void (*on_shutdown)(void);
} lum_App_desc;

int lum_app_init_desc(lum_App_desc* desc, void (*on_start)(void), void (*on_update)(float dt), void (*on_render)(void), void (*on_shutdown)(void));

int lum_app_run(const lum_App_desc* desc);
void lum_app_quit(void);

struct lum_Camera2d* lum_app_get_camera(void);

float lum_app_get_delta(void);

lum_Vec2 lum_app_screen_to_world(lum_Vec2 screen);
lum_Vec2 lum_app_world_to_screen(lum_Vec2 world);

#endif //Lum_Application_H