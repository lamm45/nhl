#include "uargs.h"

#include <argp.h>
#include <stdlib.h>
#include <string.h>

#include <nhl/version.h>

#include "config.h"

// Documentation for argp
static const char argdoc[] = "[DAY...]";
static const char doc[] = "Display scores from the National Hockey League (NHL).\v"
    "DAY can be `yesterday`, `today` or `tomorrow`, any weekday such as `monday`, "
    "or a date in the format DD, MM-DD or YYYY-MM-DD. For weekdays and dates without "
    "a year, the nearest compatible day is chosen. If no DAY is given, either `yesterday` "
    "or `today` is assumed, based on heuristics.\n\n"
    "TEAMS is a comma-separated list of team, division and conference names. A team name "
    "can be a location (e.g., `\"Los Angeles\"`), an official team name (e.g., `Oilers`), "
    "or an official abbreviation (e.g., `NYI`).\n\n"
    "Arguments are case-insensitive. Abbreviations are accepted "
    "(e.g., `Mon` for `monday` or `metro` for `Metropolitan`).\n\n"
    "The default location of the cache file is <prefix>/" DEFAULT_CACHEFILE ", where "
    "<prefix>=$" ENV_CACHEDIR " if the environment variable " ENV_CACHEDIR " is set, "
    "and <prefix>=$" ENV_HOMEDIR "/" DEFAULT_CACHEDIR " otherwise.";

// Available command-line flags
enum keys {
    KEY_TEAMS = 't',
    KEY_HIGHLIGHT = 'h',
    // KEY_LONGLIST = 'l',
    KEY_SHORTLIST = 's',
    KEY_OFFLINE = 'o',
    KEY_UPDATE = 'u',
    KEY_VERBOSE = 'v',

    // long-only options
    KEY_TEKSTITV = 1000,
    KEY_TIMEZONE,
    KEY_CACHEFILE,
    // KEY_READONLY,
};

static const struct argp_option options[] = {
    // name, key, arg, flags, doc, group
    {0, 0, 0, 0, "Game selection:", 0},
    {"teams", KEY_TEAMS, "TEAMS", 0, "Show only games played by TEAMS", 0},
    {0, 0, 0, 0, "Output formatting:", 0},
    {"highlight", KEY_HIGHLIGHT, "TEAMS", 0, "Highlight TEAMS", 0},
    {"short", KEY_SHORTLIST, 0, 0, "Use compact layout", 0},
    // {"long", KEY_LONGLIST, 0, 0, "Use detailed layout", 0},
    {"tekstitv", KEY_TEKSTITV, 0, 0, "Enable Teksti-TV mode", 0},
    {"time-zone", KEY_TIMEZONE, "HOUR", 0, "Show times using non-local time zone", 0},
    {0, 0, 0, 0, "Cache settings:", 0},
    {"cache-file", KEY_CACHEFILE, "FILE", 0, "Use non-default cache file", 0},
    {"offline", KEY_OFFLINE, 0, 0, "Do not connect to the Internet", 0},
    // {"read-only", KEY_READONLY, 0, 0, "Do not write to cache", 0},
    {"update", KEY_UPDATE, 0, 0, "Do not read from cache", 0},
    {0, 0, 0, 0, "Help and diagnostics:", -1},
    {"verbose", KEY_VERBOSE, 0, 0, "Increase verbosity level for debugging", 0},
    {0}
};

/* Parse comma-separated strings, append the strings to a string array `items`, and
 * update the array size `num_items`.
 */
static void parse_comma_str(const char *comma_str, char ***items, int *num_items);

// Parser function for argp
static error_t parse(int key, char *arg, struct argp_state *state) {
    UserArgs *uargs = state->input;
    switch (key) {
        case KEY_TEAMS:
            parse_comma_str(arg, &uargs->teams, &uargs->num_teams);
            break;
        case KEY_HIGHLIGHT:
            parse_comma_str(arg, &uargs->highlight, &uargs->num_highlight);
            break;
        // case KEY_LONGLIST:
        //     uargs->details = true;
        //     break;
        case KEY_SHORTLIST:
            uargs->compact = true;
            uargs->tekstitv = false;
            break;
        case KEY_TEKSTITV:
            uargs->compact = false;
            uargs->tekstitv = true;
            break;
        case KEY_TIMEZONE:
            uargs->timezone_set = true;
            uargs->timezone = strtod(arg, NULL);
            break;
        case KEY_CACHEFILE:
            uargs->cache_file = arg;
            break;
        // case KEY_READONLY:
        //     uargs->readonly = true;
        //     break;
        case KEY_OFFLINE:
            uargs->offline = true;
            break;
        case KEY_UPDATE:
            uargs->update = true;
            break;
        case KEY_VERBOSE:
            uargs->verbose++;
            break;
        case ARGP_KEY_ARGS:
            uargs->num_days = state->argc - state->next;
            uargs->days = state->argv + state->next;
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

void print_version(FILE *stream, struct argp_state *state) {
    (void) state;
    fprintf(stream, PROGRAM_NAME " " PROGRAM_VERSION " (libnhl %s)\n", nhl_version_string());
}

int parse_args(int argc, char **argv, UserArgs *uargs) {
    argp_program_version_hook = print_version;
    argp_program_bug_address = "https://github.com/lamm45/nhl/issues ";
    static struct argp argp = { options, parse, argdoc, doc, 0, 0, 0 };
    return argp_parse(&argp, argc, argv, 0, NULL, uargs);
}


UserArgs *reset_args(UserArgs *args) {
    if (args != NULL) {
        if (args->teams) {
            free(args->teams[0]);
            free(args->teams);
        }
        if (args->highlight) {
            free(args->highlight[0]);
            free(args->highlight);
        }
    }
    return args;
}


static void print_arg_list(const char *name, char **list, int n) {
    if (n == 0) {
        printf("  No %ss.\n", name);
    } else {
        printf("  %d %s%s:", n, name, n == 1 ? "" : "s");
        for (int i=0; i!=n; ++i) {
            printf(" \"%s\"%s", list[i], i<n-1 ? "," : "\n");
        }
    }
}

void print_args(UserArgs *args) {
    printf("User options:\n");
    print_arg_list("day", args->days, args->num_days);
    print_arg_list("team", args->teams, args->num_teams);
    print_arg_list("highlighted team", args->highlight, args->num_highlight);
    printf("  Short mode: %s\n", args->compact ? "on" : "off");
    printf("  Teksti-TV mode: %s\n", args->tekstitv ? "on" : "off");
    if (args->timezone_set)
        printf("  Time zone: UTC%+g\n", args->timezone);
    else
        printf("  Time zone: (default)\n");
    printf("  Cache file: %s\n", args->cache_file ? args->cache_file : "(default)");
    printf("  Offline mode: %s\n", args->offline ? "on" : "off");
    // printf("  Read-only cache: %s\n", args->readonly ? "on" : "off");
    printf("  Write-only cache: %s\n", args->update ? "on" : "off");
    printf("  Verbosity: %d\n", args->verbose);
}


/* Split comma-separated strings into an array of strings.
 * The number of strings found is written into `num_strings`. If the number is zero, NULL is
 * returned. Otherwise, the return value is a pointer to a newly allocated array of pointers
 * into a newly allocated contiguous char array containing the strings. The char array should
 * be freed by freeing the first pointer, and the pointer array should be freed normally.
 */
static char **split_comma_str(const char *comma_separated_strings, int *num_strings) {
    // Discard preceding commas
    while (*comma_separated_strings == ',')
        ++comma_separated_strings;

    // Nothing is allocated if nothing is found
    *num_strings = 0;
    if (*comma_separated_strings == '\0') {
        return NULL;
    }

    // Copy and parse input data, and create pointer array
    char *data = strdup(comma_separated_strings);
    int alloc_size = 8;
    char **str_pointers = malloc(alloc_size * sizeof(char*));
    for (char *str = strtok(data, ","); str; str = strtok(NULL, ","), ++*num_strings) {
        if (alloc_size <= *num_strings) {
            alloc_size *= 2;
            str_pointers = realloc(str_pointers, alloc_size * sizeof(char*));
        }
        str_pointers[*num_strings] = str;
    }
    str_pointers = realloc(str_pointers, *num_strings * sizeof(char*));
    return str_pointers;
}


/* Number of bytes in a contiguous array of n strings.
 */
static size_t str_array_size(char *const *arr, int n) {
    if (n == 0)
        return 1;
    return (arr[n-1] - arr[0]) + strlen(arr[n-1]) + 1;
}


/* Concatenate two contiguous string arrays.
 * Data and `nfrom` pointers from `from` are appended to `to` which initially contains
 * `nto` pointers and may be re-allocated. The possibly newly allocated `to` is
 * returned. In all cases, the data pointed to by pointers in `to` is re-allocated.
 */
static char **concat_str_arrays(char **to, char *const *from, int nto, int nfrom) {
    // Space for pointers to strings
    to = realloc(to, (nto+nfrom) * sizeof(char*));

    // Space for the string data
    size_t size_to = str_array_size(to, nto);
    size_t size_from = str_array_size(from, nfrom);
    char *data = malloc(size_to + size_from);
    memcpy(data, to[0], size_to);
    memcpy(data + size_to, from[0], size_from);

    // Adjust pointers to point to newly allocated data
    for (int i=1; i<nto; ++i) {
        size_t offset = to[i] - to[0];
        to[i] = data + offset;
    }
    free(to[0]);
    to[0] = data;
    for (int i=0; i!=nfrom; ++i) {
        size_t offset = from[i] - from[0];
        to[nto+i] = data + size_to + offset;
    }

    return to;
}


static void parse_comma_str(const char *comma_str, char ***items, int *num_items) {
    if (*items == NULL) {
        *items = split_comma_str(comma_str, num_items);
    } else {
        int num_append = 0;
        char **new_items = split_comma_str(comma_str, &num_append);
        if (new_items) {
            *items = concat_str_arrays(*items, new_items, *num_items, num_append);
            free(new_items[0]);
            free(new_items);
            *num_items += num_append;
        }
    }
}
