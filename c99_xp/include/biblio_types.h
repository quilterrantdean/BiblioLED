#ifndef BIBLIO_TYPES_H
#define BIBLIO_TYPES_H

#include <stddef.h>

typedef struct {
    int id;
    char title[256];
    char loan_date[20]; /* YYYY-MM-DD HH:MM */
    char type[32];
} loan_record_t;

typedef struct {
    char expiry_date[20];
    int days_left;
    char title[256];
    char type[32];
} expiry_item_t;

typedef struct {
    int daily_available;
    int monthly_available;
    int expiries_count;
    expiry_item_t expiries[256];
    int monthly_loans_count;
    loan_record_t monthly_loans[1024];
} dashboard_metrics_t;

typedef struct {
    char db_file[260];
    int loan_duration_days;
    int monthly_window_days;
    int max_daily_loans;
    int max_monthly_loans;
    char csv_delimiter;
} app_config_t;

#endif
