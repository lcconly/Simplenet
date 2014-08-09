#ifndef __LIST_H__
#define __LIST_H__


#include "stdio.h"
#include "assert.h"
#include "stdbool.h"

struct ListHead {
	struct ListHead *prev, *next;
};
typedef struct ListHead ListHead;


static inline void
list_add(ListHead *prev, ListHead *next, ListHead *data) {
	assert(data != NULL);
	data->prev = prev;
	data->next = next;
	if(prev != NULL) prev->next = data;
	if(next != NULL) next->prev = data;
}


static inline void
list_add_before(ListHead *list, ListHead *data) {
	list_add(list->prev, list, data);
}

static inline void
list_add_after(ListHead *list, ListHead *data) {
	list_add(list, list->next, data);
}

static inline void
list_del(ListHead *data) {
	assert(data != NULL);
	ListHead *prev = data->prev;//为空则队头
	ListHead *next = data->next;//为空则队尾
	if(prev != NULL) prev->next = next;
	if(next != NULL) next->prev = prev;
}

static inline void
list_init(ListHead *list) {
	list->prev = list->next = list;
}

static inline bool
list_empty(ListHead *list) {
	return list == list->next;
}

#define list_foreach(ptr, head) \
	for ((ptr) = (head)->next; (ptr) != (head); (ptr) = (ptr)->next)
#define list_each(ptr, head) \
	for ((ptr) = (head)->prev; (ptr) != (head); (ptr) = (ptr)->prev)
#define list_entry(ptr, type, member) \
	((type*)((char*)(ptr) - (int)(&((type*)0)->member)))

#endif
