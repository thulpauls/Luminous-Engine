#ifndef Lum_Serial_H
#define Lum_Serial_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "lum_math2d.h"
#include "lum_ecs.h"
#include "containers/lum_vector.h"

/*
* 序列化 Archive：组件序列化代码面向的唯一接口，格式无关。
* 文本实现为「关键词作用域式」平面格式：
*   - 行导向，每行一条记录（结构行：version/entity/parent/component；字段行：键 值...）
*   - 值以空白分隔；含空白的字符串加双引号（无转义语法）
*   - '#' 开头为注释行，空行忽略
*   - 读方向单遍流式 + 一行预读：r_* 按 key 匹配当前预读行，不匹配不消费、返回默认值；
*     结构关键字行永不被字段读取消费
*/

#define Lum_Serial_Max_Line 512
#define Lum_Serial_Max_Args 16
#define Lum_Serial_Max_Segment_Fields 32

typedef struct lum_Archive_line {
  char raw[Lum_Serial_Max_Line];
  char* argv[Lum_Serial_Max_Args];
  int argc;
  uint32_t line_no;
  bool consumed;
} lum_Archive_line;

typedef struct lum_Archive {
  bool writing;

  lum_Vector buffer;                    /* 写方向：char 缓冲 */

  const char* text;                     /* 读方向：整段文本 */
  size_t size;
  size_t pos;
  uint32_t line_no;

  lum_Vector segment;                   /* 读方向：当前组件段的全部字段行 */
  bool segment_loaded;
  lum_Archive_line pending;             /* 结构行（段结束处预读保留 / 段外直接预读） */
  bool has_pending;
} lum_Archive;

int  lum_serial_init_writer(lum_Archive* archive);
int  lum_serial_init_reader(lum_Archive* archive, const char* text);
void lum_serial_destroy(lum_Archive* archive);

/* ---------- 写方向 ---------- */

/* 结构行（entity/component/version/parent），格式化原样写入 */
int lum_serial_w_linef(lum_Archive* archive, const char* fmt, ...);

int lum_serial_w_f32 (lum_Archive* archive, const char* key, float value);
int lum_serial_w_i32 (lum_Archive* archive, const char* key, int32_t value);
int lum_serial_w_u32 (lum_Archive* archive, const char* key, uint32_t value);
int lum_serial_w_bool(lum_Archive* archive, const char* key, bool value);
int lum_serial_w_str (lum_Archive* archive, const char* key, const char* value);
int lum_serial_w_vec2(lum_Archive* archive, const char* key, lum_Vec2 value);
int lum_serial_w_vec4(lum_Archive* archive, const char* key, lum_Vec4 value);

/* 完成写入：返回文本（archive 拥有，destroy 前有效） */
const char* lum_serial_writer_finish(lum_Archive* archive, size_t* out_size);

/* ---------- 读方向 ---------- */

/* 按键匹配读取：返回 1=命中并消费该行，0=缺失（*out 保持默认值，不消费任何行） */
int lum_serial_r_f32 (lum_Archive* archive, const char* key, float def, float* out);
int lum_serial_r_i32 (lum_Archive* archive, const char* key, int32_t def, int32_t* out);
int lum_serial_r_u32 (lum_Archive* archive, const char* key, uint32_t def, uint32_t* out);
int lum_serial_r_bool(lum_Archive* archive, const char* key, bool def, bool* out);
int lum_serial_r_str (lum_Archive* archive, const char* key, const char* def, char* out, size_t cap);
int lum_serial_r_vec2(lum_Archive* archive, const char* key, lum_Vec2 def, lum_Vec2* out);
int lum_serial_r_vec4(lum_Archive* archive, const char* key, lum_Vec4 def, lum_Vec4* out);

/* 结构行导航（场景层主循环用）：预读行是否有效 / 其键与参数 / 消费掉它 */
bool        lum_serial_peek(lum_Archive* archive);
const char* lum_serial_peek_key(lum_Archive* archive);
int         lum_serial_peek_argc(lum_Archive* archive);
const char* lum_serial_peek_argv(lum_Archive* archive, int index);
uint32_t    lum_serial_peek_line(lum_Archive* archive);
void        lum_serial_consume(lum_Archive* archive);

/* 进入新的组件段：清空字段缓冲。场景层消费 component 行后、调 read 回调前调用 */
void lum_serial_begin_component(lum_Archive* archive);

/* ========== 声明式封装层：字段表驱动的组件序列化 ==========
   普通组件不写回调对——用一张宏表声明字段（偏移/键/默认值），注册时引擎自动获得读写能力。
   自定义回调（lum_ecs_set_component_serializer）仍可使用，组件级接口自动路由两者。 */

typedef enum lum_Component_field_kind {
  Lum_Component_field_F32,
  Lum_Component_field_I32,
  Lum_Component_field_U32,
  Lum_Component_field_Bool,
  Lum_Component_field_Vec2,
  Lum_Component_field_Vec4,
  Lum_Component_field_Str
} lum_Component_field_kind;

typedef union lum_Component_field_value {
  float        f;
  int32_t      i;
  uint32_t     u;
  bool         b;
  lum_Vec2     v2;
  lum_Vec4     v4;
  const char*  s;      /* Str 默认值；NULL 视为 "" */
} lum_Component_field_value;

typedef struct lum_Component_field_desc {
  const char*     key;
  lum_Component_field_kind  kind;
  size_t          offset;   /* 字段在组件结构体内的偏移（宏自动取） */
  size_t          size;     /* Str 为缓冲容量，其余为类型大小 */
  lum_Component_field_value def;      /* 字段缺失时的默认值 */
} lum_Component_field_desc;

#define Lum_Component_Make_Field_F32(T, member, key, def_value) \
  { key, Lum_Component_field_F32, offsetof(T, member), sizeof(float), { .f = (def_value) } }
#define Lum_Component_Make_Field_I32(T, member, key, def_value) \
  { key, Lum_Component_field_I32, offsetof(T, member), sizeof(int32_t), { .i = (def_value) } }
#define Lum_Component_Make_Field_U32(T, member, key, def_value) \
  { key, Lum_Component_field_U32, offsetof(T, member), sizeof(uint32_t), { .u = (def_value) } }
#define Lum_Component_Make_Field_Bool(T, member, key, def_value) \
  { key, Lum_Component_field_Bool, offsetof(T, member), sizeof(bool), { .b = (def_value) } }
#define Lum_Component_Make_Field_Vec2(T, member, key, def_x, def_y) \
  { key, Lum_Component_field_Vec2, offsetof(T, member), sizeof(lum_Vec2), { .v2 = { (def_x), (def_y) } } }
#define Lum_Component_Make_Field_Vec4(T, member, key, def_x, def_y, def_z, def_w) \
  { key, Lum_Component_field_Vec4, offsetof(T, member), sizeof(lum_Vec4), { .v4 = { (def_x), (def_y), (def_z), (def_w) } } }
#define Lum_Component_Make_Field_Str(T, member, key, def_value) \
  { key, Lum_Component_field_Str, offsetof(T, member), sizeof(((T*)0)->member), { .s = (def_value) } }

/* 注册组件并绑定字段表（= lum_ecs_register_component + 表驱动序列化）。
   字段数上限为段容量 Lum_Serial_Max_Segment_Fields，超出告警丢弃 */
lum_Component_type lum_serial_register_component(lum_Ecs* ecs, const char* name, size_t size,
                                                 const lum_Component_field_desc* fields, uint32_t field_count);

/* 组件级一站式读写：字段表优先，其次自定义回调；两者皆无返回 0（调用方可降级直转储）。
   write 输出完整块（component <名字> 行 + 全部字段行）；
   read 内部自调 begin_component——调用方消费掉 component 行后直接调用即可 */
int lum_serial_component_write(const lum_Ecs* ecs, lum_Component_type type, const void* component, lum_Archive* archive);
int lum_serial_component_read (lum_Ecs* ecs, lum_Component_type type, void* component, lum_Archive* archive);

#endif //Lum_Serial_H
