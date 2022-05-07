#ifndef NHL_GAME_H_
#define NHL_GAME_H_

#include "player.h"
#include "team.h"
#include "utils.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Status of a hockey game. */
typedef struct NhlGameStatus {
    /* Unique game status code (e.g., "3"). */
    char *code;
    /* Abstract description (e.g., "Live"). */
    char *abstract_state;
    /* Detailed description (e.g., "In Progress"). */
    char *detailed_state;
} NhlGameStatus;

/* Get single game status.
 * The returned pointer must be dereferenced with nhl_game_status_unget(). */
NhlStatus nhl_game_status_get(Nhl *nhl, const char *game_status_code, NhlQueryLevel level,
                              NhlGameStatus **game_status);

/* Dereference the game status acquired by nhl_game_status_get(). */
void nhl_game_status_unget(Nhl *nhl, NhlGameStatus *game_status);


/* Type of a hockey game. */
typedef struct NhlGameType {
    /* Unique game type code (e.g., "R"). */
    char *code;
    /* Game type description (e.g., "Regular season"). */
    char *description;
    /* True for postseason type, false for other types. */
    int postseason;
} NhlGameType;

/* Get single game type. The returned pointer must be dereferenced with nhl_game_type_unget(). */
NhlStatus nhl_game_type_get(Nhl *nhl, const char *game_type_code, NhlQueryLevel level,
                            NhlGameType **game_type);

/* Dereference the game type acquired by nhl_game_type_get(). */
void nhl_game_type_unget(Nhl *nhl, NhlGameType *game_type);


/* Time in period for a goal. */
typedef struct NhlGoalTime {
    /* Period number, starting from 1. */
    int period;
    /* Period type (e.g., "REGULAR"). */
    char *period_type;
    /* Period ordinal number (e.g., "3rd"). */
    char *period_ordinal;
    /* Game time elapsed since period start. */
    NhlTime time;
    /* Game time remaining in the period. */
    NhlTime time_remaining;
} NhlGoalTime;

/* Strength of the scoring team. */
typedef struct NhlGoalStrength {
    /* Strength code (e.g., "PPG"). */
    char *code;
    /* Strength name (e.g., "Power Play"). */
    char *name;
} NhlGoalStrength;

/* Single scoring event. */
typedef struct NhlGoal {
    /* Game time of the goal. */
    NhlGoalTime *time;
    /* Scoring team. */
    NhlTeam *scoring_team;
    /* Score of the away team after the goal. */
    int away_score;
    /* Score of the home team after the goal. */
    int home_score;

    /* Scoring player. */
    NhlPlayer *scorer;
    /* Total number of goals for the scoring player in season, including this goal. */
    int scorer_season_total;
    /* Player with primary assist, or NULL if no assist exist. */
    NhlPlayer *assist1;
    /* Total number of assists for the primary assist in season, after this goal. */
    int assist1_season_total;
    /* Player with secondary assist, or NULL if no secondary assist exist. */
    NhlPlayer *assist2;
    /* Total number of assists for the secondary assist in season, after this goal. */
    int assist2_season_total;
    /* Opposing goalie, or NULL if no goalie exist. */
    NhlPlayer *goalie;

    /* Type of the goal (e.g., "Wrist Shot"). */
    char *type;
    /* Strength of the scoring team. */
    NhlGoalStrength *strength;
    /* True if the goal is a game-winning goal.
     * TODO: Check how this works for games that are in progress. */
    int game_winning_goal;
    /* True if the goal was scored in an empty net. */
    int empty_net;
} NhlGoal;


/* Game stats for a single period. */
typedef struct NhlGamePeriod {
    /* Number of the period, starting from 1. */
    int num;

    /* Number of goals scored by the away team in the period. */
    int away_goals;
    /* Shots on goal by the away team in the period. */
    int away_shots;

    /* Number of goals scored by the home team in the period. */
    int home_goals;
    /* Shots on goal by the home team in the period. */
    int home_shots;

    /* Ordinal number of the period (e.g., "3rd"). */
    char *ordinal_num;
    /* Type of the period (e.g., "REGULAR"). */
    char *period_type;
    /* Start time of the period in UTC, or empty string if the period has not started. */
    NhlDateTime start_time;
    /* End time of the period in UTC, or empty string if the period has not ended. */
    NhlDateTime end_time;
} NhlGamePeriod;

/* Stats for a shootout. */
typedef struct NhlGameShootout {
    /* Goals scored by the away team. */
    int away_score;
    /* Attempts by the away team. */
    int away_attempts;
    /* Goals scored by the home team. */
    int home_score;
    /* Attempts by the home team. */
    int home_attempts;
    /* Start time of the shootout in UTC. */
    NhlDateTime start_time;
} NhlGameShootout;

/* Additional details about the game. */
typedef struct NhlGameDetails {
    /* Number of the current (or last) period, starting from 1. */
    int current_period_number;
    /* Name of the current (or last) period (e.g., "1st" or "OT"). */
    char *current_period_name;
    /* Time remaining in the current period. */
    NhlTime current_period_remaining;

    /* Shots on goal by the away team. */
    int away_shots;
    /* Nonzero if the away team has power play. */
    int away_power_play;
    /* Nonzero if the away team has pulled the goalie. */
    int away_goalie_pulled;
    /* Number of skaters on the ice for the away team. */
    int away_num_skaters;

    /* Shots on goal by the home team. */
    int home_shots;
    /* Nonzero if the home team has power play. */
    int home_power_play;
    /* Nonzero if the home team has pulled the goalie. */
    int home_goalie_pulled;
    /* Number of skaters on the ice for the home team. */
    int home_num_skaters;

    /* Nonzero if a power play situation is on. */
    int powerplay;
    /* Power play strength (e.g., "5-on-4"). See also `away_power_play` and `home_power_play`. */
    char *power_play_strength;
    /* Seconds elapsed in power play situation. */
    int powerplay_time_secs;
    /* Seconds remaining in power play situation. */
    int powerplay_time_remaining_secs;

    /* Nonzero if an intermission is on. */
    int intermission;
    /* Seconds elapsed in the intermission. */
    int intermission_time_secs;
    /* Seconds remaining in the intermission */
    int intermission_time_remaining_secs;

    /* Number of periods for which stats exist. */
    int num_periods;
    /* Array of period stats for each available period. */
    NhlGamePeriod *periods;

    /* Shootout information, or NULL if there has not been a shootout. */
    NhlGameShootout *shootout;
} NhlGameDetails;


/* Season record for one team. */
typedef struct NhlTeamRecord {
    /* Total number of games played. */
    int games_played;
    /* Games won. */
    int wins;
    /* Games lost in regulation. */
    int losses;
    /* Games lots in overtime. */
    int overtime_losses;
} NhlTeamRecord;


/* Game information. */
typedef struct NhlGame {
    /* Unique game ID. */
    int unique_id;
    /* Away team. */
    NhlTeam *away;
    /* Home team. */
    NhlTeam *home;

    /* Season (e.g., "20212022"). */
    char *season;
    /* Game type. */
    NhlGameType *type;
    /* Game date in North American schedule. */
    NhlDate date;
    /* Start date and time in UTC. */
    NhlDateTime start_time;

    /* Status of the game. */
    NhlGameStatus *status;
    /* NUmber of goals scored by away team. */
    int away_score;
    /* NUmber of goals scored by home team. */
    int home_score;

    /* Length of the `goals` array. */
    int num_goals;
    /* Goal information in an array. */
    NhlGoal *goals;

    /* Season record for the away team. */
    NhlTeamRecord *away_record;
    /* Season record for the home team. */
    NhlTeamRecord *home_record;

    /* Additional details about the game and periods. */
    NhlGameDetails *details;
} NhlGame;

/* Get single NHL game. The returned pointer must be dereferenced with nhl_game_unget(). */
NhlStatus nhl_game_get(Nhl *nhl, int game_id, NhlQueryLevel level, NhlGame **game);

/* Dereference the game acquired by nhl_game_get(). */
void nhl_game_unget(Nhl *nhl, NhlGame *game);


/* Game schedule for a single date. */
typedef struct NhlSchedule {
    /* Date in North America. */
    NhlDate date;
    /* Length of `*games` */
    int num_games;
    /* Pointer array of scheduled games. */
    NhlGame **games;
} NhlSchedule;

/* Get schedule for a single day.
 * The returned pointer must be dereferenced with nhl_schedule_unget(). */
NhlStatus nhl_schedule_get(Nhl *nhl, const NhlDate *date, NhlQueryLevel level,
                           NhlSchedule **schedule);

/* Dereference the schedule acquired by nhl_schedule_get(). */
void nhl_schedule_unget(Nhl *nhl, NhlSchedule *schedule);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NHL_GAME_H_ */
