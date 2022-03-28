#ifndef NHL_UTILS_H_
#define NHL_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif


/* Calendar date. */
typedef struct NhlDate {
    /* Calendar year. */
    int year;
    /* Month (January is 1 and December is 12). */
    int month;
    /* Day in month between 1 and 31. */
    int day;
} NhlDate;

/* Convert date string to NhlDate. The string must be of the form "YYYY-MM-DD". */
NhlDate nhl_string_to_date(const char *str);

/* Convert NhlDate to a string of the form "YYYY-MM-DD" (with leading zeros for month and day when
 * needed). The returned string must be released by free(). Returns NULL if error occurs. */
char *nhl_date_to_string(const NhlDate *date);

/* Compare dates. Negative result means that the first argument is "smaller", i.e., earlier.
 * Positive means that the second argument is "smaller". Zero means equality. */
int nhl_date_compare(const NhlDate *date1, const NhlDate *date2);


/* Clock time. */
typedef struct NhlTime {
    /* Hours between 0 and 24. */
    int hours;
    /* Minutes between 0 and 60. */
    int mins;
    /* Seconds between 0 and 60. */
    int secs;
} NhlTime;

/* Convert time string to NhlTime. The time must be of the form "HH:MM:SS" or "MM:SS". */
NhlTime nhl_string_to_time(const char *str);

/* Convert NhlTime to a string of the form "HH:MM:SS" (with leading zeros when needed).
 * The returned string must be released by free(). Returns NULL if error occurs. */
char *nhl_time_to_string(const NhlTime *time);

/* Compare times. Negative result means that the first argument is "smaller", i.e., earlier.
 * Positive means that the second argument is "smaller". Zero means equality. */
int nhl_time_compare(const NhlTime *time1, const NhlTime *time2);


/* Complete datetime. */
typedef struct NhlDateTime {
    /* Date component of the datetime. */
    NhlDate date;
    /* Time component of the datetime. */
    NhlTime time;
} NhlDateTime;

/* Convert datetime string to NhlDateTime. The string must follow the ISO 8601 convention
 * "YYYY-MM-DDTHH:MM:SSZ", where T can also be a space character, and Z is optional. */
NhlDateTime nhl_string_to_datetime(const char *str);

/* Convert NhlDateTime to a ISO 8601 string "YYYY-MM-DDTHH:MM:SSZ" with T and Z verbatim.
 * The returned string must be released by free(). Returns NULL if error occurs. */
char *nhl_datetime_to_string(const NhlDateTime *time);

/* Compare datetimes. Negative result means that the first argument is "smaller", i.e., earlier.
 * Positive means that the second argument is "smaller". Zero means equality. */
int nhl_datetime_compare(const NhlDateTime *time1, const NhlDateTime *time2);


/* Height of a person in feet-inch notation. */
typedef struct NhlHeight {
    int feet;
    int inches;
} NhlHeight;

/* Convert string of the form F' I" to NhlHeight. */
NhlHeight nhl_string_to_height(const char *str);

/* Convert NhlHeight to a string F' I". The returned string must be released by free().
 * Returns NULL if error occurs. */
char *nhl_height_to_string(const NhlHeight *height);

/* Convert feet and inches to centimeters. */
double nhl_height_to_cm(const NhlHeight *height);


/* Convert pounds to kilograms. */
double nhl_pounds_to_kg(double pounds);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NHL_UTILS_H_ */
