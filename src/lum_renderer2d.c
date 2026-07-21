#include "lum_renderer2d.h"

#include <glad/glad.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lum_renderer.h"
#include "lum_shader.h"

#define Lum_Renderer2d_Initial_Command_Capacity (128u)
#define Lum_Renderer2d_Max_Batch_Quads (2048u)
#define Lum_Renderer2d_Vertices_Per_Quad (4u)
#define Lum_Renderer2d_Indices_Per_Quad (6u)

typedef struct lum_RenderCommand {
  const lum_Texture2d* texture;
  lum_Mat4 model;
  lum_Vec4 color;
  int32_t layer;
  uint64_t sequence;
} lum_RenderCommand;

typedef struct lum_BatchVertex {
  float position[3];
  float uv[2];
  float color[4];
} lum_BatchVertex;

typedef struct lum_Renderer2d {
  bool initialized;

  lum_Shader sprite_shader;
  lum_Texture2d white_texture;

  lum_Mat4 view_projection;
  bool has_view_projection;

  lum_RenderCommand* commands;
  uint32_t command_count;
  uint32_t command_capacity;
  uint64_t next_sequence;

  uint32_t batch_vao;
  uint32_t batch_vbo;
  uint32_t batch_ebo;
  lum_BatchVertex* batch_vertices;
  uint32_t batch_quad_count;
  const lum_Texture2d* batch_texture;
  int32_t batch_layer;

  uint32_t current_frame_batch_count;
  uint32_t current_frame_draw_call_count;

  uint32_t last_frame_command_count;
  uint32_t last_frame_quad_count;
  uint32_t last_frame_batch_count;
  uint32_t last_frame_draw_call_count;
} lum_Renderer2d;

static lum_Renderer2d g_renderer2d;

static void lum_renderer2d_reset_state(void) {
  memset(&g_renderer2d, 0, sizeof(g_renderer2d));
}

static void lum_renderer2d_reset_batch(void) {
  g_renderer2d.batch_quad_count = 0;
  g_renderer2d.batch_texture = NULL;
  g_renderer2d.batch_layer = 0;
}

static void lum_renderer2d_destroy_batch_buffers(void) {
  if (g_renderer2d.batch_ebo) glDeleteBuffers(1, &g_renderer2d.batch_ebo);
  if (g_renderer2d.batch_vbo) glDeleteBuffers(1, &g_renderer2d.batch_vbo);
  if (g_renderer2d.batch_vao) glDeleteVertexArrays(1, &g_renderer2d.batch_vao);

  g_renderer2d.batch_vao = 0;
  g_renderer2d.batch_vbo = 0;
  g_renderer2d.batch_ebo = 0;

  free(g_renderer2d.batch_vertices);
  g_renderer2d.batch_vertices = NULL;
}

static int lum_renderer2d_init_batch_buffers(void) {
  const size_t vertex_count = (size_t)Lum_Renderer2d_Max_Batch_Quads * Lum_Renderer2d_Vertices_Per_Quad;
  const size_t index_count = (size_t)Lum_Renderer2d_Max_Batch_Quads * Lum_Renderer2d_Indices_Per_Quad;

  g_renderer2d.batch_vertices = (lum_BatchVertex*)malloc(sizeof(lum_BatchVertex) * vertex_count);
  if (!g_renderer2d.batch_vertices) return 0;

  uint32_t* indices = (uint32_t*)malloc(sizeof(uint32_t) * index_count);
  if (!indices) {
    lum_renderer2d_destroy_batch_buffers();
    return 0;
  }

  for (uint32_t quad_index = 0; quad_index < Lum_Renderer2d_Max_Batch_Quads; ++quad_index) {
    uint32_t vertex_base = quad_index * Lum_Renderer2d_Vertices_Per_Quad;
    uint32_t index_base = quad_index * Lum_Renderer2d_Indices_Per_Quad;

    indices[index_base + 0] = vertex_base + 0;
    indices[index_base + 1] = vertex_base + 1;
    indices[index_base + 2] = vertex_base + 2;
    indices[index_base + 3] = vertex_base + 2;
    indices[index_base + 4] = vertex_base + 3;
    indices[index_base + 5] = vertex_base + 0;
  }

  glGenVertexArrays(1, &g_renderer2d.batch_vao);
  glGenBuffers(1, &g_renderer2d.batch_vbo);
  glGenBuffers(1, &g_renderer2d.batch_ebo);

  if (!(g_renderer2d.batch_vao && g_renderer2d.batch_vbo && g_renderer2d.batch_ebo)) {
    free(indices);
    lum_renderer2d_destroy_batch_buffers();
    return 0;
  }

  glBindVertexArray(g_renderer2d.batch_vao);

  glBindBuffer(GL_ARRAY_BUFFER, g_renderer2d.batch_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(lum_BatchVertex) * vertex_count, NULL, GL_DYNAMIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_renderer2d.batch_ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * index_count, indices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(lum_BatchVertex), (void*)offsetof(lum_BatchVertex, position));
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(lum_BatchVertex), (void*)offsetof(lum_BatchVertex, uv));
  glEnableVertexAttribArray(1);

  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(lum_BatchVertex), (void*)offsetof(lum_BatchVertex, color));
  glEnableVertexAttribArray(2);

  glBindVertexArray(0);

  free(indices);
  lum_renderer2d_reset_batch();
  return 1;
}

static int lum_renderer2d_ensure_command_capacity(uint32_t min_capacity) {
  if (g_renderer2d.command_capacity >= min_capacity) return 1;

  uint32_t new_capacity = g_renderer2d.command_capacity ? g_renderer2d.command_capacity : Lum_Renderer2d_Initial_Command_Capacity;
  while (new_capacity < min_capacity) new_capacity *= 2u;

  lum_RenderCommand* new_commands = (lum_RenderCommand*)realloc(g_renderer2d.commands, sizeof(lum_RenderCommand) * new_capacity);
  if (!new_commands) return 0;

  g_renderer2d.commands = new_commands;
  g_renderer2d.command_capacity = new_capacity;
  return 1;
}

static int lum_renderer2d_push_command(const lum_Texture2d* texture, lum_Mat4 model, int32_t layer, lum_Vec4 color) {
  if (!lum_renderer2d_ensure_command_capacity(g_renderer2d.command_count + 1u)) return 0;

  lum_RenderCommand* command = &g_renderer2d.commands[g_renderer2d.command_count++];
  command->texture = texture;
  command->model = model;
  command->color = color;
  command->layer = layer;
  command->sequence = g_renderer2d.next_sequence++;
  return 1;
}

static int lum_renderer2d_command_compare(const void* left, const void* right) {
  const lum_RenderCommand* a = (const lum_RenderCommand*)left;
  const lum_RenderCommand* b = (const lum_RenderCommand*)right;

  if (a->layer != b->layer) return a->layer < b->layer ? -1 : 1;

  uint32_t a_texture_id = a->texture ? a->texture->id : 0;
  uint32_t b_texture_id = b->texture ? b->texture->id : 0;
  if (a_texture_id != b_texture_id) return a_texture_id < b_texture_id ? -1 : 1;

  if (a->sequence == b->sequence) return 0;
  return a->sequence < b->sequence ? -1 : 1;
}

static void lum_renderer2d_write_quad_vertices(const lum_RenderCommand* command) {
  static const lum_Vec2 local_positions[Lum_Renderer2d_Vertices_Per_Quad] = {
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {1.0f, 1.0f},
    {0.0f, 1.0f}
  };
  static const lum_Vec2 uvs[Lum_Renderer2d_Vertices_Per_Quad] = {
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {1.0f, 1.0f},
    {0.0f, 1.0f}
  };

  lum_BatchVertex* vertices = &g_renderer2d.batch_vertices[g_renderer2d.batch_quad_count * Lum_Renderer2d_Vertices_Per_Quad];
  for (uint32_t i = 0; i < Lum_Renderer2d_Vertices_Per_Quad; ++i) {
    lum_Vec4 transformed = lum_mat4_mul_vec4(command->model, lum_vec4_create(local_positions[i].x, local_positions[i].y, 0.0f, 1.0f));

    vertices[i].position[0] = transformed.x;
    vertices[i].position[1] = transformed.y;
    vertices[i].position[2] = transformed.z;
    vertices[i].uv[0] = uvs[i].x;
    vertices[i].uv[1] = uvs[i].y;
    vertices[i].color[0] = command->color.x;
    vertices[i].color[1] = command->color.y;
    vertices[i].color[2] = command->color.z;
    vertices[i].color[3] = command->color.w;
  }
}

static int lum_renderer2d_batch_can_accept(const lum_RenderCommand* command) {
  if (g_renderer2d.batch_quad_count >= Lum_Renderer2d_Max_Batch_Quads) return 0;
  if (!g_renderer2d.batch_texture) return 1;
  if (g_renderer2d.batch_layer != command->layer) return 0;
  return g_renderer2d.batch_texture == command->texture;
}

static void lum_renderer2d_batch_submit_command(const lum_RenderCommand* command) {
  if (!g_renderer2d.batch_texture) {
    g_renderer2d.batch_texture = command->texture;
    g_renderer2d.batch_layer = command->layer;
  }

  lum_renderer2d_write_quad_vertices(command);
  g_renderer2d.batch_quad_count++;
}

static void lum_renderer2d_flush_batch(void) {
  if (!g_renderer2d.batch_quad_count) return;

  if (g_renderer2d.has_view_projection && g_renderer2d.batch_texture) {
    size_t vertex_count = (size_t)g_renderer2d.batch_quad_count * Lum_Renderer2d_Vertices_Per_Quad;

    lum_shader_use(&g_renderer2d.sprite_shader);
    lum_shader_uniform_set4m(&g_renderer2d.sprite_shader, "u_view_projection", g_renderer2d.view_projection);
    lum_shader_uniform_set1i(&g_renderer2d.sprite_shader, "u_texture", 0);

    lum_texture2d_bind(g_renderer2d.batch_texture, 0);

    glBindVertexArray(g_renderer2d.batch_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_renderer2d.batch_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(lum_BatchVertex) * vertex_count, g_renderer2d.batch_vertices);
    glDrawElements(GL_TRIANGLES, (GLsizei)(g_renderer2d.batch_quad_count * Lum_Renderer2d_Indices_Per_Quad), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    lum_texture2d_unbind(0);

    g_renderer2d.current_frame_batch_count++;
    g_renderer2d.current_frame_draw_call_count++;
  }

  lum_renderer2d_reset_batch();
}

int lum_renderer2d_init(uint32_t viewport_w, uint32_t viewport_h, const char* vertex_shader_source, const char* fragment_shader_source) {
  assert(vertex_shader_source && fragment_shader_source);
  lum_renderer2d_reset_state();

  if (!lum_renderer_init(viewport_w, viewport_h)) return 0;
  if (!lum_shader_create_from_source(&g_renderer2d.sprite_shader, vertex_shader_source, fragment_shader_source)) {
    lum_renderer_shutdown();
    return 0;
  }
  if (!lum_texture2d_create_white(&g_renderer2d.white_texture)) {
    lum_shader_destroy(&g_renderer2d.sprite_shader);
    lum_renderer_shutdown();
    return 0;
  }
  if (!lum_renderer2d_init_batch_buffers()) {
    lum_texture2d_destroy(&g_renderer2d.white_texture);
    lum_shader_destroy(&g_renderer2d.sprite_shader);
    lum_renderer_shutdown();
    return 0;
  }

  g_renderer2d.initialized = true;
  return 1;
}

void lum_renderer2d_shutdown(void) {
  if (!g_renderer2d.initialized) return;

  lum_renderer2d_destroy_batch_buffers();
  free(g_renderer2d.commands);
  g_renderer2d.commands = NULL;

  lum_texture2d_destroy(&g_renderer2d.white_texture);
  lum_shader_destroy(&g_renderer2d.sprite_shader);

  lum_renderer_shutdown();
  lum_renderer2d_reset_state();
}

bool lum_renderer2d_is_initialized(void) {
  return g_renderer2d.initialized;
}

void lum_renderer2d_set_viewport(int x, int y, int w, int h) {
  if (!g_renderer2d.initialized) return;
  lum_renderer_set_viewport((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h);
}

void lum_renderer2d_set_clear_color(lum_Vec4 color) {
  if (!g_renderer2d.initialized) return;
  lum_renderer_set_clear_color(color);
}

void lum_renderer2d_begin_frame(void) {
  if (!g_renderer2d.initialized) return;

  g_renderer2d.command_count = 0;
  g_renderer2d.next_sequence = 0;
  g_renderer2d.current_frame_batch_count = 0;
  g_renderer2d.current_frame_draw_call_count = 0;
  lum_renderer2d_reset_batch();

  lum_renderer_begin_frame();
}

void lum_renderer2d_end_frame(void) {
  if (!g_renderer2d.initialized) return;

  g_renderer2d.last_frame_command_count = g_renderer2d.command_count;
  g_renderer2d.last_frame_quad_count = g_renderer2d.command_count;
  g_renderer2d.last_frame_batch_count = 0;
  g_renderer2d.last_frame_draw_call_count = 0;

  if (g_renderer2d.command_count > 1u) {
    qsort(g_renderer2d.commands, g_renderer2d.command_count, sizeof(lum_RenderCommand), lum_renderer2d_command_compare);
  }

  for (uint32_t i = 0; i < g_renderer2d.command_count; ++i) {
    const lum_RenderCommand* command = &g_renderer2d.commands[i];
    if (!lum_renderer2d_batch_can_accept(command)) lum_renderer2d_flush_batch();
    lum_renderer2d_batch_submit_command(command);
  }

  lum_renderer2d_flush_batch();

  g_renderer2d.last_frame_batch_count = g_renderer2d.current_frame_batch_count;
  g_renderer2d.last_frame_draw_call_count = g_renderer2d.current_frame_draw_call_count;

  lum_renderer_end_frame();
}

void lum_renderer2d_clear(void) {
  if (!g_renderer2d.initialized) return;
  lum_renderer_clear();
}

void lum_renderer2d_set_camera(const lum_Camera2d* camera) {
  assert(camera);
  if (!g_renderer2d.initialized) return;
  lum_renderer2d_set_view_projection(lum_camera2d_get_view_projection_matrix(camera));
}

void lum_renderer2d_set_view_projection(lum_Mat4 view_proj) {
  if (!g_renderer2d.initialized) return;

  g_renderer2d.view_projection = view_proj;
  g_renderer2d.has_view_projection = true;
  lum_renderer_set_view_projection(view_proj);
}

void lum_renderer2d_enable_blend(void) {
  if (!g_renderer2d.initialized) return;
  lum_renderer_enable_blend();
}

void lum_renderer2d_disable_blend(void) {
  if (!g_renderer2d.initialized) return;
  lum_renderer_disable_blend();
}

void lum_renderer2d_draw_sprite(const lum_Texture2d* texture, const lum_Transform2d* transform, lum_Vec4 color) {
  lum_renderer2d_draw_sprite_layer(texture, transform, 0, color);
}

void lum_renderer2d_draw_rect(const lum_Transform2d* transform, lum_Vec4 color) {
  lum_renderer2d_draw_rect_layer(transform, 0, color);
}

void lum_renderer2d_draw_sprite_layer(const lum_Texture2d* texture, const lum_Transform2d* transform, int32_t layer, lum_Vec4 color) {
  assert(texture && transform);
  if (!g_renderer2d.initialized) return;

  lum_Mat4 model = lum_transform2d_get_matrix(transform);
  lum_renderer2d_push_command(texture, model, layer, color);
}

void lum_renderer2d_draw_rect_layer(const lum_Transform2d* transform, int32_t layer, lum_Vec4 color) {
  assert(transform);
  if (!g_renderer2d.initialized) return;

  lum_renderer2d_draw_sprite_layer(&g_renderer2d.white_texture, transform, layer, color);
}

void lum_renderer2d_draw_sprite_ex(const lum_Texture2d* texture, lum_Vec2 position, lum_Vec2 size, float rotation_rad, lum_Vec2 origin, lum_Vec4 color) {
  lum_renderer2d_draw_sprite_ex_layer(texture, position, size, rotation_rad, origin, 0, color);
}

void lum_renderer2d_draw_rect_ex(lum_Vec2 position, lum_Vec2 size, float rotation_rad, lum_Vec2 origin, lum_Vec4 color) {
  lum_renderer2d_draw_rect_ex_layer(position, size, rotation_rad, origin, 0, color);
}

void lum_renderer2d_draw_sprite_ex_layer(const lum_Texture2d* texture, lum_Vec2 position, lum_Vec2 size, float rotation_rad, lum_Vec2 origin, int32_t layer, lum_Vec4 color) {
  if (!g_renderer2d.initialized) return;

  lum_Transform2d t = lum_transform2d_create(position, size, lum_vec2_1(), rotation_rad, origin);
  lum_renderer2d_draw_sprite_layer(texture, &t, layer, color);
}

void lum_renderer2d_draw_rect_ex_layer(lum_Vec2 position, lum_Vec2 size, float rotation_rad, lum_Vec2 origin, int32_t layer, lum_Vec4 color) {
  if (!g_renderer2d.initialized) return;
  lum_renderer2d_draw_sprite_ex_layer(&g_renderer2d.white_texture, position, size, rotation_rad, origin, layer, color);
}

uint32_t lum_renderer2d_get_last_frame_command_count(void) {
  return g_renderer2d.last_frame_command_count;
}

uint32_t lum_renderer2d_get_last_frame_quad_count(void) {
  return g_renderer2d.last_frame_quad_count;
}

uint32_t lum_renderer2d_get_last_frame_batch_count(void) {
  return g_renderer2d.last_frame_batch_count;
}

uint32_t lum_renderer2d_get_last_frame_draw_call_count(void) {
  return g_renderer2d.last_frame_draw_call_count;
}
