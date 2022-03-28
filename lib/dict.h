#ifndef NHL_DICT_H_
#define NHL_DICT_H_


/* Dictionary-like type that stores (possible non-unique) keys, values and timestamps. */
typedef struct NhlDict NhlDict;

/* Key types for NhlDict. */
typedef enum NhlDictKeyType {
    /* Integer type (int). */
    NHL_DICT_KEY_NUMERIC,
    /* String type (char *). */
    NHL_DICT_KEY_TEXT
} NhlDictKeyType;

/* Create new dict with the given key type. Release with nhl_dict_delete(). */
NhlDict *nhl_dict_create(NhlDictKeyType key_type);

/* Release resources acquired with nhl_dict_create(). */
void nhl_dict_delete(NhlDict *dict);

/* Return value and timestamp for given key, and increment the reference count.
 * Returns NULL if the key does not exist.
 * In case of duplicate keys, the most recently added value is returned. */
void *nhl_dict_find(NhlDict *dict, const void *key, char **timestamp);

/* Insert key, value and timestamp, and set reference count to one. Returns nonzero if success. */
int nhl_dict_insert(NhlDict *dict, const void *key, void *val, const char *timestamp);

/* Decrement reference count for a given (unique) value and return the decremented
 * reference count. Returns -1 if the value is not found. */
int nhl_dict_unref(NhlDict *dict, void *val);


#endif /* NHL_DICT_H_ */
