#ifndef NHL_LIST_H_
#define NHL_LIST_H_

typedef struct NhlList NhlList;

/* Forward list */
struct NhlList {
    char *elem;
    NhlList *next;
};

/* Create new list. After use, release with nhl_list_delete(). */
NhlList *nhl_list_create(void);

/* Release resources allocated by nhl_list_create(). */
void nhl_list_delete(NhlList *list);

/* Add item to the beginning of the list. */
NhlList *nhl_list_prepend(NhlList *list, const char *item);

/* Returns nonzero if the list contains item. */
int nhl_list_contains(const NhlList *list, const char *item);

#endif /* NHL_LIST_H_ */
