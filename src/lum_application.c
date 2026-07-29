#include "lum_application.h"
#include "lum_window.h"
#include "lum_input.h"
#include "lum_time.h"
#include "lum_renderer2d.h"
#include <string.h>
#include <stdbool.h>
#include <assert.h>

typedef struct lum_Application {
    bool running;
    uint32_t last_framebuffer_w, last_framebuffer_h;
    lum_Camera2d camera;
    float delta;
} lum_Application;

static lum_Application g_application;

static int lum_app_init_subsystems(const lum_App_desc* desc) {
    if (!lum_log_init()) return 0;
    lum_log_set_min_level(desc->log_level);

    if (!lum_window_init(desc->title, desc->width, desc->height)) {
        Lum_Log_Fatal("app: window init failed.");
        goto failure_window;
    }
    lum_window_set_vsync(desc->vsync);

    if (!lum_input_init()) {
        Lum_Log_Fatal("app: input init failed.");
        goto failure_input;
    }

    if (!lum_time_init()) {
        Lum_Log_Fatal("app: time init failed.");
        goto failure_time;
    }
    if (desc->max_fps > 0) lum_time_set_max_fps(desc->max_fps);
    
    if (!lum_renderer2d_init(desc->width, desc->height)) {
        Lum_Log_Fatal("app: renderer2d init failed.");
        goto failure_renderer2d;
    }
    lum_renderer2d_set_clear_color(desc->clear_color);
    lum_renderer2d_enable_blend();

    lum_camera2d_init(&g_application.camera, (float)desc->width, (float)desc->height);

    g_application.last_framebuffer_w = lum_window_get_framebuffer_width();
    g_application.last_framebuffer_h = lum_window_get_framebuffer_height();
    return 1;

failure_renderer2d:
    lum_time_shutdown();
failure_time:
    lum_input_shutdown();
failure_input:
    lum_window_shutdown();
failure_window:
    lum_log_shutdown();
    return 0;
}

static void lum_app_shutdown_subsystems(void) {
    lum_renderer2d_shutdown();
    lum_time_shutdown();
    lum_input_shutdown();
    lum_window_shutdown();
    lum_log_shutdown();
}

static void lum_app_sync_viewport(void) {
    uint32_t framebuffer_w = lum_window_get_framebuffer_width();
    uint32_t framebuffer_h = lum_window_get_framebuffer_height();
    if (framebuffer_w == g_application.last_framebuffer_w && framebuffer_h == g_application.last_framebuffer_h) return;

    g_application.last_framebuffer_w = framebuffer_w;
    g_application.last_framebuffer_h = framebuffer_h;

    lum_renderer2d_set_viewport(0, 0, (int)framebuffer_w, (int)framebuffer_h);
    lum_camera2d_set_viewport(&g_application.camera, (float)framebuffer_w, (float)framebuffer_h);
}

int lum_app_init_desc(lum_App_desc* desc, void (*on_start)(void), void (*on_update)(float dt), void (*on_render)(void), void (*on_shutdown)(void)) {
    memset(desc, 0, sizeof(*desc));
    desc->title = "No title";
    desc->width = 800;
    desc->height = 600;
    desc->max_fps = 0;
    desc->log_level = Lum_Log_Level_Info;
    desc->vsync = true;
    desc->resizable = true;
    desc->clear_color = lum_vec4_0();

    desc->on_start = on_start;
    desc->on_update = on_update;
    desc->on_render = on_render;
    desc->on_shutdown = on_shutdown;

    desc->initialized = true;
}

int lum_app_run(const lum_App_desc* desc) {
    assert(desc);
    if (!desc->initialized) return 0;

    lum_App_desc d = *desc;

    memset(&g_application, 0, sizeof(g_application));
    g_application.running = true;

    if (!lum_app_init_subsystems(&d)) return 0;

    if (d.on_start) d.on_start();

    while (lum_window_is_open() && g_application.running) {
        lum_time_begin_frame();
        lum_window_poll_events();
        lum_input_update();
        lum_time_update();
        g_application.delta = lum_time_get_delta();

        lum_app_sync_viewport();
        lum_renderer2d_set_camera(&g_application.camera);

        if (d.on_update) d.on_update(g_application.delta);

        lum_renderer2d_begin_frame();
        lum_renderer2d_clear();
        if (d.on_render) d.on_render();
        lum_renderer2d_end_frame();

        lum_window_swap_buffers();
        lum_time_end_frame();
    }

    if (d.on_shutdown) d.on_shutdown();

    lum_app_shutdown_subsystems();
    return 0;
}

void lum_app_quit(void) {
    g_application.running = false;
    lum_window_set_should_close(true);
}

lum_Camera2d* lum_app_get_camera(void) {
    return &g_application.camera;
}

float lum_app_get_delta(void) {
    return g_application.delta;
}

lum_Vec2 lum_app_screen_to_world(lum_Vec2 screen) {
  return lum_camera2d_screen_to_world(&g_application.camera, screen, 
    lum_vec2_create((float)lum_window_get_width(), (float)lum_window_get_height()));
}
lum_Vec2 lum_app_world_to_screen(lum_Vec2 world) {
  return lum_camera2d_world_to_screen(&g_application.camera, world, 
    lum_vec2_create((float)lum_window_get_width(), (float)lum_window_get_height()));
}