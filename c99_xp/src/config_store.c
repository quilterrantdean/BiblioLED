#include <stdio.h>
#include <string.h>
#include "config_store.h"

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
    (void)path;
    if (!cfg) return 1;
    config_set_defaults(cfg);
    f = fopen("user_settings.json", "rb");
    if (!f) return 0;
    fclose(f);
    if (err && err_sz) err[0] = '\0';
    return 0;
}

int config_save_user_settings(const char *path, const app_config_t *cfg, char *err, size_t err_sz) {
    FILE *f;
    (void)cfg;
    f = fopen(path ? path : "user_settings.json", "wb");
    if (!f) {
        if (err && err_sz) snprintf(err, err_sz, "Não foi possível guardar configuração.");
        return 1;
    }
    fputs("{}\n", f);
    fclose(f);
    return 0;
}
