#include <stdio.h>
#include "datetime_utils.h"

int parse_db_datetime(const char *s, struct tm *out_tm) {
    int y,m,d,h,mi;
    if (!s || !out_tm) return 1;
    if (sscanf(s, "%d-%d-%d %d:%d", &y, &m, &d, &h, &mi) != 5) return 1;
    out_tm->tm_year = y - 1900;
    out_tm->tm_mon = m - 1;
    out_tm->tm_mday = d;
    out_tm->tm_hour = h;
    out_tm->tm_min = mi;
    out_tm->tm_sec = 0;
    out_tm->tm_isdst = -1;
    return 0;
}

int format_db_datetime(const struct tm *tmv, char out[20]) {
    if (!tmv || !out) return 1;
    if (strftime(out, 20, "%Y-%m-%d %H:%M", tmv) == 0) return 1;
    return 0;
}

int days_between(time_t a, time_t b) {
    double secs = difftime(b, a);
    if (secs < 0) secs = -secs;
    return (int)(secs / 86400.0);
}
