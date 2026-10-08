#ifndef Lum_Input_H
#define Lum_Input_H

#include <stdbool.h>
#include "glimmer/lum_math2d.h"
#include "glimmer/lum_input_codes.h"

#define Lum_Input_Text_Max 32   /* 每帧字符流缓冲（含 NUL） */

int lum_input_init(void);
void lum_input_shutdown(void);
bool lum_input_is_initialized(void);

void lum_input_update(void);

/* 本帧字符流（文本输入用；poll_events 期间由字符回调累积，帧内恒定）：
   返回 NUL 结尾字符串；本帧无输入时返回空串。重复调用返回同一缓冲 */
const char* lum_input_text(void);

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
