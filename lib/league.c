#include <nhl/league.h>

#include <stdlib.h>
#include <stdio.h>

#include <nhl/update.h>
#include "cache.h"
#include "dict.h"
#include "get.h"
#include "handle.h"
#include "mem.h"
#include "urls.h"


/* Create conference from cached conference. Release with delete_conference(). */
static NhlConference *create_conference(const NhlCacheConference *cache_conference) {
    NhlConference *conference = malloc(sizeof(NhlConference));
    conference->unique_id = cache_conference->id;
    conference->name = nhl_copy_string(cache_conference->name);
    conference->name_short = nhl_copy_string(cache_conference->shortName);
    conference->abbreviation = nhl_copy_string(cache_conference->abbreviation);
    conference->active = cache_conference->active;
    return conference;
}

/* Release resources acquired by create_conference(). */
static void delete_conference(NhlConference *conference) {
    if (conference != NULL) {
        free(conference->name);
        free(conference->name_short);
        free(conference->abbreviation);
        free(conference);
    }
}

/* Callback for getting cached conference. */
static NhlStatus get_cache_conference_cb(Nhl *nhl, int update, void **dest, void *userp) {
    NhlStatus status = 0;
    NhlCacheConference *cache_conference = NULL;
    int *conference_id = userp;

    if (update) {
        status |= nhl_update_from_url(nhl, NHL_URL_CONFERENCES, NHL_CONTENT_CONFERENCES);
    }

    cache_conference = nhl_cache_conference_get(nhl, *conference_id);
    if (cache_conference == NULL) {
        if (update) { /* Try alternate URL */
            char url[sizeof(NHL_URL_CONFERENCES) + 1 + NHL_INTSTR_LEN + 1];
            sprintf(url, "%s/%d", NHL_URL_CONFERENCES, *conference_id);
            status |= nhl_update_from_url(nhl, url, NHL_CONTENT_CONFERENCES);
            cache_conference = nhl_cache_conference_get(nhl, *conference_id);
        }
        if (cache_conference == NULL) {
            return status | NHL_CACHE_READ_NOT_FOUND;
        }
    }

    if (*dest != NULL) {
        nhl_cache_conference_free(*dest);
    }
    *dest = cache_conference;

    return status | NHL_CACHE_READ_OK;
}

NhlStatus nhl_conference_get(Nhl *nhl, int conference_id, NhlQueryLevel level, NhlConference **conference) {
    int start = nhl_prepare(nhl);
    NhlCacheConference *cache_conference = NULL;
    NhlStatus status = nhl_get(nhl, nhl->conferences, &conference_id, nhl->params->league_max_age,
                     get_cache_conference_cb, &conference_id, (void **) conference, (void **) &cache_conference);

    if (cache_conference != NULL) {
        *conference = create_conference(cache_conference);
        nhl_dict_insert(nhl->conferences, &conference_id, *conference, cache_conference->meta->timestamp);
        nhl_cache_conference_free(cache_conference);
    }

    (void) level;
    nhl_finish(nhl, start);
    return status;
}

void nhl_conference_unget(Nhl *nhl, NhlConference *conference) {
    if (conference != NULL && nhl_dict_unref(nhl->conferences, conference) == 0) {
        delete_conference(conference);
    }
}


/* Create division from cached division. Release with delete_division(). */
static NhlDivision *create_division(const NhlCacheDivision *cache_division) {
    NhlDivision *division = malloc(sizeof(NhlDivision));
    division->unique_id = cache_division->id;
    division->name = nhl_copy_string(cache_division->name);
    division->name_short = nhl_copy_string(cache_division->nameShort);
    division->abbreviation = nhl_copy_string(cache_division->abbreviation);
    division->conference = NULL;
    division->active = cache_division->active;
    return division;
}

/* Release resources acquired with create_division(). */
static void delete_division(NhlDivision *division) {
    if (division != NULL) {
        free(division->name);
        free(division->name_short);
        free(division->abbreviation);
        free(division);
    }
}

/* Callback for getting cached division. */
static NhlStatus get_cache_division_cb(Nhl *nhl, int update, void **dest, void *userp) {
    NhlStatus status = 0;
    NhlCacheDivision *cache_division = NULL;
    int *division_id = userp;

    if (update) {
        status |= nhl_update_from_url(nhl, NHL_URL_DIVISIONS, NHL_CONTENT_DIVISIONS);
    }

    cache_division = nhl_cache_division_get(nhl, *division_id);
    if (cache_division == NULL) {
        if (update) { /* Try alternate URL */
            char url[sizeof(NHL_URL_DIVISIONS) + 1 + NHL_INTSTR_LEN + 1];
            sprintf(url, "%s/%d", NHL_URL_DIVISIONS, *division_id);
            status |= nhl_update_from_url(nhl, url, NHL_CONTENT_DIVISIONS);
            cache_division = nhl_cache_division_get(nhl, *division_id);
        }
        if (cache_division == NULL) {
            return status | NHL_CACHE_READ_NOT_FOUND;
        }
    }

    if (*dest != NULL) {
        nhl_cache_division_free(*dest);
    }
    *dest = cache_division;

    return status | NHL_CACHE_READ_OK;
}

NhlStatus nhl_division_get(Nhl *nhl, int division_id, NhlQueryLevel level, NhlDivision **division) {
    int start = nhl_prepare(nhl);
    NhlCacheDivision *cache_division = NULL;
    NhlStatus status = nhl_get(nhl, nhl->divisions, &division_id, nhl->params->league_max_age,
                               get_cache_division_cb, &division_id, (void **) division, (void **) &cache_division);

    if (cache_division != NULL) {
        *division = create_division(cache_division);
        nhl_dict_insert(nhl->divisions, &division_id, *division, cache_division->meta->timestamp);
    }

    if (*division != NULL && level & NHL_QUERY_BASIC) {
        int conference_id = 0;
        NhlConference *conference_old = (*division)->conference;
        NhlConference *conference;

        if (conference_old != NULL) {
            conference_id = conference_old->unique_id;
        } else {
            if (cache_division == NULL) {
                cache_division = nhl_cache_division_get(nhl, division_id);
            }
            if (cache_division != NULL) {
                conference_id = cache_division->conference;
            }
        }

        status |= nhl_conference_get(nhl, conference_id, level, &conference);
        (*division)->conference = conference;
        nhl_conference_unget(nhl, conference_old);
    }

    nhl_cache_division_free(cache_division);
    nhl_finish(nhl, start);
    return status;
}

void nhl_division_unget(Nhl *nhl, NhlDivision *division) {
    if (division != NULL && nhl_dict_unref(nhl->divisions, division) == 0) {
        nhl_conference_unget(nhl, division->conference);
        delete_division(division);
    }
}
