#define _GNU_SOURCE

#include "days.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <nhl/nhl.h>


Date default_date(void) {
    time_t currtime = time(NULL);
    struct tm *now = localtime(&currtime);
    time_t esttime = currtime - now->tm_gmtoff - 5*60*60; // Eastern Standard Time

    // Delay of 6 hours before extracting date: Date changes at 6 AM EST
    time_t delayed = esttime - 6*60*60;

    struct tm *then = localtime(&delayed);
    Date date = {
        .year = then->tm_year + 1900,
        .month = then->tm_mon + 1,
        .day = then->tm_mday,
    };
    return date;
}


static const char *const days[] = {
    "sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday",
    "yesterday", "today", "tomorrow"
};
static const int num_days = sizeof(days) / sizeof(days[0]);

/* Store index to matching day.
 */
static DateStatus find_day(const char *day, int *index) {
    int num_matches = 0;
    for (int i = 0 ; i!=num_days; ++i) {
        if (strcasestr(days[i], day) == days[i]) {
            *index = i;
            ++num_matches;
        }
    }

    if (num_matches == 1)
        return DATE_OK;
    else if (num_matches == 0)
        return DATE_NOT_FOUND;
    else
        return DATE_NOT_UNIQUE;
}


/* Find year such that the given day is closest to the current day.
 */
static int closest_year(int month, int day) {
    time_t currtime = time(NULL);
    struct tm *now = localtime(&currtime);

    // Same month, same year
    if (month == now->tm_mon + 1)
        return now->tm_year + 1900;

    // Compare to the current time
    struct tm cmp = *now;
    cmp.tm_mon = month - 1;
    cmp.tm_mday = day;
    time_t cmptime = mktime(&cmp);
    double diff = difftime(cmptime, currtime);

    if (diff > 0) {
        // Proposed time is after current time when the year is same
        --cmp.tm_year;
        cmptime = mktime(&cmp);
        if (difftime(currtime, cmptime) < diff)
            return cmp.tm_year + 1900;
    } else {
        // Proposed time is before current time when the year is same
        ++cmp.tm_year;
        cmptime = mktime(&cmp);
        if (difftime(currtime, cmptime) > diff)
            return cmp.tm_year + 1900;
    }

    return now->tm_year + 1900;
}

/* Find month such that the given day is closest to the current day.
 */
static int closest_month(int day) {
    time_t currtime = time(NULL);
    struct tm *now = localtime(&currtime);

    // Same day, same month
    if (day == now->tm_mday)
        return now->tm_mon + 1;

    // Compare to the current time
    struct tm cmp = *now;
    cmp.tm_mday = day;
    time_t cmptime = mktime(&cmp);
    double diff = difftime(cmptime, currtime);

    if (diff > 0) {
        // Proposed time is after current time when the month is same
        cmp.tm_mon = (12 + cmp.tm_mon - 1) % 12;
        cmptime = mktime(&cmp);
        if (difftime(currtime, cmptime) < diff)
            return cmp.tm_mon + 1;
    } else {
        // Proposed time is before current time when the month is same
        cmp.tm_mon = (12 + cmp.tm_mon + 1) % 12;
        cmptime = mktime(&cmp);
        if (difftime(currtime, cmptime) > diff)
            return cmp.tm_mon + 1;
    }

    return now->tm_mon + 1;
}


DateStatus date_from_str(const char *day, Date *date) {
    int num1 = 0;
    int num2 = 0;
    int num3 = 0;
    switch (sscanf(day, "%d-%d-%d", &num1, &num2, &num3)) {
        case 3:
            // YYYY-MM-DD
            date->year = num1;
            date->month = num2;
            date->day = num3;
            return validate_date(date);
        case 2:
            // MM-DD
            date->month = num1;
            date->day = num2;
            date->year = closest_year(date->month, date->day);
            return validate_date(date);
        case 1:
            // DD
            date->day = num1;
            date->month = closest_month(date->day);
            date->year = closest_year(date->month, date->day);
            return validate_date(date);
    }

    // Text argument
    int wday;
    DateStatus status = find_day(day, &wday);
    if (status == DATE_OK) {
        time_t currtime = time(NULL);
        struct tm *now = localtime(&currtime);
        if (wday > 6) {
            // 7: yesterday, 8: today, 9: tomorrow
            wday = (7 + now->tm_wday + (wday - 8)) % 7;
        }

        /* now\wday  0  1  2  3  4  5  6
         * 0 sun     0 +1 +2 +3 -3 -2 -1
         * 1 mon    -1  0 +1 +2 +3 -3 -2
         * 2 tue    -2 -1  0 +1 +2 +3 -3
         * 3 wed    -3 -2 -1  0 +1 +2 +3
         * 4 thu    +3 -3 -2 -1  0 +1 +2
         * 5 fri    +2 +3 -3 -2 -1  0 +1
         * 6 sat    +1 +2 +3 -3 -2 -1  0
         */
        int diff = wday - now->tm_wday;
        if (diff > 3) {
            diff -= 7;
        } else if (diff < -3) {
            diff += 7;
        }

        time_t goaltime = currtime + diff * 60*60*24;
        struct tm *goal = localtime(&goaltime);
        date->year = goal->tm_year + 1900;
        date->month = goal->tm_mon + 1;
        date->day = goal->tm_mday;
    }

    return status;
}

DateStatus validate_date(const Date *date) {
    struct tm tmdate = {
        .tm_year = date->year - 1900,
        .tm_mon = date->month - 1,
        .tm_mday = date->day,
        .tm_hour = 12,
    };

    if (mktime(&tmdate) == (time_t) -1)
        return DATE_NOT_CALENDAR;

    if (tmdate.tm_year + 1900 != date->year ||
        tmdate.tm_mon + 1 != date->month ||
        tmdate.tm_mday != date->day)
        return DATE_NOT_CALENDAR;

    return DATE_OK;
}

double local_timezone() {
    time_t currtime = time(NULL);
    struct tm *now = localtime(&currtime);
    return now->tm_gmtoff / (60.0 * 60.0);
}
