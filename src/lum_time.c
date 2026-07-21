#include "lum_time.h"
#include "lum_window.h"
#include "lum_math2d.h"
#include <string.h>
#include <assert.h>

typedef struct lum_Time {
  bool initialized;
  float start, current, previous, delta;
  float max_delta;
  uint32_t frame_count;
  float fps, fps_acc;
  float fps_update_interval;
  uint32_t fps_frame_counter;
  uint32_t max_fps;
  float target_frame_time, frame_begin_time;
} lum_Time;

static lum_Time g_time;

static void lum_time_reset(void) {
  memset(&g_time, 0, sizeof(g_time));
}

int lum_time_init(void) {
  lum_time_reset();

  float now = lum_window_get_time();
  if (now < 0.0f) return 0;
  
  g_time.start = now;
  g_time.current = now;
  g_time.previous = now;
  g_time.delta = 0.0f;
  g_time.max_delta = 0.25f;
  g_time.frame_count = 0;
  g_time.fps = 0.0f;
  g_time.fps_acc = 0.0f;
  g_time.fps_frame_counter = 0;
  g_time.fps_update_interval = 1.0f;

  g_time.initialized = true;
  return 1;
}

void lum_time_shutdown(void) {
  if (!g_time.initialized) return;
  lum_time_reset();
}

bool lum_time_is_initialized(void) {
  return g_time.initialized;
}

void lum_time_begin_frame(void) {
  if (!g_time.initialized) return;
  g_time.frame_begin_time = lum_window_get_time();
}

void lum_time_end_frame(void) {
  if (!g_time.initialized) return;
  if (g_time.target_frame_time <= 0.0f) return;

  float target_time = g_time.frame_begin_time + g_time.target_frame_time;
  float now, remaining;
  for (;;) {
    now = lum_window_get_time();
    remaining = target_time - now;

    if (remaining <= 0.0f) break;

    if (remaining > Lum_Sleep_Margin) {
      lum_window_sleep(remaining - Lum_Sleep_Margin);
    }
  }
}

void lum_time_update(void) {
  if (!g_time.initialized) return;

  float now = lum_window_get_time();
  if (now < 0.0f) return;

  g_time.previous = g_time.current;
  g_time.current = now;
  g_time.delta = lum_maxf(g_time.current - g_time.previous, 0.0f);

  g_time.delta = lum_minf(g_time.delta, g_time.max_delta);
  
  g_time.frame_count++;
  g_time.fps_acc += g_time.delta;
  g_time.fps_frame_counter++;
  
  if (g_time.fps_acc >= g_time.fps_update_interval) {
    g_time.fps = (float)g_time.fps_frame_counter / g_time.fps_acc;
    g_time.fps_acc = 0.0f;
    g_time.fps_frame_counter = 0;
  }
}

float lum_time_get_total(void) {
  if (!g_time.initialized) return 0.0f;
  return g_time.current - g_time.start;
}

float lum_time_get_delta(void) {
  if (!g_time.initialized) return 0.0f;
  return g_time.delta;
}

uint32_t lum_time_get_frame_count(void) {
  if (!g_time.initialized) return 0;
  return g_time.frame_count;
}

float lum_time_get_fps(void) {
  if (!g_time.initialized) return 0.0f;
  return g_time.fps;
}

void lum_time_set_fps_update_interval(float interval) {
  assert(interval > 0.0f);
  if (!g_time.initialized) return;
  g_time.fps_update_interval = interval;
}

float lum_time_get_fps_update_interval(void) {
  if (!g_time.initialized) return 0.0f;
  return g_time.fps_update_interval;
}

void lum_time_set_max_fps(uint32_t max) {
  if (!g_time.initialized) return;
  g_time.max_fps = max;
  g_time.target_frame_time = 1.0f / (float)max;
}

uint32_t lum_time_get_max_fps(void) {
  if (!g_time.initialized) return 0;
  return g_time.max_fps;
}

void lum_time_set_max_delta(float max) {
  assert(max > 0.0f);
  if (!g_time.initialized) return;
  g_time.max_delta = max;
}

float lum_time_get_max_delta(void) {
  if (!g_time.initialized) return 0.0f;
  return g_time.max_delta;
}
