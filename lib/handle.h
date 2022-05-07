#ifndef NHL_HANDLE_H_
#define NHL_HANDLE_H_

#include <curl/curl.h>
#include <sqlite3.h>

#include <nhl/core.h>
#include "dict.h"
#include "list.h"

struct Nhl {
    NhlInitParams *params;

    NhlDict *schedules;
    NhlDict *games;
    NhlDict *teams;
    NhlDict *players;

    NhlDict *conferences;
    NhlDict *divisions;
    NhlDict *franchises;

    NhlDict *game_statuses;
    NhlDict *game_types;
    NhlDict *player_positions;
    NhlDict *roster_statuses;

    CURL *curl;
    sqlite3 *db;

    /* List of URLs that the handle has already accessed or tried to access. */
    NhlList *visited_urls;

    /* If nonzero, nhl_prepare() is called without a matching call to nhl_finish(). */
    int in_progress;
};

#endif /* NHL_HANDLE_H_ */
