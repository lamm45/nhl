#ifndef NHL_PLAYER_H_
#define NHL_PLAYER_H_

#include "core.h"
#include "team.h"
#include "utils.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Player position on the ice. */
typedef struct NhlPlayerPosition {
    /* Unique position code (e.g., "R"). */
    char *code;
    /* Full name of the position (e.g., "Right Wing"). */
    char *name;
    /* Abbreviated position name (e.g., "RW"). */
    char *abbreviation;
    /* Type or category of the position (e.g., Forward). */
    char *type;
} NhlPlayerPosition;

/* Get single on-ice position.
 * The returned pointer must be dereferenced with nhl_player_position_unget(). */
NhlStatus nhl_player_position_get(Nhl *nhl, const char *position_code, NhlQueryLevel level,
                                  NhlPlayerPosition **position);

/* Deference the position acquired by nhl_player_position_get(). */
void nhl_player_position_unget(Nhl *nhl, NhlPlayerPosition *position);


/* Roster status of a player. */
typedef struct NhlPlayerRosterStatus {
    /* Unique code of the roster status. */
    char *code;
    /* Full description of the roster status. */
    char *description;
} NhlPlayerRosterStatus;

/* Get single roster status.
 * The returned pointer must be dereferenced with nhl_player_roster_status_unget(). */
NhlStatus nhl_player_roster_status_get(Nhl *nhl, const char *roster_code, NhlQueryLevel level,
                                       NhlPlayerRosterStatus **roster_status);

/* Deference the roster status acquired by nhl_player_roster_status_get(). */
void nhl_player_roster_status_unget(Nhl *nhl, NhlPlayerRosterStatus *roster_status);


/* Player information. */
typedef struct NhlPlayer {
    /* Unique player ID. */
    int unique_id;

    /* First name of the player. */
    char *first_name;
    /* Last name of the player. */
    char *last_name;
    /* Full name of the player. */
    char *full_name;

    /* Date of birth. */
    NhlDate birth_date;
    /* Birth city. */
    char *birth_city;
    /* Birth state or province when applicable (mainly North American players).
     * Otherwise an empty string. */
    char *birth_state_province;
    /* Country of birth. */
    char *birth_country;
    /* Current nationality. */
    char *nationality;

    /* Height in feet and inches. */
    NhlHeight height;
    /* Weight in pounds. */
    int weight_pounds;
    /* Shooting side for skaters and catching side for goalies. */
    char *shoots_catches;

    /* True for active players, false for former players. */
    int active;
    /* Current NHL team. */
    NhlTeam *current_team;
    /* Current roster status. */
    NhlPlayerRosterStatus *roster_status;
    /* Primary playing position. */
    NhlPlayerPosition *primary_position;
    /* Primary jersey number. */
    int primary_number;
    /* True if the player is a captain of his team. */
    int captain;
    /* True if the player is an alternate captain of his team. */
    int alternate_captain;
    /* True if the player is a rookie. */
    int rookie;
} NhlPlayer;

/* Get single player. The returned pointer must be dereferenced with nhl_player_unget(). */
NhlStatus nhl_player_get(Nhl *nhl, int player_id, NhlQueryLevel level, NhlPlayer **player);

/* Deference player acquired with nhl_player_get(). */
void nhl_player_unget(Nhl *nhl, NhlPlayer *player);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NHL_PLAYER_H_ */
