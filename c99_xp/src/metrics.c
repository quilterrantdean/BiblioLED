#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include "metrics.h"
#include "datetime_utils.h"

static void now_str(char out[20]) {
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    strftime(out, 20, "%Y-%m-%d %H:%M", lt);
}

static void day_start_str(char out[20]) {
    time_t t = time(NULL);
    struct tm lt = *localtime(&t);
    lt.tm_hour = 0; lt.tm_min = 0; lt.tm_sec = 0;
    strftime(out, 20, "%Y-%m-%d %H:%M", &lt);
}

static void minus_days_str(int days, char out[20]) {
    time_t t = time(NULL) - (time_t)days * 86400;
    struct tm *lt = localtime(&t);
    strftime(out, 20, "%Y-%m-%d %H:%M", lt);
}

int metrics_get_dashboard(const app_config_t *cfg, dashboard_metrics_t *out, char *err, size_t err_sz) {
    sqlite3 *db = NULL; sqlite3_stmt *stmt = NULL;
    char now[20], day_start[20], month_start[20], expiry_threshold[20];
    int daily_count = 0, monthly_count = 0;
    const char *db_file = (cfg && cfg->db_file[0]) ? cfg->db_file : "biblioteca.db";
    if (!out) return 1;
    memset(out, 0, sizeof(*out));

    now_str(now); day_start_str(day_start);
    minus_days_str(cfg ? cfg->monthly_window_days : 30, month_start);
    minus_days_str(cfg ? cfg->loan_duration_days : 21, expiry_threshold);

    if (sqlite3_open(db_file, &db) != SQLITE_OK) return 1;

    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM loans WHERE loan_date >= ? AND loan_date <= ?", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, day_start, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, now, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) daily_count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt); stmt = NULL;

    if (sqlite3_prepare_v2(db, "SELECT id, title, loan_date, type FROM loans WHERE loan_date >= ? AND loan_date <= ? ORDER BY loan_date ASC", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, month_start, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, now, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (out->monthly_loans_count < 1024) {
                loan_record_t *r = &out->monthly_loans[out->monthly_loans_count];
                r->id = sqlite3_column_int(stmt, 0);
                snprintf(r->title, sizeof(r->title), "%s", (const char*)sqlite3_column_text(stmt, 1));
                snprintf(r->loan_date, sizeof(r->loan_date), "%s", (const char*)sqlite3_column_text(stmt, 2));
                snprintf(r->type, sizeof(r->type), "%s", (const char*)sqlite3_column_text(stmt, 3));
            }
            out->monthly_loans_count++;
        }
    }
    sqlite3_finalize(stmt); stmt = NULL;

    if (sqlite3_prepare_v2(db, "SELECT title, loan_date, type FROM loans WHERE loan_date >= ? ORDER BY loan_date ASC", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, expiry_threshold, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW && out->expiries_count < 256) {
            const char *title = (const char*)sqlite3_column_text(stmt, 0);
            const char *date_s = (const char*)sqlite3_column_text(stmt, 1);
            const char *type = (const char*)sqlite3_column_text(stmt, 2);
            struct tm tmv; char expiry[20]; time_t loan_ts, expiry_ts, now_ts;
            if (parse_db_datetime(date_s, &tmv) != 0) continue;
            loan_ts = mktime(&tmv);
            expiry_ts = loan_ts + (time_t)(cfg ? cfg->loan_duration_days : 21) * 86400;
            now_ts = time(NULL);
            if (expiry_ts >= now_ts) {
                struct tm *etm = localtime(&expiry_ts);
                expiry_item_t *e = &out->expiries[out->expiries_count++];
                strftime(expiry, 20, "%Y-%m-%d %H:%M", etm);
                snprintf(e->expiry_date, sizeof(e->expiry_date), "%s", expiry);
                e->days_left = days_between(now_ts, expiry_ts);
                snprintf(e->title, sizeof(e->title), "%s", title ? title : "");
                snprintf(e->type, sizeof(e->type), "%s", type ? type : "");
            }
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    monthly_count = out->monthly_loans_count;
    out->daily_available = (cfg ? cfg->max_daily_loans : 5) - daily_count;
    if (out->daily_available < 0) out->daily_available = 0;
    out->monthly_available = (cfg ? cfg->max_monthly_loans : 15) - monthly_count;
    if (out->monthly_available < 0) out->monthly_available = 0;
    if (err && err_sz) err[0] = '\0';
    return 0;
}
