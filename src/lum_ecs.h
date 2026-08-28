#ifndef Lum_Ecs_H
#define Lum_Ecs_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint32_t lum_Entity;
#define Lum_Entity_Null 0u
#define Lum_Ecs_Max_Entities (1u << 20)

typedef uint32_t lum_Component_type;
#define Lum_Component_Invalid ((lum_Component_type)0xFFFFFFFFu)

typedef struct lum_Ecs lum_Ecs;

lum_Ecs* lum_ecs_create(void);
void     lum_ecs_destroy(lum_Ecs* ecs);

lum_Entity lum_ecs_create_entity (lum_Ecs* ecs);
bool       lum_ecs_alive         (const lum_Ecs* ecs, lum_Entity e);
void       lum_ecs_destroy_entity(lum_Ecs* ecs, lum_Entity e);
void       lum_ecs_flush         (lum_Ecs* ecs);

/* 槽位序遍历（序列化/调试用）：slot_index ∈ [0, entity_slot_count)；
   空槽或死实体返回 Lum_Entity_Null。句柄 ⇄ 槽位下标互转 */
uint32_t   lum_ecs_entity_slot_count(const lum_Ecs* ecs);
lum_Entity lum_ecs_entity_at_slot   (const lum_Ecs* ecs, uint32_t slot_index);
uint32_t   lum_ecs_entity_slot_index(lum_Entity entity);

lum_Component_type lum_ecs_register_component(lum_Ecs* ecs, const char* name, size_t size);
uint32_t           lum_ecs_component_type_count(const lum_Ecs* ecs);
lum_Component_type lum_ecs_component_type_from_name(const lum_Ecs* ecs, const char* name);
const char*        lum_ecs_component_name(const lum_Ecs* ecs, lum_Component_type type);

/* 组件序列化：回调面向 Archive 抽象（前置声明，核心不依赖 serial 模块） */
struct lum_Archive;
typedef int (*lum_Component_write_fn)(const void* component, struct lum_Archive* archive);
typedef int (*lum_Component_read_fn)(void* component, struct lum_Archive* archive);

void lum_ecs_set_component_serializer(lum_Ecs* ecs, lum_Component_type type,
                                      lum_Component_write_fn write, lum_Component_read_fn read);
bool lum_ecs_component_serializer(const lum_Ecs* ecs, lum_Component_type type,
                                  lum_Component_write_fn* out_write, lum_Component_read_fn* out_read);

/* 枚举实体挂的全部组件类型；返回总数（cap 不够时 out 截断，可用返回值重新分配） */
uint32_t lum_ecs_entity_component_types(const lum_Ecs* ecs, lum_Entity e,
                                        lum_Component_type* out_types, uint32_t cap);

/* 声明式字段表：ECS 只存不透明指针（内容由 serial 层定义解释），随世界销毁自动消失 */
void lum_ecs_set_component_field_table(lum_Ecs* ecs, lum_Component_type type,
                                       const void* fields, uint32_t field_count);
bool lum_ecs_component_field_table(const lum_Ecs* ecs, lum_Component_type type,
                                   const void** out_fields, uint32_t* out_field_count);

void* lum_ecs_add   (lum_Ecs* ecs, lum_Entity e, lum_Component_type t);
void* lum_ecs_get   (const lum_Ecs* ecs, lum_Entity e, lum_Component_type t);
bool  lum_ecs_has   (const lum_Ecs* ecs, lum_Entity e, lum_Component_type t);
void  lum_ecs_remove(lum_Ecs* ecs, lum_Entity e, lum_Component_type t);

typedef struct lum_Ecs_view {
  lum_Ecs* ecs;
  const struct lum_Pool* pools[3];   /* 严格保持调用者的参数顺序：each 的出参按此赋值 */
  int      n;
  uint32_t primary;                  /* 元素最少的池下标（主迭代，性能优化） */
  uint32_t i;
} lum_Ecs_view;

lum_Ecs_view   lum_ecs_view_create (lum_Ecs* ecs, lum_Component_type a);
lum_Ecs_view   lum_ecs_view_create2(lum_Ecs* ecs, lum_Component_type a, lum_Component_type b);
lum_Ecs_view   lum_ecs_view_create3(lum_Ecs* ecs, lum_Component_type a, lum_Component_type b, lum_Component_type c);

lum_Entity lum_ecs_view_each (lum_Ecs_view* v, void** out_a);/* 返回 NULL 表示结束 */
lum_Entity lum_ecs_view_each2(lum_Ecs_view* v, void** out_a, void** out_b);
lum_Entity lum_ecs_view_each3(lum_Ecs_view* v, void** out_a, void** out_b, void** out_c);

#endif //Lum_Ecs_H
