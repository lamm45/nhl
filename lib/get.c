#include "get.h"

#include <string.h>

#include "cache.h"
#include "dict.h"


NhlStatus nhl_get_from_dict(Nhl *nhl, NhlDict *dict, int max_age, void *key, void **item, int *age) {
    char *timestamp;
    *item = nhl_dict_find(dict, key, &timestamp);
    if (*item != NULL) {
        *age = nhl_cache_timestamp_age(nhl, timestamp);
        if (0 <= *age) {
            if (*age <= max_age || max_age < 0) {
                return NHL_CACHE_READ_OK;
            }
            return NHL_CACHE_READ_EXPIRED;
        }
        return NHL_CACHE_READ_ERROR;
    }
    return NHL_CACHE_READ_NOT_FOUND;
}


NhlStatus nhl_get_from_cache(Nhl *nhl, int prev_age, int max_age,
                             NhlStatus (*get_from_cache_cb)(Nhl*, int, void**, void*), void *data_cb,
                             void **cache_item) {

    NhlStatus status = 0;
    *cache_item = NULL;
    status |= get_from_cache_cb(nhl, 0, cache_item, data_cb);

    if (status & NHL_CACHE_READ_OK) {
        char *timestamp = ((NhlCacheMeta *) ((NhlCacheMeta*) *cache_item)->source)->timestamp;
        int cache_age = nhl_cache_timestamp_age(nhl, timestamp);
        if (0 <= cache_age && (cache_age <= max_age || max_age < 0)) {
            return status;
        }
    }

    status = get_from_cache_cb(nhl, 1, cache_item, data_cb);
    if (status & NHL_CACHE_READ_OK) {
        char *timestamp = ((NhlCacheMeta *) ((NhlCacheMeta*) *cache_item)->source)->timestamp;
        int cache_age = nhl_cache_timestamp_age(nhl, timestamp);
        status &= NHL_CACHE_WRITE_OK;
        if (0 <= cache_age) {
            if (cache_age < max_age || max_age < 0) {
                return status | NHL_CACHE_READ_OK;
            } else if (prev_age < 0 || cache_age < prev_age) {
                return status | NHL_CACHE_READ_EXPIRED;
            }
            return status | NHL_CACHE_READ_NOT_FOUND; /* found, but not useful */
        }
        return status | NHL_CACHE_READ_ERROR;
    }
    return status | NHL_CACHE_READ_NOT_FOUND;
}

/* Good from dict: NHL_CACHE_READ_OK, *item != NULL, cache_item == NULL
 * Good from db (possibly after download): NHL_CACHE_READ_OK, *item == NULL, cache_item != NULL
 * Expired from dict: NHL_CACHE_READ_EXPIRED, *item != NULL, cache_item NULL
 * Expired from db: NHL_CACHE_READ_EXPIRED, *item == NULL, cache_item != NULL
 * Otherwise: *item == NULL, cache_item NULL
 *
 * get_from_cache_cb: nhl, with-download-flag, cache_item[in/out], user_data (vaihda jarjestys?)
 */
NhlStatus nhl_get(Nhl *nhl, NhlDict *dict, void *key, int max_age,
                  NhlStatus (*get_from_cache_cb)(Nhl *, int, void **, void *), void *data_cb,
                  void **item, void **cache_item) {

    int dict_age = -1;
    NhlStatus dict_status = 0;
    NhlStatus cache_status = 0;

    /* Check if dict already contains an up-to-date item */
    if (dict != NULL) {
        dict_status = nhl_get_from_dict(nhl, dict, max_age, key, item, &dict_age);
        if (dict_status & NHL_CACHE_READ_OK) {
            *cache_item = NULL;
            return dict_status;
        }
    }

    /* Check if dict contains an expired but still newest item */
    cache_status = nhl_get_from_cache(nhl, dict_age, max_age, get_from_cache_cb, data_cb, cache_item);
    if (dict_status & NHL_CACHE_READ_EXPIRED && !(cache_status & (NHL_CACHE_READ_OK | NHL_CACHE_READ_EXPIRED))) {
        *cache_item = NULL;
        return dict_status;
    }

    /* Dict item is not useful, so release it */
    if (*item != NULL && dict != NULL) {
        nhl_dict_unref(dict, *item);
        *item = NULL;
    }

    return cache_status;
}
