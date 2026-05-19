#ifndef DATETIME_UTILS_H
#define DATETIME_UTILS_H

#include <time.h>

int parse_db_datetime(const char *s, struct tm *out_tm);
int format_db_datetime(const struct tm *tmv, char out[20]);
int days_between(time_t a, time_t b);

#endif
