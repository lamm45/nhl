#ifndef NHL_APP_DAYS_H_
#define NHL_APP_DAYS_H_

#include <stdbool.h>


typedef struct Date {
    int year;
    int month;
    int day;
} Date;

typedef enum DateStatus {
    DATE_OK,
    DATE_NOT_FOUND,
    DATE_NOT_UNIQUE,
    DATE_NOT_CALENDAR,
} DateStatus;


/* Return default date which is either today or yesterday. */
Date default_date(void);

/* Convert string to an NhlDate object.
 *
 * If success, the result is written into `date` and the return value
 * is true. Otherwise, `date` is unmodified and false is returned.
 *
 * Valid case-insensitive strings are `yesterday`, `today` and `tomorrow`,
 * any weekday such as `monday`, or a date of YYYY-MM-DD, MM-DD, or DD.
 */
DateStatus date_from_str(const char *day_str, Date *date); // parse_day

/* Check if the date is valid, i.e., found in a calendar.
 */
DateStatus validate_date(const Date *date);

/* Find and sort unique dates. NOT IMPLEMENTED.
 */
int unique_dates(const Date *dates, Date *unique_dates);

/* Determine if a sorted date sequence is continuous. NOT IMPLEMENTED.
 */
int is_continuous_dates(const Date *dates);

/* Local time zone setting as an hour offset from the UTC.
 */
double local_timezone(void);

#endif /* NHL_APP_DAYS_H_ */
