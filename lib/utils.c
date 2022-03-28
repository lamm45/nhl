#include <nhl/utils.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


NhlDate nhl_string_to_date(const char *str) {
    int year;
    int month;
    int day;
    NhlDate date = {0};

    if (sscanf(str, "%d-%d-%d", &year, &month, &day) == 3) {
        date.year = year;
        date.month = month;
        date.day = day;
    }

    return date;
}

char *nhl_date_to_string(const NhlDate *date) {
    char *str = NULL;
    if (date->year <= 0 || 9999 < date->year)
        return str;
    if (date->month <= 0 || 12 < date->month)
        return str;
    if (date->day <= 0 || 31 < date->day)
        return str;

    str = malloc(sizeof("YYYY-MM-DD"));
    sprintf(str, "%d-%02d-%02d", date->year, date->month, date->day);
    return str;
}

int nhl_date_compare(const NhlDate *date1, const NhlDate *date2) {
    if (date1->year < date2->year)
        return -1;
    else if (date1->year > date2->year)
        return 1;
    else if (date1->month < date2->month)
        return -1;
    else if (date1->month > date2->month)
        return 1;
    else if (date1->day < date2->day)
        return -1;
    else if (date1->day > date2->day)
        return 1;
    return 0;
}


NhlTime nhl_string_to_time(const char *str) {
    int hours;
    int mins;
    int secs;
    NhlTime time = {0};

    if (sscanf(str, "%d:%d:%d", &hours, &mins, &secs) == 3) {
        time.hours = hours;
        time.mins = mins;
        time.secs = secs;
    } else if (sscanf(str, "%d:%d", &mins, &secs) == 2) {
        time.mins = mins;
        time.secs = secs;
    }

    return time;
}

char *nhl_time_to_string(const NhlTime *time) {
    char *str = NULL;
    if (time->hours < 0 || 24 < time->hours) {
        return str;
    }
    if (time->mins < 0 || 60 < time->mins) {
        return str;
    }
    if (time->secs < 0 || 60 < time->secs) {
        return str;
    }

    str = malloc(sizeof("00:00:00"));
    sprintf(str, "%02d:%02d:%02d", time->hours, time->mins, time->secs);
    return str;
}

int nhl_time_compare(const NhlTime *time1, const NhlTime *time2) {
    if (time1->hours < time2->hours)
        return -1;
    else if (time1->hours > time2->hours)
        return 1;
    else if (time1->mins < time2->mins)
        return -1;
    else if (time1->mins > time2->mins)
        return 1;
    else if (time1->secs < time2->secs)
        return -1;
    else if (time1->secs > time2->secs)
        return 1;
    return 0;
}


NhlDateTime nhl_string_to_datetime(const char *str) {
    static const size_t prefix_len = sizeof("yyyy-mm-ddT") - 1; /* without null-terminator */
    NhlDateTime datetime = {0};
    datetime.date = nhl_string_to_date(str);
    if (strlen(str) > prefix_len) {
        datetime.time = nhl_string_to_time(str + prefix_len);
    }
    return datetime;
}

char *nhl_datetime_to_string(const NhlDateTime *time) {
    char *date_str = nhl_date_to_string(&time->date);
    size_t date_len = strlen(date_str);
    char *time_str = nhl_time_to_string(&time->time);
    size_t time_len = strlen(time_str);

    /* <date>T<time>Z + null-terminator */
    char *str = malloc(date_len + 1 + time_len + 1 + 1);
    memcpy(str, date_str, date_len);
    memcpy(str + date_len + 1, time_str, time_len);
    str[date_len] = 'T';
    str[date_len + 1 + time_len] = 'Z';
    str[date_len + 1 + time_len + 1] = '\0';

    return str;
}

int nhl_datetime_compare(const NhlDateTime *time1, const NhlDateTime *time2) {
    int date_diff = nhl_date_compare(&time1->date, &time2->date);
    if (date_diff != 0)
        return date_diff;
    return nhl_time_compare(&time1->time, &time2->time);
}


NhlHeight nhl_string_to_height(const char *str) {
    int feet;
    int inches;
    NhlHeight height = {0};

    if (sscanf(str, "%d' %d\"", &feet, &inches) == 2) {
        height.feet = feet;
        height.inches = inches;
    }

    return height;
}

char *nhl_height_to_string(const NhlHeight *height) {
    char *str = NULL;
    if (height->feet < 0 || 99 < height->feet) {
        return str;
    }
    if (height->inches < 0 || 99 < height->inches) {
        return str;
    }

    str = malloc(sizeof("99' 99\""));
    sprintf(str, "%d' %d\"", height->feet, height->inches);
    return str;
}

double nhl_height_to_cm(const NhlHeight *height) {
    return 30.48 * height->feet + 2.54 * height->inches;
}


double nhl_pounds_to_kg(double pounds) {
    return 0.45359237 * pounds;
}
