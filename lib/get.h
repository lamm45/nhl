#ifndef NHL_GET_H_
#define NHL_GET_H_

#include "handle.h"

/* Automatic getter for different kinds of objects.
 * 1. If an item with `key` is found from `dict`, and if the timestamp of the item is at most
 *     `max_age` seconds old, then `*item` is set to point to the item. Returns NHL_CACHE_READ_OK.
 * 2. If a serialized item is made available after a call to `get_from_cache_cb`, and if the
 *     timestamp of the serialized item is at most `max_age` seconds old, then `*cache_item` is set
 *     to point to the serialized item. Returns NHL_CACHE_READ_OK.
 * 3. If an item with `key` is found from `dict`, then `*item` is set to point to the item. Returns
 *     NHL_CACHE_READ_EXPIRED.
 * 4. If a serialized item is made available after a call to `get_from_cache_cb`, then
 *     `*cache_item` is set to point to the serializsed item. Returns NHL_CACHE_READ_EXPIRED.
 * 5. Otherwise, returns NHL_CACHE_READ_NOT_FOUND or NHL_CACHE_READ_ERROR.
 *
 * Above, not setting `*item` or `*cache_item` implies setting it to NULL.
 *
 * If the second argument to the callback function `get_from_cache_cb` is nonzero, then (and only
 * then), an attempt to download most recent content from the Internet should be made. The third
 * argument is the destination for `cache_item` and should be free'd if non-NULL. The fourth
 * argument can be any user data, passed through `data_cb`. */
NhlStatus nhl_get(Nhl *nhl, NhlDict *dict, void *key, int max_age,
                  NhlStatus (*get_from_cache_cb)(Nhl *, int, void **, void *), void *data_cb,
                  void **item, void **cache_item);


#endif /* NHL_GET_H_ */
