#include "list.h"

#include <stdlib.h>
#include <string.h>

#include "mem.h"


NhlList *nhl_list_create(void) {
    NhlList *list = malloc(sizeof(NhlList));
    list->elem = NULL;
    list->next = NULL;
    return list;
}

void nhl_list_delete(NhlList *list) {
    if (list != NULL) {
        NhlList *iter = list;
        NhlList *next;
        while (iter->elem != NULL) {
            next = iter->next;
            free(iter->elem);
            free(iter);
            iter = next;
        }
        free(iter);
    }
}

NhlList *nhl_list_prepend(NhlList *list, const char *elem) {
    NhlList *first = malloc(sizeof(NhlList));
    first->elem = nhl_copy_string(elem);
    first->next = list;
    return first;
}

int nhl_list_contains(const NhlList *list, const char *elem) {
    const NhlList *iter;
    for (iter = list; iter->elem != NULL; iter = iter->next) {
        if (!strcmp(iter->elem, elem)) {
            return 1;
        }
    }
    return 0;
}
