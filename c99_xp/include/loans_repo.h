#ifndef LOANS_REPO_H
#define LOANS_REPO_H

#include <stddef.h>
#include "biblio_types.h"

int loans_init_db(const app_config_t *cfg, char *err, size_t err_sz);
int loans_add(const app_config_t *cfg, const char *title, const char *loan_date, const char *type, char *err, size_t err_sz);
int loans_delete(const app_config_t *cfg, int record_id, char *err, size_t err_sz);
int loans_edit(const app_config_t *cfg, int record_id, const char *new_title, const char *new_date, const char *new_type, char *err, size_t err_sz);
int loans_read_all(const app_config_t *cfg, loan_record_t *out, int out_cap, int *out_count, char *err, size_t err_sz);

#endif
