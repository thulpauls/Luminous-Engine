#include "lum_quad.h"

#include <glad/glad.h>
#include <assert.h>
#include <string.h>

int lum_quad_init(lum_Quad* quad) {
  assert(quad);
  
  memset(quad, 0, sizeof(quad));
  
  glGenVertexArrays(1, &quad->VAO);
  glGenBuffers(1, &quad->VBO);
  glGenBuffers(1, &quad->EBO);

  if (!(quad->VAO && quad->VBO && quad->EBO)) {
    lum_quad_destroy(quad);
    return 0;
  }
  
  const float vertices[20] = {
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
    1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
    0.0f, 1.0f, 0.0f, 0.0f, 1.0f
  };

  const unsigned int indices[6] = {
    0, 1, 2, 2, 3, 0
  };
  
  glBindVertexArray(quad->VAO);
  glBindBuffer(GL_ARRAY_BUFFER, quad->VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad->EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)(sizeof(float) * 3));
  glEnableVertexAttribArray(1);

  glBindVertexArray(0);

  quad->initialized = true;
  return 1;
}

void lum_quad_destroy(lum_Quad* quad) {
  assert(quad);
  
  if (quad->VAO) glDeleteVertexArrays(1, &quad->VAO);
  if (quad->VBO) glDeleteBuffers(1, &quad->VBO);
  if (quad->EBO) glDeleteBuffers(1, &quad->EBO);
  quad->VAO = 0;
  quad->VBO = 0;
  quad->EBO = 0;
  quad->initialized = false;
}

void lum_quad_bind(const lum_Quad* quad) {
  assert(quad);
  if (quad->initialized) glBindVertexArray(quad->VAO);
}
void lum_quad_unbind() { glBindVertexArray(0); }

void lum_quad_draw(const lum_Quad* quad) {
  assert(quad);

  if (quad->initialized) {
    lum_quad_bind(quad);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    lum_quad_unbind();
  }
}

