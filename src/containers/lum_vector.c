#include "containers/lum_vector.h"
#include "lum_assert.h"
#include "lum_log.h"
#include <stdlib.h>
#include <string.h>

#define Lum_Vector_Initial_Cap 8u

int lum_vector_init(lum_Vector* v, size_t elem_size) {
  Lum_Assert(v);
  memset(v, 0, sizeof(*v));
  if (elem_size == 0) {
    Lum_Log_Error("vector: elem_size must be > 0.");
    return 0;
  }
  v->elem_size = elem_size;
  return 1;
}

void lum_vector_destroy(lum_Vector* v) {
  Lum_Assert(v);
  free(v->data);
  memset(v, 0, sizeof(*v));
}

static int lum__vector_grow(lum_Vector* v, uint32_t need) {
  if (need <= v->capacity) return 1;
  uint32_t cap = v->capacity ? v->capacity : Lum_Vector_Initial_Cap;
  while (cap < need) cap *= 2u;

  unsigned char* data = (unsigned char*)realloc(v->data, (size_t)cap * v->elem_size);
  if (!data) {
    Lum_Log_Error("vector: alloc failed (need %u x %llu).", need, (unsigned long long)v->elem_size);
    return 0;
  }
  v->data = data;
  v->capacity = cap;
  return 1;
}

int lum_vector_reserve(lum_Vector* v, uint32_t capacity) {
  Lum_Assert(v);
  return lum__vector_grow(v, capacity);
}

void* lum_vector_push(lum_Vector* v, const void* elem) {
  Lum_Assert(v && elem);
  if (!lum__vector_grow(v, v->size + 1u)) return NULL;
  void* slot = v->data + (size_t)v->size * v->elem_size;
  memcpy(slot, elem, v->elem_size);
  v->size++;
  return slot;
}

void* lum_vector_emplace(lum_Vector* v) {
  Lum_Assert(v);
  if (!lum__vector_grow(v, v->size + 1u)) return NULL;
  void* slot = v->data + (size_t)v->size * v->elem_size;
  memset(slot, 0, v->elem_size);
  v->size++;
  return slot;
}

void lum_vector_pop(lum_Vector* v) {
  Lum_Assert(v);
  if (v->size == 0) return;
  v->size--;
}

void* lum_vector_get(lum_Vector* v, uint32_t index) {
  Lum_Assert(v && index < v->size);
  if (index >= v->size) return NULL;
  return v->data + (size_t)index * v->elem_size;
}

const void* lum_vector_get_const(const lum_Vector* v, uint32_t index) {
  Lum_Assert(v && index < v->size);
  if (index >= v->size) return NULL;
  return v->data + (size_t)index * v->elem_size;
}

void lum_vector_remove_swap(lum_Vector* v, uint32_t index) {
  Lum_Assert(v && index < v->size);
  if (index >= v->size) return;
  uint32_t last = v->size - 1u;
  if (index != last) {
    memcpy(v->data + (size_t)index * v->elem_size,
           v->data + (size_t)last * v->elem_size,
           v->elem_size);
  }
  v->size = last;
}

void lum_vector_remove(lum_Vector* v, uint32_t index) {
  Lum_Assert(v && index < v->size);
  if (index >= v->size) return;
  uint32_t last = v->size - 1u;
  if (index != last) {
    memmove(v->data + (size_t)index * v->elem_size,
            v->data + (size_t)(index + 1u) * v->elem_size,
            (size_t)(last - index) * v->elem_size);
  }
  v->size = last;
}

void lum_vector_clear(lum_Vector* v) {
  Lum_Assert(v);
  v->size = 0;
}

int lum_vector_shrink(lum_Vector* v) {
  Lum_Assert(v);
  if (v->capacity == v->size) return 1;

  if (v->size == 0) {
    free(v->data);
    v->data = NULL;
    v->capacity = 0;
    return 1;
  }

  unsigned char* data = (unsigned char*)realloc(v->data, (size_t)v->size * v->elem_size);
  if (!data) {
    Lum_Log_Warn("vector: shrink alloc failed, keep capacity %u.", v->capacity);
    return 0;
  }
  v->data = data;
  v->capacity = v->size;
  return 1;
}

bool lum_vector_empty(const lum_Vector* v) {
  Lum_Assert(v);
  return v->size == 0;
}

uint32_t lum_vector_size(const lum_Vector* v) {
  Lum_Assert(v);
  return v->size;
}

void* lum_vector_data(lum_Vector* v) {
  Lum_Assert(v);
  return v->data;
}
