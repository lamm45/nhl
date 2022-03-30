#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <nhl/nhl.h>

#include "days.h"
#include "display.h"
#include "config.h"
#include "uargs.h"


/* Create folders recursively as needed. Returns zero if success.
 */
static int ensure_dirs(const char *path) {
    int status = 0;
    char *mpath = strdup(path);
    for (char *p = mpath; *p != '\0'; ++p) {
        if (*p == '/') {
            *p = '\0';
            status |= mkdir(mpath, 0775);
            *p = '/';
        }
    }
    status |= mkdir(mpath, 0775);

    free(mpath);
    return status;
}

/* Determine location of the (possibly non-existing) cache file.
 * Returns dynamically allocated string, or NULL if an error occurs.
 */
static char *resolve_cache_file(const char *suggestion) {
    const char *env_cachedir;
    const char *env_homedir;
    char *cache_file = NULL;

    if (suggestion) {
        cache_file = malloc(strlen(suggestion) + 1);
        strcpy(cache_file, suggestion);
    } else if ((env_cachedir = getenv(ENV_CACHEDIR)) && *env_cachedir) {
        cache_file = malloc(strlen(env_cachedir) + 1 + strlen(DEFAULT_CACHEFILE) + 1);
        strcpy(cache_file, env_cachedir);
        strcat(cache_file, "/" DEFAULT_CACHEFILE);
    } else if ((env_homedir = getenv(ENV_HOMEDIR)) && *env_homedir) {
        cache_file = malloc(strlen(env_homedir) + 1 + strlen(DEFAULT_CACHEDIR) + 1 +
            strlen(DEFAULT_CACHEFILE) + 1);
        strcpy(cache_file, env_homedir);
        strcat(cache_file, "/" DEFAULT_CACHEDIR "/" DEFAULT_CACHEFILE);
    }

    if (cache_file != NULL) {
        char *cache_dir = strdup(cache_file);
        ensure_dirs(dirname(cache_dir));
        free(cache_dir);
    }

    return cache_file;
}


int main(int argc, char **argv) {
    // Read command-line arguments
    UserArgs uargs = {0};
    parse_args(argc, argv, &uargs);
    if (uargs.verbose >= 2)
        print_args(&uargs);

    // Interpret dates
    int num_dates = uargs.num_days > 0 ? uargs.num_days : 1;
    Date *dates = malloc(num_dates * sizeof(Date));
    if (uargs.num_days == 0) {
        dates[0] = default_date();
        if (uargs.verbose >= 2)
            printf("Default date: %d-%02d-%02d\n", dates[0].year, dates[0].month, dates[0].day);
    } else {
        for (int i = 0; i != uargs.num_days; ++i) {
            DateStatus status = date_from_str(uargs.days[i], &dates[i]);
            if (status != DATE_OK)
                fprintf(stderr, "Unable to interpret \"%s\".\n", uargs.days[i]);
            else if (uargs.verbose >= 2)
                printf("Day \"%s\" interpreted as: %d-%02d-%02d\n",
                    uargs.days[i], dates[i].year, dates[i].month, dates[i].day);
        }
    }

    // Determine time zone
    double tzone = uargs.timezone_set ? uargs.timezone : local_timezone();
    if (uargs.verbose >= 1)
        printf("Time zone: %+g\n", tzone);

    // Determine cache file path (or NULL if cache is disabled)
    char *cache_file = NULL;
    if (!(uargs.update && uargs.readonly)) {
        cache_file = resolve_cache_file(uargs.cache_file);
        if (cache_file == NULL) {
            fprintf(stderr, "WARNING: Disabling cache, unable to determine suitable path.\n");
        } else if (uargs.verbose >= 1) {
            printf("Cache file: %s\n", cache_file);
        }
    }

    // Initialize library
    NhlInitParams params;
    nhl_default_params(&params);
    params.cache_file = cache_file;
    params.verbose = uargs.verbose;
    params.offline = uargs.offline;
    if (uargs.update) {
        params.schedule_max_age = 0;
        params.game_live_max_age = 0;
        params.game_final_max_age = 0;
        params.team_max_age = 0;
        params.player_max_age = 0;
        params.league_max_age = 0;
        params.meta_max_age = 0;
    }
    Nhl *nhl = nhl_init(&params);

    // Get and show results
    NhlQueryLevel level = NHL_QUERY_BASIC | NHL_QUERY_GAMEDETAILS;
    DisplayStyle style = STYLE_DEFAULT;
    if (uargs.compact) {
        level = NHL_QUERY_BASIC | NHL_QUERY_GAMEDETAILS;
        style = STYLE_COMPACT;
    } else if (uargs.tekstitv) {
        level = NHL_QUERY_FULL;
        style = STYLE_TEKSTITV;
    }
    DisplayOptions opts = {
        .style = style,
        .teams = uargs.teams,
        .num_teams = uargs.num_teams,
        .highlight = uargs.highlight,
        .num_highlight = uargs.num_highlight,
        .utc_offset = tzone,
    };
    for (int i = 0; i != num_dates; ++i) {
        if (dates[i].day <= 0)
            continue;
        NhlDate date = {
            .year = dates[i].year,
            .month = dates[i].month,
            .day = dates[i].day
        };
        NhlSchedule *schedule;
        nhl_schedule_get(nhl, &date, level, &schedule); // TODO: Check return value
        display(schedule, &opts);
        if (i < num_dates-1 && opts.style != STYLE_COMPACT)
            printf("\n");
        nhl_schedule_unget(nhl, schedule); // TODO: This is inefficient if there are many dates
    }

    // Clean-up
    nhl_close(nhl);
    free(cache_file);
    free(dates);
    reset_args(&uargs);

    return 0;
}
