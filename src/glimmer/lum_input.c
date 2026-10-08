#include "glimmer/lum_input.h"
#include "glimmer/lum_window.h"
#include <GLFW/glfw3.h>
#include <string.h>
#include <stddef.h>
#include "glimmer/lum_assert.h"

typedef struct lum_Input {
  bool initialized;

  unsigned char current_keys[Lum_Key_Last + 1];
  unsigned char previous_keys[Lum_Key_Last + 1];

  unsigned char current_mouse_buttons[Lum_Mouse_Button_Last + 1];
  unsigned char previous_mouse_buttons[Lum_Mouse_Button_Last + 1];

  lum_Vec2 mouse_position;
  lum_Vec2 mouse_position_delta;
  lum_Vec2 previous_mouse_position;
  lum_Vec2 scroll, scroll_acc;

  char text_pending[Lum_Input_Text_Max];    /* 字符回调写入（poll_events 期间） */
  char text_current[Lum_Input_Text_Max];    /* update 时换入，供本帧读取 */
} lum_Input;

static lum_Input g_input;

static void lum__input_reset(void) {
  memset(&g_input, 0, sizeof(g_input));
}

/* 字符输入回调（GLFW char：码点为 Unicode；v1 仅收 ASCII 可打印字符） */
static void lum__input_char_callback(GLFWwindow* window, unsigned int codepoint) {
  (void)window;
  size_t len = strlen(g_input.text_pending);
  if (codepoint >= 32 && codepoint < 127 && len + 1 < Lum_Input_Text_Max) {
    g_input.text_pending[len] = (char)codepoint;
    g_input.text_pending[len + 1] = '\0';
  }
}

static void lum__input_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
  (void)window;
  g_input.scroll_acc.x += (float)xoffset;
  g_input.scroll_acc.y += (float)yoffset;
}

int lum_input_init(void) {
  lum__input_reset();
  GLFWwindow* window = (GLFWwindow*)lum_window_get_native_handle();
  if (!window) return 0;

  double mouse_x, mouse_y;
  glfwSetScrollCallback(window, lum__input_scroll_callback);
  glfwSetCharCallback(window, lum__input_char_callback);
  glfwGetCursorPos(window, &mouse_x, &mouse_y);
  g_input.mouse_position.x = (float)mouse_x;
  g_input.mouse_position.y = (float)mouse_y;
  g_input.previous_mouse_position.x = (float)mouse_x;
  g_input.previous_mouse_position.y = (float)mouse_y;

  g_input.initialized = true;
  return 1;
}

void lum_input_shutdown(void) {
  if (!g_input.initialized) return;
  lum__input_reset();
}

bool lum_input_is_initialized(void) {
  return g_input.initialized;
}

void lum_input_update(void) {
  if (!g_input.initialized) return;

  GLFWwindow* window = (GLFWwindow*)lum_window_get_native_handle();
  if (!window) return;

  memcpy(g_input.previous_keys, g_input.current_keys, sizeof(g_input.current_keys));
  memcpy(g_input.previous_mouse_buttons, g_input.current_mouse_buttons, sizeof(g_input.current_mouse_buttons));
  g_input.scroll = g_input.scroll_acc;
  g_input.scroll_acc = lum_vec2_0();

  /* 本帧字符流：poll_events 期间累积的 pending 换入 current 并清空 */
  memcpy(g_input.text_current, g_input.text_pending, sizeof(g_input.text_current));
  memset(g_input.text_pending, 0, sizeof(g_input.text_pending));

  for (size_t key = 0; key <= Lum_Key_Last; ++key) {
    int state = glfwGetKey(window, key);
    g_input.current_keys[key] = (unsigned char)((state == GLFW_PRESS || state == GLFW_REPEAT) ? 1 : 0);
  }

  for (size_t button = 0; button <= Lum_Mouse_Button_Last; ++button) {
    int state = glfwGetMouseButton(window, button);
    g_input.current_mouse_buttons[button] = (unsigned char)(state == GLFW_PRESS ? 1 : 0);
  }

  double mouse_x, mouse_y;
  glfwGetCursorPos(window, &mouse_x, &mouse_y);
  g_input.previous_mouse_position = g_input.mouse_position;
  g_input.mouse_position.x = (float)mouse_x;
  g_input.mouse_position.y = (float)mouse_y;
  g_input.mouse_position_delta.x = g_input.mouse_position.x - g_input.previous_mouse_position.x;
  g_input.mouse_position_delta.y = g_input.mouse_position.y - g_input.previous_mouse_position.y;
}

bool lum_input_is_key_down(lum_Keycode key) {
  Lum_Assert(key >= 0 && key <= Lum_Key_Last);
  if (!g_input.initialized) return false;
  return g_input.current_keys[key];
}

bool lum_input_is_key_pressed(lum_Keycode key) {
  Lum_Assert(key >= 0 && key <= Lum_Key_Last);
  if (!g_input.initialized) return false;
  return g_input.current_keys[key] && !g_input.previous_keys[key];
}

bool lum_input_is_key_released(lum_Keycode key) {
  Lum_Assert(key >= 0 && key <= Lum_Key_Last);
  if (!g_input.initialized) return false;
  return !g_input.current_keys[key] && g_input.previous_keys[key];
}

bool lum_input_is_mouse_down(lum_Mousebutton button) {
  Lum_Assert(button >= 0 && button <= Lum_Mouse_Button_Last);
  if (!g_input.initialized) return false;
  return g_input.current_mouse_buttons[button];
}

bool lum_input_is_mouse_pressed(lum_Mousebutton button) {
  Lum_Assert(button >= 0 && button <= Lum_Mouse_Button_Last);
  if (!g_input.initialized) return false;
  return g_input.current_mouse_buttons[button] && !g_input.previous_mouse_buttons[button];
}

bool lum_input_is_mouse_released(lum_Mousebutton button) {
  Lum_Assert(button >= 0 && button <= Lum_Mouse_Button_Last);
  if (!g_input.initialized) return false;
  return !g_input.current_mouse_buttons[button] && g_input.previous_mouse_buttons[button];
}

lum_Vec2 lum_input_get_mouse_position(void) {
  return g_input.mouse_position;
}

lum_Vec2 lum_input_get_mouse_position_delta(void) {
  return g_input.mouse_position_delta;
}

lum_Vec2 lum_input_get_scroll(void) {
  return g_input.scroll;
}

const char* lum_input_text(void) {
  return g_input.text_current;
}
