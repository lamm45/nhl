#include "mem.h"

#include <stdlib.h>
#include <string.h>

char *nhl_copy_string(const char *str) {
    if (str != NULL) {
        size_t len = strlen(str);
        char *copy = malloc(len + 1);
        if (copy != NULL)
            return memcpy(copy, str, len + 1);
    }
    return NULL;
}
