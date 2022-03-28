#ifndef NHL_APP_DISPLAY_H_
#define NHL_APP_DISPLAY_H_

#include <nhl/game.h>


typedef enum DisplayStyle {
    STYLE_DEFAULT,
    STYLE_COMPACT,
    STYLE_TEKSTITV,
} DisplayStyle;


typedef struct DisplayOptions {
    DisplayStyle style;
    double utc_offset;

    int num_teams;
    char **teams;

    int num_highlight;
    char **highlight;
} DisplayOptions;


/* Print scheduled games to screen according to the options.
 * Returns the number of games printed. */
int display(const NhlSchedule *schedule, const DisplayOptions *opts);


#endif /* NHL_APP_DISPLAY_H_ */
