#ifndef EMU_LIST_H
#define EMU_LIST_H

#include <stdint.h>

typedef struct {
	int capacity;
	int length;
	uintptr_t *items;
} list_t;

#define list_for_each(list, var) \
	int dummydummy = 0; \
	for (var = list->items[0]; dummydummy < list->length; dummydummy++, var = list->items[dummydummy]) \

list_t *create_list(void);
void list_remove(list_t *list, uintptr_t item);
void list_free(list_t *list);
void list_add(list_t *list, uintptr_t item);
void list_insert(list_t *list, int index, uintptr_t item);
void list_del(list_t *list, int index);
void list_free_items_and_destroy(list_t *list);
#endif
