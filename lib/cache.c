#include "cache.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>


/* Current time from SQLite. */
const char *nhl_cache_current_time(Nhl *nhl) {
    static const char sql[] = "SELECT datetime('now');";
    static char current_time[] = "0000-00-00 00:00:00";

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(nhl->db, sql, -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) == SQLITE_TEXT) {
        const char *datetime = (const char *) sqlite3_column_text(stmt, 0);
        int sz = sqlite3_column_bytes(stmt, 0);
        if (sz + 1 == sizeof(current_time)) {
            memcpy(current_time, datetime, sz);
        }
    }
    sqlite3_finalize(stmt);

    return current_time;
}

/* Age of the given timestamp in seconds. Negative value indicates an error. */
int nhl_cache_timestamp_age(Nhl *nhl, const char *timestamp) {
    int age = -1;
    char *sql = sqlite3_mprintf(
        "SELECT strftime('%%s', 'now') - strftime('%%s', %Q);", timestamp);

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(nhl->db, sql, -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) == SQLITE_INTEGER) {
        age = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    sqlite3_free(sql);
    return age;
}


/* Definition of a single column in SQLite table. */
typedef struct NhlCacheColumn {
    char *name;
    char *type;
} NhlCacheColumn;

/* Possible values when checking for column existence. */
typedef enum NhlCacheColumnType {
    NHL_CACHE_COLUMN_NOT_FOUND,
    NHL_CACHE_COLUMN_INTEGER,
    NHL_CACHE_COLUMN_TEXT,
    NHL_CACHE_COLUMN_OTHER
} NhlCacheColumnType;

/* Convert null-terminated array of column definitions to a string suitable for SQL substitution.
 * For example, "NAME1 TEXT, NAME2 INTEGER", or "NAME1, NAME2" if names_only is nonzero.
 * The returned string must be released with free(). */
static char *columns_to_string(const NhlCacheColumn *columns, int names_only) {
    size_t total_len = 0;
    size_t next_len = 0;
    size_t allocated = 64;
    char *list = malloc(allocated);

    if (list != NULL) {
        const NhlCacheColumn *col = columns;
        list[0] = '\0';

        for ( ; col->name; ++col) {
            if (names_only) {
                next_len += strlen(col->name) + 2; /* comma and space */
            } else {
                next_len += strlen(col->name) + strlen(col->type) + 3; /* comma and two spaces */
            }

            if (allocated < next_len + 1) {
                allocated = 2 * (next_len + 1);
                list = realloc(list, allocated);
                if (list == NULL) {
                    goto failure;
                }
            }

            if (names_only) {
                sprintf(list + total_len, "%s, ", col->name);
            } else {
                sprintf(list + total_len, "%s %s, ", col->name, col->type);
            }

            total_len = next_len;
        }

        if (total_len > 1) {
            /* Remove last whitespace and comma */
            list[total_len-1] = '\0';
            list[total_len-2] = '\0';
            list = realloc(list, total_len-1);
            if (list == NULL) {
                goto failure;
            }
        }
        return list;
    }

    failure:
        fprintf(stderr, "FATAL ERROR: allocation failure in %s:%d", __FILE__, __LINE__);
        exit(3);
}

/* Find column from a null-terminated array of column definitions. */
static NhlCacheColumnType find_column(const NhlCacheColumn *columns, const char *col) {
    for ( ; columns->name != NULL; ++columns) {
        if (strcmp(columns->name, col) == 0) {
            switch (columns->type[0]) {
                case 'i':
                case 'I':
                    return NHL_CACHE_COLUMN_INTEGER;
                case 't':
                case 'T':
                    return NHL_CACHE_COLUMN_TEXT;
                default:
                    return NHL_CACHE_COLUMN_OTHER;
            }
        }
    }
    return NHL_CACHE_COLUMN_NOT_FOUND;
}

/* Create database table if it does not already exist. Returns zero if error occurs. */
static int ensure_table(Nhl *nhl, const char *table, const NhlCacheColumn *columns) {
    char *clist = columns_to_string(columns, 0);
    char *sql = sqlite3_mprintf("CREATE TABLE IF NOT EXISTS %Q (%q);", table, clist);
    sqlite3_exec(nhl->db, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    free(clist);
    return 1; /* TODO: Check return value */
}

/* Create template for SQL SELECT string. An example output is
 *     "SELECT NAME1, NAME2 FROM <table> WHERE %q=<spec>;"
 * where <table> and <spec> are specified in the input (spec is typically %d or %s), and NAME1 and
 * NAME2 are deduced from the null-terminated array of column definitions.
 * The returned string must be released with free(). */
static char *sql_select_template(const char *table, const NhlCacheColumn *columns, const char *spec) {
    static const char template[] = "SELECT "   " FROM "   " WHERE %q=%_;";
    char *clist = columns_to_string(columns, 1);
    size_t len = strlen(template) + strlen(clist) + strlen(table);
    char *sql = malloc(len+1);
    sprintf(sql, "SELECT %s FROM %s WHERE %%q=%s;", clist, table, spec);
    free(clist);
    return sql;
}

/* Extract text from a single column in a SQL statement result. Release with free(). */
static char *copy_column_text(sqlite3_stmt *stmt, int col) {
    const char *text = (const char *) sqlite3_column_text(stmt, col);
    int len = sqlite3_column_bytes(stmt, col);
    char *copy = malloc(len+1);
    memcpy(copy, text, len);
    copy[len] = '\0';
    return copy;
}

/* Destroy NhlCacheMeta object. */
static void free_meta(NhlCacheMeta *meta) {
    if (meta) {
        free(meta->source);
        free(meta->timestamp);
        free(meta);
    }
}

static size_t num_columns(const NhlCacheColumn *columns) {
    size_t num_columns = 0;
    while (columns[num_columns].name)
        ++num_columns;
    return num_columns;
}

/* Create template for SQL INSERT string. An example output is
 *     "INSERT OR REPLACE INTO <table> VALUES (%Q,%%Q);"
 * where <table> is specified by the input, and placeholders with one or two % characters
 * correspond to ordinary column types and metadata column types (names beginning with underscore),
 * respectively.
 * The returned string must be released with free().
 */
static char *sql_insert_template(const char *table, const NhlCacheColumn *columns) {
    static const char prefix[] = "INSERT OR REPLACE INTO ";
    static const char middle[] = " VALUES (";
    static const char suffix[] = ");";

    size_t tablename_len = strlen(table);
    size_t num_cols = num_columns(columns);
    size_t values_len = num_cols > 0 ? 4*num_cols - 1 : 0; /* %d, or %Q, and last without comma */
    size_t sql_len = (sizeof(prefix)-1) + tablename_len + (sizeof(middle)-1) + values_len + (sizeof(suffix)-1);

    char *sql = malloc(sql_len + 1);
    int pos = sprintf(sql, "%s%s%s", prefix, table, middle);

    for ( ; columns->name; ++columns) {
        if (tolower(columns->type[0]) == 'i') { /* Integer type */
            if (columns->name[0] != '_') {
                strcpy(sql + pos, "%d,");
                pos += 3;
            } else {
                strcpy(sql + pos, "%%d,"); /* Delayed substitution for metadata variable */
                pos += 4;
            }
        } else if (tolower(columns->type[0]) == 't') { /* Text type */
            if (columns->name[0] != '_') {
                strcpy(sql + pos, "%Q,");
                pos += 3;
            } else {
                strcpy(sql + pos, "%%Q,"); /* Delayed substitution for metadata variable */
                pos += 4;
            }
        } else {
            printf("ERROR: Invalid database column type.\n");
            exit(3);
        }
    }

    /* Overwrite last comma */
    strcpy(sql + pos - 1, suffix);
    return sql;
}


/*** Source URLs ***/
static const char source_table[] = "Sources";
static const NhlCacheColumn source_columns[] = {
    {"url", "TEXT PRIMARY KEY"},
    {0}
};

/* Add non-existing source URL to database and return its numeric ID. */
static int add_source(Nhl *nhl, const char *source) {
    static const char sql_template[] = "INSERT INTO %q VALUES (%Q);";
    static const char sql_rowid[] = "SELECT last_insert_rowid();";
    char *sql = sqlite3_mprintf(sql_template, source_table, source);
    int source_id = 0;
    sqlite3_stmt *stmt;

    sqlite3_exec(nhl->db, sql, NULL, NULL, NULL);

    sqlite3_prepare_v2(nhl->db, sql_rowid, -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        source_id = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    sqlite3_free(sql);
    return source_id;
}

/* Get numeric ID of a given source URL, and add URL to database if it does not already exist. */
static int source_to_num(Nhl *nhl, const char *source) {
    static const char sql_template[] = "SELECT rowid,%q FROM %q WHERE %q=%Q;";
    char *sql = sqlite3_mprintf(sql_template, source_columns[0].name, source_table, source_columns[0].name, source);
    int source_id;
    sqlite3_stmt *stmt;

    ensure_table(nhl, source_table, source_columns);

    sqlite3_prepare_v2(nhl->db, sql, -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        source_id = sqlite3_column_int(stmt, 0);
    } else {
        source_id = add_source(nhl, source);
    }

    sqlite3_finalize(stmt);
    sqlite3_free(sql);
    return source_id;
}

/* Return source URL as string for given numeric ID, or NULL if not found. Release with free(). */
static char *num_to_source(Nhl *nhl, int source_id) {
    static const char sql_template[] = "SELECT %q FROM %q WHERE rowid=%d;";
    char *sql = sqlite3_mprintf(sql_template, source_columns[0].name, source_table, source_id);
    char *source = NULL;
    sqlite3_stmt *stmt;

    ensure_table(nhl, source_table, source_columns);

    sqlite3_prepare_v2(nhl->db, sql, -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        source = copy_column_text(stmt, 0);
    }

    sqlite3_finalize(stmt);
    sqlite3_free(sql);
    return source;
}


/* Add new row or update an existing row in a cache table. The trailing arguments must match with
 * the corresponding variables in the column definitions of the table. */
static NhlStatus cache_put(Nhl *nhl, const char *table, const NhlCacheColumn *columns, NhlCacheMeta *meta, ...) {
    va_list args;
    int source_id;
    char *sql_template1;
    char *sql_template2;
    char *sql;

    ensure_table(nhl, table, columns);

    source_id = source_to_num(nhl, meta->source);

    /* Construct SQL query */
    sql_template1 = sql_insert_template(table, columns);
    va_start(args, meta);
    sql_template2 = sqlite3_vmprintf(sql_template1, args);
    va_end(args);
    sql = sqlite3_mprintf(sql_template2, source_id, meta->timestamp, meta->invalid);

    sqlite3_exec(nhl->db, sql, NULL, NULL, NULL);

    sqlite3_free(sql);
    sqlite3_free(sql_template2);
    free(sql_template1);

    return NHL_CACHE_WRITE_OK; /* TODO: check return value */
}

/* Read single row from a table and store values into the trailing arguments that must match with
 * the given null-terminated array of column definitions. Text values must be released with free().
 * The row is found by input arguments where_col and where_val which determine the name of the
 * column and its desired value, respectively. Nonzero return value implies success. */
static int cache_get(Nhl *nhl, const char *table, const NhlCacheColumn *columns,
                      const char *where_col, const void *where_val, ...) {
    va_list args;
    char *template;
    char *sql;
    sqlite3_stmt *stmt;
    NhlCacheColumnType colType = find_column(columns, where_col);
    int col;
    int ok;
    NhlCacheMeta **meta;

    ensure_table(nhl, table, columns);
    if (colType == NHL_CACHE_COLUMN_INTEGER) {
        template = sql_select_template(table, columns, "%d");
        sql = sqlite3_mprintf(template, where_col, *(const int *) where_val);
    } else if (colType == NHL_CACHE_COLUMN_TEXT) {
        template = sql_select_template(table, columns, "%Q");
        sql = sqlite3_mprintf(template, where_col, (const char *) where_val);
    } else {
        fprintf(stderr, "ERROR\n");
        exit(3);
    }

    va_start(args, where_val);

    sqlite3_prepare_v2(nhl->db, sql, -1, &stmt, NULL);
    ok = sqlite3_step(stmt) == SQLITE_ROW;

    for (col = 0; columns->name != NULL && columns->name[0] != '_'; ++columns, ++col) {
        switch (columns->type[0]) {
            case 'i': case 'I':
                *va_arg(args, int*) = ok ? sqlite3_column_int(stmt, col) : 0;
                break;
            case 't': case 'T':
                *va_arg(args, char**) = ok ? copy_column_text(stmt, col) : NULL;
                break;
            default:
                fprintf(stderr, "ERROR\n");
                exit(3);
        }
    }
    meta = va_arg(args, NhlCacheMeta **);
    if (ok) {
        *meta = malloc(sizeof(NhlCacheMeta));
        (*meta)->source = ok ? num_to_source(nhl, sqlite3_column_int(stmt, col)) : NULL;
        (*meta)->timestamp = ok ? copy_column_text(stmt, col+1) : NULL;
        (*meta)->invalid = ok ? sqlite3_column_int(stmt, col+2) : 0;
    }

    sqlite3_finalize(stmt);
    sqlite3_free(sql);
    free(template);
    va_end(args);
    return ok;
}


/*** Schedules ***/
static const char schedule_table[] = "Schedules";
static const NhlCacheColumn schedule_columns[] = {
    {"date",       "TEXT PRIMARY KEY"},
    {"totalGames", "INTEGER"},
    {"_source",    "INTEGER"},
    {"_timestamp", "TEXT"},
    {"_invalid",   "INTEGER"},
    {0}
};

NhlStatus nhl_cache_schedule_put(Nhl *nhl, const NhlCacheSchedule *schedule) {
    return cache_put(nhl, schedule_table, schedule_columns, schedule->meta,
        schedule->date,
        schedule->totalGames);
}

NhlCacheSchedule *nhl_cache_schedule_get(Nhl *nhl, const char *schedule_date) {
    NhlCacheSchedule *schedule = malloc(sizeof(NhlCacheSchedule));
    int success = cache_get(nhl, schedule_table, schedule_columns, "date", schedule_date,
        &schedule->date,
        &schedule->totalGames,
        &schedule->meta);
    if (!success) {
        free(schedule);
        schedule = NULL;
    }
    return schedule;
}

void nhl_cache_schedule_free(NhlCacheSchedule *schedule) {
    if (schedule != NULL) {
        free_meta(schedule->meta);
        free(schedule->date);
        free(schedule);
    }
}


/*** Games ***/
static const char game_table[] = "Games";
static const NhlCacheColumn game_columns[] = {
    {"gamePk",          "INTEGER PRIMARY KEY"},
    {"date",            "TEXT"},
    {"gameType",        "TEXT"},
    {"season",          "TEXT"},
    {"gameDate",        "TEXT"},
    {"statusCode",      "TEXT"},
    {"awayTeam",        "INTEGER"},
    {"awayScore",       "INTEGER"},
    {"awayWins",        "INTEGER"},
    {"awayLosses",      "INTEGER"},
    {"awayOt",          "INTEGER"},
    {"awayRecordType",  "TEXT"},
    {"homeTeam",        "INTEGER"},
    {"homeScore",       "INTEGER"},
    {"homeWins",        "INTEGER"},
    {"homeLosses",      "INTEGER"},
    {"homeOt",          "INTEGER"},
    {"homeRecordType",  "TEXT"},
    {"_source",         "INTEGER"},
    {"_timestamp",      "TEXT"},
    {"_invalid",        "INTEGER"},
    {0}
};

NhlStatus nhl_cache_game_put(Nhl *nhl, const NhlCacheGame *game) {
    return cache_put(nhl, game_table, game_columns, game->meta,
        game->gamePk,
        game->date,
        game->gameType,
        game->season,
        game->gameDate,
        game->statusCode,
        game->awayTeam,
        game->awayScore,
        game->awayWins,
        game->awayLosses,
        game->awayOt,
        game->awayRecordType,
        game->homeTeam,
        game->homeScore,
        game->homeWins,
        game->homeLosses,
        game->homeOt,
        game->homeRecordType);
}

int *nhl_cache_games_find(Nhl *nhl, const char *date, int *num_games) {
    char *template;
    char *sql;
    sqlite3_stmt *stmt;
    int num_alloc = 4;
    int *gamePk = malloc(num_alloc * sizeof(int));

    ensure_table(nhl, game_table, game_columns);
    template = sql_select_template(game_table, game_columns, "%Q");
    sql = sqlite3_mprintf(template, "date", date);

    sqlite3_prepare_v2(nhl->db, sql, -1, &stmt, NULL);
    *num_games = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (num_alloc <= *num_games) {
            num_alloc *= 2;
            gamePk = realloc(gamePk, num_alloc * sizeof(int));
        }
        gamePk[*num_games] = sqlite3_column_int(stmt, 0);
        ++*num_games;
    }
    gamePk = realloc(gamePk, *num_games * sizeof(int));

    sqlite3_finalize(stmt);
    sqlite3_free(sql);
    free(template);
    return gamePk;
}

NhlCacheGame *nhl_cache_game_get(Nhl *nhl, int game_id) {
    NhlCacheGame *game = malloc(sizeof(NhlCacheGame));
    int success = cache_get(nhl, game_table, game_columns, "gamePk", &game_id,
        &game->gamePk,
        &game->date,
        &game->gameType,
        &game->season,
        &game->gameDate,
        &game->statusCode,
        &game->awayTeam,
        &game->awayScore,
        &game->awayWins,
        &game->awayLosses,
        &game->awayOt,
        &game->awayRecordType,
        &game->homeTeam,
        &game->homeScore,
        &game->homeWins,
        &game->homeLosses,
        &game->homeOt,
        &game->homeRecordType,
        &game->meta);
    if (!success) {
        free(game);
        game = NULL;
    }
    return game;
}

void nhl_cache_game_free(NhlCacheGame *game) {
    if (game != NULL) {
        free_meta(game->meta);
        free(game->date);
        free(game->gameType);
        free(game->season);
        free(game->gameDate);
        free(game->statusCode);
        free(game->awayRecordType);
        free(game->homeRecordType);
        free(game);
    }
}


/*** Game types ***/
static const char gametyp_table[] = "GameTypes";
static const NhlCacheColumn gametyp_columns[] = {
    {"id",          "TEXT PRIMARY KEY"},
    {"description", "TEXT"},
    {"postseason",  "INTEGER"},
    {"_source",     "INTEGER"},
    {"_timestamp",  "TEXT"},
    {"_invalid",    "INTEGER"},
    {0}
};

NhlStatus nhl_cache_game_type_put(Nhl *nhl, const NhlCacheGameType *game_type) {
    return cache_put(nhl, gametyp_table, gametyp_columns, game_type->meta,
        game_type->id,
        game_type->description,
        game_type->postseason);
}

NhlCacheGameType *nhl_cache_game_type_get(Nhl *nhl, const char *game_type_id) {
    NhlCacheGameType *gametyp = malloc(sizeof(NhlCacheGameType));
    int success = cache_get(nhl, gametyp_table, gametyp_columns, "id", game_type_id,
        &gametyp->id,
        &gametyp->description,
        &gametyp->postseason,
        &gametyp->meta);
    if (!success) {
        free(gametyp);
        gametyp = NULL;
    }
    return gametyp;
}

void nhl_cache_game_type_free(NhlCacheGameType *game_type) {
    if (game_type != NULL) {
        free_meta(game_type->meta);
        free(game_type->id);
        free(game_type->description);
        free(game_type);
    }
}


/*** Game statuses ***/
static const char gamest_table[] = "GameStatuses";
static const NhlCacheColumn gamest_columns[] = {
    {"code",              "TEXT PRIMARY KEY"},
    {"abstractGameState", "TEXT"},
    {"detailedState",     "TEXT"},
    {"startTimeTBD",      "INTEGER"},
    {"_source",           "INTEGER"},
    {"_timestamp",        "TEXT"},
    {"_invalid",          "INTEGER"},
    {0}
};

NhlStatus nhl_cache_game_status_put(Nhl *nhl, const NhlCacheGameStatus *gamest) {
    return cache_put(nhl, gamest_table, gamest_columns, gamest->meta,
        gamest->code,
        gamest->abstractGameState,
        gamest->detailedState,
        gamest->startTimeTBD);
}

NhlCacheGameStatus *nhl_cache_game_status_get(Nhl *nhl, const char *game_status_code) {
    NhlCacheGameStatus *gamest = malloc(sizeof(NhlCacheGameStatus));
    int success = cache_get(nhl, gamest_table, gamest_columns, "code", game_status_code,
        &gamest->code,
        &gamest->abstractGameState,
        &gamest->detailedState,
        &gamest->startTimeTBD,
        &gamest->meta);
    if (!success) {
        free(gamest);
        gamest = NULL;
    }
    return gamest;
}

void nhl_cache_game_status_free(NhlCacheGameStatus *gamest) {
    if (gamest != NULL) {
        free_meta(gamest->meta);
        free(gamest->code);
        free(gamest->abstractGameState);
        free(gamest->detailedState);
        free(gamest);
    }
}


/*** Linescores ***/
static const char linescore_table[] = "Linescores";
static const NhlCacheColumn linescore_columns[] = {
    {"game",                        "INTEGER PRIMARY KEY"},
    {"currentPeriod",               "INTEGER"},
    {"currentPeriodOrdinal",        "TEXT"},
    {"currentPeriodTimeRemaining",  "TEXT"},
    {"awayShootoutScores",          "INTEGER"},
    {"awayShootoutAttempts",        "INTEGER"},
    {"homeShootoutScores",          "INTEGER"},
    {"homeShootoutAttempts",        "INTEGER"},
    {"shootoutStartTime",           "TEXT"},
    {"awayShotsOnGoal",             "INTEGER"},
    {"awayGoaliePulled",            "INTEGER"},
    {"awayNumSkaters",              "INTEGER"},
    {"awayPowerPlay",               "INTEGER"},
    {"homeShotsOnGoal",             "INTEGER"},
    {"homeGoaliePulled",            "INTEGER"},
    {"homeNumSkaters",              "INTEGER"},
    {"homePowerPlay",               "INTEGER"},
    {"powerPlayStrength",           "TEXT"},
    {"hasShootout",                 "INTEGER"},
    {"intermissionTimeRemaining",   "INTEGER"},
    {"intermissionTimeElapsed",     "INTEGER"},
    {"intermission",                "INTEGER"},
    {"powerPlaySituationRemaining", "INTEGER"},
    {"powerPlaySituationElapsed",   "INTEGER"},
    {"powerPlayInSituation",        "INTEGER"},
    {"_source",                     "INTEGER"},
    {"_timestamp",                  "TEXT"},
    {"_invalid",                    "INTEGER"},
    {0}
};


NhlCacheLinescore *nhl_cache_linescore_get(Nhl *nhl, int game_id) {
    NhlCacheLinescore *linescore = malloc(sizeof(NhlCacheLinescore));
    int success = cache_get(nhl, linescore_table, linescore_columns, "game", &game_id,
        &linescore->game,
        &linescore->currentPeriod,
        &linescore->currentPeriodOrdinal,
        &linescore->currentPeriodTimeRemaining,
        &linescore->awayShootoutScores,
        &linescore->awayShootoutAttempts,
        &linescore->homeShootoutScores,
        &linescore->homeShootoutAttempts,
        &linescore->shootoutStartTime,
        &linescore->awayShotsOnGoal,
        &linescore->awayGoaliePulled,
        &linescore->awayNumSkaters,
        &linescore->awayPowerPlay,
        &linescore->homeShotsOnGoal,
        &linescore->homeGoaliePulled,
        &linescore->homeNumSkaters,
        &linescore->homePowerPlay,
        &linescore->powerPlayStrength,
        &linescore->hasShootout,
        &linescore->intermissionTimeRemaining,
        &linescore->intermissionTimeElapsed,
        &linescore->intermission,
        &linescore->powerPlaySituationRemaining,
        &linescore->powerPlaySituationElapsed,
        &linescore->powerPlayInSituation,
        &linescore->meta);
    if (!success) {
        free(linescore);
        linescore = NULL;
    }
    return linescore;
}

NhlStatus nhl_cache_linescore_put(Nhl *nhl, const NhlCacheLinescore *linescore) {
    return cache_put(nhl, linescore_table, linescore_columns, linescore->meta,
        linescore->game,
        linescore->currentPeriod,
        linescore->currentPeriodOrdinal,
        linescore->currentPeriodTimeRemaining,
        linescore->awayShootoutScores,
        linescore->awayShootoutAttempts,
        linescore->homeShootoutScores,
        linescore->homeShootoutAttempts,
        linescore->shootoutStartTime,
        linescore->awayShotsOnGoal,
        linescore->awayGoaliePulled,
        linescore->awayNumSkaters,
        linescore->awayPowerPlay,
        linescore->homeShotsOnGoal,
        linescore->homeGoaliePulled,
        linescore->homeNumSkaters,
        linescore->homePowerPlay,
        linescore->powerPlayStrength,
        linescore->hasShootout,
        linescore->intermissionTimeRemaining,
        linescore->intermissionTimeElapsed,
        linescore->intermission,
        linescore->powerPlaySituationRemaining,
        linescore->powerPlaySituationElapsed,
        linescore->powerPlayInSituation);
}

void nhl_cache_linescore_free(NhlCacheLinescore *linescore) {
    if (linescore != NULL) {
        free_meta(linescore->meta);
        free(linescore->currentPeriodOrdinal);
        free(linescore->currentPeriodTimeRemaining);
        free(linescore->shootoutStartTime);
        free(linescore->powerPlayStrength);
        free(linescore);
    }
}


/*** Periods ***/
static const char period_table[] = "Periods";
static const NhlCacheColumn period_columns[] = {
    {"game",            "INTEGER"},
    {"periodIndex",     "INTEGER"},
    {"periodType",      "TEXT"},
    {"startTime",       "TEXT"},
    {"endTime",         "TEXT"},
    {"num",             "INTEGER"},
    {"ordinalNum",      "TEXT"},
    {"awayGoals",       "INTEGER"},
    {"awayShotsOnGoal", "INTEGER"},
    {"awayRinkSide",    "TEXT"},
    {"homeGoals",       "INTEGER"},
    {"homeShotsOnGoal", "INTEGER"},
    {"homeRinkSide",    "TEXT"},
    {"_source",         "INTEGER"},
    {"_timestamp",      "TEXT"},
    {"_invalid",        "INTEGER"},
    {0}
};

NhlStatus nhl_cache_periods_reset(Nhl *nhl, int game_id) {
    char *sql = sqlite3_mprintf("DELETE FROM %Q WHERE game=%d", period_table, game_id);
    ensure_table(nhl, period_table, period_columns);
    sqlite3_exec(nhl->db, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    return NHL_CACHE_WRITE_OK;
}

NhlStatus nhl_cache_period_put(Nhl *nhl, const NhlCachePeriod *period) {
    return cache_put(nhl, period_table, period_columns, period->meta,
        period->game,
        period->periodIndex,
        period->periodType,
        period->startTime,
        period->endTime,
        period->num,
        period->ordinalNum,
        period->awayGoals,
        period->awayShotsOnGoal,
        period->awayRinkSide,
        period->homeGoals,
        period->homeShotsOnGoal,
        period->homeRinkSide);
}

NhlCachePeriod *nhl_cache_periods_get(Nhl *nhl, int game_id, int *num_periods) {
    char *template;
    char *sql;
    sqlite3_stmt *stmt;

    int num_alloc = 4;
    NhlCachePeriod *periods = malloc(num_alloc * sizeof(NhlCachePeriod));
    *num_periods = 0;

    ensure_table(nhl, period_table, period_columns);
    template = sql_select_template(period_table, period_columns, "%d");
    sql = sqlite3_mprintf(template, "game", game_id);

    sqlite3_prepare_v2(nhl->db, sql, -1, &stmt, NULL);

    while(sqlite3_step(stmt) == SQLITE_ROW) {
        int col = 0;

        if (num_alloc <= *num_periods) {
            num_alloc *= 2;
            periods = realloc(periods, num_alloc * sizeof(NhlCachePeriod));
        }

        periods[*num_periods].game = sqlite3_column_int(stmt, col++);
        periods[*num_periods].periodIndex = sqlite3_column_int(stmt, col++);
        periods[*num_periods].periodType = copy_column_text(stmt, col++);
        periods[*num_periods].startTime = copy_column_text(stmt, col++);
        periods[*num_periods].endTime = copy_column_text(stmt, col++);
        periods[*num_periods].num = sqlite3_column_int(stmt, col++);
        periods[*num_periods].ordinalNum = copy_column_text(stmt, col++);
        periods[*num_periods].awayGoals = sqlite3_column_int(stmt, col++);
        periods[*num_periods].awayShotsOnGoal = sqlite3_column_int(stmt, col++);
        periods[*num_periods].awayRinkSide = copy_column_text(stmt, col++);
        periods[*num_periods].homeGoals = sqlite3_column_int(stmt, col++);
        periods[*num_periods].homeShotsOnGoal = sqlite3_column_int(stmt, col++);
        periods[*num_periods].homeRinkSide = copy_column_text(stmt, col++);

        periods[*num_periods].meta = malloc(sizeof(NhlCacheMeta));
        periods[*num_periods].meta->source = copy_column_text(stmt, col);
        periods[*num_periods].meta->timestamp = copy_column_text(stmt, col+1);
        periods[*num_periods].meta->invalid = sqlite3_column_int(stmt, col+2);

        ++*num_periods;
    }

    sqlite3_finalize(stmt);
    sqlite3_free(sql);
    free(template);
    if (*num_periods != 0) {
        periods = realloc(periods, *num_periods * sizeof(NhlCachePeriod));
    } else {
        free(periods);
        periods = NULL;
    }
    return periods;
}

void nhl_cache_periods_free(NhlCachePeriod *periods, int num_periods) {
    if (periods != NULL) {
        int idx;
        for (idx = 0; idx != num_periods; ++idx) {
            free_meta(periods[idx].meta);
            free(periods[idx].periodType);
            free(periods[idx].startTime);
            free(periods[idx].endTime);
            free(periods[idx].ordinalNum);
            free(periods[idx].awayRinkSide);
            free(periods[idx].homeRinkSide);
        }
        free(periods);
    }
}


/*** Goals ***/
static const char goal_table[] = "Goals";
static const NhlCacheColumn goal_columns[] = {
    {"game",                "INTEGER"},
    {"goalNumber",          "INTEGER"},
    {"scorer",              "INTEGER"},
    {"scorerSeasonTotal",   "INTEGER"},
    {"assist1",             "INTEGER"},
    {"assist1SeasonTotal",  "INTEGER"},
    {"assist2",             "INTEGER"},
    {"assist2SeasonTotal",  "INTEGER"},
    {"goalie",              "INTEGER"},
    {"secondaryType",       "TEXT"},
    {"strengthCode",        "TEXT"},
    {"strengthName",        "TEXT"},
    {"gameWinningGoal",     "INTEGER"},
    {"emptyNet",            "INTEGER"},
    {"period",              "INTEGER"},
    {"periodType",          "TEXT"},
    {"ordinalNum",          "TEXT"},
    {"periodTime",          "TEXT"},
    {"periodTimeRemaining", "TEXT"},
    {"dateTime",            "TEXT"},
    {"goalsAway",           "INTEGER"},
    {"goalsHome",           "INTEGER"},
    {"team",                "INTEGER"},
    {"_source",             "INTEGER"},
    {"_timestamp",          "TEXT"},
    {"_invalid",            "INTEGER"},
    {0}
};

NhlStatus nhl_cache_goals_reset(Nhl *nhl, int game_id) {
    char *sql = sqlite3_mprintf("DELETE FROM %Q WHERE game=%d", goal_table, game_id);
    ensure_table(nhl, goal_table, goal_columns);
    sqlite3_exec(nhl->db, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    return NHL_CACHE_WRITE_OK;
}

NhlStatus nhl_cache_goal_put(Nhl *nhl, const NhlCacheGoal *goal) {
    return cache_put(nhl, goal_table, goal_columns, goal->meta,
        goal->game,
        goal->goalNumber,
        goal->scorer,
        goal->scorerSeasonTotal,
        goal->assist1,
        goal->assist1SeasonTotal,
        goal->assist2,
        goal->assist2SeasonTotal,
        goal->goalie,
        goal->secondaryType,
        goal->strengthCode,
        goal->strengthName,
        goal->gameWinningGoal,
        goal->emptyNet,
        goal->period,
        goal->periodType,
        goal->ordinalNum,
        goal->periodTime,
        goal->periodTimeRemaining,
        goal->dateTime,
        goal->goalsAway,
        goal->goalsHome,
        goal->team);
}

NhlCacheGoal *nhl_cache_goals_get(Nhl *nhl, int game_id, int *num_goals) {
    char *template;
    char *sql;
    sqlite3_stmt *stmt;

    int num_alloc = 4;
    NhlCacheGoal *goals = malloc(num_alloc * sizeof(NhlCacheGoal));
    *num_goals = 0;

    ensure_table(nhl, goal_table, goal_columns);
    template = sql_select_template(goal_table, goal_columns, "%d");
    sql = sqlite3_mprintf(template, "game", game_id);

    sqlite3_prepare_v2(nhl->db, sql, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = 0;

        if (num_alloc <= *num_goals) {
            num_alloc *= 2;
            goals = realloc(goals, num_alloc * sizeof(NhlCacheGoal));
        }

        goals[*num_goals].game = sqlite3_column_int(stmt, col++);
        goals[*num_goals].goalNumber = sqlite3_column_int(stmt, col++);
        goals[*num_goals].scorer = sqlite3_column_int(stmt, col++);
        goals[*num_goals].scorerSeasonTotal = sqlite3_column_int(stmt, col++);
        goals[*num_goals].assist1 = sqlite3_column_int(stmt, col++);
        goals[*num_goals].assist1SeasonTotal = sqlite3_column_int(stmt, col++);
        goals[*num_goals].assist2 = sqlite3_column_int(stmt, col++);
        goals[*num_goals].assist2SeasonTotal = sqlite3_column_int(stmt, col++);
        goals[*num_goals].goalie = sqlite3_column_int(stmt, col++);
        goals[*num_goals].secondaryType = copy_column_text(stmt, col++);
        goals[*num_goals].strengthCode = copy_column_text(stmt, col++);
        goals[*num_goals].strengthName = copy_column_text(stmt, col++);
        goals[*num_goals].gameWinningGoal = sqlite3_column_int(stmt, col++);
        goals[*num_goals].emptyNet = sqlite3_column_int(stmt, col++);
        goals[*num_goals].period = sqlite3_column_int(stmt, col++);
        goals[*num_goals].periodType = copy_column_text(stmt, col++);
        goals[*num_goals].ordinalNum = copy_column_text(stmt, col++);
        goals[*num_goals].periodTime = copy_column_text(stmt, col++);
        goals[*num_goals].periodTimeRemaining = copy_column_text(stmt, col++);
        goals[*num_goals].dateTime = copy_column_text(stmt, col++);
        goals[*num_goals].goalsAway = sqlite3_column_int(stmt, col++);
        goals[*num_goals].goalsHome = sqlite3_column_int(stmt, col++);
        goals[*num_goals].team = sqlite3_column_int(stmt, col++);
        goals[*num_goals].meta = malloc(sizeof(NhlCacheMeta));

        goals[*num_goals].meta->source = copy_column_text(stmt, col);
        goals[*num_goals].meta->timestamp = copy_column_text(stmt, col+1);
        goals[*num_goals].meta->invalid = sqlite3_column_int(stmt, col+2);

        ++*num_goals;
    }

    sqlite3_finalize(stmt);
    sqlite3_free(sql);
    free(template);
    if (*num_goals != 0) {
        goals = realloc(goals, *num_goals * sizeof(NhlCacheGoal));
    } else {
        free(goals);
        goals = NULL;
    }
    return goals;
}

void nhl_cache_goals_free(NhlCacheGoal *goals, int num_goals) {
    if (goals != NULL) {
        int idx;
        for (idx = 0; idx != num_goals; ++idx) {
            free_meta(goals[idx].meta);
            free(goals[idx].secondaryType);
            free(goals[idx].strengthCode);
            free(goals[idx].strengthName);
            free(goals[idx].periodType);
            free(goals[idx].ordinalNum);
            free(goals[idx].periodTime);
            free(goals[idx].periodTimeRemaining);
            free(goals[idx].dateTime);
        }
        free(goals);
    }
}


/*** Conferences ***/
static const char conference_table[] = "Conferences";
static const NhlCacheColumn conference_columns[] = {
    {"id",           "INTEGER PRIMARY KEY"},
    {"name",         "TEXT"},
    {"abbreviation", "TEXT"},
    {"shortName",    "TEXT"},
    {"active",       "INTEGER"},
    {"_source",      "INTEGER"},
    {"_timestamp",   "TEXT"},
    {"_invalid",     "INTEGER"},
    {0}
};

NhlStatus nhl_cache_conference_put(Nhl *nhl, const NhlCacheConference *conference) {
    return cache_put(nhl, conference_table, conference_columns, conference->meta,
        conference->id,
        conference->name,
        conference->abbreviation,
        conference->shortName,
        conference->active);
}

NhlCacheConference *nhl_cache_conference_get(Nhl *nhl, int conference_id) {
    NhlCacheConference *conference = malloc(sizeof(NhlCacheConference));
    int success = cache_get(nhl, conference_table, conference_columns, "id", &conference_id,
        &conference->id,
        &conference->name,
        &conference->abbreviation,
        &conference->shortName,
        &conference->active,
        &conference->meta);
    if (!success) {
        free(conference);
        conference = NULL;
    }
    return conference;
}

void nhl_cache_conference_free(NhlCacheConference *conference) {
    if (conference != NULL) {
        free_meta(conference->meta);
        free(conference->name);
        free(conference->abbreviation);
        free(conference->shortName);
        free(conference);
    }
}


/*** Divisions ***/
static const char division_table[] = "Divisions";
static const NhlCacheColumn division_columns[] = {
    {"id",           "INTEGER PRIMARY KEY"},
    {"name",         "TEXT"},
    {"nameShort",    "TEXT"},
    {"abbreviation", "TEXT"},
    {"conference",   "INTEGER"},
    {"active",       "INTEGER"},
    {"_source",      "INTEGER"},
    {"_timestamp",   "TEXT"},
    {"_invalid",     "INTEGER"},
    {0}
};

NhlStatus nhl_cache_division_put(Nhl *nhl, const NhlCacheDivision *division) {
    return cache_put(nhl, division_table, division_columns, division->meta,
        division->id,
        division->name,
        division->nameShort,
        division->abbreviation,
        division->conference,
        division->active);
}

NhlCacheDivision *nhl_cache_division_get(Nhl *nhl, int division_id) {
    NhlCacheDivision *division = malloc(sizeof(NhlCacheDivision));
    int success = cache_get(nhl, division_table, division_columns, "id", &division_id,
        &division->id,
        &division->name,
        &division->nameShort,
        &division->abbreviation,
        &division->conference,
        &division->active,
        &division->meta);
    if (!success) {
        free(division);
        division = NULL;
    }
    return division;
}

void nhl_cache_division_free(NhlCacheDivision *division) {
    if (division != NULL) {
        free_meta(division->meta);
        free(division->name);
        free(division->nameShort);
        free(division->abbreviation);
        free(division);
    }
}


/*** Players ***/
static const char player_table[] = "Players";
static const NhlCacheColumn player_columns[] = {
    {"id",                 "INTEGER PRIMARY KEY"},
    {"fullName",           "TEXT"},
    {"firstName",          "TEXT"},
    {"lastName",           "TEXT"},
    {"primaryNumber",      "TEXT"},
    {"birthDate",          "TEXT"},
    {"birthCity",          "TEXT"},
    {"birthStateProvince", "TEXT"},
    {"birthCountry",       "TEXT"},
    {"nationality",        "TEXT"},
    {"height",             "TEXT"},
    {"weight",             "INTEGER"},
    {"active",             "INTEGER"},
    {"alternateCaptain",   "INTEGER"},
    {"captain",            "INTEGER"},
    {"rookie",             "INTEGER"},
    {"shootsCatches",      "TEXT"},
    {"rosterStatus",       "TEXT"},
    {"currentTeam",        "INTEGER"},
    {"primaryPosition",    "TEXT"},
    {"_source",            "INTEGER"},
    {"_timestamp",         "TEXT"},
    {"_invalid",           "INTEGER"},
    {0}
};

NhlStatus nhl_cache_player_put(Nhl *nhl, const NhlCachePlayer *player) {
    return cache_put(nhl, player_table, player_columns, player->meta,
        player->id,
        player->fullName,
        player->firstName,
        player->lastName,
        player->primaryNumber,
        player->birthDate,
        player->birthCity,
        player->birthStateProvince,
        player->birthCountry,
        player->nationality,
        player->height,
        player->weight,
        player->active,
        player->alternateCaptain,
        player->captain,
        player->rookie,
        player->shootsCatches,
        player->rosterStatus,
        player->currentTeam,
        player->primaryPosition);
}

NhlCachePlayer *nhl_cache_player_get(Nhl *nhl, int player_id) {
    NhlCachePlayer *player = malloc(sizeof(NhlCachePlayer));
    int success = cache_get(nhl, player_table, player_columns, "id", &player_id,
        &player->id,
        &player->fullName,
        &player->firstName,
        &player->lastName,
        &player->primaryNumber,
        &player->birthDate,
        &player->birthCity,
        &player->birthStateProvince,
        &player->birthCountry,
        &player->nationality,
        &player->height,
        &player->weight,
        &player->active,
        &player->alternateCaptain,
        &player->captain,
        &player->rookie,
        &player->shootsCatches,
        &player->rosterStatus,
        &player->currentTeam,
        &player->primaryPosition,
        &player->meta);
    if (!success) {
        free(player);
        player = NULL;
    }
    return player;
}

void nhl_cache_player_free(NhlCachePlayer *player) {
    if (player != NULL) {
        free_meta(player->meta);
        free(player->fullName);
        free(player->firstName);
        free(player->lastName);
        free(player->primaryNumber);
        free(player->birthDate);
        free(player->birthCity);
        free(player->birthStateProvince);
        free(player->birthCountry);
        free(player->nationality);
        free(player->height);
        free(player->shootsCatches);
        free(player->rosterStatus);
        free(player->primaryPosition);
        free(player);
    }
}


/*** Positions ***/
static const char position_table[] = "Positions";
static const NhlCacheColumn position_columns[] = {
    {"abbrev",   "TEXT"},
    {"code",     "TEXT PRIMARY KEY"},
    {"fullName", "TEXT"},
    {"type",     "TEXT"},
    {"_source",      "INTEGER"},
    {"_timestamp",   "TEXT"},
    {"_invalid",     "INTEGER"},
    {0}
};

NhlStatus nhl_cache_position_put(Nhl *nhl, const NhlCachePosition *position) {
    return cache_put(nhl, position_table, position_columns, position->meta,
        position->abbrev,
        position->code,
        position->fullName,
        position->type);
}

NhlCachePosition *nhl_cache_position_get(Nhl *nhl, const char *position_code) {
    NhlCachePosition *position = malloc(sizeof(NhlCachePosition));
    int success = cache_get(nhl, position_table, position_columns, "code", position_code,
        &position->abbrev,
        &position->code,
        &position->fullName,
        &position->type,
        &position->meta);
    if (!success) {
        free(position);
        position = NULL;
    }
    return position;
}

void nhl_cache_position_free(NhlCachePosition *position) {
    if (position != NULL) {
        free_meta(position->meta);
        free(position->abbrev);
        free(position->code);
        free(position->fullName);
        free(position->type);
        free(position);
    }
}


/*** Roster statuses ***/
static const char rosterst_table[] = "RosterStatuses";
static const NhlCacheColumn rosterst_columns[] = {
    {"code",        "TEXT PRIMARY KEY"},
    {"description", "TEXT"},
    {"_source",     "INTEGER"},
    {"_timestamp",  "TEXT"},
    {"_invalid",    "INTEGER"},
    {0}
};

NhlStatus nhl_cache_roster_status_put(Nhl *nhl, const NhlCacheRosterStatus *rosterst) {
    return cache_put(nhl, rosterst_table, rosterst_columns, rosterst->meta,
        rosterst->code,
        rosterst->description);
}

NhlCacheRosterStatus *nhl_cache_roster_status_get(Nhl *nhl, const char *roster_status_code) {
    NhlCacheRosterStatus *rosterst = malloc(sizeof(NhlCacheRosterStatus));
    int success = cache_get(nhl, rosterst_table, rosterst_columns, "code", roster_status_code,
        &rosterst->code,
        &rosterst->description,
        &rosterst->meta);
    if (!success) {
        free(rosterst);
        rosterst = NULL;
    }
    return rosterst;
}

void nhl_cache_roster_status_free(NhlCacheRosterStatus *rosterst) {
    if (rosterst != NULL) {
        free_meta(rosterst->meta);
        free(rosterst->code);
        free(rosterst->description);
        free(rosterst);
    }
}


/*** Teams ***/
static const char team_table[] = "Teams";
static const NhlCacheColumn team_columns[] = {
    {"id",               "INTEGER PRIMARY KEY"},
    {"name",             "TEXT"},
    {"abbreviation",     "TEXT"},
    {"teamName",         "TEXT"},
    {"locationName",     "TEXT"},
    {"firstYearOfPlay",  "TEXT"},
    {"division",         "INTEGER"},
    {"conference",       "INTEGER"},
    {"franchise",        "INTEGER"},
    {"shortName",        "TEXT"},
    {"officialSiteUrl",  "TEXT"},
    {"active",           "INTEGER"},
    {"_source",          "INTEGER"},
    {"_timestamp",       "TEXT"},
    {"_invalid",         "INTEGER"},
    {0}
};

NhlStatus nhl_cache_team_put(Nhl *nhl, const NhlCacheTeam *team) {
    return cache_put(nhl, team_table, team_columns, team->meta,
        team->id,
        team->name,
        team->abbreviation,
        team->teamName,
        team->locationName,
        team->firstYearOfPlay,
        team->division,
        team->conference,
        team->franchise,
        team->shortName,
        team->officialSiteUrl,
        team->active);
}

NhlCacheTeam *nhl_cache_team_get(Nhl *nhl, int team_id) {
    NhlCacheTeam *team = malloc(sizeof(NhlCacheTeam));
    int success = cache_get(nhl, team_table, team_columns, "id", &team_id,
        &team->id,
        &team->name,
        &team->abbreviation,
        &team->teamName,
        &team->locationName,
        &team->firstYearOfPlay,
        &team->division,
        &team->conference,
        &team->franchise,
        &team->shortName,
        &team->officialSiteUrl,
        &team->active,
        &team->meta);
    if (!success) {
        free(team);
        team = NULL;
    }
    return team;
}

void nhl_cache_team_free(NhlCacheTeam *team) {
    if (team != NULL) {
        free_meta(team->meta);
        free(team->name);
        free(team->abbreviation);
        free(team->teamName);
        free(team->locationName);
        free(team->firstYearOfPlay);
        free(team->shortName);
        free(team->officialSiteUrl);
        free(team);
    }
}


/*** Franchises ***/
static const char franchise_table[] = "Franchises";
static const NhlCacheColumn franchise_columns[] = {
    {"franchiseId",      "INTEGER PRIMARY KEY"},
    {"firstSeasonId",    "INTEGER"},
    {"lastSeasonId",     "INTEGER"},
    {"mostRecentTeamId", "INTEGER"},
    {"teamName",         "TEXT"},
    {"locationName",     "TEXT"},
    {"_source",          "INTEGER"},
    {"_timestamp",       "TEXT"},
    {"_invalid",         "INTEGER"},
    {0}
};

NhlStatus nhl_cache_franchise_put(Nhl *nhl, const NhlCacheFranchise *franchise) {
    return cache_put(nhl, franchise_table, franchise_columns, franchise->meta,
        franchise->franchiseId,
        franchise->firstSeasonId,
        franchise->lastSeasonId,
        franchise->mostRecentTeamId,
        franchise->teamName,
        franchise->locationName);
}

NhlCacheFranchise *nhl_cache_franchise_get(Nhl *nhl, int franchise_id) {
    NhlCacheFranchise *franchise = malloc(sizeof(NhlCacheFranchise));
    int success = cache_get(nhl, franchise_table, franchise_columns, "franchiseId", &franchise_id,
        &franchise->franchiseId,
        &franchise->firstSeasonId,
        &franchise->lastSeasonId,
        &franchise->mostRecentTeamId,
        &franchise->teamName,
        &franchise->locationName,
        &franchise->meta);
    if (!success) {
        free(franchise);
        franchise = NULL;
    }
    return franchise;
}

void nhl_cache_franchise_free(NhlCacheFranchise *franchise) {
    if (franchise != NULL) {
        free_meta(franchise->meta);
        free(franchise->teamName);
        free(franchise->locationName);
        free(franchise);
    }
}
