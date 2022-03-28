#include "handle.h"

#include <stdlib.h>

#include <curl/curl.h>
#include <sqlite3.h>

#include "cache.h"
#include "mem.h"

/* TODO: check curl and sqlite error codes */


void nhl_default_params(NhlInitParams *params) {
    params->cache_file = NULL;
    params->dump_folder = NULL;

    params->offline = 0;
    params->verbose = 0;

    params->schedule_max_age = 60;
    params->game_live_max_age = 60;
    params->game_final_max_age = 60;
    params->team_max_age = -1;
    params->player_max_age = -1;
    params->league_max_age = -1;
    params->meta_max_age = -1;
}

static NhlInitParams *copy_params(NhlInitParams *dest, const NhlInitParams *src) {
    *dest = *src;
    if (src->cache_file != NULL) {
        dest->cache_file = nhl_copy_string(src->cache_file);
    }
    if (src->dump_folder != NULL) {
        dest->dump_folder = nhl_copy_string(src->dump_folder);
    }
    return dest;
}

static void free_params(NhlInitParams *params) {
    if (params != NULL) {
        free(params->cache_file);
        free(params->dump_folder);
        free(params);
    }
}


static int nhl_handle_initialize(Nhl *nhl, const NhlInitParams *params) {
    nhl->params = malloc(sizeof(NhlInitParams));
    if (nhl->params == NULL)
        return 0;

    if (params != NULL) {
        copy_params(nhl->params, params);
    } else {
        nhl_default_params(nhl->params);
    }

    nhl->schedules = nhl_dict_create(NHL_DICT_KEY_TEXT);
    nhl->games = nhl_dict_create(NHL_DICT_KEY_NUMERIC);
    nhl->teams = nhl_dict_create(NHL_DICT_KEY_NUMERIC);
    nhl->players = nhl_dict_create(NHL_DICT_KEY_NUMERIC);

    nhl->conferences = nhl_dict_create(NHL_DICT_KEY_NUMERIC);
    nhl->divisions = nhl_dict_create(NHL_DICT_KEY_NUMERIC);
    nhl->franchises = nhl_dict_create(NHL_DICT_KEY_NUMERIC);

    nhl->game_statuses = nhl_dict_create(NHL_DICT_KEY_TEXT);
    nhl->game_types = nhl_dict_create(NHL_DICT_KEY_TEXT);
    nhl->player_positions = nhl_dict_create(NHL_DICT_KEY_TEXT);
    nhl->roster_statuses = nhl_dict_create(NHL_DICT_KEY_TEXT);

    nhl->visited_urls = nhl_list_create();


    nhl->curl = curl_easy_init();
    curl_easy_setopt(nhl->curl, CURLOPT_ACCEPT_ENCODING, "");

    if (nhl->params->cache_file == NULL)
        sqlite3_open_v2(":memory:", &nhl->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    else
        sqlite3_open_v2(params->cache_file, &nhl->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);

    nhl->in_progress = 0;

    return 1;
}


Nhl *nhl_init(const NhlInitParams *params) {
    Nhl *nhl = malloc(sizeof(Nhl));
    nhl_handle_initialize(nhl, params);
    return nhl;
}

void nhl_reset(Nhl *nhl, const NhlInitParams *params) {
    nhl_close(nhl);
    nhl_handle_initialize(nhl, params);
}

void nhl_close(Nhl *nhl) {
    if (nhl != NULL) {
        sqlite3_close(nhl->db);
        curl_easy_cleanup(nhl->curl);

        nhl_list_delete(nhl->visited_urls);

        nhl_dict_delete(nhl->roster_statuses);
        nhl_dict_delete(nhl->player_positions);
        nhl_dict_delete(nhl->game_types);
        nhl_dict_delete(nhl->game_statuses);
        nhl_dict_delete(nhl->franchises);
        nhl_dict_delete(nhl->divisions);
        nhl_dict_delete(nhl->conferences);
        nhl_dict_delete(nhl->players);
        nhl_dict_delete(nhl->teams);
        nhl_dict_delete(nhl->games);
        nhl_dict_delete(nhl->schedules);

        free_params(nhl->params);
        free(nhl);
    }
}


int nhl_prepare(Nhl *nhl) {
    if (nhl->in_progress) {
        return 0;
    }
    sqlite3_exec(nhl->db, "BEGIN;", NULL, NULL, NULL);
    nhl->in_progress = 1;
    return 1;
}

void nhl_finish(Nhl *nhl, int start) {
    if (start) {
        sqlite3_exec(nhl->db, "COMMIT;", NULL, NULL, NULL);
        nhl->in_progress = 0;
    }
}
