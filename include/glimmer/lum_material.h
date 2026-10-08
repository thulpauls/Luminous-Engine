#ifndef Lum_Material_H
#define Lum_Material_H

#include "glimmer/lum_math2d.h"
#include "glimmer/lum_shader.h"
#include "glimmer/lum_texture.h"
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

/* ========================================================================
 * lum_Material3d —— 3D 渲染器认识的材质（M3）
 *
 * 与上面的 lum_Material 的分工：
 *   · lum_Material   = **通用容器**：任意 shader + 4 个贴图槽 + 16 个按名字设置的自定义参数。
 *                      语义由使用者定义，2D 渲染器就是逐个参数往下发的。
 *   · lum_Material3d = **固定语义**：albedo 贴图 + 基色乘子 + uv 平铺。3D 渲染器的内置着色器
 *                      认识这几个字段，所以不必走"字符串查 uniform"那一套，也不怕名字打错。
 * 两者放在同一个文件里，因为它们是同一个概念（材质）在不同渲染器下的两种形态。
 *
 * 字段刻意很少：M3 只需要"贴不贴图、什么颜色、贴图铺几遍"。粗糙度/金属度/法线贴图那些
 * 等光照与 PBR 那期再加，加的时候是往这个结构体里添字段，不会推翻现在的用法。
 *
 * 生命周期：材质**不持有**贴图（只借指针，与 lum_Material 的贴图槽一致），
 * 纹理的创建与销毁仍由调用方负责。
 * ======================================================================== */

typedef struct lum_Material3d {
  bool initialized;
  const lum_Texture2d* albedo;   /* NULL = 用渲染器内置的 1×1 白纹理（等价于"只有基色"） */
  lum_Vec4 color;                /* 基色乘子，默认白（1,1,1,1） */
  lum_Vec2 uv_scale;             /* uv 平铺倍数，默认 (1,1) */
} lum_Material3d;

/* 就地把材质设成"无贴图、白色、uv 不平铺" */
void lum_material3d_init(lum_Material3d* material);

/* 纯色材质的便捷构造（按值返回，风格与 lum_vec4_create 一致） */
lum_Material3d lum_material3d_create(lum_Vec4 color);

void lum_material3d_set_albedo(lum_Material3d* material, const lum_Texture2d* albedo);
void lum_material3d_set_color(lum_Material3d* material, lum_Vec4 color);
void lum_material3d_set_uv_scale(lum_Material3d* material, lum_Vec2 uv_scale);

#endif //Lum_Material_H