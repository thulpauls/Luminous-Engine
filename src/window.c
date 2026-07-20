#include "window.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string.h>
#include <assert.h>

typedef struct lum_Window {
  GLFWwindow* handle;
  uint32_t width, height;
  uint32_t framebuffer_width, framebuffer_height;
  bool initialized;
} lum_Window;

static lum_Window g_window;

static void lum_window_reset(void) {
  memset(&g_window, 0, sizeof(g_window));
}

static void lum_window_framebuffer_size_callback(GLFWwindow* window, int w, int h) {
  g_window.framebuffer_width = (uint32_t)w;
  g_window.framebuffer_height = (uint32_t)h;
  glViewport(0, 0, w, h);
}

static void lum_window_size_callback(GLFWwindow* window, int w, int h) {
  g_window.width = (uint32_t)w;
  g_window.height = (uint32_t)h;
}

int lum_window_init(const char* title, uint32_t w, uint32_t h) {
  assert(title);
  lum_window_reset();

  if (!glfwInit()) return 0;
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow* handle = glfwCreateWindow(w, h, title, NULL, NULL);
  if (!handle) {
    glfwTerminate();
    return 0;
  }

  glfwMakeContextCurrent(handle);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    glfwDestroyWindow(handle);
    glfwTerminate();
    return 0;
  }

  glfwSetFramebufferSizeCallback(handle, lum_window_framebuffer_size_callback);
  glfwSetWindowSizeCallback(handle, lum_window_size_callback);

  g_window.handle = handle;
  g_window.initialized = true;

  glfwGetWindowSize(handle, &g_window.width, &g_window.height);
  glfwGetFramebufferSize(handle, &g_window.framebuffer_width, &g_window.framebuffer_height);

  return 1;
}

void lum_window_shutdown(void) {
  if (!g_window.initialized) return;
  glfwDestroyWindow(g_window.handle);
  glfwTerminate();
  lum_window_reset();
}

bool lum_window_is_initialized(void) {
  return g_window.initialized;
}

bool lum_window_is_open(void) {
  if (!g_window.initialized) return false;
  return !glfwWindowShouldClose(g_window.handle);
}

void lum_window_set_title(const char* title) {
  assert(title);
  if (!g_window.initialized) return;
  glfwSetWindowTitle(g_window.handle, title);
}

void lum_window_set_vsync(bool enabled) {
  if (!g_window.initialized) return;
  glfwSwapInterval(enabled);
}

void lum_window_poll_events(void) {
  if (!g_window.initialized) return;
  glfwPollEvents();
}

void lum_window_swap_buffers(void) {
  if (!g_window.initialized) return;
  glfwSwapBuffers(g_window.handle);
}

void* lum_window_get_native_handle(void) {
  if (!g_window.initialized) return NULL;
  return (void*)g_window.handle;
}

uint32_t lum_window_get_width(void) {
  if (!g_window.initialized) return 0;
  return g_window.width;
}

uint32_t lum_window_get_height(void) {
  if (!g_window.initialized) return 0;
  return g_window.height;
}

uint32_t lum_window_get_framebuffer_width(void) {
  if (!g_window.initialized) return 0;
  return g_window.framebuffer_width;
}

uint32_t lum_window_get_framebuffer_height(void) {
  if (!g_window.initialized) return 0;
  return g_window.framebuffer_height;
}

void lum_window_set_should_close(bool value) {
  if (!g_window.initialized) return;
  glfwSetWindowShouldClose(g_window.handle, value ? GLFW_TRUE : GLFW_FALSE);
}
