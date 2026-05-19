#include <stdio.h>
#include "loans_repo.h"

int loans_init_db(const app_config_t *cfg, char *err, size_t err_sz) { (void)cfg; if (err&&err_sz) err[0]='\0'; return 0; }
int loans_add(const app_config_t *cfg, const char *title, const char *loan_date, const char *type, char *err, size_t err_sz) {
    (void)cfg; (void)title; (void)loan_date; (void)type; if (err&&err_sz) err[0]='\0'; return 0; }
int loans_delete(const app_config_t *cfg, int record_id, char *err, size_t err_sz) { (void)cfg; (void)record_id; if (err&&err_sz) err[0]='\0'; return 0; }
int loans_edit(const app_config_t *cfg, int record_id, const char *new_title, const char *new_date, const char *new_type, char *err, size_t err_sz) {
    (void)cfg; (void)record_id; (void)new_title; (void)new_date; (void)new_type; if (err&&err_sz) err[0]='\0'; return 0; }
int loans_read_all(const app_config_t *cfg, loan_record_t *out, int out_cap, int *out_count, char *err, size_t err_sz) {
    (void)cfg; (void)out; (void)out_cap; if (out_count) *out_count = 0; if (err&&err_sz) err[0]='\0'; return 0; }
