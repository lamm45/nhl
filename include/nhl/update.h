#ifndef NHL_UPDATE_H_
#define NHL_UPDATE_H_

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Content types when manually updating the database. */
typedef enum NhlUpdateContentType {
    /* Detailed schedule and game info for one day.
     * Example URL: https://statsapi.web.nhl.com/api/v1/schedule?expand=schedule.linescore&expand=schedule.scoringplays&date=2021-11-22
     * Note: The question mark (?) should be replaced with escape code %3F for file:// URLs.
     */
    NHL_CONTENT_SCHEDULE,

    /* Full bio for one player.
     * Example URL: https://statsapi.web.nhl.com/api/v1/people/8475799
     */
    NHL_CONTENT_PEOPLE,

    /* All active teams or a single (possibly inactive) team.
     * Example URLs:
     * https://statsapi.web.nhl.com/api/v1/teams
     * https://statsapi.web.nhl.com/api/v1/teams/3
     */
    NHL_CONTENT_TEAMS,

    /* All franchises or a single franchise.
     * Example URLs:
     * https://statsapi.web.nhl.com/api/v1/franchises
     * https://statsapi.web.nhl.com/api/v1/franchises/20
     */
    NHL_CONTENT_FRANCHISES,

    /* All active divisions or a single (possibly inactive) division.
     * Example URLs:
     * https://statsapi.web.nhl.com/api/v1/divisions
     * https://statsapi.web.nhl.com/api/v1/divisions/17
     */
    NHL_CONTENT_DIVISIONS,

    /* All active conferences or a single (possibly inactive) conference.
     * Example URLs:
     * https://statsapi.web.nhl.com/api/v1/conferences
     * https://statsapi.web.nhl.com/api/v1/conferences/5
     */
    NHL_CONTENT_CONFERENCES,

    /* List of supported game statuses.
     * Example URL: https://statsapi.web.nhl.com/api/v1/gameStatus
     */
    NHL_CONTENT_GAME_STATUSES,

    /* List of supported game types.
     * Example URL: https://statsapi.web.nhl.com/api/v1/gameTypes
     */
    NHL_CONTENT_GAME_TYPES,

    /* List of supported player on-ice positions.
     * Example URL: https://statsapi.web.nhl.com/api/v1/positions
     */
    NHL_CONTENT_POSITIONS,

    /* List of supported player roster statuses.
     * Example URL: https://statsapi.web.nhl.com/api/v1/rosterStatuses
     */
    NHL_CONTENT_ROSTER_STATUSES,


    /* CURRENTLY NOT SUPPORTED.
     * Example URL: https://statsapi.web.nhl.com/api/v1/game/2021020281/boxscore
     */
    NHL_CONTENT_BOXSCORE,

    /* CURRENTLY NOT SUPPORTED.
     * Example URL: https://statsapi.web.nhl.com/api/v1/game/2021020281/linescore
     */
    NHL_CONTENT_LINESCORE,

    /* CURRENTLY NOT SUPPORTED.
     * Example URL: https://statsapi.web.nhl.com/api/v1/playTypes
     */
    NHL_CONTENT_PLAY_TYPES,

    /* CURRENTLY NOT SUPPORTED.
     * Example URL: https://statsapi.web.nhl.com/api/v1/venues
     */
    NHL_CONTENT_VENUES
} NhlUpdateContentType;


/* Read contents from the given URL and update database.
 *
 * First, this function tries to download contents from `url` which can be
 * any ordinary URL, such as file://... or https://...
 *
 * Then, the database (cache) is unconditionally updated by treating the downloaded
 * content to be the most up-to-date. Usually this function should not be
 * called from a user application, because the updating is done automatically
 * by other functions whenever needed.
 */
NhlStatus nhl_update_from_url(Nhl *nhl, const char *url, NhlUpdateContentType type);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NHL_UPDATE_H_ */
