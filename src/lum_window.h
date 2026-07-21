#ifndef Lum_Window_H
#define Lum_Window_H

#include <stdint.h>

int lum_window_init(const char* title, uint32_t w, uint32_t h);
void lum_window_shutdown(void);
bool lum_window_is_initialized(void);

bool lum_window_is_open(void);

void lum_window_set_title(const char* title);
void lum_window_set_vsync(bool enabled);

void lum_window_poll_events(void);
void lum_window_swap_buffers(void);

void* lum_window_get_native_handle(void);

uint32_t lum_window_get_width(void);
uint32_t lum_window_get_height(void);
uint32_t lum_window_get_framebuffer_width(void);
uint32_t lum_window_get_framebuffer_height(void);

float lum_window_get_time(void);
void lum_window_sleep(float sec);

void lum_window_set_should_close(bool value);

#endif
