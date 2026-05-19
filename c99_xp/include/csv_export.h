#ifndef CSV_EXPORT_H
#define CSV_EXPORT_H

#include <stddef.h>
#include "biblio_types.h"

int csv_export_history(const app_config_t *cfg, const loan_record_t *items, int count, const char *out_path, char *err, size_t err_sz);

#endif
