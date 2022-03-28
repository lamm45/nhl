#ifndef NHL_APP_UARGS_H_
#define NHL_APP_UARGS_H_

#include <stdbool.h>


/* Command-line arguments given by the user.
 */
typedef struct UserArgs {
    // Days for which to display scores
    int num_days;
    char **days;

    // Teams for filtering the output
    int num_teams;
    char **teams;

    // Output format options
    int num_highlight;
    char **highlight;
    bool compact;
    bool tekstitv;
    bool timezone_set;
    double timezone;

    // Cache settings
    char *cache_file;
    bool readonly;
    bool offline;
    bool update;

    // Miscellaneous settings
    int verbose;
} UserArgs;


/* Parse command-line arguments.
 *
 * Reads command-line arguments and modifies the UserArgs object pointed by `uargs`.
 * The modified object should be released with the `reset_args` function.
 *
 * Returns zero on success. An error might cause the program to exit.
 */
int parse_args(int argc, char **argv, UserArgs *uargs);


/* Release resources allocated by `parse_args` and reset `uargs` members to zero.
 *
 * Returns the input argument, or NULL if an error occurs.
 */
UserArgs *reset_args(UserArgs *args);


/* Pretty-print arguments to standard output.
 */
void print_args(UserArgs *args);


#endif /* NHL_APP_UARGS_H_ */
