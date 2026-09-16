#ifndef Lum_Material_H
#define Lum_Material_H

#include "lum_math2d.h"
#include "lum_shader.h"
#include "lum_texture.h"
#include <stdint.h>
#include <stdbool.h>

#define Lum_Material_Max_Params (16u)
#define Lum_Material_Max_Name (31u)
#define Lum_Material_Max_Textures (4u)

typedef enum lum_Material_parameter_type {
    Lum_Material_Parameter_1i = 0,
    Lum_Material_Parameter_1f,
    Lum_Material_Parameter_2v,
    Lum_Material_Parameter_3v,
    Lum_Material_Parameter_4v,
    Lum_Material_Parameter_4m,
} lum_Material_parameter_type;

typedef struct lum_Material_parameter {
    char name[Lum_Material_Max_Name + 1u];
    lum_Material_parameter_type type;
    union {
        int i;
        float f;
        lum_Vec2 v2;
        lum_Vec3 v3;
        lum_Vec4 v4;
        lum_Mat4 m4;
    } value;
} lum_Material_parameter;

typedef struct lum_Material {
    bool initialized;
    const lum_Shader* shader;
    const lum_Texture2d* textures[Lum_Material_Max_Textures];
    lum_Material_parameter params[Lum_Material_Max_Params];
    uint32_t param_count;
} lum_Material;

void lum_material_init(lum_Material* material);
void lum_material_create(lum_Material* material, const lum_Shader* shader);
void lum_material_destroy(lum_Material* material);

void lum_material_set_texture(lum_Material* material, uint32_t slot, const lum_Texture2d* texture);
void lum_material_set_shader(lum_Material* material, const lum_Shader* shader);

int lum_material_remove_parameter(lum_Material* material, const char* name);

int lum_material_set_parameter_1i(lum_Material* material, const char* name, int value);
int lum_material_set_parameter_1f(lum_Material* material, const char* name, float value);
int lum_material_set_parameter_2v(lum_Material* material, const char* name, lum_Vec2 value);
int lum_material_set_parameter_3v(lum_Material* material, const char* name, lum_Vec3 value);
int lum_material_set_parameter_4v(lum_Material* material, const char* name, lum_Vec4 value);
int lum_material_set_parameter_4m(lum_Material* material, const char* name, lum_Mat4 value);

lum_Material_parameter lum_material_construct_parameter_1i(const char* name, int value);
lum_Material_parameter lum_material_construct_parameter_1f(const char* name, float value);
lum_Material_parameter lum_material_construct_parameter_2v(const char* name, lum_Vec2 value);
lum_Material_parameter lum_material_construct_parameter_3v(const char* name, lum_Vec3 value);
lum_Material_parameter lum_material_construct_parameter_4v(const char* name, lum_Vec4 value);
lum_Material_parameter lum_material_construct_parameter_4m(const char* name, lum_Mat4 value);

typedef struct lum_Material_variant_pool {
  lum_Material* variants;
  uint32_t capacity;
  uint32_t count;
  uint32_t next;
  uint32_t key_count;
  const lum_Shader* shader;
} lum_Material_variant_pool;

int  lum_material_variant_pool_init(lum_Material_variant_pool* pool, const lum_Shader* shader, uint32_t capacity, uint32_t key_count);
void lum_material_variant_pool_destroy(lum_Material_variant_pool* pool);
lum_Material* lum_material_variant_pool_acquire(lum_Material_variant_pool* pool, const lum_Material_parameter* key);

#endif //Lum_Material_H