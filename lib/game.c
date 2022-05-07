#include <nhl/game.h>

#include <stdlib.h>
#include <string.h>

#include <nhl/player.h>
#include <nhl/team.h>
#include <nhl/update.h>
#include <nhl/utils.h>
#include "cache.h"
#include "dict.h"
#include "get.h"
#include "handle.h"
#include "mem.h"
#include "urls.h"


/* Convert cached schedule to schedule.
 * The returned pointer must be released with delete_schedule(). */
static NhlSchedule *create_schedule(const NhlCacheSchedule *cache_schedule) {
    NhlSchedule *schedule = malloc(sizeof(NhlSchedule));
    schedule->date = nhl_string_to_date(cache_schedule->date);
    schedule->num_games = cache_schedule->totalGames;
    schedule->games = NULL;
    return schedule;
}

/* Release resources acquired with create_schedule(). */
static void delete_schedule(NhlSchedule *schedule) {
    if (schedule != NULL) {
        /* Nothing special to do here. */
        free(schedule);
    }
}

/* Generate schedule URL based on date. The returned string must be released with free().
 * NOTE: Currently, both expands (linescore, scoringPlays) are activated.*/
static char *schedule_url(const NhlDate *date) {
    size_t base_len = strlen(NHL_URL_PREFIX_SCHEDULE);
    char *date_str = nhl_date_to_string(date);
    size_t date_len = strlen(date_str);
    char *url = malloc(base_len + date_len + 1);
    memcpy(url, NHL_URL_PREFIX_SCHEDULE, base_len);
    memcpy(url + base_len, date_str, date_len + 1);
    free(date_str);
    return url;
}

/* Callback for getting cached schedule. */
static NhlStatus get_cache_schedule_cb(Nhl *nhl, int update, void **dest, void *userp) {
    NhlStatus status = 0;
    NhlCacheSchedule *cache_schedule = NULL;
    NhlDate *date = userp;
    char *date_str = nhl_date_to_string(date);

    if (update) {
        char *url = schedule_url(date);
        status |= nhl_update_from_url(nhl, url, NHL_CONTENT_SCHEDULE);
        free(url);
    }

    cache_schedule = nhl_cache_schedule_get(nhl, date_str);
    if (cache_schedule == NULL) {
        free(date_str);
        return status | NHL_CACHE_READ_NOT_FOUND;
    }

    if (*dest != NULL) {
        nhl_cache_schedule_free(*dest);
    }
    *dest = cache_schedule;

    free(date_str);
    return status | NHL_CACHE_READ_OK;
}

/* Compare games based on start date. */
static int compare_games(const void *game1, const void *game2) {
    const NhlGame *g1 = *(const NhlGame **) game1;
    const NhlGame *g2 = *(const NhlGame **) game2;

    /* NULL is INF */
    if (g1 == NULL)
        return 1;
    if (g2 == NULL)
        return -1;

    return nhl_datetime_compare(&g1->start_time, &g2->start_time);
}

NhlStatus nhl_schedule_get(Nhl *nhl, const NhlDate *date, NhlQueryLevel level, NhlSchedule **schedule) {
    int start = nhl_prepare(nhl);
    NhlCacheSchedule *cache_schedule = NULL;
    NhlDate date_copy = *date;
    char *date_str = nhl_date_to_string(date);
    NhlStatus status = nhl_get(nhl, nhl->schedules, date_str, nhl->params->schedule_max_age,
        get_cache_schedule_cb, &date_copy, (void **) schedule, (void **) &cache_schedule);

    if (cache_schedule != NULL) {
        *schedule = create_schedule(cache_schedule);
        nhl_dict_insert(nhl->schedules, date_str, *schedule, cache_schedule->meta->timestamp);
    }

    if (*schedule != NULL && level & NHL_QUERY_BASIC) {
        int idx;
        int num_games = 0;
        int *game_ids = NULL;

        if ((*schedule)->games != NULL) {
            num_games = (*schedule)->num_games;
            game_ids = malloc(num_games * sizeof(int));
            for (idx = 0; idx != num_games; ++idx) {
                game_ids[idx] = (*schedule)->games[idx]->unique_id;
            }
        } else {
            game_ids = nhl_cache_games_find(nhl, date_str, &num_games);
            (*schedule)->games = calloc(num_games, sizeof(NhlGame *));
        }

        for (idx = 0; idx != num_games; ++idx) {
            NhlGame *game_old = (*schedule)->games[idx];
            NhlGame *game;
            status |= nhl_game_get(nhl, game_ids[idx], level, &game);
            (*schedule)->games[idx] = game;
            nhl_game_unget(nhl, game_old);
        }
        (*schedule)->num_games = num_games;

        qsort((*schedule)->games, num_games, sizeof(NhlGame *), compare_games);

        free(game_ids);
    }

    free(date_str);
    nhl_cache_schedule_free(cache_schedule);
    nhl_finish(nhl, start);
    return status;
}

void nhl_schedule_unget(Nhl *nhl, NhlSchedule *schedule) {
    if (schedule != NULL && nhl_dict_unref(nhl->schedules, schedule) == 0) {
        int idx;
        for (idx=0; idx != schedule->num_games; ++idx) {
            nhl_game_unget(nhl, schedule->games[idx]);
        }
        free(schedule->games);
        delete_schedule(schedule);
    }
}


/* Convert cached game status to game status.
 * The returned item must be released with delete_game_status(). */
static NhlGameStatus *create_game_status(const NhlCacheGameStatus *cache_game_status) {
    NhlGameStatus *game_status = malloc(sizeof(NhlGameStatus));
    game_status->code = nhl_copy_string(cache_game_status->code);
    game_status->abstract_state = nhl_copy_string(cache_game_status->abstractGameState);
    game_status->detailed_state = nhl_copy_string(cache_game_status->detailedState);
    return game_status;
}

/* Release resources acquired with create_game_status(). */
static void delete_game_status(NhlGameStatus *game_status) {
    if (game_status != NULL) {
        free(game_status->code);
        free(game_status->abstract_state);
        free(game_status->detailed_state);
        free(game_status);
    }
}

/* Callback for getting cached game status. */
static NhlStatus get_cache_game_status_cb(Nhl *nhl, int update, void **dest, void *userp) {
    NhlStatus status = 0;
    NhlCacheGameStatus *cache_game_status = NULL;
    char *code = userp;

    if (update) {
        status |= nhl_update_from_url(nhl, NHL_URL_GAME_STATUS, NHL_CONTENT_GAME_STATUSES);
    }

    cache_game_status = nhl_cache_game_status_get(nhl, code);
    if (cache_game_status == NULL) {
        return status | NHL_CACHE_READ_NOT_FOUND;
    }

    if (*dest != NULL) {
        nhl_cache_game_status_free(*dest);
    }
    *dest = cache_game_status;

    return status | NHL_CACHE_READ_OK;
}

NhlStatus nhl_game_status_get(Nhl *nhl, const char *game_status_code, NhlQueryLevel level, NhlGameStatus **game_status) {
    int start = nhl_prepare(nhl);
    NhlCacheGameStatus *cache_game_status = NULL;
    char *code = nhl_copy_string(game_status_code);
    NhlStatus status = nhl_get(nhl, nhl->game_statuses, code, nhl->params->meta_max_age,
                               get_cache_game_status_cb, code, (void **) game_status, (void **) &cache_game_status);
    free(code);

    if (cache_game_status != NULL) {
        *game_status = create_game_status(cache_game_status);
        nhl_dict_insert(nhl->game_statuses, game_status_code, *game_status, cache_game_status->meta->timestamp);
        nhl_cache_game_status_free(cache_game_status);
    }

    (void) level;
    nhl_finish(nhl, start);
    return status;
}

void nhl_game_status_unget(Nhl *nhl, NhlGameStatus *game_status) {
    if (game_status != NULL && nhl_dict_unref(nhl->game_statuses, game_status) == 0) {
        delete_game_status(game_status);
    }
}


/* Convert cached game type to game type. Release with delete_game_type(). */
static NhlGameType *create_game_type(const NhlCacheGameType *cache_game_type) {
    NhlGameType *game_type = malloc(sizeof(NhlGameType));
    game_type->code = nhl_copy_string(cache_game_type->id);
    game_type->description = nhl_copy_string(cache_game_type->description);
    game_type->postseason = cache_game_type->postseason;
    return game_type;
}

/* Release resources acquired with create_game_type(). */
static void delete_game_type(NhlGameType *game_type) {
    if (game_type != NULL) {
        free(game_type->code);
        free(game_type->description);
        free(game_type);
    }
}

/* Callback for getting cached game type. */
static NhlStatus get_cache_game_type_cb(Nhl *nhl, int update, void **dest, void *userp) {
    NhlStatus status = 0;
    NhlCacheGameType *cache_game_type = NULL;
    char *code = userp;

    if (update) {
        status |= nhl_update_from_url(nhl, NHL_URL_GAME_TYPES, NHL_CONTENT_GAME_TYPES);
    }

    cache_game_type = nhl_cache_game_type_get(nhl, code);
    if (cache_game_type == NULL) {
        return status | NHL_CACHE_READ_NOT_FOUND;
    }

    if (*dest != NULL) {
        nhl_cache_game_type_free(*dest);
    }
    *dest = cache_game_type;

    return status | NHL_CACHE_READ_OK;
}

NhlStatus nhl_game_type_get(Nhl *nhl, const char *game_type_code, NhlQueryLevel level, NhlGameType **game_type) {
    int start = nhl_prepare(nhl);
    NhlCacheGameType *cache_game_type = NULL;
    char *code = nhl_copy_string(game_type_code);
    NhlStatus status = nhl_get(nhl, nhl->game_types, code, nhl->params->meta_max_age,
        get_cache_game_type_cb, code, (void **) game_type, (void **) &cache_game_type);
    free(code);

    if (cache_game_type != NULL) {
        *game_type = create_game_type(cache_game_type);
        nhl_dict_insert(nhl->game_types, game_type_code, *game_type, cache_game_type->meta->timestamp);
        nhl_cache_game_type_free(cache_game_type);
    }

    (void) level;
    nhl_finish(nhl, start);
    return status;
}

void nhl_game_type_unget(Nhl *nhl, NhlGameType *game_type) {
    if (game_type != NULL && nhl_dict_unref(nhl->game_types, game_type) == 0) {
        delete_game_type(game_type);
    }
}


/* Create goal array from cached goal array. Release with delete_goals().*/
static NhlGoal *create_goals(const NhlCacheGoal *cache_goals, int num_goals) {
    if (num_goals > 0) {
        NhlGoal *goals = malloc(num_goals * sizeof(NhlGoal));
        int idx;
        for (idx = 0; idx != num_goals; ++idx) {
            NhlGoalTime *time;
            NhlGoalStrength *strength;

            time = malloc(sizeof(NhlGoalTime));
            time->period = cache_goals[idx].period;
            time->period_type = nhl_copy_string(cache_goals[idx].periodType);
            time->period_ordinal = nhl_copy_string(cache_goals[idx].ordinalNum);
            time->time = nhl_string_to_time(cache_goals[idx].periodTime);
            time->time_remaining = nhl_string_to_time(cache_goals[idx].periodTimeRemaining);
            goals[idx].time = time;

            goals[idx].scoring_team = NULL;
            goals[idx].away_score = cache_goals[idx].goalsAway;
            goals[idx].home_score = cache_goals[idx].goalsHome;
            goals[idx].scorer = NULL;
            goals[idx].scorer_season_total = cache_goals[idx].scorerSeasonTotal;
            goals[idx].assist1 = NULL;
            goals[idx].assist1_season_total = cache_goals[idx].assist1SeasonTotal;
            goals[idx].assist2 = NULL;
            goals[idx].assist2_season_total = cache_goals[idx].assist2SeasonTotal;
            goals[idx].goalie = NULL;
            goals[idx].type = nhl_copy_string(cache_goals[idx].secondaryType);

            strength = malloc(sizeof(NhlGoalStrength));
            strength->code = nhl_copy_string(cache_goals[idx].strengthCode);
            strength->name = nhl_copy_string(cache_goals[idx].strengthName);
            goals[idx].strength = strength;

            goals[idx].game_winning_goal = cache_goals[idx].gameWinningGoal;
            goals[idx].empty_net = cache_goals[idx].emptyNet;
        }
        return goals;
    }
    return NULL;
}

/* Release resources acquired by create_goals(). */
static void delete_goals(NhlGoal *goals, int num_goals) {
    if (goals != NULL) {
        int idx;
        for (idx = 0; idx != num_goals; ++idx) {
            free(goals[idx].time->period_type);
            free(goals[idx].time->period_ordinal);
            free(goals[idx].time);
            free(goals[idx].type);
            free(goals[idx].strength->code);
            free(goals[idx].strength->name);
            free(goals[idx].strength);
        }
        free(goals);
    }
}

/* Assign goal array and its number of elements to the corresponding output arguments.
 * This function NOT use shared pointer model. In addition, this function does not check whether
 * the cache database is up-to-date. Thus, this function should only be called internally from a
 * function that ensures that the cache is up-to-date.
 */
static NhlStatus nhl_goals_get(Nhl *nhl, int game_id, NhlQueryLevel level, NhlGoal **goals, int *num_goals) {
    NhlStatus status = 0;
    NhlCacheGoal *cache_goals = nhl_cache_goals_get(nhl, game_id, num_goals);
    *goals = create_goals(cache_goals, *num_goals);

    if (level & NHL_QUERY_BASIC) {
        int idx;
        for (idx = 0; idx != *num_goals; ++idx) {
            NhlTeam *team = NULL;
            status |= nhl_team_get(nhl, cache_goals[idx].team, level, &team);
            (*goals)[idx].scoring_team = team;
        }
    }

    if (level & NHL_QUERY_PLAYERS) {
        int idx;
        for (idx = 0; idx != *num_goals; ++idx) {
            NhlPlayer *scorer = NULL;
            NhlPlayer *assist1 = NULL;
            NhlPlayer *assist2 = NULL;
            NhlPlayer *goalie = NULL;

            /* Note: It is normal to have players (except perhaps the scorer) to be NULL. */
            if (cache_goals[idx].scorer)
                status |= nhl_player_get(nhl, cache_goals[idx].scorer, level, &scorer);
            if (cache_goals[idx].assist1)
                status |= nhl_player_get(nhl, cache_goals[idx].assist1, level, &assist1);
            if (cache_goals[idx].assist2)
                status |= nhl_player_get(nhl, cache_goals[idx].assist2, level, &assist2);
            if (cache_goals[idx].goalie)
                status |= nhl_player_get(nhl, cache_goals[idx].goalie, level, &goalie);

            (*goals)[idx].scorer = scorer;
            (*goals)[idx].assist1 = assist1;
            (*goals)[idx].assist2 = assist2;
            (*goals)[idx].goalie = goalie;
        }
    }

    nhl_cache_goals_free(cache_goals, *num_goals);
    return status;
}

/* Release resources acquired by nhl_goals_get. */
static void nhl_goals_unget(Nhl *nhl, NhlGoal *goals, int num_goals) {
    if (goals != NULL) {
        int idx;
        for (idx = 0; idx != num_goals; ++idx) {
            nhl_team_unget(nhl, goals[idx].scoring_team);
            nhl_player_unget(nhl, goals[idx].scorer);
            nhl_player_unget(nhl, goals[idx].assist1);
            nhl_player_unget(nhl, goals[idx].assist2);
            nhl_player_unget(nhl, goals[idx].goalie);
        }
        delete_goals(goals, num_goals);
    }
}


/* Create period array from cached period array. Release with delete_periods().*/
static NhlGamePeriod *create_periods(const NhlCachePeriod *cache_periods, int num_periods) {
    if (num_periods > 0) {
        NhlGamePeriod *periods = malloc(num_periods * sizeof(NhlGamePeriod));
        int idx;
        for (idx = 0; idx != num_periods; ++idx) {
            periods[idx].num = cache_periods[idx].num;
            periods[idx].away_goals = cache_periods[idx].awayGoals;
            periods[idx].away_shots = cache_periods[idx].awayShotsOnGoal;
            periods[idx].home_goals = cache_periods[idx].homeGoals;
            periods[idx].home_shots = cache_periods[idx].homeShotsOnGoal;
            periods[idx].ordinal_num = nhl_copy_string(cache_periods[idx].ordinalNum);
            periods[idx].period_type = nhl_copy_string(cache_periods[idx].periodType);
            periods[idx].start_time = nhl_string_to_datetime(cache_periods[idx].startTime);
            periods[idx].end_time = nhl_string_to_datetime(cache_periods[idx].endTime);
        }
        return periods;
    }
    return NULL;
}

/* Release resources acquired by create_periods(). */
static void delete_periods(NhlGamePeriod *periods, int num_periods) {
    if (periods != NULL) {
        int idx;
        for (idx = 0; idx != num_periods; ++idx) {
            free(periods[idx].ordinal_num);
            free(periods[idx].period_type);
        }
        free(periods);
    }
}

/* Assign period array and its number of elements to the corresponding output arguments.
 * This function does NOT use shared pointer model. In addition, this function does not check
 * whether the cache database is up-to-date. Thus, this function should only be called internally
 * from a function that ensures that the cache is up-to-date.
 */
static NhlStatus nhl_periods_get(Nhl *nhl, int game_id, NhlQueryLevel level, NhlGamePeriod **periods, int *num_periods) {
    NhlStatus status = 0;
    NhlCachePeriod *cache_periods = nhl_cache_periods_get(nhl, game_id, num_periods);
    *periods = create_periods(cache_periods, *num_periods);

    (void) level;
    nhl_cache_periods_free(cache_periods, *num_periods);
    return status;
}

/* Release resources acquired by nhl_periods_get. */
static void nhl_periods_unget(Nhl *nhl, NhlGamePeriod *periods, int num_periods) {
    (void) nhl;
    if (periods != NULL) {
        delete_periods(periods, num_periods);
    }
}


/* Create game details from cache linescore. Release with delete_details().*/
static NhlGameDetails *create_details(const NhlCacheLinescore *cache_details) {
    NhlGameDetails *details = malloc(sizeof(NhlGameDetails));
    details->current_period_number = cache_details->currentPeriod;
    details->current_period_name = nhl_copy_string(cache_details->currentPeriodOrdinal);
    details->current_period_remaining = nhl_string_to_time(cache_details->currentPeriodTimeRemaining);
    details->away_shots = cache_details->awayShotsOnGoal;
    details->away_power_play = cache_details->awayPowerPlay;
    details->away_goalie_pulled = cache_details->awayGoaliePulled;
    details->away_num_skaters = cache_details->awayNumSkaters;
    details->home_shots = cache_details->homeShotsOnGoal;
    details->home_power_play = cache_details->homePowerPlay;
    details->home_goalie_pulled = cache_details->homeGoaliePulled;
    details->home_num_skaters = cache_details->homeNumSkaters;
    details->powerplay = cache_details->powerPlayInSituation;
    details->power_play_strength = nhl_copy_string(cache_details->powerPlayStrength);
    details->powerplay_time_secs = cache_details->powerPlaySituationElapsed;
    details->powerplay_time_remaining_secs = cache_details->powerPlaySituationRemaining;
    details->intermission = cache_details->intermission;
    details->intermission_time_secs = cache_details->intermissionTimeElapsed;
    details->intermission_time_remaining_secs = cache_details->intermissionTimeRemaining;
    details->num_periods = cache_details->currentPeriod;
    details->periods = NULL;

    if (cache_details->hasShootout) {
        details->shootout = malloc(sizeof(NhlGameShootout));
        details->shootout->away_score = cache_details->awayShootoutScores;
        details->shootout->away_attempts = cache_details->awayShootoutAttempts;
        details->shootout->home_score = cache_details->homeShootoutScores;
        details->shootout->home_attempts = cache_details->homeShootoutAttempts;
        details->shootout->start_time = nhl_string_to_datetime(cache_details->shootoutStartTime);
    } else {
        details->shootout = NULL;
    }

    return details;
}

/* Release resources acquired by create_details(). */
static void delete_details(NhlGameDetails *details) {
    if (details != NULL) {
        free(details->current_period_name);
        free(details->power_play_strength);
        free(details->shootout);
        free(details);
    }
}

/* Assign game details a.k.a. linescore. This function does NOT use shared pointer model.
 * In addition, this function does not check whether the cache database is up-to-date.
 * Thus, this function should only be called internally from a function that ensures that
 * the cache is up-to-date.
 */
static NhlStatus nhl_game_details_get(Nhl *nhl, int game_id, NhlQueryLevel level, NhlGameDetails **details) {
    NhlStatus status = 0;
    NhlCacheLinescore *cache_linescore = nhl_cache_linescore_get(nhl, game_id);
    *details = create_details(cache_linescore);

    if (level & NHL_QUERY_BASIC) {
        status |= nhl_periods_get(nhl, game_id, level, &(*details)->periods, &(*details)->num_periods);
    }

    nhl_cache_linescore_free(cache_linescore);
    return status;
}

/* Release resources acquired by nhl_game_details_get. */
static void nhl_game_details_unget(Nhl *nhl, NhlGameDetails *details) {
    if (details != NULL) {
        nhl_periods_unget(nhl, details->periods, details->num_periods);
        delete_details(details);
    }
}


/* Convert cached game to game. Release with delete_game(). */
static NhlGame *create_game(const NhlCacheGame *cache_game) {
    NhlGame *game = malloc(sizeof(NhlGame));

    NhlTeamRecord *away_record;
    NhlTeamRecord *home_record;

    game->unique_id = cache_game->gamePk;
    game->away = NULL;
    game->home = NULL;
    game->season = nhl_copy_string(cache_game->season);
    game->type = NULL;
    game->date = nhl_string_to_date(cache_game->date);
    game->start_time = nhl_string_to_datetime(cache_game->gameDate);
    game->status = NULL;
    game->away_score = cache_game->awayScore;
    game->home_score = cache_game->homeScore;
    game->num_goals = -1;
    game->goals = NULL;

    away_record = malloc(sizeof(NhlTeamRecord));
    away_record->wins = cache_game->awayWins;
    away_record->losses = cache_game->awayLosses;
    away_record->overtime_losses = cache_game->awayOt;
    away_record->games_played = away_record->wins + away_record->losses + away_record->overtime_losses;
    game->away_record = away_record;

    home_record = malloc(sizeof(NhlTeamRecord));
    home_record->wins = cache_game->homeWins;
    home_record->losses = cache_game->homeLosses;
    home_record->overtime_losses = cache_game->homeOt;
    home_record->games_played = home_record->wins + home_record->losses + home_record->overtime_losses;
    game->home_record = home_record;

    game->details = NULL;
    return game;
}

/* Release resources acquired with create_game(). */
static void delete_game(NhlGame *game) {
    if (game != NULL) {
        free(game->season);
        free(game->away_record);
        free(game->home_record);
        free(game);
    }
}

/* Callback for getting cached game. */
static NhlStatus get_cache_game_cb(Nhl *nhl, int update, void **dest, void *userp) {
    NhlStatus status = 0;
    NhlCacheGame *cache_game = NULL;
    int *game_id = userp;

    if (update) {
        status |= nhl_update_from_url(nhl, NULL, 0);
    }

    cache_game = nhl_cache_game_get(nhl, *game_id);
    if (cache_game == NULL) {
        return status | NHL_CACHE_READ_NOT_FOUND;
    }

    if (*dest != NULL) {
        nhl_cache_game_free(*dest);
    }
    *dest = cache_game;

    return status | NHL_CACHE_READ_OK;
}

NhlStatus nhl_game_get(Nhl *nhl, int game_id, NhlQueryLevel level, NhlGame **game) {
    int start = nhl_prepare(nhl);
    NhlCacheGame *cache_game = NULL;
    NhlStatus status = nhl_get(nhl, nhl->games, &game_id, nhl->params->game_live_max_age,
                               get_cache_game_cb, &game_id, (void **) game, (void **) &cache_game);

    if (cache_game != NULL) {
        *game = create_game(cache_game);
        nhl_dict_insert(nhl->games, &game_id, *game, cache_game->meta->timestamp);
    }

    if (*game != NULL && level & NHL_QUERY_BASIC) {
        int away_id = 0;
        NhlTeam *away_old = (*game)->away;
        NhlTeam *away;

        int home_id = 0;
        NhlTeam *home_old = (*game)->home;
        NhlTeam *home;

        char *status_code = NULL;
        NhlGameStatus *game_status_old = (*game)->status;
        NhlGameStatus *game_status;

        char *type_code = NULL;
        NhlGameType *game_type_old = (*game)->type;
        NhlGameType *game_type;

        if (away_old && home_old && game_status_old && game_type_old) {
            away_id = away_old->unique_id;
            home_id = home_old->unique_id;
            status_code = nhl_copy_string(game_status_old->code);
            type_code = nhl_copy_string(game_type_old->code);
        } else {
            if (cache_game == NULL) {
                cache_game = nhl_cache_game_get(nhl, game_id);
            }
            if (cache_game != NULL) {
                away_id = cache_game->awayTeam;
                home_id = cache_game->homeTeam;
                status_code = nhl_copy_string(cache_game->statusCode);
                type_code = nhl_copy_string(cache_game->gameType);
            }
        }

        status |= nhl_team_get(nhl, away_id, level, &away);
        (*game)->away = away;
        nhl_team_unget(nhl, away_old);

        status |= nhl_team_get(nhl, home_id, level, &home);
        (*game)->home = home;
        nhl_team_unget(nhl, home_old);

        status |= nhl_game_status_get(nhl, status_code, level, &game_status);
        (*game)->status = game_status;
        nhl_game_status_unget(nhl, game_status_old);
        free(status_code);

        status |= nhl_game_type_get(nhl, type_code, level, &game_type);
        (*game)->type = game_type;
        nhl_game_type_unget(nhl, game_type_old);
        free(type_code);
    }

    if (*game != NULL && (*game)->details == NULL && level & NHL_QUERY_GAMEDETAILS) {
        status |= nhl_game_details_get(nhl, game_id, level, &(*game)->details);
    }

    if (*game != NULL && (*game)->goals == NULL && level & NHL_QUERY_GOALS) {
        status |= nhl_goals_get(nhl, game_id, level, &(*game)->goals, &(*game)->num_goals);
    }

    nhl_cache_game_free(cache_game);
    nhl_finish(nhl, start);
    return status;
}

void nhl_game_unget(Nhl *nhl, NhlGame *game) {
    if (game != NULL && nhl_dict_unref(nhl->games, game) == 0) {
        nhl_team_unget(nhl, game->away);
        nhl_team_unget(nhl, game->home);
        nhl_game_type_unget(nhl, game->type);
        nhl_game_status_unget(nhl, game->status);
        nhl_game_details_unget(nhl, game->details);
        nhl_goals_unget(nhl, game->goals, game->num_goals);
        delete_game(game);
    }
}
