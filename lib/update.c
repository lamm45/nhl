#include <nhl/update.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cjson/cJSON.h>
#include <curl/curl.h>

#include "cache.h"
#include "handle.h"
#include "list.h"
#include "mem.h"


/* Macro for reading nodes from a JSON tree.
 * TODO: The usage of static variable is a bit hacky and definitely not thread-safe.
 */
static cJSON *leaf;
#define read_leaf(parent, name, type) \
    (leaf = cJSON_GetObjectItemCaseSensitive((parent), (name)), leaf ? leaf->type : 0);


/* User data for CURL write callback function. */
typedef struct cb_data {
    char *str; /* null-terminated data, or NULL */
    size_t len; /* string length (without terminator) */
} cb_data;

/* CURL write callback function. */
static size_t read_url_cb(void *url_contents, size_t size, size_t len, void *writedata) {
    size_t bytes = size * len;
    cb_data *data = writedata;
    data->str = realloc(data->str, data->len + bytes + 1);
    if (data->str == NULL) {
        fprintf(stderr, "FATAL ERROR: realloc failure!\n");
        exit(EXIT_FAILURE);
    }
    memcpy(data->str + data->len, url_contents, bytes);
    data->len += bytes;
    data->str[data->len] = '\0';
    return bytes;
}

/* Read URL into a string. The returned string must be released with free(). */
static char *read_url(Nhl *nhl, const char *url) {
    cb_data data = { NULL, 0 };
    if (nhl->params->verbose) {
        fprintf(stderr, "Receiving %s ...", url);
    }

    curl_easy_setopt(nhl->curl, CURLOPT_URL, url);
    curl_easy_setopt(nhl->curl, CURLOPT_WRITEFUNCTION, read_url_cb);
    curl_easy_setopt(nhl->curl, CURLOPT_WRITEDATA, &data);
    curl_easy_perform(nhl->curl);

    if (nhl->params->verbose) {
        fprintf(stderr, " %s\n", data.str != NULL ? "OK." : "Failed!");
    }
    return data.str;
}


/* Period updater needed by update_from_linescore(). */
static NhlStatus update_from_period(Nhl *nhl, cJSON *period, int game, int period_idx, NhlCacheMeta *meta) {
    NhlStatus status = 0;
    NhlCachePeriod p = {0};
    cJSON *away;
    cJSON *home;

    p.game = game;
    p.periodIndex = period_idx;

    p.periodType = read_leaf(period, "periodType", valuestring);
    p.startTime = read_leaf(period, "startTime", valuestring);
    p.endTime = read_leaf(period, "endTime", valuestring);
    p.num = read_leaf(period, "num", valueint);
    p.ordinalNum = read_leaf(period, "ordinalNum", valuestring);

    away = cJSON_GetObjectItemCaseSensitive(period, "away");
    p.awayGoals = read_leaf(away, "goals", valueint);
    p.awayShotsOnGoal = read_leaf(away, "shotsOnGoal", valueint);
    p.awayRinkSide = read_leaf(away, "rinkSide", valuestring);

    home = cJSON_GetObjectItemCaseSensitive(period, "home");
    p.homeGoals = read_leaf(home, "goals", valueint);
    p.homeShotsOnGoal = read_leaf(home, "shotsOnGoal", valueint);
    p.homeRinkSide = read_leaf(home, "rinkSide", valuestring);

    p.meta = meta;
    status |= nhl_cache_period_put(nhl, &p);
    return status;
}

/* Linescore updater needed by update_from_game(). */
static NhlStatus update_from_linescore(Nhl *nhl, cJSON *linescore, int game, NhlCacheMeta *meta) {
    NhlStatus status = 0;
    NhlCacheLinescore s = {0};
    cJSON *periods;
    cJSON *periods_elem;
    cJSON *shootoutInfo;
    cJSON *teams;
    cJSON *team;
    cJSON *intermissionInfo;
    cJSON *powerPlayInfo;
    int period_idx = 0;

    s.game = game;

    s.currentPeriod = read_leaf(linescore, "currentPeriod", valueint);
    s.currentPeriodOrdinal = read_leaf(linescore, "currentPeriodOrdinal", valuestring);
    s.currentPeriodTimeRemaining = read_leaf(linescore, "currentPeriodTimeRemaining", valuestring);

    periods = cJSON_GetObjectItemCaseSensitive(linescore, "periods");
    nhl_cache_periods_reset(nhl, s.game);
    cJSON_ArrayForEach(periods_elem, periods) {
        status |= update_from_period(nhl, periods_elem, s.game, period_idx, meta);
        ++period_idx;
    }

    shootoutInfo = cJSON_GetObjectItemCaseSensitive(linescore, "shootoutInfo");
    team = cJSON_GetObjectItemCaseSensitive(shootoutInfo, "away");
    s.awayShootoutScores = read_leaf(team, "scores", valueint);
    s.awayShootoutAttempts = read_leaf(team, "attempts", valueint);
    team = cJSON_GetObjectItemCaseSensitive(shootoutInfo, "home");
    s.homeShootoutScores = read_leaf(team, "scores", valueint);
    s.homeShootoutAttempts = read_leaf(team, "attempts", valueint);
    s.shootoutStartTime = read_leaf(shootoutInfo, "startTime", valuestring);

    teams = cJSON_GetObjectItemCaseSensitive(linescore, "teams");
    team = cJSON_GetObjectItemCaseSensitive(teams, "away");
    s.awayShotsOnGoal = read_leaf(team, "shotsOnGoal", valueint);
    s.awayGoaliePulled = read_leaf(team, "goaliePulled", valueint);
    s.awayNumSkaters = read_leaf(team, "numSkaters", valueint);
    s.awayPowerPlay = read_leaf(team, "powerPlay", valueint);
    team = cJSON_GetObjectItemCaseSensitive(teams, "home");
    s.homeShotsOnGoal = read_leaf(team, "shotsOnGoal", valueint);
    s.homeGoaliePulled = read_leaf(team, "goaliePulled", valueint);
    s.homeNumSkaters = read_leaf(team, "numSkaters", valueint);
    s.homePowerPlay = read_leaf(team, "powerPlay", valueint);

    s.powerPlayStrength = read_leaf(linescore, "powerPlayStrength", valuestring);
    s.hasShootout = read_leaf(linescore, "hasShootout", valueint);

    intermissionInfo = cJSON_GetObjectItemCaseSensitive(linescore, "intermissionInfo");
    s.intermissionTimeRemaining = read_leaf(intermissionInfo, "intermissionTimeRemaining", valueint);
    s.intermissionTimeElapsed = read_leaf(intermissionInfo, "intermissionTimeElapsed", valueint);
    s.intermission = read_leaf(intermissionInfo, "intermission", valueint);

    powerPlayInfo = cJSON_GetObjectItemCaseSensitive(linescore, "powerPlayInfo");
    s.powerPlaySituationRemaining = read_leaf(powerPlayInfo, "situationTimeRemaining", valueint);
    s.powerPlaySituationElapsed = read_leaf(powerPlayInfo, "situationTimeElapsed", valueint);
    s.powerPlayInSituation = read_leaf(powerPlayInfo, "inSituation", valueint);

    s.meta = meta;
    status |= nhl_cache_linescore_put(nhl, &s);
    return status;
}

/* Goal updater needed by update_from_game(). */
static NhlStatus update_from_goal(Nhl *nhl, cJSON *goal, int game, int goal_idx, NhlCacheMeta *meta) {
    NhlStatus status = 0;
    NhlCacheGoal g = {0};
    cJSON *players;
    cJSON *players_elem;
    cJSON *result;
    cJSON *strength;
    cJSON *about;
    cJSON *goals;
    cJSON *team;

    g.game = game;
    g.goalNumber = goal_idx;

    players = cJSON_GetObjectItemCaseSensitive(goal, "players");
    cJSON_ArrayForEach(players_elem, players) {
        cJSON *player = cJSON_GetObjectItemCaseSensitive(players_elem, "player");
        char *playerType = read_leaf(players_elem, "playerType", valuestring);
        if (strcmp(playerType, "Scorer") == 0) {
            g.scorer = read_leaf(player, "id", valueint);
            g.scorerSeasonTotal = read_leaf(players_elem, "seasonTotal", valueint);
        } else if (strcmp(playerType, "Assist") == 0 && g.assist1 == 0) {
            g.assist1 = read_leaf(player, "id", valueint);
            g.assist1SeasonTotal = read_leaf(players_elem, "seasonTotal", valueint);
        } else if (strcmp(playerType, "Assist") == 0) {
            g.assist2 = read_leaf(player, "id", valueint);
            g.assist2SeasonTotal = read_leaf(players_elem, "seasonTotal", valueint);
        } else if (strcmp(playerType, "Goalie") == 0) {
            g.goalie = read_leaf(player, "id", valueint);
        } else {
            /* ERROR */
        }
    }

    result = cJSON_GetObjectItemCaseSensitive(goal, "result");
    g.secondaryType = read_leaf(result, "secondaryType", valuestring);
    strength = cJSON_GetObjectItemCaseSensitive(result, "strength");
    g.strengthCode = read_leaf(strength, "code", valuestring);
    g.strengthName = read_leaf(strength, "name", valuestring);
    g.gameWinningGoal = read_leaf(result, "gameWinningGoal", valueint);
    g.emptyNet = read_leaf(result, "emptyNet", valueint);

    about = cJSON_GetObjectItemCaseSensitive(goal, "about");
    g.period = read_leaf(about, "period", valueint);
    g.periodType = read_leaf(about, "periodType", valuestring);
    g.ordinalNum = read_leaf(about, "ordinalNum", valuestring);
    g.periodTime = read_leaf(about, "periodTime", valuestring);
    g.periodTimeRemaining = read_leaf(about, "periodTimeRemaining", valuestring);
    g.dateTime = read_leaf(about, "dateTime", valuestring);
    goals = cJSON_GetObjectItemCaseSensitive(about, "goals");
    g.goalsAway = read_leaf(goals, "away", valueint);
    g.goalsHome = read_leaf(goals, "home", valueint);

    team = cJSON_GetObjectItemCaseSensitive(goal, "team");
    g.team = read_leaf(team, "id", valueint);

    g.meta = meta;
    status |= nhl_cache_goal_put(nhl, &g);

    return status;
}

/* Game updater needed by update_from_schedule(). */
static NhlStatus update_from_game(Nhl *nhl, cJSON *game, char *date, NhlCacheMeta *meta) {
    NhlStatus status = 0;
    NhlCacheGame g;
    cJSON *game_status;
    cJSON *teams;
    cJSON *awayHome;
    cJSON *leagueRecord;
    cJSON *team;
    cJSON *scoringPlays;
    cJSON *scoringPlays_elem;
    cJSON *linescore;
    int goal_idx = 0;

    g.gamePk = read_leaf(game, "gamePk", valueint);
    g.date = date;
    g.gameType = read_leaf(game, "gameType", valuestring);
    g.season = read_leaf(game, "season", valuestring);
    g.gameDate = read_leaf(game, "gameDate", valuestring);
    game_status = cJSON_GetObjectItemCaseSensitive(game, "status");
    g.statusCode = read_leaf(game_status, "statusCode", valuestring);

    teams = cJSON_GetObjectItemCaseSensitive(game, "teams");

    awayHome = cJSON_GetObjectItemCaseSensitive(teams, "away");
    team = cJSON_GetObjectItemCaseSensitive(awayHome, "team");
    g.awayTeam = read_leaf(team, "id", valueint);
    g.awayScore = read_leaf(awayHome, "score", valueint);
    leagueRecord = cJSON_GetObjectItemCaseSensitive(awayHome, "leagueRecord");
    g.awayWins = read_leaf(leagueRecord, "wins", valueint);
    g.awayLosses = read_leaf(leagueRecord, "losses", valueint);
    g.awayOt = read_leaf(leagueRecord, "ot", valueint);
    g.awayRecordType = read_leaf(leagueRecord, "type", valuestring);

    awayHome = cJSON_GetObjectItemCaseSensitive(teams, "home");
    team = cJSON_GetObjectItemCaseSensitive(awayHome, "team");
    g.homeTeam = read_leaf(team, "id", valueint);
    g.homeScore = read_leaf(awayHome, "score", valueint);
    leagueRecord = cJSON_GetObjectItemCaseSensitive(awayHome, "leagueRecord");
    g.homeWins = read_leaf(leagueRecord, "wins", valueint);
    g.homeLosses = read_leaf(leagueRecord, "losses", valueint);
    g.homeOt = read_leaf(leagueRecord, "ot", valueint);
    g.homeRecordType = read_leaf(leagueRecord, "type", valuestring);

    g.meta = meta;
    status |= nhl_cache_game_put(nhl, &g);

    scoringPlays = cJSON_GetObjectItemCaseSensitive(game, "scoringPlays");
    if (scoringPlays != NULL) {
        nhl_cache_goals_reset(nhl, g.gamePk);
        cJSON_ArrayForEach(scoringPlays_elem, scoringPlays) {
            status |= update_from_goal(nhl, scoringPlays_elem, g.gamePk, goal_idx, meta);
            ++goal_idx;
        }
    }

    linescore = cJSON_GetObjectItemCaseSensitive(game, "linescore");
    if (linescore != NULL) {
        status |= update_from_linescore(nhl, linescore, g.gamePk, meta);
    }

    return status;
}

static NhlStatus update_from_schedule(Nhl *nhl, cJSON *root, NhlCacheMeta *meta) {
    NhlStatus status = 0;
    cJSON *dates = cJSON_GetObjectItemCaseSensitive(root, "dates");
    cJSON *dates_elem;
    cJSON_ArrayForEach(dates_elem, dates) {
        NhlCacheSchedule s;
        cJSON *games;
        cJSON *games_elem;

        s.date = read_leaf(dates_elem, "date", valuestring);
        s.totalGames = read_leaf(dates_elem, "totalGames", valueint);
        s.meta = meta;
        status |= nhl_cache_schedule_put(nhl, &s);

        games = cJSON_GetObjectItemCaseSensitive(dates_elem, "games");
        cJSON_ArrayForEach(games_elem, games) {
            status |= update_from_game(nhl, games_elem, s.date, meta);
        }
    }
    return status;
}


static NhlStatus update_from_people(Nhl *nhl, cJSON *root, NhlCacheMeta *meta) {
    NhlStatus status = 0;
    cJSON *people = cJSON_GetObjectItemCaseSensitive(root, "people");
    cJSON *people_elem;
    cJSON_ArrayForEach(people_elem, people) {
        NhlCachePlayer p;
        cJSON *currentTeam;
        cJSON *primaryPosition;
        p.id = read_leaf(people_elem, "id", valueint);
        p.fullName = read_leaf(people_elem, "fullName", valuestring);
        p.firstName = read_leaf(people_elem, "firstName", valuestring);
        p.lastName = read_leaf(people_elem, "lastName", valuestring);
        p.primaryNumber = read_leaf(people_elem, "primaryNumber", valuestring);
        p.birthDate = read_leaf(people_elem, "birthDate", valuestring);
        p.birthCity = read_leaf(people_elem, "birthCity", valuestring);
        p.birthStateProvince = read_leaf(people_elem, "birthStateProvince", valuestring);
        p.birthCountry = read_leaf(people_elem, "birthCountry", valuestring);
        p.nationality = read_leaf(people_elem, "nationality", valuestring);
        p.height = read_leaf(people_elem, "height", valuestring);
        p.weight = read_leaf(people_elem, "weight", valueint);
        p.active = read_leaf(people_elem, "active", valueint);
        p.alternateCaptain = read_leaf(people_elem, "alternateCaptain", valueint);
        p.captain = read_leaf(people_elem, "captain", valueint);
        p.rookie = read_leaf(people_elem, "rookie", valueint);
        p.shootsCatches = read_leaf(people_elem, "shootsCatches", valuestring);
        p.rosterStatus = read_leaf(people_elem, "rosterStatus", valuestring);
        currentTeam = cJSON_GetObjectItemCaseSensitive(people_elem, "currentTeam");
        p.currentTeam = read_leaf(currentTeam, "id", valueint);
        primaryPosition = cJSON_GetObjectItemCaseSensitive(people_elem, "primaryPosition");
        p.primaryPosition = read_leaf(primaryPosition, "code", valuestring);
        p.meta = meta;
        status |= nhl_cache_player_put(nhl, &p);
    }
    return status;
}


static NhlStatus update_from_teams(Nhl *nhl, cJSON *root, NhlCacheMeta *meta) {
    NhlStatus status = 0;
    cJSON *teams = cJSON_GetObjectItemCaseSensitive(root, "teams");
    cJSON *teams_elem;
    cJSON_ArrayForEach(teams_elem, teams) {
        NhlCacheTeam t;
        cJSON *division;
        cJSON *conference;
        cJSON *franchise;
        t.id = read_leaf(teams_elem, "id", valueint);
        t.name = read_leaf(teams_elem, "name", valuestring);
        t.abbreviation = read_leaf(teams_elem, "abbreviation", valuestring);
        t.teamName = read_leaf(teams_elem, "teamName", valuestring);
        t.locationName = read_leaf(teams_elem, "locationName", valuestring);
        t.firstYearOfPlay = read_leaf(teams_elem, "firstYearOfPlay", valuestring);
        division = cJSON_GetObjectItemCaseSensitive(teams_elem, "division");
        t.division = read_leaf(division, "id", valueint);
        conference = cJSON_GetObjectItemCaseSensitive(teams_elem, "conference");
        t.conference = read_leaf(conference, "id", valueint);
        franchise = cJSON_GetObjectItemCaseSensitive(teams_elem, "franchise");
        t.franchise = read_leaf(franchise, "franchiseId", valueint);
        t.shortName = read_leaf(teams_elem, "shortName", valuestring);
        t.officialSiteUrl = read_leaf(teams_elem, "officialSiteUrl", valuestring);
        t.active = read_leaf(teams_elem, "active", valueint);
        t.meta = meta;
        status |= nhl_cache_team_put(nhl, &t);
    }
    return status;
}


static NhlStatus update_from_franchises(Nhl *nhl, cJSON *root, NhlCacheMeta *meta) {
    NhlStatus status = 0;
    cJSON *franchises = cJSON_GetObjectItemCaseSensitive(root, "franchises");
    cJSON *franchises_elem;
    cJSON_ArrayForEach(franchises_elem, franchises) {
        NhlCacheFranchise f;
        f.franchiseId = read_leaf(franchises_elem, "franchiseId", valueint);
        f.firstSeasonId = read_leaf(franchises_elem, "firstSeasonId", valueint);
        f.lastSeasonId = read_leaf(franchises_elem, "lastSeasonId", valueint);
        f.mostRecentTeamId = read_leaf(franchises_elem, "mostRecentTeamId", valueint);
        f.teamName = read_leaf(franchises_elem, "teamName", valuestring);
        f.locationName = read_leaf(franchises_elem, "locationName", valuestring);
        f.meta = meta;
        status |= nhl_cache_franchise_put(nhl, &f);
    }
    return status;
}


static NhlStatus update_from_divisions(Nhl *nhl, cJSON *root, NhlCacheMeta *meta) {
    NhlStatus status = 0;
    cJSON *divisions = cJSON_GetObjectItemCaseSensitive(root, "divisions");
    cJSON *divisions_elem;
    cJSON_ArrayForEach(divisions_elem, divisions) {
        NhlCacheDivision d;
        cJSON *conference;
        d.id = read_leaf(divisions_elem, "id", valueint);
        d.name = read_leaf(divisions_elem, "name", valuestring);
        d.nameShort = read_leaf(divisions_elem, "nameShort", valuestring);
        d.abbreviation = read_leaf(divisions_elem, "abbreviation", valuestring);
        conference = cJSON_GetObjectItemCaseSensitive(divisions_elem, "conference");
        d.conference = read_leaf(conference, "id", valueint);
        d.active = read_leaf(divisions_elem, "active", valueint);
        d.meta = meta;
        status |= nhl_cache_division_put(nhl, &d);
    }
    return status;
}


static NhlStatus update_from_conferences(Nhl *nhl, cJSON *root, NhlCacheMeta *meta) {
    NhlStatus status = 0;
    cJSON *conferences = cJSON_GetObjectItemCaseSensitive(root, "conferences");
    cJSON *conferences_elem;
    cJSON_ArrayForEach(conferences_elem, conferences) {
        NhlCacheConference c;
        c.id = read_leaf(conferences_elem, "id", valueint);
        c.name = read_leaf(conferences_elem, "name", valuestring);
        c.abbreviation = read_leaf(conferences_elem, "abbreviation", valuestring);
        c.shortName = read_leaf(conferences_elem, "shortName", valuestring);
        c.active = read_leaf(conferences_elem, "active", valueint);
        c.meta = meta;
        status |= nhl_cache_conference_put(nhl, &c);
    }
    return status;
}


static NhlStatus update_from_game_statuses(Nhl *nhl, cJSON *root, NhlCacheMeta *meta) {
    NhlStatus status = 0;
    cJSON *gamest_elem;
    cJSON_ArrayForEach(gamest_elem, root) {
        NhlCacheGameStatus g;
        g.code = read_leaf(gamest_elem, "code", valuestring);
        g.abstractGameState = read_leaf(gamest_elem, "abstractGameState", valuestring);
        g.detailedState = read_leaf(gamest_elem, "detailedState", valuestring);
        g.startTimeTBD = read_leaf(gamest_elem, "startTimeTBD", valueint);
        g.meta = meta;
        status |= nhl_cache_game_status_put(nhl, &g);
    }
    return status;
}


static NhlStatus update_from_game_types(Nhl *nhl, cJSON *root, NhlCacheMeta *meta) {
    NhlStatus status = 0;
    cJSON *gametyp_elem;
    cJSON_ArrayForEach(gametyp_elem, root) {
        NhlCacheGameType t;
        t.id = read_leaf(gametyp_elem, "id", valuestring);
        t.description = read_leaf(gametyp_elem, "description", valuestring);
        t.postseason = read_leaf(gametyp_elem, "postseason", valueint);
        t.meta = meta;
        status |= nhl_cache_game_type_put(nhl, &t);
    }
    return status;
}


static NhlStatus update_from_positions(Nhl *nhl, cJSON *root, NhlCacheMeta *meta) {
    NhlStatus status = 0;
    cJSON *position_elem;
    cJSON_ArrayForEach(position_elem, root) {
        NhlCachePosition p;
        p.abbrev = read_leaf(position_elem, "abbrev", valuestring);
        p.code = read_leaf(position_elem, "code", valuestring);
        p.fullName = read_leaf(position_elem, "fullName", valuestring);
        p.type = read_leaf(position_elem, "type", valuestring);
        p.meta = meta;
        status |= nhl_cache_position_put(nhl, &p);
    }
    return status;
}


static NhlStatus update_from_roster_statuses(Nhl *nhl, cJSON *root, NhlCacheMeta *meta) {
    NhlStatus status = 0;
    cJSON *rosters_elem;
    cJSON_ArrayForEach(rosters_elem, root) {
        NhlCacheRosterStatus r;
        r.code = read_leaf(rosters_elem, "code", valuestring);
        r.description = read_leaf(rosters_elem, "description", valuestring);
        r.meta = meta;
        status |= nhl_cache_roster_status_put(nhl, &r);
    }
    return status;
}


NhlStatus nhl_update_from_url(Nhl *nhl, const char *url, NhlUpdateContentType type) {
    if (nhl->params->offline || url == NULL || nhl_list_contains(nhl->visited_urls, url)) {
        if (nhl->params->verbose)
            fprintf(stderr, "Skipping %s\n", url != NULL ? url : "(null)");
        return NHL_DOWNLOAD_SKIPPED;

    } else {
        char *json = read_url(nhl, url);
        nhl->visited_urls = nhl_list_prepend(nhl->visited_urls, url);
        if (json == NULL) {
            return NHL_DOWNLOAD_ERROR;

        } else {
            NhlStatus status = NHL_DOWNLOAD_OK;
            const char *timestamp = nhl_cache_current_time(nhl);
            NhlCacheMeta meta = { NULL, NULL, 0 };
            cJSON *root = cJSON_Parse(json);

            meta.source = nhl_copy_string(url);
            meta.timestamp = nhl_copy_string(timestamp);

            switch (type) {
                case NHL_CONTENT_SCHEDULE:
                    status = update_from_schedule(nhl, root, &meta);
                    break;
                case NHL_CONTENT_PEOPLE:
                    status |= update_from_people(nhl, root, &meta);
                    break;
                case NHL_CONTENT_TEAMS:
                    status |= update_from_teams(nhl, root, &meta);
                    break;
                case NHL_CONTENT_FRANCHISES:
                    status |= update_from_franchises(nhl, root, &meta);
                    break;
                case NHL_CONTENT_DIVISIONS:
                    status |= update_from_divisions(nhl, root, &meta);
                    break;
                case NHL_CONTENT_CONFERENCES:
                    status |= update_from_conferences(nhl, root, &meta);
                    break;
                case NHL_CONTENT_GAME_STATUSES:
                    status |= update_from_game_statuses(nhl, root, &meta);
                    break;
                case NHL_CONTENT_GAME_TYPES:
                    status |= update_from_game_types(nhl, root, &meta);
                    break;
                case NHL_CONTENT_POSITIONS:
                    status |= update_from_positions(nhl, root, &meta);
                    break;
                case NHL_CONTENT_ROSTER_STATUSES:
                    status |= update_from_roster_statuses(nhl, root, &meta);
                    break;
                default:
                    status |= NHL_INVALID_REQUEST;
                    break;
            }

            cJSON_Delete(root);
            free(meta.timestamp);
            free(meta.source);
            free(json);
            return status;
        }
    }
}
