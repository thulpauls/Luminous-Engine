#ifndef Lum_Input_H
#define Lum_Input_H

#include "lum_math2d.h"
#include "lum_input_codes.h"

int lum_input_init(void);
void lum_input_shutdown(void);
bool lum_input_is_initialized(void);

void lum_input_update(void);

bool lum_input_is_key_down(lum_Keycode key);
bool lum_input_is_key_pressed(lum_Keycode key);
bool lum_input_is_key_released(lum_Keycode key);

bool lum_input_is_mouse_down(lum_Mousebutton button);
bool lum_input_is_mouse_pressed(lum_Mousebutton button);
bool lum_input_is_mouse_released(lum_Mousebutton button);

lum_Vec2 lum_input_get_mouse_position(void);
lum_Vec2 lum_input_get_mouse_position_delta(void);

lum_Vec2 lum_input_get_scroll(void);

#endif
