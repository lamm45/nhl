#include <nhl/team.h>

#include <stdio.h>
#include <stdlib.h>

#include <nhl/update.h>
#include "cache.h"
#include "dict.h"
#include "get.h"
#include "handle.h"
#include "mem.h"
#include "urls.h"


/* TODO: rename */
static const int team_recursion = NHL_QUERY_FULL + 1;
static const int franchise_recursion = team_recursion << 1;

static NhlTeam *create_team(const NhlCacheTeam *cache_team) {
    NhlTeam *team = malloc(sizeof(NhlTeam));
    team->unique_id = cache_team->id;
    team->name = nhl_copy_string(cache_team->name);
    team->location_name = nhl_copy_string(cache_team->locationName);
    team->team_name = nhl_copy_string(cache_team->teamName);
    team->short_name = nhl_copy_string(cache_team->shortName);
    team->abbreviation = nhl_copy_string(cache_team->abbreviation);
    team->franchise = NULL;
    /* team->venue = NULL; */
    team->division = NULL;
    team->conference = NULL;
    team->official_site_url = nhl_copy_string(cache_team->officialSiteUrl);
    sscanf(cache_team->firstYearOfPlay, "%d", &team->first_year_of_play);
    team->active = cache_team->active;
    return team;
}

static void delete_team(NhlTeam *team) {
    if (team != NULL) {
        free(team->name);
        free(team->location_name);
        free(team->team_name);
        free(team->short_name);
        free(team->abbreviation);
        free(team->official_site_url);
        free(team);
    }
}

static NhlStatus get_cache_team_cb(Nhl *nhl, int update, void **dest, void *userp) {
    NhlStatus status = 0;
    NhlCacheTeam *cache_team = NULL;
    int *team_id = userp;

    if (update) {
        status |= nhl_update_from_url(nhl, NHL_URL_TEAMS, NHL_CONTENT_TEAMS);
    }

    cache_team = nhl_cache_team_get(nhl, *team_id);
    if (cache_team == NULL) {
        if (update) { /* Try alternate URL */
            char url[sizeof(NHL_URL_TEAMS) + 1 + NHL_INTSTR_LEN + 1];
            sprintf(url, "%s/%d", NHL_URL_TEAMS, *team_id);
            status |= nhl_update_from_url(nhl, url, NHL_CONTENT_TEAMS);
            cache_team = nhl_cache_team_get(nhl, *team_id);
        }
        if (cache_team == NULL) {
            return status | NHL_CACHE_READ_NOT_FOUND;
        }
    }

    if (*dest != NULL) {
        nhl_cache_team_free(*dest);
    }
    *dest = cache_team;

    return status | NHL_CACHE_READ_OK;
}

NhlStatus nhl_team_get(Nhl *nhl, int team_id, NhlQueryLevel level, NhlTeam **team) {
    int start = nhl_prepare(nhl);
    NhlCacheTeam *cache_team = NULL;
    NhlStatus status = nhl_get(nhl, nhl->teams, &team_id, nhl->params->team_max_age,
                               get_cache_team_cb, &team_id, (void **) team, (void **) &cache_team);

    if (cache_team != NULL) {
        *team = create_team(cache_team);
        nhl_dict_insert(nhl->teams, &team_id, *team, cache_team->meta->timestamp);
    }

    if (*team != NULL && level & NHL_QUERY_BASIC) {
        int franchise_id = 0;
        NhlFranchise *franchise_old = (*team)->franchise;
        NhlFranchise *franchise;

        int conference_id = 0;
        NhlConference *conference_old = (*team)->conference;
        NhlConference *conference;

        int division_id = 0;
        NhlDivision *division_old = (*team)->division;
        NhlDivision *division;

        if (franchise_old != NULL && conference_old != NULL && division_old != NULL) {
            franchise_id = franchise_old->unique_id;
            conference_id = conference_old->unique_id;
            division_id = division_old->unique_id;
        } else {
            if (cache_team == NULL) {
                cache_team = nhl_cache_team_get(nhl, team_id);
            }
            if (cache_team != NULL) {
                franchise_id = cache_team->franchise;
                conference_id = cache_team->conference;
                division_id = cache_team->division;
            }
        }

        if (!(level & franchise_recursion)) {
            status |= nhl_franchise_get(nhl, franchise_id, level | team_recursion, &franchise);
            (*team)->franchise = franchise;
            if (franchise != NULL) {
                (*team)->franchise->most_recent_team = *team;
            }
            nhl_franchise_unget(nhl, franchise_old);
        }

        status |= nhl_conference_get(nhl, conference_id, level, &conference);
        (*team)->conference = conference;
        nhl_conference_unget(nhl, conference_old);

        status |= nhl_division_get(nhl, division_id, level, &division);
        (*team)->division = division;
        nhl_division_unget(nhl, division_old);
    }

    nhl_cache_team_free(cache_team);
    nhl_finish(nhl, start);
    return status;
}

void nhl_team_unget(Nhl *nhl, NhlTeam *team) {
    if (team != NULL && nhl_dict_unref(nhl->teams, team) == 0) {
        nhl_franchise_unget(nhl, team->franchise);
        nhl_conference_unget(nhl, team->conference);
        nhl_division_unget(nhl, team->division);
        delete_team(team);
    }
}


static NhlFranchise *create_franchise(const NhlCacheFranchise *cache_franchise) {
    NhlFranchise *franchise = malloc(sizeof(NhlFranchise));
    franchise->unique_id = cache_franchise->franchiseId;
    franchise->first_season = cache_franchise->firstSeasonId;
    franchise->last_season = cache_franchise->lastSeasonId;
    franchise->most_recent_team = NULL;
    return franchise;
}

static void delete_franchise(NhlFranchise *franchise) {
    if (franchise != NULL) {
        free(franchise);
    }
}

static NhlStatus get_cache_franchise_cb(Nhl *nhl, int update, void **dest, void *userp) {
    NhlStatus status = 0;
    NhlCacheFranchise *cache_franchise = NULL;
    int *franchise_id = userp;

    if (update) {
        status |= nhl_update_from_url(nhl, NHL_URL_FRANCHISES, NHL_CONTENT_FRANCHISES);
    }

    cache_franchise = nhl_cache_franchise_get(nhl, *franchise_id);
    if (cache_franchise == NULL) {
        if (update) { /* Try alternate URL */
            char url[sizeof(NHL_URL_FRANCHISES) + 1 + NHL_INTSTR_LEN + 1];
            sprintf(url, "%s/%d", NHL_URL_FRANCHISES, *franchise_id);
            status |= nhl_update_from_url(nhl, url, NHL_CONTENT_FRANCHISES);
            cache_franchise = nhl_cache_franchise_get(nhl, *franchise_id);
        }
        return status | NHL_CACHE_READ_NOT_FOUND;
    }

    if (*dest != NULL) {
        nhl_cache_franchise_free(*dest);
    }
    *dest = cache_franchise;

    return status | NHL_CACHE_READ_OK;
}

NhlStatus nhl_franchise_get(Nhl *nhl, int franchise_id, NhlQueryLevel level, NhlFranchise **franchise) {
    int start = nhl_prepare(nhl);
    NhlCacheFranchise *cache_franchise = NULL;
    NhlStatus status = nhl_get(nhl, nhl->franchises, &franchise_id, nhl->params->team_max_age,
                               get_cache_franchise_cb, &franchise_id, (void **) franchise, (void **) &cache_franchise);

    if (cache_franchise != NULL) {
        *franchise = create_franchise(cache_franchise);
        nhl_dict_insert(nhl->franchises, &franchise_id, *franchise, cache_franchise->meta->timestamp);
    }

    if (*franchise != NULL && level & NHL_QUERY_BASIC && !(level & team_recursion)) {
        int team_id = 0;
        NhlTeam *team_old = (*franchise)->most_recent_team;
        NhlTeam *team;

        if (team_old != NULL) {
            team_id = team_old->unique_id;
        } else {
            if (cache_franchise == NULL) {
                cache_franchise = nhl_cache_franchise_get(nhl, franchise_id);
            }
            if (cache_franchise != NULL) {
                team_id = cache_franchise->mostRecentTeamId;
            }
        }

        status |= nhl_team_get(nhl, team_id, level | franchise_recursion, &team);
        (*franchise)->most_recent_team = team;
        if (team != NULL) {
            (*franchise)->most_recent_team->franchise = *franchise;
        }
        nhl_team_unget(nhl, team_old);
    }

    nhl_cache_franchise_free(cache_franchise);
    nhl_finish(nhl, start);
    return status;
}

void nhl_franchise_unget(Nhl *nhl, NhlFranchise *franchise) {
    if (franchise != NULL && nhl_dict_unref(nhl->franchises, franchise) == 0) {
        nhl_team_unget(nhl, franchise->most_recent_team);
        delete_franchise(franchise);
    }
}
