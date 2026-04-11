#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../include/list.h"

list_t *create_list(void) {
	list_t *list = malloc(sizeof(list_t));
	if (!list) {
		perror("malloc()");
		return NULL;
	}
	list->capacity = 10;
	list->length = 0;
	list->items = malloc(sizeof(uintptr_t) * list->capacity);
	if (!list->items) {
		perror("malloc()");
		free(list);
		return NULL;
	}
	return list;
}

static void list_resize(list_t *list) {
	if (list->length == list->capacity) {
		list->capacity *= 2;
		list->items = realloc(list->items, sizeof(uintptr_t) * list->capacity);
	}
}

void list_free(list_t *list) {
	if (list == NULL) {
		return;
	}
	free(list->items);
	free(list);
}

void list_add(list_t *list, uintptr_t item) {
	list_resize(list);
	list->items[list->length++] = item;
}

void list_del(list_t *list, int index) {
	list->length--;
	memmove(&list->items[index], &list->items[index + 1], sizeof(void*) * (list->length - index));
}

void list_remove(list_t *list, uintptr_t item) {
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
