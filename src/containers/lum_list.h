#ifndef Lum_Containers_List_H
#define Lum_Containers_List_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct lum_List_node {
  struct lum_List_node *prev, *next;
} lum_List_node;

typedef struct lum_List {
  lum_List_node head;    /* 哨兵：head.next = 首元素，head.prev = 尾元素 */
  uint32_t      size;
} lum_List;

static inline void lum_list_init(lum_List* list) {
  list->head.prev = &list->head;
  list->head.next = &list->head;
  list->size = 0;
}

static inline bool lum_list_empty(const lum_List* list) { return list->head.next == &list->head; }
static inline uint32_t lum_list_size(const lum_List* list) { return list->size; }
static inline lum_List_node* lum_list_front(lum_List* list) { return list->head.next != &list->head ? list->head.next : NULL; }
static inline lum_List_node* lum_list_back(lum_List* list) { return list->head.prev != &list->head ? list->head.prev : NULL; }

static inline void lum_list_insert_after(lum_List* list, lum_List_node* pos, lum_List_node* node) {
  node->prev = pos;
  node->next = pos->next;
  pos->next->prev = node;
  pos->next = node;
  list->size++;
}

static inline void lum_list_insert_before(lum_List* list, lum_List_node* pos, lum_List_node* node) {
  lum_list_insert_after(list, pos->prev, node);
}

static inline void lum_list_push_front(lum_List* list, lum_List_node* node) {
  lum_list_insert_after(list, &list->head, node);
}

static inline void lum_list_push_back(lum_List* list, lum_List_node* node) {
  lum_list_insert_after(list, list->head.prev, node);
}

static inline lum_List_node* lum_list_pop_front(lum_List* list) {
  lum_List_node* node = list->head.next;
  if (node == &list->head) return NULL;
  node->prev->next = node->next;
  node->next->prev = node->prev;
  list->size--;
  return node;
}

static inline lum_List_node* lum_list_pop_back(lum_List* list) {
  lum_List_node* node = list->head.prev;
  if (node == &list->head) return NULL;
  node->prev->next = node->next;
  node->next->prev = node->prev;
  list->size--;
  return node;
}

static inline void lum_list_remove(lum_List* list, lum_List_node* node) {
  node->prev->next = node->next;
  node->next->prev = node->prev;
  list->size--;
}

#define lum_list_each(list, node) \
  for ((node) = (list)->head.next; (node) != &(list)->head; (node) = (node)->next)

/* 迭代中可安全 remove 当前 node 的版本 */
#define lum_list_each_safe(list, node, tmp) \
  for ((node) = (list)->head.next, (tmp) = (node)->next; \
       (node) != &(list)->head; \
       (node) = (tmp), (tmp) = (node)->next)

#define lum_list_entry(node, T, member) \
  ((T*)((char*)(node) - offsetof(T, member)))

#endif //Lum_Containers_List_H
