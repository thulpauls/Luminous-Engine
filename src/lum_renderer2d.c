#include "lum_renderer2d.h"
#include "lum_shader.h"
#include "lum_log.h"
#include <string.h>
#include "lum_assert.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <math.h>

#define Lum_Renderer2d_Mesh_Initial_Vertex_Capacity (4096u)
#define Lum_Renderer2d_Mesh_Initial_Index_Capacity (8192u)
#define Lum_Renderer2d_Mesh_Initial_Command_Capacity (128u)

static const char* sprite_vertex_shader =
  "#version 330 core\n"
  "\n"
  "layout(location = 0) in vec3 aPos;\n"
  "layout(location = 1) in vec2 aUV;\n"
  "layout(location = 2) in vec4 aColor;\n"
  "\n"
  "uniform mat4 u_view_projection;\n"
  "\n"
  "out vec2 vUV;\n"
  "out vec4 vColor;\n"
  "\n"
  "void main() {\n"
  "    vUV = aUV;\n"
  "    vColor = aColor;\n"
  "    gl_Position = u_view_projection * vec4(aPos, 1.0);\n"
  "}\n";

static const char* sprite_fragment_shader =
  "#version 330 core\n"
  "\n"
  "in vec2 vUV;\n"
  "in vec4 vColor;\n"
  "out vec4 FragColor;\n"
  "\n"
  "uniform sampler2D u_texture;\n"
  "\n"
  "void main() {\n"
  "    FragColor = texture(u_texture, vUV) * vColor;\n"
  "}\n";

static const char* circle_vertex_shader =
  "#version 330 core\n"
  "\n"
  "layout(location = 0) in vec3 aPos;\n"
  "layout(location = 1) in vec2 aUV;\n"
  "layout(location = 2) in vec4 aColor;\n"
  "\n"
  "uniform mat4 u_view_projection;\n"
  "\n"
  "out vec2 vLocalPos;\n"
  "out vec4 vColor;\n"
  "\n"
  "void main() {\n"
  "    vLocalPos = (aUV - 0.5) * 2.0;\n"
  "    vColor = aColor;\n"
  "    gl_Position = u_view_projection * vec4(aPos, 1.0);\n"
  "}\n";

static const char* circle_fragment_shader =
  "#version 330 core\n"
  "\n"
  "in vec2 vLocalPos;\n"
  "in vec4 vColor;\n"
  "out vec4 FragColor;\n"
  "\n"
  "uniform sampler2D u_texture;\n"
  "\n"
  "void main() {\n"
  "    float d = length(vLocalPos);\n"
  "    float alpha = 1.0 - smoothstep(0.98, 1.0, d);\n"
  "    if (alpha <= 0.0) discard;\n"
  "    FragColor = vec4(vColor.rgb, vColor.a * alpha);\n"
  "}\n";

typedef struct lum_Mesh_command {
  const lum_Material* material;
  const lum_Texture2d* texture;
  lum_Primitive primitive;
  lum_Blend_mode blend;
  int32_t layer;
  uint64_t sequence;
  uint32_t vertex_offset, vertex_count;
  uint32_t index_offset, index_count;
} lum_Mesh_command;

typedef struct lum_Renderer2d {
  bool initialized;

  uint32_t viewport_x, viewport_y, viewport_w, viewport_h;
  lum_Vec4 clear_color;
  
  lum_Shader sprite_shader, circle_shader;
  lum_Texture2d white_texture;

  lum_Mat4 view_projection;
  bool has_view_projection;

  uint32_t current_frame_draw_call_count;

  lum_Material sprite_material, circle_material;

  lum_Mesh_command* mesh_commands;
  uint32_t mesh_command_count, mesh_command_capacity;
  uint64_t mesh_next_sequence;

  lum_Vertex* frame_vertices;
  uint32_t frame_vertex_count, frame_vertex_capacity;
  uint32_t* frame_indices;
  uint32_t frame_index_count, frame_index_capacity;

  uint32_t mesh_VAO, mesh_VBO, mesh_EBO;
  uint32_t mesh_gl_vertex_capacity, mesh_gl_index_capacity;

  uint32_t* mesh_gather_index, mesh_gather_index_capacity;
  lum_Vertex* mesh_scratch_vertices;
  uint32_t mesh_scratch_vertex_capacity;

  float current_line_width;
  float current_point_size;
  lum_Blend_mode current_blend_mode;   /* draw_mesh 时记入命令；flush 每批按命令设 GL */
} lum_Renderer2d;

static lum_Renderer2d g_renderer2d;

static void lum__renderer2d_reset_state(void) {
  memset(&g_renderer2d, 0, sizeof(g_renderer2d));
}

static void lum__renderer2d_destroy_mesh_buffers(void) {
  if (g_renderer2d.mesh_EBO) glDeleteBuffers(1, &g_renderer2d.mesh_EBO);
  if (g_renderer2d.mesh_VBO) glDeleteBuffers(1, &g_renderer2d.mesh_VBO);
  if (g_renderer2d.mesh_VAO) glDeleteVertexArrays(1, &g_renderer2d.mesh_VAO);
  g_renderer2d.mesh_EBO = 0;  
  g_renderer2d.mesh_VBO = 0;
  g_renderer2d.mesh_VAO = 0;
}

static int lum__renderer2d_init_mesh_buffers(void) {
  glGenVertexArrays(1, &g_renderer2d.mesh_VAO);
  glGenBuffers(1, &g_renderer2d.mesh_VBO);
  glGenBuffers(1, &g_renderer2d.mesh_EBO);
  if (!(g_renderer2d.mesh_VAO && g_renderer2d.mesh_VBO && g_renderer2d.mesh_EBO)) {
    lum__renderer2d_destroy_mesh_buffers();
    return 0;
  }

  glBindVertexArray(g_renderer2d.mesh_VAO);
  glBindBuffer(GL_ARRAY_BUFFER, g_renderer2d.mesh_VBO);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(lum_Vertex), (void*)offsetof(lum_Vertex, position));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(lum_Vertex), (void*)offsetof(lum_Vertex, uv));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(lum_Vertex), (void*)offsetof(lum_Vertex, color));
  glEnableVertexAttribArray(2);

  glBindVertexArray(0);
  return 1;
}

static uint32_t lum__next_capacity(uint32_t capacity, uint32_t need, uint32_t initial_capacity) {
  uint32_t cap = lum_maxi(capacity, initial_capacity);
  while(cap < need) cap *= 2u;
  return cap;
}

static int lum__grow_buffer(void** ptr, uint32_t* capacity, uint32_t need, size_t elem_sz, uint32_t init_cap, const char* tag) {
  if (need < *capacity) return 1;
  uint32_t cap = lum__next_capacity(*capacity, need, init_cap);
  void* p = realloc(*ptr, elem_sz * (size_t)cap);
  
  if (!p) { 
    Lum_Log_Warn("renderer2d: %s realloc failed (need %u).", tag, need); 
    return 0; 
  }

  *ptr = p;
  *capacity = cap;
  return 1;
}

static void lum__grow_gl_buffer(uint32_t target, uint32_t buffer, uint32_t* capacity, uint32_t need, size_t elem_sz, uint32_t init_cap) {
  if (need <= *capacity) return;
  uint32_t cap = lum__next_capacity(*capacity, need, init_cap);
  glBindBuffer(target, buffer);
  glBufferData(target, (GLsizeiptr)(elem_sz * (size_t)cap), NULL, GL_DYNAMIC_DRAW);
  *capacity = cap;
}

static int lum__mesh_ensure_command_capacity(uint32_t need) {
  return lum__grow_buffer((void**)&g_renderer2d.mesh_commands, &g_renderer2d.mesh_command_capacity, need, sizeof(lum_Mesh_command), Lum_Renderer2d_Mesh_Initial_Command_Capacity, "mesh commands");
}


static int lum__arena_ensure_vertices(uint32_t add) {
  uint32_t need = g_renderer2d.frame_vertex_count + add;
  return lum__grow_buffer((void**)&g_renderer2d.frame_vertices, &g_renderer2d.frame_vertex_capacity, need, sizeof(lum_Vertex), Lum_Renderer2d_Mesh_Initial_Vertex_Capacity, "vertex arena");
}

static int lum__arena_ensure_indices(uint32_t add) {
  uint32_t need = g_renderer2d.frame_index_count + add;
  return lum__grow_buffer((void**)&g_renderer2d.frame_indices, &g_renderer2d.frame_index_capacity, need, sizeof(uint32_t), Lum_Renderer2d_Mesh_Initial_Index_Capacity, "index arena");
}

static int lum__mesh_ensure_gather_index_capacity(uint32_t need) {  
  return lum__grow_buffer((void**)&g_renderer2d.mesh_gather_index, &g_renderer2d.mesh_gather_index_capacity, need, sizeof(uint32_t), Lum_Renderer2d_Mesh_Initial_Index_Capacity, "gather index");
}

static int lum__mesh_ensure_scratch_vertices(uint32_t need) {  
  return lum__grow_buffer((void**)&g_renderer2d.mesh_scratch_vertices, &g_renderer2d.mesh_scratch_vertex_capacity, need, sizeof(lum_Vertex), Lum_Renderer2d_Mesh_Initial_Vertex_Capacity, "scratch vertices");
}

static void lum__mesh_ensure_gl_vertex_capacity(uint32_t need) {
  lum__grow_gl_buffer(GL_ARRAY_BUFFER, g_renderer2d.mesh_VBO, &g_renderer2d.mesh_gl_vertex_capacity, need, sizeof(lum_Vertex), Lum_Renderer2d_Mesh_Initial_Vertex_Capacity);
}

static void lum__mesh_ensure_gl_index_capacity(uint32_t need) {
  lum__grow_gl_buffer(GL_ELEMENT_ARRAY_BUFFER, g_renderer2d.mesh_EBO, &g_renderer2d.mesh_gl_index_capacity, need, sizeof(uint32_t), Lum_Renderer2d_Mesh_Initial_Index_Capacity);
}

int lum_renderer2d_init(uint32_t viewport_w, uint32_t viewport_h) {
  lum__renderer2d_reset_state();

  g_renderer2d.viewport_x = 0;
  g_renderer2d.viewport_y = 0;
  g_renderer2d.viewport_w = viewport_w;
  g_renderer2d.viewport_h = viewport_h;

  glViewport(0, 0, viewport_w, viewport_h);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glDisable(GL_DEPTH_TEST);

  g_renderer2d.initialized = true;

  if (!lum_shader_create_from_source(&g_renderer2d.sprite_shader, sprite_vertex_shader, sprite_fragment_shader) ||
      !lum_shader_create_from_source(&g_renderer2d.circle_shader, circle_vertex_shader, circle_fragment_shader)) {
    lum_renderer2d_shutdown();
    return 0;
  }
  if (!lum_texture2d_create_white(&g_renderer2d.white_texture)) {
    lum_renderer2d_shutdown();
    return 0;
  }
  if (!lum__renderer2d_init_mesh_buffers()) {
    lum_renderer2d_shutdown();
    return 0;
  }

  lum_material_create(&g_renderer2d.sprite_material, &g_renderer2d.sprite_shader);
  lum_material_create(&g_renderer2d.circle_material, &g_renderer2d.circle_shader);
  g_renderer2d.current_line_width = 1.0f;
  g_renderer2d.current_point_size = 1.0f;
  g_renderer2d.current_blend_mode = Lum_Blend_Alpha;

  return 1;
}

void lum_renderer2d_shutdown(void) {
  if (!g_renderer2d.initialized) return;

  lum__renderer2d_destroy_mesh_buffers();
  free(g_renderer2d.mesh_commands); 
  g_renderer2d.mesh_commands = NULL;
  free(g_renderer2d.frame_vertices); 
  g_renderer2d.frame_vertices = NULL;
  free(g_renderer2d.frame_indices);      
  g_renderer2d.frame_indices = NULL;
  free(g_renderer2d.mesh_gather_index);    
  g_renderer2d.mesh_gather_index = NULL;
  free(g_renderer2d.mesh_scratch_vertices); 
  g_renderer2d.mesh_scratch_vertices = NULL;

  lum_texture2d_destroy(&g_renderer2d.white_texture);
  lum_shader_destroy(&g_renderer2d.sprite_shader);
  lum_shader_destroy(&g_renderer2d.circle_shader);

  lum__renderer2d_reset_state();
}

void lum_renderer2d_set_viewport(int x, int y, int w, int h) {
  if (!g_renderer2d.initialized) return;
  
  g_renderer2d.viewport_x = x;
  g_renderer2d.viewport_y = y;
  g_renderer2d.viewport_w = w;
  g_renderer2d.viewport_h = h;

  glViewport(x, y, w, h);
}

void lum_renderer2d_set_clear_color(lum_Vec4 color) {
  if (!g_renderer2d.initialized) return;
  g_renderer2d.clear_color = color;
}

void lum_renderer2d_begin_frame(void) {
  if (!g_renderer2d.initialized) return;

  g_renderer2d.mesh_command_count = 0;
  g_renderer2d.mesh_next_sequence = 0;
  g_renderer2d.frame_vertex_count = 0;
  g_renderer2d.frame_index_count = 0;
  g_renderer2d.current_frame_draw_call_count = 0;
  g_renderer2d.current_line_width = 1.0f;
  g_renderer2d.current_point_size = 1.0f;
  g_renderer2d.current_blend_mode = Lum_Blend_Alpha;
}

static GLenum lum__mesh_primitive_to_gl(lum_Primitive p) {
  switch (p) {
    case Lum_Primitive_Triangles: return GL_TRIANGLES;
    case Lum_Primitive_Triangle_Strip: return GL_TRIANGLE_STRIP;
    case Lum_Primitive_Triangle_Fan: return GL_TRIANGLE_FAN;
    case Lum_Primitive_Lines: return GL_LINES;
    case Lum_Primitive_Line_Strip: return GL_LINE_STRIP;
    case Lum_Primitive_Line_Loop: return GL_LINE_LOOP;
    case Lum_Primitive_Points: return GL_POINTS;
    default: return GL_TRIANGLES;
  }
}

static void lum__mesh_apply_param(const lum_Material* mat, const lum_Material_parameter* p) {
  switch (p->type) {
    case Lum_Material_Parameter_1i: lum_shader_uniform_set1i(mat->shader, p->name, p->value.i);  break;
    case Lum_Material_Parameter_1f: lum_shader_uniform_set1f(mat->shader, p->name, p->value.f);  break;
    case Lum_Material_Parameter_2v: lum_shader_uniform_set2v(mat->shader, p->name, p->value.v2); break;
    case Lum_Material_Parameter_3v: lum_shader_uniform_set3v(mat->shader, p->name, p->value.v3); break;
    case Lum_Material_Parameter_4v: lum_shader_uniform_set4v(mat->shader, p->name, p->value.v4); break;
    case Lum_Material_Parameter_4m: lum_shader_uniform_set4m(mat->shader, p->name, p->value.m4); break;
    default: break;
  }
}

static int lum__mesh_command_compare(const void* left, const void* right) {
  const lum_Mesh_command* a = (const lum_Mesh_command*)left;
  const lum_Mesh_command* b = (const lum_Mesh_command*)right;
  if (a->layer != b->layer) return (a->layer > b->layer) - (a->layer < b->layer);
  if (a->material != b->material) return (a->material > b->material) - (a->material < b->material);
  if (a->texture != b->texture) {
    uint32_t at = a->texture ? a->texture->id : 0;
    uint32_t bt = b->texture ? b->texture->id : 0;
    return (at > bt) - (at < bt);
  }
  if (a->primitive != b->primitive) return (int)a->primitive - (int)b->primitive;
  if (a->blend != b->blend) return (int)a->blend - (int)b->blend;
  if (a->sequence == b->sequence) return 0;
  return (a->sequence > b->sequence) - (a->sequence < b->sequence);
}

static void lum__renderer2d_flush_mesh_batch(uint32_t begin, uint32_t end, uint32_t batch_vcount, uint32_t batch_icount) {
  lum__mesh_ensure_gl_vertex_capacity(batch_vcount);
  lum__mesh_ensure_gl_index_capacity(batch_icount);
  if (!lum__mesh_ensure_gather_index_capacity(batch_icount)) return;

  glBindVertexArray(g_renderer2d.mesh_VAO);
  glBindBuffer(GL_ARRAY_BUFFER, g_renderer2d.mesh_VBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_renderer2d.mesh_EBO);

  uint32_t batch_vpos = 0, batch_ipos = 0;
  for (uint32_t k = begin; k < end; ++k) {
    const lum_Mesh_command* c = &g_renderer2d.mesh_commands[k];
    glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)((size_t)batch_vpos * sizeof(lum_Vertex)), (GLsizeiptr)(size_t)(c->vertex_count * sizeof(lum_Vertex)), g_renderer2d.frame_vertices + c->vertex_offset);
    
    const uint32_t* src = g_renderer2d.frame_indices + c->index_offset;
    uint32_t* dst = g_renderer2d.mesh_gather_index + batch_ipos;
    for (uint32_t m = 0; m < c->index_count; ++m) {
      dst[m] = src[m] + batch_vpos;
    }
    batch_vpos += c->vertex_count;
    batch_ipos += c->index_count;
  }
  glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, (GLsizeiptr)((size_t)batch_icount * sizeof(uint32_t)), g_renderer2d.mesh_gather_index);

  const lum_Mesh_command* base = &g_renderer2d.mesh_commands[begin];
  const lum_Material* material = base->material;

  GLenum gl_mode = lum__mesh_primitive_to_gl(base->primitive);
  if (gl_mode == GL_LINES || gl_mode == GL_LINE_STRIP || gl_mode == GL_LINE_LOOP) {
    glLineWidth(g_renderer2d.current_line_width);
  } else if (gl_mode == GL_POINTS) {
    glPointSize(g_renderer2d.current_point_size);
  }

  lum_shader_use(material->shader);
  lum_shader_uniform_set4m(material->shader, "u_view_projection", g_renderer2d.view_projection);
  lum_shader_uniform_set1i(material->shader, "u_texture", 0);
  for (uint32_t p = 0; p < material->param_count; ++p) {
    lum__mesh_apply_param(material, &material->params[p]);
  }

  lum_texture2d_bind(base->texture, 0);
  for (uint32_t s = 1; s < Lum_Material_Max_Textures; ++s) {
    if (material->textures[s]) lum_texture2d_bind(material->textures[s], s);
  }

  lum_renderer2d_set_blend_mode(base->blend);

  glDrawElements(gl_mode, (GLsizei)batch_icount, GL_UNSIGNED_INT, 0);
  
  lum_texture2d_unbind(0);
  for (uint32_t s = 1; s < Lum_Material_Max_Textures; ++s) {
    if (material->textures[s]) lum_texture2d_unbind(s);
  }
  glBindVertexArray(0);

  g_renderer2d.current_frame_draw_call_count++;
}

static void lum__renderer2d_flush_mesh_commands(void) {
  if (!g_renderer2d.has_view_projection) return;
  if (g_renderer2d.mesh_command_count == 0) return;

  if (g_renderer2d.mesh_command_count > 1u) {
    qsort(g_renderer2d.mesh_commands, g_renderer2d.mesh_command_count, sizeof(lum_Mesh_command), lum__mesh_command_compare);
  }

  uint32_t i = 0;
  while (i < g_renderer2d.mesh_command_count) {
    const lum_Mesh_command* base = &g_renderer2d.mesh_commands[i];
    uint32_t j = i, batch_vcount = 0, batch_icount = 0;
    while (j < g_renderer2d.mesh_command_count) {
      const lum_Mesh_command* c = &g_renderer2d.mesh_commands[j];
      if (c->layer != base->layer || c->material != base->material || c->texture != base->texture || c->primitive != base->primitive || c->blend != base->blend) break;
      batch_vcount += c->vertex_count;
      batch_icount += c->index_count;
      ++j;
    }
    lum__renderer2d_flush_mesh_batch(i, j, batch_vcount, batch_icount);
    i = j;
  }
}

void lum_renderer2d_end_frame(void) {
  if (!g_renderer2d.initialized) return;
  lum__renderer2d_flush_mesh_commands();
}

void lum_renderer2d_clear(void) {
  if (!g_renderer2d.initialized) return;
  
  glClearColor(g_renderer2d.clear_color.x, g_renderer2d.clear_color.y, g_renderer2d.clear_color.z, g_renderer2d.clear_color.w);
  glClear(GL_COLOR_BUFFER_BIT);
}

void lum_renderer2d_set_camera(const lum_Camera2d* camera) {
  Lum_Assert(camera);
  if (!g_renderer2d.initialized) return;
  lum_renderer2d_set_view_projection(lum_camera2d_get_view_projection_matrix(camera));
}

void lum_renderer2d_set_view_projection(lum_Mat4 view_proj) {
  if (!g_renderer2d.initialized) return;

  g_renderer2d.view_projection = view_proj;
  g_renderer2d.has_view_projection = true;
}

void lum_renderer2d_set_blend_mode(lum_Blend_mode mode) {
  if (!g_renderer2d.initialized) return;
  g_renderer2d.current_blend_mode = mode;   /* 记状态：draw_mesh 记入命令，flush 按命令设 GL */
  switch (mode) {
    case Lum_Blend_None: glDisable(GL_BLEND); break;
    case Lum_Blend_Alpha: glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); break;
    case Lum_Blend_Additive: glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE); break;
    case Lum_Blend_Premul: glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); break;
    default: break;
  }
}

static void lum__renderer2d_create_vertex(lum_Vertex* vertex, lum_Vec3 position, lum_Vec2 uv, lum_Vec4 color) {
  vertex->position[0] = position.x;
  vertex->position[1] = position.y;
  vertex->position[2] = 0.0f;
  vertex->uv[0] = uv.x;
  vertex->uv[1] = uv.y;
  vertex->color[0] = color.x;
  vertex->color[1] = color.y;
  vertex->color[2] = color.z;
  vertex->color[3] = color.w;
}

static void lum__build_quad_uv(lum_Mat4 model, lum_Vec4 uv_rect, lum_Vec4 color, lum_Vertex out_verts[4]) {
  static const lum_Vec2 local[4] = { {0.0f,0.0f}, {1.0f,0.0f}, {1.0f,1.0f}, {0.0f,1.0f} };
  const lum_Vec2 uv[4] = {
    { uv_rect.x, uv_rect.y },
    { uv_rect.z, uv_rect.y },
    { uv_rect.z, uv_rect.w },
    { uv_rect.x, uv_rect.w },
  };

  for (int i = 0; i < 4; ++i) {
    lum_Vec4 p = lum_mat4_mul_vec4(model, lum_vec4_create(local[i].x, local[i].y, 0.0f, 1.0f));

    out_verts[i].position[0] = p.x; out_verts[i].position[1] = p.y; out_verts[i].position[2] = p.z;
    out_verts[i].uv[0] = uv[i].x; out_verts[i].uv[1] = uv[i].y;
    out_verts[i].color[0] = color.x; out_verts[i].color[1] = color.y;
    out_verts[i].color[2] = color.z; out_verts[i].color[3] = color.w;
  }
}

void lum_build_quad(lum_Mat4 model, lum_Vec4 color, lum_Vertex out_verts[4]) {
  lum__build_quad_uv(model, lum_vec4_create(0.0f, 0.0f, 1.0f, 1.0f), color, out_verts);
}

void lum_renderer2d_draw_mesh(const lum_Vertex* vertices, uint32_t vertex_count, const uint32_t* indices, uint32_t index_count, lum_Primitive primitive, const lum_Material* material, const lum_Texture2d* texture, int32_t layer) {
  if (!g_renderer2d.initialized) return;
  if (!material || !material->initialized) return;
  if (!material->shader || !material->shader->initialized) return;
  if (!vertices || vertex_count == 0) return;

  uint32_t gen_count;
  if (indices) {
    if (index_count == 0) return;
    gen_count = index_count;
  } else {
    gen_count = vertex_count;
  }

  if (!lum__arena_ensure_vertices(vertex_count)) return;
  if (!lum__arena_ensure_indices(gen_count)) return;
  if (!lum__mesh_ensure_command_capacity(g_renderer2d.mesh_command_count + 1u)) return;

  const lum_Texture2d* eff_texture = texture ? texture : &g_renderer2d.white_texture;

  uint32_t voff = g_renderer2d.frame_vertex_count;
  memcpy(g_renderer2d.frame_vertices + voff, vertices, sizeof(lum_Vertex) * vertex_count);
  g_renderer2d.frame_vertex_count += vertex_count;

  uint32_t ioff = g_renderer2d.frame_index_count;
  if (indices) {
    memcpy(g_renderer2d.frame_indices + ioff, indices, sizeof(uint32_t) * index_count);
    g_renderer2d.frame_index_count += index_count;
  } else {
    for (uint32_t i = 0; i < vertex_count; ++i) {
      g_renderer2d.frame_indices[g_renderer2d.frame_index_count++] = i;
    }
  }

  lum_Mesh_command* cmd = &g_renderer2d.mesh_commands[g_renderer2d.mesh_command_count++];
  cmd->material = material;
  cmd->texture = eff_texture;
  cmd->primitive = primitive;
  cmd->blend = g_renderer2d.current_blend_mode;
  cmd->layer = layer;
  cmd->sequence = g_renderer2d.mesh_next_sequence++;
  cmd->vertex_offset = voff;
  cmd->vertex_count = vertex_count;
  cmd->index_offset = ioff;
  cmd->index_count = gen_count;
}

void lum_renderer2d_draw_fullscreen(const lum_Material* material, const lum_Texture2d* src, int32_t layer) {
  Lum_Assert(material && src);
  if (!g_renderer2d.initialized) return;
  if (!material || !material->initialized) return;

  static const lum_Vertex vertices[4] = {
    { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { {  1.0f,  1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    { { -1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
  };
  static const uint32_t indices[6] = { 0, 1, 2, 2, 3, 0 };

  lum_Mat4 vp = g_renderer2d.view_projection;
  bool has_vp = g_renderer2d.has_view_projection;
  lum_renderer2d_set_view_projection(lum_mat4_identity());

  lum_renderer2d_draw_mesh(vertices, 4, indices, 6, Lum_Primitive_Triangles, material, src, layer);

  g_renderer2d.view_projection = vp;
  g_renderer2d.has_view_projection = has_vp;
}

void lum_renderer2d_draw_sprite(const lum_Texture2d* texture, const lum_Transform2d* transform, int32_t layer, lum_Vec4 color) {
  static const uint32_t lum_quad_indices[6] = { 0, 1, 2, 2, 3, 0 };

  Lum_Assert(texture && transform);
  if (!g_renderer2d.initialized) return;
  lum_Mat4 model = lum_transform2d_get_matrix(transform);
  lum_Vertex v[4];
  lum_build_quad(model, color, v);
  lum_renderer2d_draw_mesh(v, 4, lum_quad_indices, 6, Lum_Primitive_Triangles, &g_renderer2d.sprite_material, texture, layer);
}

void lum_renderer2d_draw_sprite_uv(const lum_Texture2d* texture, lum_Vec2 position, lum_Vec2 size, lum_Vec4 uv_rect, int32_t layer, lum_Vec4 color) {
  static const uint32_t lum_quad_indices[6] = { 0, 1, 2, 2, 3, 0 };

  Lum_Assert(texture);
  if (!g_renderer2d.initialized) return;

  lum_Transform2d t = lum_transform2d_create(position, size, lum_vec2_1(), 0.0f, lum_vec2_0());
  lum_Mat4 model = lum_transform2d_get_matrix(&t);
  lum_Vertex v[4];
  lum__build_quad_uv(model, uv_rect, color, v);
  lum_renderer2d_draw_mesh(v, 4, lum_quad_indices, 6, Lum_Primitive_Triangles, &g_renderer2d.sprite_material, texture, layer);
}

void lum_renderer2d_draw_sprite_matrix_uv(const lum_Texture2d* texture, lum_Mat4 model,
                                                 lum_Vec4 uv_rect, int32_t layer, lum_Vec4 color) {
  static const uint32_t lum_quad_indices[6] = { 0, 1, 2, 2, 3, 0 };

  Lum_Assert(texture);
  if (!g_renderer2d.initialized) return;

  lum_Vertex v[4];
  lum__build_quad_uv(model, uv_rect, color, v);
  lum_renderer2d_draw_mesh(v, 4, lum_quad_indices, 6, Lum_Primitive_Triangles,
                           &g_renderer2d.sprite_material, texture, layer);
}

void lum_renderer2d_draw_rect(lum_Vec2 position, lum_Vec2 size, float rotation_rad, lum_Vec2 origin, int32_t layer, lum_Vec4 color) {
  lum_Transform2d t = lum_transform2d_create(position, size, lum_vec2_1(), rotation_rad, origin);
  lum_renderer2d_draw_sprite(&g_renderer2d.white_texture, &t, layer, color);
}

void lum_renderer2d_draw_line(lum_Vec2 start, lum_Vec2 end, float thickness, int32_t layer, lum_Vec4 color) {
  if (!g_renderer2d.initialized || thickness < 0.0f) return;
  lum_Vec2 d = lum_vec2_sub(start, end);
  lum_Vec2 center = lum_vec2_lerp(start, end, 0.5f);

  float len = lum_vec2_len(d);
  if (lum_float_is_zero(len)) return;
  float angle = atan2f(d.y, d.x);

  lum_renderer2d_draw_rect(center, lum_vec2_create(len, thickness), angle, lum_vec2_create(len * 0.5f, thickness * 0.5f), layer, color);
}

void lum_renderer2d_draw_circle(lum_Vec2 position, float radius, int32_t layer, lum_Vec4 color) {
  if (!g_renderer2d.initialized) return;
  if (radius < 0.0f) return;
  lum_Vec2 size = lum_vec2_scale(lum_vec2_create(radius, radius), 2.0f);
  lum_Transform2d t = lum_transform2d_create(position, size, lum_vec2_1(), 0.0f, lum_vec2_scale(size, 0.5f));
  lum_Mat4 model = lum_transform2d_get_matrix(&t);
  lum_Vertex v[4];
  lum_build_quad(model, color, v);
  static const uint32_t lum_quad_indices[6] = { 0, 1, 2, 2, 3, 0 };
  lum_renderer2d_draw_mesh(v, 4, lum_quad_indices, 6, Lum_Primitive_Triangles, &g_renderer2d.circle_material, NULL, layer);
}

void lum_renderer2d_draw_triangle(lum_Vec2 p0, lum_Vec2 p1, lum_Vec2 p2, int32_t layer, lum_Vec4 color) {
  if (!g_renderer2d.initialized) return;
  lum_Vertex v[3];
  lum__renderer2d_create_vertex(&v[0], lum_vec2_to_vec3(p0), lum_vec2_0(), color);
  lum__renderer2d_create_vertex(&v[1], lum_vec2_to_vec3(p1), lum_vec2_0(), color);
  lum__renderer2d_create_vertex(&v[2], lum_vec2_to_vec3(p2), lum_vec2_0(), color);
  static const uint32_t idx[3] = { 0, 1, 2 };
  lum_renderer2d_draw_mesh(v, 3, idx, 3, Lum_Primitive_Triangles, &g_renderer2d.sprite_material, &g_renderer2d.white_texture, layer);
}

void lum_renderer2d_draw_polygon_convex(const lum_Vec2* points, uint32_t count, int32_t layer, lum_Vec4 color) {
  if (!g_renderer2d.initialized) return;
  if (!points || count < 3) return;
  if (!lum__mesh_ensure_scratch_vertices(count)) return;
  for (uint32_t i = 0; i < count; ++i) {
    lum__renderer2d_create_vertex(&g_renderer2d.mesh_scratch_vertices[i], lum_vec2_to_vec3(points[i]), lum_vec2_0(), color);
  }

  lum_renderer2d_draw_mesh(g_renderer2d.mesh_scratch_vertices, count, NULL, 0, Lum_Primitive_Triangle_Fan, &g_renderer2d.sprite_material, &g_renderer2d.white_texture, layer);
}

void lum_renderer2d_draw_polyline(const lum_Vec2* points, uint32_t count, float thickness, int32_t layer, lum_Vec4 color) {
  if (!g_renderer2d.initialized) return;
  if (!points || count < 2) return;
  if (!lum__mesh_ensure_scratch_vertices(count)) return;
  for (uint32_t i = 0; i < count; ++i){
    lum__renderer2d_create_vertex(&g_renderer2d.mesh_scratch_vertices[i], lum_vec2_to_vec3(points[i]), lum_vec2_0(), color);
  }
  g_renderer2d.current_line_width = (thickness > 0.0f) ? thickness : 1.0f;
  lum_renderer2d_draw_mesh(g_renderer2d.mesh_scratch_vertices, count, NULL, 0, Lum_Primitive_Line_Strip, &g_renderer2d.sprite_material, &g_renderer2d.white_texture, layer);
}

void lum_renderer2d_draw_points(const lum_Vec2* points, uint32_t count, float size, int32_t layer, lum_Vec4 color) {
  if (!g_renderer2d.initialized) return;
  if (!points || count == 0) return;
  if (!lum__mesh_ensure_scratch_vertices(count)) return;
  for (uint32_t i = 0; i < count; ++i){
    lum__renderer2d_create_vertex(&g_renderer2d.mesh_scratch_vertices[i], lum_vec2_to_vec3(points[i]), lum_vec2_0(), color);
  }
  g_renderer2d.current_point_size = (size > 0.0f) ? size : 1.0f;
  lum_renderer2d_draw_mesh(g_renderer2d.mesh_scratch_vertices, count, NULL, 0, Lum_Primitive_Points, &g_renderer2d.sprite_material, &g_renderer2d.white_texture, layer);
}

uint32_t lum_renderer2d_get_draw_call_count(void) {
  if (!g_renderer2d.initialized) return 0u;
  return g_renderer2d.current_frame_draw_call_count;
}