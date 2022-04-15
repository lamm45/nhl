#define _GNU_SOURCE

#include "display.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nhl/nhl.h>

#include "colors.h"


/* Print specified number of whitespace characters. */
static void print_blank(int spaces) {
    while (spaces--)
        putc(' ', stdout);
}

/* Count printable length of a (unicode) string. See, e.g., https://stackoverflow.com/a/32936928 */
static size_t printlen(const char *str) {
    size_t len = 0;
    while (*str) {
        len += (*str++ & 0xC0) != 0x80;
    }
    return len;
}

/* Convert UTC time to local time.
 * TODO: This does not account for daylight saving changes between now and the printed time. */
static NhlTime utc_to_local(const NhlTime *utc_time, double utc_offset) {
    int utc_seconds = utc_time->secs + 60*utc_time->mins + 60*60*utc_time->hours;
    int local_seconds = utc_seconds + 60*60*utc_offset; // TODO: check rounding for non-integers
    if (local_seconds < 0)
        local_seconds += 60*60*24;

    NhlTime local_time = {
        .hours = (local_seconds / 60 / 60) % 24,
        .mins = (local_seconds / 60) % 60,
        .secs = local_seconds % 60
    };

    return local_time;
}

/* Returns short_srt is a subset at the beginning of long_str (case insensitive). */
static inline bool begins_with(const char *short_str, const char *long_str) {
    return strcasestr(long_str, short_str) == long_str;
}

/* Returns true if the given team is included in the list of team descriptors. */
static bool team_in_list(const NhlTeam *team, char **const list, int list_len) {
    for (int i = 0; i != list_len; ++i) {
        if (begins_with(list[i], team->abbreviation))
            return true;
        if (begins_with(list[i], team->location_name))
            return true;
        if (begins_with(list[i], team->team_name))
            return true;
        if (begins_with(list[i], team->short_name))
            return true;
        if (team->division != NULL && begins_with(list[i], team->division->name))
            return true;
        if (team->conference != NULL && begins_with(list[i], team->conference->name))
            return true;
    }
    return false;
}

/* 0 = no display, 1 = ordinary display, 2 = highlighted display */
static int team_disp_mode(const NhlTeam *team, const DisplayOptions *opts) {
    if (opts->num_teams > 0 && !team_in_list(team, opts->teams, opts->num_teams))
        return 0;

    if (opts->num_highlight > 0 && team_in_list(team, opts->highlight, opts->num_highlight))
        return 2;

    return 1;
}

/* Print string and highlight it if necessary. */
static inline void print_team(const char *team, int mode) {
    if (mode == 2) {
        printf(FG_BYELLOW "%s" COLOR_RESET, team);
    } else {
        printf("%s", team);
    }
}


/* Print header for the default display mode. */
static void display_normal_header(const NhlGame *game, double utc_offset) {
    const char *status = game->status->abstract_state;
    const char *detail = game->status->detailed_state;
    if (strcasecmp(status, "Preview") == 0) {
        if (strcasecmp(detail, "Scheduled") == 0 || strcasecmp(detail, "Pre-Game") == 0) {
            NhlTime local_time = utc_to_local(&game->start_time.time, utc_offset);
            printf("%d:%02d %s\n", (local_time.hours % 12) > 0 ? (local_time.hours % 12) : 12,
                local_time.mins, local_time.hours < 12 ? "AM" : "PM");
        } else if (strcasecmp(detail, "Postponed") == 0) {
            printf("PPD\n");
        } else {
            printf("XX:XX\n");
        }
    } else if (strcasecmp(status, "Live") == 0) {
        NhlTime remaining = game->details->current_period_remaining;
        printf("%d:%02d | %s\n", remaining.mins, remaining.secs, game->details->current_period_name);
    } else {
        if (game->details->shootout) {
            printf("Final / SO\n");
        } else if (game->details->current_period_number > 3) {
            printf("Final / OT\n");
        } else {
            printf("Final\n");
        }
    }
}

int display_normal(const NhlSchedule *schedule, const DisplayOptions *opts) {
    char *date_str = nhl_date_to_string(&schedule->date);
    printf("NHL %s\n", date_str);

    int num_printed = 0;
    for (int i = 0; i != schedule->num_games; ++i) {
        NhlGame *game = schedule->games[i];
        int away_mode = team_disp_mode(game->away, opts);
        int home_mode = team_disp_mode(game->home, opts);
        if (away_mode == 0 && home_mode == 0)
            continue;

        printf("\n");
        display_normal_header(game, opts->utc_offset);

        const char *status = game->status->abstract_state;
        if (strcasecmp(status, "Live") == 0 || strcasecmp(status, "Final") == 0) {
            // Determine amount of space needed for text alignment
            size_t away_len = printlen(game->away->team_name);
            size_t home_len = printlen(game->home->team_name);
            size_t max_len = away_len > home_len ? away_len : home_len;

            // Away line
            print_team(game->away->team_name, away_mode);
            printf("  ");
            print_blank(max_len - away_len);
            printf("%d (%d SOG)\n", game->away_score, game->details->away_shots);

            // Home line
            print_team(game->home->team_name, home_mode);
            printf("  ");
            print_blank(max_len - home_len);
            printf("%d (%d SOG)\n", game->home_score, game->details->home_shots);
        } else {
            print_team(game->away->team_name, away_mode);
            printf("\n");
            print_team(game->home->team_name, home_mode);
            printf("\n");
        }

        ++num_printed;
    }

    free(date_str);
    return num_printed;
}


int display_compact(const NhlSchedule *schedule, const DisplayOptions *opts) {
    (void) opts;
    int num_printed = 0;
    for (int i = 0; i != schedule->num_games; ++i) {
        const NhlGame *game = schedule->games[i];
        int away_mode = team_disp_mode(game->away, opts);
        int home_mode = team_disp_mode(game->home, opts);
        if (away_mode == 0 && home_mode == 0)
            continue;

        print_team(game->away->abbreviation, away_mode);
        printf("-");
        print_team(game->home->abbreviation, home_mode);
        printf(":");
        if (strcasecmp(game->status->abstract_state, "Preview") == 0) {
            printf("x-x\n");
        } else if (strcasecmp(game->status->abstract_state, "Live") == 0) {
            printf("%d-%d...\n", game->away_score, game->home_score);
        } else {
            printf("%d-%d", game->away_score, game->home_score);
            if (game->details->shootout) {
                printf("/SO");
            } else if (game->details->current_period_number > 3) {
                printf("/OT");
            }
            printf("\n");
        }
        ++num_printed;
    }
    return num_printed;
}


static void print_tekstitv_header(const NhlDate *date) {
    printf(" " BG_BLUE FG_BWHITE "  NHL-J\u00c4\u00c4KIEKKO          " BG_GREEN FG_BLUE "  %02d.%02d.      " COLOR_RESET "\n",
        date->day, date->month);
    // TODO: add year (if not current)
}

static void print_tekstitv_time(const NhlTime *utc_time, double utc_offset) {
    NhlTime local_time = utc_to_local(utc_time, utc_offset);
    printf("%02d.%02d", local_time.hours, local_time.mins);
}

static void sprint_tekstitv_time(char *str, const NhlTime *utc_time, double utc_offset) {
    NhlTime local_time = utc_to_local(utc_time, utc_offset);
    sprintf(str, "%02d.%02d", local_time.hours, local_time.mins);
}

static void print_tekstitv_game_header(const NhlGame *game, double utc_offset) {
    if (strcasecmp(game->status->abstract_state, "live") == 0) {
        printf(" "FG_BCYAN);
        print_tekstitv_time(&game->start_time.time, utc_offset);
        printf(" (");

        // always split scores into three periods
        for (int k = 0; k != 3; ++k) {
            if (k < game->details->current_period_number)
                printf("%d-%d", game->details->periods[k].home_goals, game->details->periods[k].away_goals);
            else
                printf("x-x");
            if (k < 2)
                printf(",");
        }

        printf(")" COLOR_RESET "\n");
    }
}

static void print_tekstitv_game_teams(const NhlGame *game, int home_mode, int away_mode, double utc_offset) {
    char *home_name = game->home->short_name;
    printf(" ");
    print_team(home_name, home_mode);
    print_blank(15 - printlen(home_name));

    // 21
    char *away_name = game->away->short_name;
    printf("- ");
    print_team(away_name, away_mode);

    static const size_t result_sz = 16;
    char result[result_sz];

    if (strcasecmp(game->status->abstract_state, "Preview") == 0) {
        if (strcasecmp(game->status->detailed_state, "Scheduled") == 0 ||
            strcasecmp(game->status->detailed_state, "Pre-Game") == 0)
            sprint_tekstitv_time(result, &game->start_time.time, utc_offset);
        else if (strcasecmp(game->status->detailed_state, "Postponed") == 0)
            strcpy(result, "siir.");
        else
            strcpy(result, "xx.xx");
    } else {
        if (strcasecmp(game->status->abstract_state, "Live") == 0)
            printf(FG_BCYAN);
        else
            printf(FG_BGREEN);

        char *prefix;
        if (game->details->shootout)
            prefix = "vl";
        else if (game->details->current_period_number > 3)
            prefix = "ja";
        else
            prefix = "";

        sprintf(result, "%s %d-%d", prefix, game->home_score, game->away_score);
    }

    print_blank(21 - printlen(away_name) - printlen(result));
    printf("%s" COLOR_RESET "\n", result);
}

static bool tekstitv_player_emph(const NhlPlayer *player) {
    static const char target_nationality[] = "FIN";
    return player != NULL && player->nationality != NULL && strcasecmp(player->nationality, target_nationality) == 0;
}

/* If the goal has one assist to be highlighted, the last name is assigned to 'assist1'. If there
 * are two highlighted assists, both 'assist1' and 'assist2' are used. Otherwise, the arguments are
 * not modified. Returns one if at least one assist is highlighted, zero otherwise. */
static int tekstitv_extract_assist(const NhlGoal *goal, char **assist1, char **assist2) {
    int has_assist = 0;
    if (goal->assist1 != NULL && tekstitv_player_emph(goal->assist1)) {
        *assist1 = goal->assist1->last_name;
        if (goal->assist2 != NULL && tekstitv_player_emph(goal->assist2))
            *assist2 = goal->assist2->last_name;
        has_assist = 1;
    } else if (goal->assist2 != NULL && tekstitv_player_emph(goal->assist2)) {
        *assist1 = goal->assist2->last_name;
        has_assist = 1;
    }
    return has_assist;
}

/* Goal */
static int print_tekstitv_goal(const NhlGoal *goal, char **assist1, char **assist2) {
    int completed = 1;
    int mins = 20*(goal->time->period-1) + goal->time->time.mins;
    if (strcasecmp(goal->time->period_ordinal, "SO") == 0)
        mins = 65;
    int total_len = 1;
    if (mins >= 10)
        ++total_len;
    if (mins >= 100)
        ++total_len;

    printf(" " FG_BCYAN);
    if (goal->time->period > 3)
        printf(FG_BMAGENTA);
    else if (goal->scorer != NULL && tekstitv_player_emph(goal->scorer))
        printf(FG_BGREEN);
    if (goal->scorer != NULL) {
        printf("%s", goal->scorer->last_name);
        total_len += printlen(goal->scorer->last_name);
    }

    completed -= tekstitv_extract_assist(goal, assist1, assist2);

    print_blank(16 - total_len);
    printf("%d" COLOR_RESET, mins);
    return completed;
}

/* At least one input argument must be non-NULL. Arguments that are printed out are converted to
 * NULL. Returns one if both arguments are NULL after the execution, otherwise returns zero.
 * TODO: Single-line multi-assist.*/
static int print_tekstitv_assist(char **assist1, char **assist2) {
    int completed = 0;
    int total_len = 0;

    printf(FG_BGREEN " ");

    if (*assist1 != NULL && *assist2 == NULL) {
        // Sole assist
        printf("(%s)", *assist1);
        total_len += 2 + printlen(*assist1);
        *assist1 = NULL;
        completed = 1;
    } else if (*assist1 != NULL && *assist2 != NULL) {
        // First out of two assists
        printf("(%s,", *assist1);
        total_len += 2 + printlen(*assist1);
        *assist1 = NULL;
    } else if (*assist1 == NULL && *assist2 != NULL) {
        // Second out of two assists
        printf("%s)", *assist2);
        total_len += 1 + printlen(*assist2);
        *assist2 = NULL;
        completed = 1;
    } else {
        // Should not reach
        printf("ERROR\n");
        return 1;
    }
    print_blank(16 - total_len);
    printf(COLOR_RESET);

    return completed;
}

static void print_tekstitv_goals(const NhlGame *game) {
    NhlGoal *goals = game->goals;
    int num_goals = game->num_goals;

    int home_idx = 0;
    int away_idx = 0;

    char *home_assist1 = NULL;
    char *home_assist2 = NULL;
    char *away_assist1 = NULL;
    char *away_assist2 = NULL;

    int num_printed = 0;
    while (num_printed < num_goals) {
        // Single line for the home team
        if (home_assist1 != NULL || home_assist2 != NULL) {
            num_printed += print_tekstitv_assist(&home_assist1, &home_assist2);
        } else {
            // Move to the next home goal, or one past the end
            while (home_idx < num_goals && goals[home_idx].scoring_team->unique_id != game->home->unique_id)
                ++home_idx;
            if (home_idx < num_goals) {
                num_printed += print_tekstitv_goal(&goals[home_idx], &home_assist1, &home_assist2);
                ++home_idx;
            } else {
                print_blank(17);
            }
        }

        // Single line for the away team
        if (away_assist1 != NULL || away_assist2 != NULL) {
            num_printed += print_tekstitv_assist(&away_assist1, &away_assist2);
        } else {
            // Move to the next away goal, or one past the end
            while (away_idx < num_goals && goals[away_idx].scoring_team->unique_id != game->away->unique_id)
                ++away_idx;
            if (away_idx < num_goals) {
                num_printed += print_tekstitv_goal(&goals[away_idx], &away_assist1, &away_assist2);
                ++away_idx;
            } else {
                // Do nothing
            }
        }

        printf("\n");
    }
}

int display_tekstitv(const NhlSchedule *schedule, const DisplayOptions *opts) {
    int num_printed = 0;
    print_tekstitv_header(&schedule->date);
    for (int i = 0; i != schedule->num_games; ++i) {
        NhlGame *game = schedule->games[i];
        int home_mode = team_disp_mode(game->home, opts);
        int away_mode = team_disp_mode(game->away, opts);
        if (home_mode == 0 && away_mode == 0)
            continue;
        printf("\n");
        print_tekstitv_game_header(game, opts->utc_offset);
        print_tekstitv_game_teams(game, home_mode, away_mode, opts->utc_offset);
        print_tekstitv_goals(game);
        ++num_printed;
    }
    return num_printed;
}

int display(const NhlSchedule *schedule, const DisplayOptions *opts) {
    if (schedule == NULL)
        return 0;

    switch (opts->style) {
        case STYLE_DEFAULT:
            return display_normal(schedule, opts);
        case STYLE_COMPACT:
            return display_compact(schedule, opts);
        case STYLE_TEKSTITV:
            return display_tekstitv(schedule, opts);
    }
    return 0;
}
