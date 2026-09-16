#ifndef Lum_Containers_Vector_H
#define Lum_Containers_Vector_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct lum_Vector {
  unsigned char* data;
  uint32_t       size;
  uint32_t       capacity;
  size_t         elem_size;
} lum_Vector;

int   lum_vector_init(lum_Vector* v, size_t elem_size);
void  lum_vector_destroy(lum_Vector* v);

void* lum_vector_push(lum_Vector* v, const void* elem);
void* lum_vector_emplace(lum_Vector* v);
void  lum_vector_pop(lum_Vector* v);

void* lum_vector_get(lum_Vector* v, uint32_t index);
const void* lum_vector_get_const(const lum_Vector* v, uint32_t index);

void lum_vector_remove_swap(lum_Vector* v, uint32_t index);
void lum_vector_remove(lum_Vector* v, uint32_t index);

void lum_vector_clear(lum_Vector* v);
int  lum_vector_reserve(lum_Vector* v, uint32_t capacity);
int  lum_vector_shrink(lum_Vector* v);

bool     lum_vector_empty(const lum_Vector* v);
uint32_t lum_vector_size(const lum_Vector* v);
void*    lum_vector_data(lum_Vector* v);

#define lum_vector_get_t(v, index, T)   ((T*)lum_vector_get((v), (index)))
#define lum_vector_at_t(v, index, T)    ((const T*)lum_vector_get_const((v), (index)))

#endif //Lum_Containers_Vector_H
