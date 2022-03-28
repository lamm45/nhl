#include "dict.h"

#include <stdlib.h>
#include <string.h>

#include "mem.h"


/* Macro for checking if the idx'th item in the dict has the given key. */
#define key_equals(dict, idx, key) \
    ((dict->key_type == NHL_DICT_KEY_NUMERIC && dict->items[idx].key.num == *(int *) key) || \
     (dict->key_type == NHL_DICT_KEY_TEXT && !strcmp(dict->items[idx].key.text, (char *) key)))


/* Single item in a dict. */
typedef struct NhlDictItem {
    union {
        int num;
        char *text;
    } key;
    void *val;
    int num_refs;
    char *timestamp;
} NhlDictItem;

/* Complete dict. */
struct NhlDict {
    NhlDictKeyType key_type;
    NhlDictItem *items;
    int num_items;
    int num_alloc;
};


NhlDict *nhl_dict_create(NhlDictKeyType key_type) {
    NhlDict *dict = malloc(sizeof(NhlDict));
    dict->key_type = key_type;
    dict->items = NULL;
    dict->num_items = 0;
    dict->num_alloc = 0;
    return dict;
}

void nhl_dict_delete(NhlDict *dict) {
    if (dict != NULL) {
        free(dict->items);
        free(dict);
    }
}


void *nhl_dict_find(NhlDict *dict, const void *key, char **timestamp) {
    /* Reverse loop finds the most recently added item, in case of duplicate keys. */
    int idx;
    for (idx = dict->num_items - 1; idx != -1; --idx)
        if (key_equals(dict, idx, key)) {
            dict->items[idx].num_refs++;
            *timestamp = dict->items[idx].timestamp;
            return dict->items[idx].val;
        }
    return NULL;
}


int nhl_dict_insert(NhlDict *dict, const void *key, void *val, const char *timestamp) {
    /* Grow dict if necessary */
    if (dict->num_alloc <= dict->num_items) {
        if (dict->num_alloc == 0)
            dict->num_alloc = 8;
        else
            dict->num_alloc *= 2;

        dict->items = realloc(dict->items, dict->num_alloc * sizeof(NhlDictItem));
        if (dict->items == NULL)
            return 0;
    }

    /* Copy key */
    if (dict->key_type == NHL_DICT_KEY_NUMERIC)
        dict->items[dict->num_items].key.num = *(int *) key;
    else
        dict->items[dict->num_items].key.text = nhl_copy_string((char *) key);

    /* Append new value to items. */
    dict->items[dict->num_items].val = val;
    dict->items[dict->num_items].num_refs = 1;
    dict->items[dict->num_items].timestamp = nhl_copy_string(timestamp);
    dict->num_items++;

    return 1;
}


int nhl_dict_unref(NhlDict *dict, void *val) {
    int idx;
    for (idx = 0; idx != dict->num_items; ++idx) {
        if (dict->items[idx].val == val) {
            /* Decremented reference count */
            int num_refs = --(dict->items[idx].num_refs);

            if (num_refs == 0) {
                int k;

                /* Release resources if refcount went to zero */
                free(dict->items[idx].timestamp);
                if (dict->key_type == NHL_DICT_KEY_TEXT) {
                    free(dict->items[idx].key.text);
                }

                /* Adjust dict size */
                dict->num_items--;
                for (k = idx; k != dict->num_items; ++k) {
                    dict->items[k] = dict->items[k+1];
                }
            }

            return num_refs;
        }
    }
    return -1; /* not found */
}
