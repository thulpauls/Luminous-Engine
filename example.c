#include "src/luminous.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>

typedef struct Snake_body {
  lum_Vec2 position;
  struct Snake_body* next;
  struct Snake_body* prev;
} Snake_body;

typedef struct Snake {
  Snake_body* head;
  Snake_body* tail;
  uint32_t bodies_count;
  uint32_t max_length;
  float head_angle;
} Snake;

typedef struct Gold_blocks {
  lum_Vec2 golds[30];
  size_t gold_count;
} Gold_blocks;

static Snake g_snake;
static Gold_blocks g_golds;
static lum_Vec2 g_new_position;
static lum_Font g_font;
static uint32_t g_score;
static bool g_running;

static Snake_body* make_body(lum_Vec2 position) {
  Snake_body* body = malloc(sizeof(*body));
  memset(body, 0, sizeof(*body));
  body->position = position;
  return body;
}

static void push_body(Snake_body* body) {
  if (!g_snake.bodies_count) {
    g_snake.head = body;
    g_snake.tail = body;
  } else {
    g_snake.tail->next = body;
    body->prev = g_snake.tail;
    g_snake.tail = body;
  }
  g_snake.bodies_count++;
}

static void push_body_front(Snake_body* body) {
  if (!g_snake.bodies_count) {
    g_snake.head = body;
    g_snake.tail = body;
  } else {
    g_snake.head->prev = body;
    body->next = g_snake.head;
    g_snake.head = body;
  }
  g_snake.bodies_count++;
}

static void destroy_body(Snake_body* body) {
  free(body);
}

static Snake_body* remove_tail(void) {
  if (g_snake.bodies_count > 1) {
    g_snake.bodies_count--;
    Snake_body* tail = g_snake.tail;
    g_snake.tail = g_snake.tail->prev;
    g_snake.tail->next = NULL;
    return tail;
  }
  return NULL;
}

static lum_Vec2 get_grid_position(void) {
  float x = g_snake.head->position.x - fmod(g_snake.head->position.x, 200.0f);
  float y = g_snake.head->position.y - fmod(g_snake.head->position.y, 200.0f);
  return lum_vec2_create(x, y);
}

bool is_collided(lum_Vec2 p1, lum_Vec2 p2, float r1, float r2) {
  float center_d = lum_vec2_distance(p1, p2);
  return center_d <= (r1 + r2);
}

static void generate_gold(lum_Vec2 position, uint32_t d) {
  if (g_golds.gold_count > 29) return;
generate_begin:
  float angle = (float)(rand() % 1000) / 100.0f;
  float distance = (float)(rand() % d);
  lum_Vec2 offset = lum_vec2_scale(lum_vec2_from_angle(angle), distance);
  lum_Vec2 gold_position = lum_vec2_add(position, offset);
  bool overlap = false;
  if (g_snake.bodies_count > 1) {
    Snake_body* next = g_snake.head->next;
    while (next) {
      if (is_collided(next->position, gold_position, 15, 10)) {
        overlap = true;
      }
      next = next->next;
    }
  }
  if (overlap) goto generate_begin;
  g_golds.golds[g_golds.gold_count] = gold_position;
  g_golds.gold_count++;
}

static void generate_gold_nearby(void) {
  size_t index = lum_maxi((rand() % g_snake.bodies_count - 1), 1);
  size_t i = 0;
  Snake_body* next = g_snake.head;
  while (i < index) {
    i++;
    if (next->next) next = next->next;
  }
  generate_gold(next->position, 100);
}

static void remove_gold(size_t index) {
  memcpy(&g_golds.golds[index], &g_golds.golds[index + 1], sizeof(lum_Vec2) * (g_golds.gold_count - index));
  g_golds.gold_count--;
}

static void on_start(void) {
  time_t t;
  time(&t);
  srand((long)t);

  lum_renderer2d_set_clear_color(lum_vec4_create(0.8f, 0.8f, 0.8f, 0.8f));

  lum_tr_load_font(&g_font, "resources/FiraCode-Regular.ttf", 30);

  g_score = 0u;
  g_running = true;
  memset(&g_golds, 0, sizeof(g_golds));
  memset(&g_snake, 0, sizeof(g_snake));
  g_snake.max_length = 30;
  push_body(make_body(lum_vec2_0()));
  g_new_position = g_snake.head->position;

  for (size_t i = 0; i < 29; ++i) {
    generate_gold(get_grid_position(), 1000);
  }
}

static void on_update(float dt) {
  if (g_running) {
  if (lum_input_is_key_down(Lum_Key_A)) {
    g_snake.head_angle += 2.0f * dt;
  }
  if (lum_input_is_key_down(Lum_Key_D)) {
    g_snake.head_angle -= 2.0f * dt;
  }
  g_new_position = lum_vec2_add(g_new_position, lum_vec2_scale(lum_vec2_from_angle(g_snake.head_angle), dt * 100));
  
  float d = lum_vec2_distance(g_snake.head->position, g_new_position);
  if (d > 5.0f) {
    push_body_front(make_body(g_new_position));
    if (g_snake.bodies_count > g_snake.max_length) {
      destroy_body(remove_tail());
    }
  }

  for (size_t i = 0; i < g_golds.gold_count; ++i) {
    if (is_collided(g_snake.head->position, g_golds.golds[i], 15, 10)) {
      remove_gold(i);
      generate_gold_nearby();
      g_snake.max_length += 10;
      g_score++;
    }
  }

  if (g_snake.bodies_count > 1) {
    int count = 0;
    Snake_body* next = g_snake.head->next;
    while (next) {
      count++;
      if (is_collided(next->position, g_snake.head->position, 15, 15) && count > 10) {
        g_running = false;
      }
      next = next->next;
    }
  }

  lum_camera2d_set_position(lum_app_get_camera(), g_new_position);
  }

  if (lum_input_is_key_released(Lum_Key_Q)){
    lum_app_quit();
  }
}  

static void on_render(void) {
  Snake_body* head = g_snake.head;
  lum_renderer2d_draw_circle_layer(head->position, 7, 2, lum_vec4_create(0.5f, 1.0f, 0.5f, 1.0f));
  Snake_body* next = head;
  while (next) {
    lum_renderer2d_draw_circle_layer(next->position, 15, 1, lum_vec4_create(0.3f, 0.8f, 0.3f, 1.0f));
    next = next->next;
  }

  for (size_t i = 0; i < g_golds.gold_count; ++i) {
    lum_renderer2d_draw_circle(g_golds.golds[i], 10, lum_vec4_create(0.9f, 0.9f, 0.2f, 1.0f));
  }

  lum_Vec2 grid = get_grid_position();
  int cx = floorf(grid.x / 200), cy = floorf(grid.y / 200);

  for (int32_t i = -10; i < 10; ++i) {
    for (int32_t j = -10; j < 10; ++j) {
      char cbufx[10], cbufy[10];
      itoa(cx + i, cbufx, 10);
      itoa(cy + j, cbufy, 10);
      lum_Vec2 pos = lum_vec2_add(grid, lum_vec2_create((float)i * 200.0f, (float)j * 200.0f));
      lum_tr_draw_layer(&g_font, cbufx, lum_vec2_add(pos, lum_vec2_create(2.0f, 2.0f)), 0, lum_vec4_create(0.4f, 0.4f, 0.4f, 0.3f));
      lum_tr_draw_layer(&g_font, cbufy, lum_vec2_add(pos, lum_vec2_create(42.0f, 2.0f)), 0, lum_vec4_create(0.4f, 0.4f, 0.4f, 0.3f));
      lum_renderer2d_draw_line_layer(lum_vec2_add(lum_vec2_create(-200.0f, 0.0f), pos), lum_vec2_add(lum_vec2_create(200.0f, 0.0f), pos), 1, 0, lum_vec4_create(0.4f, 0.4f, 0.4f, 1.0f));
      lum_renderer2d_draw_line_layer(lum_vec2_add(lum_vec2_create(0.0f, -200.0f), pos), lum_vec2_add(lum_vec2_create(0.0f, 200.0f), pos), 1, 0, lum_vec4_create(0.4f, 0.4f, 0.4f, 1.0f));
    }
  }
  
  lum_renderer2d_draw_rect_ex_layer(lum_app_screen_to_world(lum_vec2_create(110, 50)), lum_vec2_create((float)lum_window_get_width() - 220, 40), 0, lum_vec2_0(), 10, lum_vec4_create(0.1f, 0.1f, 0.1f, 1));
  lum_renderer2d_draw_rect_ex_layer(lum_app_screen_to_world(lum_vec2_create(110, lum_window_get_height() - 10)), lum_vec2_create((float)lum_window_get_width() - 220, 40), 0, lum_vec2_0(), 10, lum_vec4_create(0.1f, 0.1f, 0.1f, 1));

  lum_tr_draw_layer(&g_font, "[A/D] move around", lum_app_screen_to_world(lum_vec2_create(125.0f, lum_window_get_height() - 20)), 10, lum_vec4_create(1.0f, 1.0f, 1.0f, 1.0f));
  lum_tr_draw_layer(&g_font, "[Q] quit", lum_app_screen_to_world(lum_vec2_create((float)lum_window_get_width() - 245, lum_window_get_height() - 20)), 10, lum_vec4_create(1.0f, 1.0f, 1.0f, 1.0f));

  char fps[10];
  itoa(lum_time_get_fps(), fps, 10);
  lum_tr_draw_layer(&g_font, "FPS:", lum_app_screen_to_world(lum_vec2_create(125.0f, 39.0f)), 10, lum_vec4_create(1.0f, 1.0f, 1.0f, 1.0f));
  lum_tr_draw_layer(&g_font, fps, lum_app_screen_to_world(lum_vec2_create(185.0f, 39.0f)), 10, lum_vec4_create(1.0f, 1.0f, 1.0f, 1.0f));

  char score[10];
  itoa(g_score, score, 10);
  lum_tr_draw_layer(&g_font, "score:", lum_app_screen_to_world(lum_vec2_create(255.0f, 39.0f)), 10, lum_vec4_create(1.0f, 1.0f, 1.0f, 1.0f));
  lum_tr_draw_layer(&g_font, score, lum_app_screen_to_world(lum_vec2_create(345.0f, 39.0f)), 10, lum_vec4_create(1.0f, 1.0f, 1.0f, 1.0f));
  if (g_running) {
    lum_tr_draw_layer(&g_font, "alive", lum_app_screen_to_world(lum_vec2_create((float)lum_window_get_width() - 200, 39.0f)), 10, lum_vec4_create(0.0f, 1.0f, 0.0f, 1.0f));
  } else {
    lum_tr_draw_layer(&g_font, "dead", lum_app_screen_to_world(lum_vec2_create((float)lum_window_get_width() - 190, 39.0f)), 10, lum_vec4_create(1.0f, 0.0f, 0.0f, 1.0f));
  }
}

static void on_shutdown(void) {

}

int main(void) {
  lum_App_desc desc;
  lum_app_init_desc(&desc, on_start, on_update, on_render, on_shutdown);
  desc.log_level = Lum_Log_Level_Debug;
  desc.title = "Snake";

  lum_app_run(&desc);

  return 0;
}