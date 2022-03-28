#include <nhl/player.h>

#include <stdlib.h>
#include <stdio.h>

#include <nhl/team.h>
#include <nhl/update.h>
#include <nhl/utils.h>
#include "cache.h"
#include "dict.h"
#include "get.h"
#include "handle.h"
#include "mem.h"
#include "urls.h"


/* Convert cached position to position. */
static NhlPlayerPosition *create_position(const NhlCachePosition *cache_position) {
    NhlPlayerPosition *position = malloc(sizeof(NhlPlayerPosition));
    position->code = nhl_copy_string(cache_position->code);
    position->name = nhl_copy_string(cache_position->fullName);
    position->abbreviation = nhl_copy_string(cache_position->abbrev);
    position->type = nhl_copy_string(cache_position->type);
    return position;
}

/* Release resources acquired by create_position(). */
static void delete_position(NhlPlayerPosition *position) {
    if (position != NULL) {
        free(position->code);
        free(position->name);
        free(position->abbreviation);
        free(position->type);
        free(position);
    }
}

/* Callback for getting cached position. */
static NhlStatus get_cache_position_cb(Nhl *nhl, int update, void **dest, void *userp) {
    NhlStatus status = 0;
    NhlCachePosition *cache_position = NULL;
    char *code = userp;

    if (update) {
        status |= nhl_update_from_url(nhl, NHL_URL_POSITIONS, NHL_CONTENT_POSITIONS);
    }

    cache_position = nhl_cache_position_get(nhl, code);
    if (cache_position == NULL) {
        return status | NHL_CACHE_READ_NOT_FOUND;
    }

    if (*dest != NULL) {
        nhl_cache_position_free(*dest);
    }
    *dest = cache_position;

    return status | NHL_CACHE_READ_OK;
}

NhlStatus nhl_player_position_get(Nhl *nhl, const char *position_code, NhlQueryLevel level,
                                  NhlPlayerPosition **position) {
    int start = nhl_prepare(nhl);
    NhlCachePosition *cache_position = NULL;
    char *code = nhl_copy_string(position_code);
    NhlStatus status = nhl_get(nhl, nhl->player_positions, code, nhl->params->meta_max_age,
                               get_cache_position_cb, code, (void **) position, (void **) &cache_position);
    free(code);

    if (cache_position != NULL) {
        *position = create_position(cache_position);
        nhl_dict_insert(nhl->player_positions, position_code, *position, cache_position->meta->timestamp);
        nhl_cache_position_free(cache_position);
    }

    (void) level;
    nhl_finish(nhl, start);
    return status;
}

void nhl_player_position_unget(Nhl *nhl, NhlPlayerPosition *position) {
    if (position != NULL && nhl_dict_unref(nhl->player_positions, position) == 0) {
        delete_position(position);
    }
}


/* Convert cached roster status to roster status. */
static NhlPlayerRosterStatus *create_roster_status(const NhlCacheRosterStatus *cache_roster_status) {
    NhlPlayerRosterStatus *roster_status = malloc(sizeof(NhlPlayerRosterStatus));
    roster_status->code = nhl_copy_string(cache_roster_status->code);
    roster_status->description = nhl_copy_string(cache_roster_status->description);
    return roster_status;
}

/* Release resources acquired by create_roster_status(). */
static void delete_roster_status(NhlPlayerRosterStatus *roster_status) {
    if (roster_status != NULL) {
        free(roster_status->code);
        free(roster_status->description);
        free(roster_status);
    }
}

/* Callback for getting cached roster status. */
static NhlStatus get_cache_roster_status_cb(Nhl *nhl, int update, void **dest, void *userp) {
    NhlStatus status = 0;
    NhlCacheRosterStatus *cache_roster_status = NULL;
    char *code = userp;

    if (update) {
        status |= nhl_update_from_url(nhl, NHL_URL_ROSTER_STATUSES, NHL_CONTENT_ROSTER_STATUSES);
    }

    cache_roster_status = nhl_cache_roster_status_get(nhl, code);
    if (cache_roster_status == NULL) {
        return status | NHL_CACHE_READ_NOT_FOUND;
    }

    if (*dest != NULL) {
        nhl_cache_roster_status_free(*dest);
    }
    *dest = cache_roster_status;

    return status | NHL_CACHE_READ_OK;
}

NhlStatus nhl_player_roster_status_get(Nhl *nhl, const char *roster_code, NhlQueryLevel level, NhlPlayerRosterStatus **roster_status) {
    int start = nhl_prepare(nhl);
    NhlCacheRosterStatus *cache_roster_status = NULL;
    char *code = nhl_copy_string(roster_code);
    NhlStatus status = nhl_get(nhl, nhl->roster_statuses, code, nhl->params->meta_max_age,
                               get_cache_roster_status_cb, code, (void **) roster_status, (void **) &cache_roster_status);
    free(code);

    if (cache_roster_status != NULL) {
        *roster_status = create_roster_status(cache_roster_status);
        nhl_dict_insert(nhl->roster_statuses, roster_code, *roster_status, cache_roster_status->meta->timestamp);
        nhl_cache_roster_status_free(cache_roster_status);
    }

    (void) level;
    nhl_finish(nhl, start);
    return status;
}

void nhl_player_roster_status_unget(Nhl *nhl, NhlPlayerRosterStatus *roster_status) {
    if (roster_status != NULL && nhl_dict_unref(nhl->roster_statuses, roster_status) == 0) {
        delete_roster_status(roster_status);
    }
}


/* Convert cached player to player. */
static NhlPlayer *create_player(const NhlCachePlayer *cache_player) {
    NhlPlayer *player = malloc(sizeof(NhlPlayer));
    player->unique_id = cache_player->id;
    player->first_name = nhl_copy_string(cache_player->firstName);
    player->last_name = nhl_copy_string(cache_player->lastName);
    player->full_name = nhl_copy_string(cache_player->fullName);
    player->birth_date = nhl_string_to_date(cache_player->birthDate);
    player->birth_city = nhl_copy_string(cache_player->birthCity);
    player->birth_state_province = nhl_copy_string(cache_player->birthStateProvince);
    player->birth_country = nhl_copy_string(cache_player->birthCountry);
    player->nationality = nhl_copy_string(cache_player->nationality);
    player->height = nhl_string_to_height(cache_player->height);
    player->weight_pounds = cache_player->weight;
    player->shoots_catches = nhl_copy_string(cache_player->shootsCatches);
    player->active = cache_player->active;
    player->current_team = NULL;
    player->roster_status = NULL;
    player->primary_position = NULL;
    if (sscanf(cache_player->primaryNumber, "%d", &player->primary_number) != 1) {
        player->primary_number = -1;
    }
    player->captain = cache_player->captain;
    player->alternate_captain = cache_player->alternateCaptain;
    player->rookie = cache_player->rookie;
    return player;
}

/* Release resources acquired with create_player(). */
static void delete_player(NhlPlayer *player) {
    if (player != NULL) {
        free(player->first_name);
        free(player->last_name);
        free(player->full_name);
        free(player->birth_city);
        free(player->birth_state_province);
        free(player->birth_country);
        free(player->nationality);
        free(player->shoots_catches);
        free(player);
    }
}

/* Callback for getting cached player. */
static NhlStatus get_cache_player_cb(Nhl *nhl, int update, void **dest, void *userp) {
    NhlStatus status = 0;
    NhlCachePlayer *cache_player = NULL;
    int *player_id = userp;

    if (update) {
        char url[sizeof(NHL_URL_PREFIX_PEOPLE) + 1 + NHL_INTSTR_LEN + 1];
        sprintf(url, "%s/%d", NHL_URL_PREFIX_PEOPLE, *player_id);
        status |= nhl_update_from_url(nhl, url, NHL_CONTENT_PEOPLE);
    }

    cache_player = nhl_cache_player_get(nhl, *player_id);
    if (cache_player == NULL) {
        return status | NHL_CACHE_READ_NOT_FOUND;
    }

    if (*dest != NULL) {
        nhl_cache_player_free(*dest);
    }
    *dest = cache_player;

    return status | NHL_CACHE_READ_OK;
}

NhlStatus nhl_player_get(Nhl *nhl, int player_id, NhlQueryLevel level, NhlPlayer **player) {
    int start = nhl_prepare(nhl);
    NhlCachePlayer *cache_player = NULL;
    NhlStatus status = nhl_get(nhl, nhl->players, &player_id, nhl->params->player_max_age,
                               get_cache_player_cb, &player_id, (void **) player, (void **) &cache_player);

    if (cache_player != NULL) {
        *player = create_player(cache_player);
        nhl_dict_insert(nhl->players, &player_id, *player, cache_player->meta->timestamp);
    }

    if (*player != NULL && level & NHL_QUERY_BASIC) {
        int team_id = 0;
        NhlTeam *team_old = (*player)->current_team;
        NhlTeam *team;

        char *primary_position_code = NULL;
        NhlPlayerPosition *position_old = (*player)->primary_position;
        NhlPlayerPosition *position;

        char *roster_status_code = NULL;
        NhlPlayerRosterStatus *roster_status_old = (*player)->roster_status;
        NhlPlayerRosterStatus *roster_status;

        if (team_old != NULL && roster_status_old != NULL && position_old != NULL) {
            team_id = team_old->unique_id;
            primary_position_code = nhl_copy_string(position_old->code);
            roster_status_code = nhl_copy_string(roster_status_old->code);
        } else {
            if (cache_player == NULL) {
                cache_player = nhl_cache_player_get(nhl, player_id);
            }
            if (cache_player != NULL) {
                team_id = cache_player->currentTeam;
                roster_status_code = nhl_copy_string(cache_player->rosterStatus);
                primary_position_code = nhl_copy_string(cache_player->primaryPosition);
            }
        }

        status |= nhl_team_get(nhl, team_id, level, &team);
        (*player)->current_team = team;
        nhl_team_unget(nhl, team_old);

        status |= nhl_player_position_get(nhl, primary_position_code, level, &position);
        (*player)->primary_position = position;
        nhl_player_position_unget(nhl, position_old);
        free(primary_position_code);

        status |= nhl_player_roster_status_get(nhl, roster_status_code, level, &roster_status);
        (*player)->roster_status = roster_status;
        nhl_player_roster_status_unget(nhl, roster_status_old);
        free(roster_status_code);
    }

    nhl_cache_player_free(cache_player);
    nhl_finish(nhl, start);
    return status;
}

void nhl_player_unget(Nhl *nhl, NhlPlayer *player) {
    if (player != NULL && nhl_dict_unref(nhl->players, player) == 0) {
        nhl_team_unget(nhl, player->current_team);
        nhl_player_roster_status_unget(nhl, player->roster_status);
        nhl_player_position_unget(nhl, player->primary_position);
        delete_player(player);
    }
}
