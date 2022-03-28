#ifndef NHL_TEAM_H_
#define NHL_TEAM_H_

#include "core.h"
#include "league.h"

#ifdef __cplusplus
extern "C" {
#endif


/*
typedef struct NhlTeamVenue {
    char *name;
    char *city;
    char *time_zone;
    char *time_zone_name;
} NhlTeamVenue;
*/


/* Forward declaration. */
typedef struct NhlTeam NhlTeam;

/* Franchise information. */
typedef struct NhlFranchise {
    /* Unique franchise ID. */
    int unique_id;
    /* First season (e.g., 19171918 for season 1917-1918) */
    int first_season;
    /* Last season for franchises that are not playing anymore. Formatted as `first_season`. */
    int last_season;
    /* Most recent team of the franchise. */
    NhlTeam *most_recent_team;
} NhlFranchise;

/* Get franchise. The returned pointer must be dereferenced with nhl_franchise_unget(). */
NhlStatus nhl_franchise_get(Nhl *nhl, int franchise_id, NhlQueryLevel level,
                            NhlFranchise **franchise);

/* Dereference the franchise acquired by nhl_franchise_get(). */
void nhl_franchise_unget(Nhl *nhl, NhlFranchise *franchise);


/* Team information. */
struct NhlTeam {
    /* Unique team ID. */
    int unique_id;

    /* Full name of the team (e.g., "New York Islanders"). */
    char *name;
    /* Location part of the name (e.g., "New York"). */
    char *location_name;
    /* Non-location part of the name (e.g., "Islanders"). */
    char *team_name;
    /* Location or other uniquely defining name (e.g., "NY Islanders"). */
    char *short_name;
    /* Official abbreviation (e.g., "NYI"). */
    char *abbreviation;

    /* Franchise of the team. */
    NhlFranchise *franchise;
    /* NHL division of the team. */
    NhlDivision *division;
    /* NHL conference of the team. */
    NhlConference *conference;

    /* Home arena. */
    /* NhlTeamVenue *venue; */

    /* Address of the official website. */
    char *official_site_url;
    /* Year the team first played in NHL. */
    int first_year_of_play;
    /* Nonzero if the team is currently playing in NHL. */
    int active;
};

/* Get team. The returned pointer must be dereferenced with nhl_team_unget(). */
NhlStatus nhl_team_get(Nhl *nhl, int team_id, NhlQueryLevel level, NhlTeam **team);

/*  Dereference the franchise acquired by nhl_team_get(). */
void nhl_team_unget(Nhl *nhl, NhlTeam *team);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NHL_TEAM_H_ */
