#ifndef NHL_GET_H_
#define NHL_GET_H_

#include "handle.h"

NhlStatus nhl_get(Nhl *nhl, NhlDict *dict, void *key, int max_age,
                  NhlStatus (*get_from_cache_cb)(Nhl *, int, void **, void *), void *data_cb,
                  void **item, void **cache_item);


#endif /* NHL_GET_H_ */
