#include "glimmer/lum_material.h"
#include "glimmer/lum_log.h"
#include <string.h>
#include <stdlib.h>
#include "glimmer/lum_assert.h"
#include <stddef.h>

static lum_Material_parameter* lum__material_find_param(lum_Material* material, const char* name) {
    for (uint32_t i = 0; i < material->param_count; ++i) {
        if (strcmp(material->params[i].name, name) == 0) {
            return &material->params[i];
        }
    }
    return NULL;
}

static lum_Material_parameter* lum__material_append_param(lum_Material* material, const char* name) {
    if (material->param_count >= Lum_Material_Max_Params) {
        Lum_Log_Warn("material: param capacity (%u) reached, cannot add '%s'.", Lum_Material_Max_Params, name);
        return NULL;
    }
    if (strlen(name) > Lum_Material_Max_Name) {
        Lum_Log_Warn("material: param name '%s' too long (max %u chars).", name, Lum_Material_Max_Name);
        return NULL;
    }

    lum_Material_parameter* p = &material->params[material->param_count++];
    memset(p, 0, sizeof(*p));
    strncpy(p->name, name, Lum_Material_Max_Name);
    return p;
}

static lum_Material_parameter* lum__material_get_or_create(lum_Material* material, const char* name, lum_Material_parameter_type type) {
    lum_Material_parameter* p = lum__material_find_param(material, name);
    if (!p) {
        p = lum__material_append_param(material, name);
        if (!p) return NULL;
    }
    p->type = type;
    return p;
}

void lum_material_init(lum_Material* material) {
    Lum_Assert(material);
    memset(material, 0, sizeof(*material));
}

void lum_material_create(lum_Material* material, const lum_Shader* shader) {
    Lum_Assert(material);
    lum_material_init(material);
    material->shader = shader;
    material->initialized = true;
}

void lum_material_destroy(lum_Material* material) {
    Lum_Assert(material);
    if (!material->initialized) return;
    memset(material, 0, sizeof(*material));
}

void lum_material_set_texture(lum_Material* material, uint32_t slot, const lum_Texture2d* texture) {
    Lum_Assert(material);
    if (!material->initialized) return;
    if (slot >= Lum_Material_Max_Textures) return;
    material->textures[slot] = texture;
}

void lum_material_set_shader(lum_Material* material, const lum_Shader* shader) {
    Lum_Assert(material);
    material->shader = shader;
}

int lum_material_remove_parameter(lum_Material* material, const char* name) {
    Lum_Assert(material && name);
    if (!material->initialized) return 0;
    
    for (size_t i = 0; i < material->param_count; ++i) {
        if (strcmp(material->params[i].name, name) == 0) {
            for (size_t j = i; j + 1 < material->param_count; ++j) {
                material->params[j] = material->params[j + 1];
            }
            material->param_count--;
            memset(&material->params[material->param_count], 0, sizeof(lum_Material_parameter));
            return 1;
        }
    }
    return 0;
}

int lum_material_set_parameter_1i(lum_Material* material, const char* name, int value) {
    Lum_Assert(material && name);
    if (!material->initialized) return 0;
    lum_Material_parameter* p = lum__material_get_or_create(material, name, Lum_Material_Parameter_1i);
    if (!p) return 0;
    p->value.i = value;
    return 1;
}

int lum_material_set_parameter_1f(lum_Material* material, const char* name, float value) {
    Lum_Assert(material && name);
    if (!material->initialized) { Lum_Log_Warn("material: not initialized ('%s').", name); return 0; }
    lum_Material_parameter* p = lum__material_get_or_create(material, name, Lum_Material_Parameter_1f);
    if (!p) return 0;
    p->value.f = value;
    return 1;
}

int lum_material_set_parameter_2v(lum_Material* material, const char* name, lum_Vec2 value) {
    Lum_Assert(material && name);
    if (!material->initialized) { Lum_Log_Warn("material: not initialized ('%s').", name); return 0; }
    lum_Material_parameter* p = lum__material_get_or_create(material, name, Lum_Material_Parameter_2v);
    if (!p) return 0;
    p->value.v2 = value;
    return 1;
}

int lum_material_set_parameter_3v(lum_Material* material, const char* name, lum_Vec3 value) {
    Lum_Assert(material && name);
    if (!material->initialized) { Lum_Log_Warn("material: not initialized ('%s').", name); return 0; }
    lum_Material_parameter* p = lum__material_get_or_create(material, name, Lum_Material_Parameter_3v);
    if (!p) return 0;
    p->value.v3 = value;
    return 1;
}

int lum_material_set_parameter_4v(lum_Material* material, const char* name, lum_Vec4 value) {
    Lum_Assert(material && name);
    if (!material->initialized) { Lum_Log_Warn("material: not initialized ('%s').", name); return 0; }
    lum_Material_parameter* p = lum__material_get_or_create(material, name, Lum_Material_Parameter_4v);
    if (!p) return 0;
    p->value.v4 = value;
    return 1;
}

int lum_material_set_parameter_4m(lum_Material* material, const char* name, lum_Mat4 value) {
    Lum_Assert(material && name);
    if (!material->initialized) { Lum_Log_Warn("material: not initialized ('%s').", name); return 0; }
    lum_Material_parameter* p = lum__material_get_or_create(material, name, Lum_Material_Parameter_4m);
    if (!p) return 0;
    p->value.m4 = value;
    return 1;
}

static void lum__material_construct_parameter_common(lum_Material_parameter* parameter, lum_Material_parameter_type type, const char* name, const void* value_ptr, size_t value_size) {
    Lum_Assert(parameter && name);
    memset(parameter, 0, sizeof(*parameter));
    strncpy(parameter->name, name, Lum_Material_Max_Name);
    parameter->type = type;
    if (value_ptr) memcpy(&parameter->value, value_ptr, value_size);
}

lum_Material_parameter lum_material_construct_parameter_1i(const char* name, int value) {
    lum_Material_parameter p;
    lum__material_construct_parameter_common(&p, Lum_Material_Parameter_1i, name, &value, sizeof(value));
    return p;
}
lum_Material_parameter lum_material_construct_parameter_1f(const char* name, float value) {
    lum_Material_parameter p;
    lum__material_construct_parameter_common(&p, Lum_Material_Parameter_1f, name, &value, sizeof(value));
    return p;
}
lum_Material_parameter lum_material_construct_parameter_2v(const char* name, lum_Vec2 value) {
    lum_Material_parameter p;
    lum__material_construct_parameter_common(&p, Lum_Material_Parameter_2v, name, &value, sizeof(value));
    return p;
}
lum_Material_parameter lum_material_construct_parameter_3v(const char* name, lum_Vec3 value) {
    lum_Material_parameter p;
    lum__material_construct_parameter_common(&p, Lum_Material_Parameter_3v, name, &value, sizeof(value));
    return p;
}
lum_Material_parameter lum_material_construct_parameter_4v(const char* name, lum_Vec4 value) {
    lum_Material_parameter p;
    lum__material_construct_parameter_common(&p, Lum_Material_Parameter_4v, name, &value, sizeof(value));
    return p;
}
lum_Material_parameter lum_material_construct_parameter_4m(const char* name, lum_Mat4 value) {
    lum_Material_parameter p;
    lum__material_construct_parameter_common(&p, Lum_Material_Parameter_4m, name, &value, sizeof(value));
    return p;
}

static int lum__material_parameter_value_equal(const lum_Material_parameter* a, const lum_Material_parameter* b) {
    if (a->type != b->type) return 0;
    switch (a->type) {
        case Lum_Material_Parameter_1i: return a->value.i == b->value.i;
        case Lum_Material_Parameter_1f: return a->value.f == b->value.f;
        case Lum_Material_Parameter_2v: return a->value.v2.x == b->value.v2.x && a->value.v2.y == b->value.v2.y;
        case Lum_Material_Parameter_3v: return a->value.v3.x == b->value.v3.x && a->value.v3.y == b->value.v3.y && a->value.v3.z == b->value.v3.z;
        case Lum_Material_Parameter_4v: return a->value.v4.x == b->value.v4.x && a->value.v4.y == b->value.v4.y && a->value.v4.z == b->value.v4.z && a->value.v4.w == b->value.v4.w;
        case Lum_Material_Parameter_4m: return memcmp(&a->value.m4, &b->value.m4, sizeof(lum_Mat4)) == 0;
        default: return 0;
    }
}

static int lum__material_matches_key(const lum_Material* material, const lum_Material_parameter* key, uint32_t key_count) {
    for (uint32_t i = 0; i < key_count; ++i) {
        const lum_Material_parameter* p = lum__material_find_param((lum_Material*)material, key[i].name);
        if (!p || !lum__material_parameter_value_equal(p, &key[i])) return 0;
    }
    return 1;
}

int lum_material_variant_pool_init(lum_Material_variant_pool* pool, const lum_Shader* shader, uint32_t capacity, uint32_t key_count) {
    Lum_Assert(pool && shader);
    Lum_Assert(capacity > 0u && key_count > 0u && key_count <= Lum_Material_Max_Params);
    memset(pool, 0, sizeof(*pool));

    pool->variants = (lum_Material*)calloc(capacity, sizeof(lum_Material));
    if (!pool->variants) {
        memset(pool, 0, sizeof(*pool));
        Lum_Log_Warn("material: variant pool allocation failed (cap %u).", capacity);
        return 0;
    }

    pool->capacity = capacity;
    pool->key_count = key_count;
    pool->shader = shader;
    return 1;
}

void lum_material_variant_pool_destroy(lum_Material_variant_pool* pool) {
    Lum_Assert(pool);
    if (!pool->variants) return;
    for (uint32_t i = 0; i < pool->count; ++i) {
        lum_material_destroy(&pool->variants[i]);
    }
    free(pool->variants);
    memset(pool, 0, sizeof(*pool));
}

lum_Material* lum_material_variant_pool_acquire(lum_Material_variant_pool* pool, const lum_Material_parameter* key) {
    Lum_Assert(pool && key);
    if (!pool->variants) return NULL;

    for (uint32_t i = 0; i < pool->count; ++i) {
        if (lum__material_matches_key(&pool->variants[i], key, pool->key_count)) {
            return &pool->variants[i];
        }
    }

    lum_Material* material = &pool->variants[pool->next];
    lum_material_destroy(material);
    lum_material_create(material, pool->shader);
    for (uint32_t k = 0; k < pool->key_count; ++k) {
        material->params[k] = key[k];
    }
    material->param_count = pool->key_count;

    if (pool->count < pool->capacity) pool->count++;
    pool->next = (pool->next + 1u) % pool->capacity;
    return material;
}

/* ============================ lum_Material3d（M3） ============================ */

void lum_material3d_init(lum_Material3d* material) {
  Lum_Assert(material);
  if (!material) return;
  material->initialized = true;
  material->albedo = NULL;
  material->color = lum_vec4_create(1.0f, 1.0f, 1.0f, 1.0f);
  material->uv_scale = lum_vec2_create(1.0f, 1.0f);
}

lum_Material3d lum_material3d_create(lum_Vec4 color) {
  lum_Material3d material;
  lum_material3d_init(&material);
  material.color = color;
  return material;
}

void lum_material3d_set_albedo(lum_Material3d* material, const lum_Texture2d* albedo) {
  Lum_Assert(material);
  if (!material) return;
  material->initialized = true;
  material->albedo = albedo;
}

void lum_material3d_set_color(lum_Material3d* material, lum_Vec4 color) {
  Lum_Assert(material);
  if (!material) return;
  material->initialized = true;
  material->color = color;
}

void lum_material3d_set_uv_scale(lum_Material3d* material, lum_Vec2 uv_scale) {
  Lum_Assert(material);
  if (!material) return;
  material->initialized = true;
  material->uv_scale = uv_scale;
}