#include "lum_input.h"
#include "lum_window.h"
#include <GLFW/glfw3.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>

typedef struct lum_Input {
  bool initialized;

  unsigned char current_keys[Lum_Key_Last + 1];
  unsigned char previous_keys[Lum_Key_Last + 1];

  unsigned char current_mouse_buttons[Lum_Mouse_Button_Last + 1];
  unsigned char previous_mouse_buttons[Lum_Mouse_Button_Last + 1];

  lum_Vec2 mouse_position;
  lum_Vec2 mouse_position_delta;
  lum_Vec2 previous_mouse_position;
  lum_Vec2 scroll;
} lum_Input;

static lum_Input g_input;

static void lum_input_reset(void) {
  memset(&g_input, 0, sizeof(g_input));
}

static void lum_input_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
  g_input.scroll.x += (float)xoffset;
  g_input.scroll.y += (float)yoffset;
}

int lum_input_init(void) {
  lum_input_reset();
  GLFWwindow* window = (GLFWwindow*)lum_window_get_native_handle();
  if (!window) return 0;

  double mouse_x, mouse_y;
  glfwSetScrollCallback(window, lum_input_scroll_callback);
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
  lum_input_reset();
}

void lum_input_update(void) {
  if (!g_input.initialized) return;

  GLFWwindow* window = (GLFWwindow*)lum_window_get_native_handle();
  if (!window) return;

  memcpy(g_input.previous_keys, g_input.current_keys, sizeof(g_input.current_keys));
  memcpy(g_input.previous_mouse_buttons, g_input.current_mouse_buttons, sizeof(g_input.current_mouse_buttons));
  g_input.scroll = lum_vec2_0();

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
  assert(key >= 0 && key <= Lum_Key_Last);
  if (!g_input.initialized) return false;
  return g_input.current_keys[key];
}

bool lum_input_is_key_pressed(lum_Keycode key) {
  assert(key >= 0 && key <= Lum_Key_Last);
  if (!g_input.initialized) return false;
  return g_input.current_keys[key] && !g_input.previous_keys[key];
}

bool lum_input_is_key_released(lum_Keycode key) {
  assert(key >= 0 && key <= Lum_Key_Last);
  if (!g_input.initialized) return false;
  return !g_input.current_keys[key] && g_input.previous_keys[key];
}

bool lum_input_is_mouse_down(lum_Mousebutton button) {
  assert(button >= 0 && button <= Lum_Mouse_Button_Last);
  if (!g_input.initialized) return false;
  return g_input.current_mouse_buttons[button];
}

bool lum_input_is_mouse_pressed(lum_Mousebutton button) {
  assert(button >= 0 && button <= Lum_Mouse_Button_Last);
  if (!g_input.initialized) return false;
  return g_input.current_mouse_buttons[button] && !g_input.current_mouse_buttons[button];
}

bool lum_input_is_mouse_released(lum_Mousebutton button) {
  assert(button >= 0 && button <= Lum_Mouse_Button_Last);
  if (!g_input.initialized) return false;
  return g_input.current_mouse_buttons[button] && !g_input.current_mouse_buttons[button];
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
