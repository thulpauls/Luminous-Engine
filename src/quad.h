#ifndef Lum_Quad_H
#define Lum_Quad_H

typedef struct lum_Quad {
  unsigned int VAO, VBO, EBO;
  bool initialized;
} lum_Quad;

int lum_quad_init(lum_Quad* quad);
void lum_quad_destroy(lum_Quad* quad);
void lum_quad_bind(const lum_Quad* quad);
void lum_quad_unbind();
void lum_quad_draw(const lum_Quad* quad);

#endif //Lum_Quad_H
