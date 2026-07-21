#ifndef Lum_Time_H
#define Lum_Time_H

#include <stdint.h>

#ifndef Lum_Sleep_Margin
#define Lum_Sleep_Margin (0.030f)
#endif

int lum_time_init(void);
void lum_time_shutdown(void);
bool lum_time_is_initialized(void);

void lum_time_update(void);

float lum_time_get_total(void);
float lum_time_get_delta(void);

uint32_t lum_time_get_frame_count(void);
float lum_time_get_fps(void);
void lum_time_set_fps_update_interval(float interval);
float lum_time_get_fps_update_interval(void);
void lum_time_set_max_fps(uint32_t max);
uint32_t lum_time_get_max_fps(void);

void lum_time_set_max_delta(float max);
float lum_time_get_max_delta(void);

void lum_time_begin_frame(void);
void lum_time_end_frame(void);

#endif //Lum_Time_H
