#ifndef NHL_CACHE_H_
#define NHL_CACHE_H_

#include "handle.h"

/* Current time from SQLite. */
const char *nhl_cache_current_time(Nhl *nhl);

/* Age of the given timestamp in seconds. Negative value indicates an error. */
int nhl_cache_timestamp_age(Nhl *nhl, const char *timestamp);


/* Common metadata struct. */
typedef struct NhlCacheMeta {
    char *source;    /* Original source (URL) of the data */
    char *timestamp; /* Time when the data was read */
    int invalid;     /* Nonzero if an inconsistency has been detected */
} NhlCacheMeta;


/* The structs correspond to tables in a cache database, and the members
 * correspond to columns in the tables. Furthermore, the member names
 * roughly correspond to item names in the raw JSON data. The foreign keys
 * are only "pseudo", because no relationships are enforced by the database.
 * This is because rows (or even tables) are generally obtained in a delayed
 * fashion whenever they are needed.
 */


/* Top-level game schedule. NHL_CONTENT_SCHEDULE */
typedef struct NhlCacheSchedule {
    NhlCacheMeta *meta;
    char *date;         /* Date in the U.S. (YYYY-MM-DD) */
    int totalGames;     /* Number of games */
} NhlCacheSchedule;

NhlStatus nhl_cache_schedule_put(Nhl *nhl, const NhlCacheSchedule *schedule);
NhlCacheSchedule *nhl_cache_schedule_get(Nhl *nhl, const char *schedule_date);
void nhl_cache_schedule_free(NhlCacheSchedule *schedule);


/* Singe game inside a schedule. NHL_CONTENT_SCHEDULE */
typedef struct NhlCacheGame {
    NhlCacheMeta *meta;

    int gamePk;           /* Unique identifier (primary key) */
    char *date;           /* Date in the U.S. (YYYY-MM-DD) */
    char *gameType;       /* Game type (pseudo foreign key) */
    char *season;         /* Four-digit season string */
    char *gameDate;       /* Start time in ISO format */
    char *statusCode;     /* Game status (pseudo foreign key) */

    int awayTeam;         /* Away team (pseudo foreign key) */
    int awayScore;        /* Current score, i.e., number of goals */
    int awayWins;         /* Wins in league records */
    int awayLosses;       /* Losses in league records */
    int awayOt;           /* Overtime losses in league records */
    char *awayRecordType; /* Type of league records */

    int homeTeam;         /* Home team (pseudo foreign key) */
    int homeScore;        /* Current score, i.e., number of goals */
    int homeWins;         /* Wins in league records */
    int homeLosses;       /* Losses in league records */
    int homeOt;           /* Overtime losses in league records */
    char *homeRecordType; /* Type of league records */
} NhlCacheGame;

NhlStatus nhl_cache_game_put(Nhl *nhl, const NhlCacheGame *game);
/* Returns an array of primary keys (gamePk). Release with free(). */
int *nhl_cache_games_find(Nhl *nhl, const char *date, int *num_games);
NhlCacheGame *nhl_cache_game_get(Nhl *nhl, int game_id);
void nhl_cache_game_free(NhlCacheGame *game);


/* Game type. NHL_CONTENT_GAME_TYPES */
typedef struct NhlCacheGameType {
    NhlCacheMeta *meta;
    char *id;          /* Unique identifier (primary key) */
    char *description; /* Game type description*/
    int postseason;    /* Nonzero for postseason games */
} NhlCacheGameType;

NhlStatus nhl_cache_game_type_put(Nhl *nhl, const NhlCacheGameType *game_type);
NhlCacheGameType *nhl_cache_game_type_get(Nhl *nhl, const char *game_type_id);
void nhl_cache_game_type_free(NhlCacheGameType *game_type);


/* Game status. NHL_CONTENT_GAME_STATUSES  */
typedef struct NhlCacheGameStatus {
    NhlCacheMeta *meta;
    char *code;              /* Status code (primary key) */
    char *abstractGameState; /* Short description */
    char *detailedState;     /* Longer description */
    int startTimeTBD;        /* Nonzero if start time is to-be-determined */
} NhlCacheGameStatus;

NhlStatus nhl_cache_game_status_put(Nhl *nhl, const NhlCacheGameStatus *game_status);
NhlCacheGameStatus *nhl_cache_game_status_get(Nhl *nhl, const char *game_status_code);
void nhl_cache_game_status_free(NhlCacheGameStatus *game_status);


/* Linescore for a game. NHL_CONTENT_SCHEDULE */
typedef struct NhlCacheLinescore {
    NhlCacheMeta *meta;
    int game;                         /* Unique ID of the game (primary key) */

    int currentPeriod;                /* Current (or last) period number */
    char *currentPeriodOrdinal;       /* Current period string (e.g., 1st) */
    char *currentPeriodTimeRemaining; /* Time remaining in period */

    /* Shootout scores */
    int awayShootoutScores;
    int awayShootoutAttempts;
    int homeShootoutScores;
    int homeShootoutAttempts;
    char *shootoutStartTime;          /* (optional) ISO start time of shootout */

    /* Booleans: Goalie pullend and power play */
    int awayShotsOnGoal;
    int awayGoaliePulled;
    int awayNumSkaters;
    int awayPowerPlay;

    int homeShotsOnGoal;
    int homeGoaliePulled;
    int homeNumSkaters;
    int homePowerPlay;

    char *powerPlayStrength;
    int hasShootout;                  /* Boolean */

    int intermissionTimeRemaining;
    int intermissionTimeElapsed;
    int intermission;                 /* Boolean */

    int powerPlaySituationRemaining;  /* Seconds */
    int powerPlaySituationElapsed;    /* Seconds */
    int powerPlayInSituation;         /* Boolean */
} NhlCacheLinescore;

NhlStatus nhl_cache_linescore_put(Nhl *nhl, const NhlCacheLinescore *linescore);
NhlCacheLinescore *nhl_cache_linescore_get(Nhl *nhl, int game_id);
void nhl_cache_linescore_free(NhlCacheLinescore *linescore);


/* Single period information. NHL_CONTENT_SCHEDULE */
typedef struct NhlCachePeriod {
    NhlCacheMeta *meta;
    int game;            /* Primary key (gamePk) of the underlying game */
    int periodIndex;     /* Period index (starting from zero) */

    char *periodType;
    char *startTime;     /* ISO time */
    char *endTime;       /* ISO time */
    int num;             /* Period number (starting from one) */
    char *ordinalNum;    /* Period name */

    int awayGoals;
    int awayShotsOnGoal;
    char *awayRinkSide;

    int homeGoals;
    int homeShotsOnGoal;
    char *homeRinkSide;
} NhlCachePeriod;

NhlStatus nhl_cache_periods_reset(Nhl *nhl, int game_id);
NhlStatus nhl_cache_period_put(Nhl *nhl, const NhlCachePeriod *period);
NhlCachePeriod *nhl_cache_periods_get(Nhl *nhl, int game_id, int *num_periods);
void nhl_cache_periods_free(NhlCachePeriod *periods, int num_periods);


/* Single goal. NHL_CONTENT_SCHEDULE */
typedef struct NhlCacheGoal {
    NhlCacheMeta *meta;
    int game;       /* Primary key (gamePk) of the underlying game */
    int goalNumber; /* Number starting from zero */

    /* full names also? */
    int scorer;
    int scorerSeasonTotal;
    int assist1;
    int assist1SeasonTotal;
    int assist2;
    int assist2SeasonTotal;
    int goalie;

    char *secondaryType;
    char *strengthCode;
    char *strengthName;
    int gameWinningGoal; /* Boolean */
    int emptyNet;        /* Boolean */

    int period;
    char *periodType;
    char *ordinalNum;
    char *periodTime;
    char *periodTimeRemaining;
    char *dateTime;

    int goalsAway;
    int goalsHome;
    int team;      /* Scoring team */
} NhlCacheGoal;

NhlStatus nhl_cache_goals_reset(Nhl *nhl, int game_id);
NhlStatus nhl_cache_goal_put(Nhl *nhl, const NhlCacheGoal *goal);
NhlCacheGoal *nhl_cache_goals_get(Nhl *nhl, int game_id, int *num_goals);
void nhl_cache_goals_free(NhlCacheGoal *goals, int num_goals);


/* Conference. NHL_CONTENT_CONFERENCES */
typedef struct NhlCacheConference {
    NhlCacheMeta *meta;
    int id;             /* Unique identifier (primary key) */
    char *name;         /* Full name of the conference */
    char *abbreviation; /* Official abbreviation */
    char *shortName;    /* Shortened name */
    int active;         /* Nonzero if conference exists today */
} NhlCacheConference;

NhlStatus nhl_cache_conference_put(Nhl *nhl, const NhlCacheConference *conference);
NhlCacheConference *nhl_cache_conference_get(Nhl *nhl, int conference_id);
void nhl_cache_conference_free(NhlCacheConference *conference);


/* Division. NHL_CONTENT_DIVISIONS */
typedef struct NhlCacheDivision {
    NhlCacheMeta *meta;
    int id;              /* Unique identifier (primary key) */
    char *name;          /* Full name of the division */
    char *nameShort;     /* Shortened name */
    char *abbreviation;  /* Official abbreviation */
    int conference;      /* Enclosing conference (pseudo foreign key) */
    int active;          /* Nonzero if division exists today */
} NhlCacheDivision;

NhlStatus nhl_cache_division_put(Nhl *nhl, const NhlCacheDivision *division);
NhlCacheDivision *nhl_cache_division_get(Nhl *nhl, int division_id);
void nhl_cache_division_free(NhlCacheDivision *division);


/* Player bio. NHL_CONTENT_PEOPLE */
typedef struct NhlCachePlayer {
    NhlCacheMeta *meta;
    int id;
    char *fullName;
    char *firstName;
    char *lastName;
    char *primaryNumber;
    char *birthDate;
    char *birthCity;
    char *birthStateProvince;
    char *birthCountry;
    char *nationality;
    char *height;
    int weight;
    int active;
    int alternateCaptain;
    int captain;
    int rookie;
    char *shootsCatches;
    char *rosterStatus;
    int currentTeam;
    char *primaryPosition;
} NhlCachePlayer;

NhlStatus nhl_cache_player_put(Nhl *nhl, const NhlCachePlayer *player);
NhlCachePlayer *nhl_cache_player_get(Nhl *nhl, int player_id);
void nhl_cache_player_free(NhlCachePlayer *player);


/* Player position. NHL_CONTENT_POSITIONS */
typedef struct NhlCachePosition {
    NhlCacheMeta *meta;
    char *abbrev;   /* Abbreviated position name */
    char *code;     /* Unique position code (primary key) */
    char *fullName; /* Full position name */
    char *type;     /* Position type */
} NhlCachePosition;

NhlStatus nhl_cache_position_put(Nhl *nhl, const NhlCachePosition *position);
NhlCachePosition *nhl_cache_position_get(Nhl *nhl, const char *position_code);
void nhl_cache_position_free(NhlCachePosition *position);


/* Player roster status. NHL_CONTENT_ROSTER_STATUSES */
typedef struct NhlCacheRosterStatus {
    NhlCacheMeta *meta;
    char *code;        /* Unique roster status code (primary key) */
    char *description; /* Detailed roster status */
} NhlCacheRosterStatus;

NhlStatus nhl_cache_roster_status_put(Nhl *nhl, const NhlCacheRosterStatus *roster_status);
NhlCacheRosterStatus *nhl_cache_roster_status_get(Nhl *nhl, const char *roster_status_code);
void nhl_cache_roster_status_free(NhlCacheRosterStatus *roster_status);


/* Single team. NHL_CONTENT_TEAMS */
typedef struct NhlCacheTeam {
    NhlCacheMeta *meta;
    int id;
    char *name;
    char *abbreviation;
    char *teamName;
    char *locationName;
    char *firstYearOfPlay;
    int division;
    int conference;
    int franchise;
    char *shortName;
    char *officialSiteUrl;
    int active;
} NhlCacheTeam;

NhlStatus nhl_cache_team_put(Nhl *nhl, const NhlCacheTeam *team);
NhlCacheTeam *nhl_cache_team_get(Nhl *nhl, int team_id);
void nhl_cache_team_free(NhlCacheTeam *team);


/* Single franchise.  */
typedef struct NhlCacheFranchise {
    NhlCacheMeta *meta;
    int franchiseId;
    int firstSeasonId;
    int lastSeasonId; /* optional */
    int mostRecentTeamId;
    char *teamName;
    char *locationName;
} NhlCacheFranchise;

NhlStatus nhl_cache_franchise_put(Nhl *nhl, const NhlCacheFranchise *franchise);
NhlCacheFranchise *nhl_cache_franchise_get(Nhl *nhl, int franchise_id);
void nhl_cache_franchise_free(NhlCacheFranchise *franchise);


/* Venue. NOT USED */
typedef struct NhlCacheVenue {
    int id;           /* Identifier (does not always exist) */
    char *name;       /* Full name of the arena (primary key) */
    char *city;       /* City where the venue is located */
    char *link;       /* Website URL */
    char *timeZone;   /* Time zone as TZ database name */
    char *appEnabled;
} NhlCacheVenue;


#endif /* NHL_CACHE_H_ */
