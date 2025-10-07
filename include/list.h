#ifndef LIST_H
#define LIST_H

typedef struct {
	int capacity;
	int length;
	void **items;
} list_t;

#define list_for_each(list, var) \
	int i = 0; \
	for (var = *(list->items); i < list->length; i++, var = list->items[i])

list_t *create_list(void);
void list_free(list_t *list);
void list_add(list_t *list, void *item);
void list_insert(list_t *list, int index, void *item);
void list_del(list_t *list, int index);
void list_free_items_and_destroy(list_t *list);
#endif
