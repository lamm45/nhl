#ifndef NHL_LEAGUE_H_
#define NHL_LEAGUE_H_

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif


/* NHL conference. */
typedef struct NhlConference {
    int unique_id;
    char *name;
    char *name_short;
    char *abbreviation;
    int active;
} NhlConference;

/* Get conference. The returned pointer must be derefenced with nhl_conference_unget(). */
NhlStatus nhl_conference_get(Nhl *nhl, int conference_id, NhlQueryLevel level,
                             NhlConference **conference);

/* Dereference resources acquired with nhl_conference_get(). */
void nhl_conference_unget(Nhl *nhl, NhlConference *conference);


/* Nhl Division. */
typedef struct NhlDivision {
    int unique_id;
    char *name;
    char *name_short;
    char *abbreviation;
    NhlConference *conference;
    int active;
} NhlDivision;

/* Get division. The returned pointer must be derefenced with nhl_division_unget(). */
NhlStatus nhl_division_get(Nhl *nhl, int division_id, NhlQueryLevel level,
                           NhlDivision **division);

/* Dereference resources acquired with nhl_division_get(). */
void nhl_division_unget(Nhl *nhl, NhlDivision *division);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NHL_LEAGUE_H_ */
