#ifndef EMU_LIST_H
#define EMU_LIST_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	int capacity;
	int length;
	uintptr_t *items;
} list_t;

#define list_for_each(list, var) \
	int dummydummy = 0; \
	for (var = list->items[0]; dummydummy < list->length; dummydummy++, var = list->items[dummydummy]) \

static inline list_t *create_list(void) {
	list_t *list = malloc(sizeof(list_t));
	if (!list) {
		return NULL;
	}
	list->capacity = 10;
	list->length = 0;
	list->items = malloc(sizeof(uintptr_t) * list->capacity);
	return list;
}

static inline void list_resize(list_t *list) {
	if (list->length == list->capacity) {
		list->capacity *= 2;
		list->items = realloc(list->items, sizeof(uintptr_t) * list->capacity);
	}
}

static inline void list_free(list_t *list) {
	if (list == NULL) {
		return;
	}
	free(list->items);
	free(list);
}

static inline void list_add(list_t *list, uintptr_t item) {
	list_resize(list);
	list->items[list->length++] = item;
}

static inline void list_del(list_t *list, int index) {
	list->length--;
	memmove(&list->items[index], &list->items[index + 1], sizeof(void*) * (list->length - index));
}

static inline void list_remove(list_t *list, uintptr_t item) {
	uintptr_t match;
	int j = 0;
	list_for_each(list, match) {
		j++;
		if (match == item)
			break;
	}
	if (!match) {
		fprintf(stderr, "list_remove: element not found\n");
		return;
	}
	list_del(list, j);
}
#endif
