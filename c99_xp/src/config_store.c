#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config_store.h"

static int parse_int_field(const char *buf, const char *key, int *out) {
    char pattern[64];
    char *p;
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr((char*)buf, pattern);
    if (!p) return 1;
    p = strchr(p, ':');
    if (!p) return 1;
    *out = atoi(p + 1);
    return 0;
}

static int parse_str_field(const char *buf, const char *key, char *out, size_t out_sz) {
    char pattern[64];
    char *p, *q;
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr((char*)buf, pattern);
    if (!p) return 1;
    p = strchr(p, ':');
    if (!p) return 1;
    p = strchr(p, '"');
    if (!p) return 1;
    p++;
    q = strchr(p, '"');
    if (!q) return 1;
    snprintf(out, out_sz, "%.*s", (int)(q - p), p);
    return 0;
}

void config_set_defaults(app_config_t *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    strcpy(cfg->db_file, "biblioteca.db");
    cfg->loan_duration_days = 21;
    cfg->monthly_window_days = 30;
    cfg->max_daily_loans = 5;
    cfg->max_monthly_loans = 15;
    cfg->csv_delimiter = ';';
}

int config_load_user_settings(const char *path, app_config_t *cfg, char *err, size_t err_sz) {
    FILE *f;
    long sz;
    char *buf;
    const char *p = path ? path : "user_settings.json";
    if (!cfg) return 1;
    config_set_defaults(cfg);
    f = fopen(p, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return 0; }
    buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return 1; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return 1; }
    buf[sz] = '\0'; fclose(f);

    parse_str_field(buf, "DB_FILE", cfg->db_file, sizeof(cfg->db_file));
    parse_int_field(buf, "LOAN_DURATION_DAYS", &cfg->loan_duration_days);
    parse_int_field(buf, "MONTHLY_WINDOW_DAYS", &cfg->monthly_window_days);
    parse_int_field(buf, "MAX_DAILY_LOANS", &cfg->max_daily_loans);
    parse_int_field(buf, "MAX_MONTHLY_LOANS", &cfg->max_monthly_loans);
    {
        char csv_d[8] = {0};
        if (parse_str_field(buf, "CSV_DELIMITER", csv_d, sizeof(csv_d)) == 0 && csv_d[0]) cfg->csv_delimiter = csv_d[0];
    }
    free(buf);
    if (err && err_sz) err[0] = '\0';
    return 0;
}

int config_save_user_settings(const char *path, const app_config_t *cfg, char *err, size_t err_sz) {
    FILE *f;
    const char *p = path ? path : "user_settings.json";
    if (!cfg) return 1;
    f = fopen(p, "wb");
    if (!f) {
        if (err && err_sz) snprintf(err, err_sz, "Não foi possível guardar configuração.");
        return 1;
    }
    fprintf(f,
        "{\n"
        "  \"DB_FILE\": \"%s\",\n"
        "  \"LOAN_DURATION_DAYS\": %d,\n"
        "  \"MONTHLY_WINDOW_DAYS\": %d,\n"
        "  \"MAX_DAILY_LOANS\": %d,\n"
        "  \"MAX_MONTHLY_LOANS\": %d,\n"
        "  \"CSV_DELIMITER\": \"%c\"\n"
        "}\n",
        cfg->db_file,
        cfg->loan_duration_days,
        cfg->monthly_window_days,
        cfg->max_daily_loans,
        cfg->max_monthly_loans,
        cfg->csv_delimiter ? cfg->csv_delimiter : ';'
    );
    fclose(f);
    if (err && err_sz) err[0] = '\0';
    return 0;
}
