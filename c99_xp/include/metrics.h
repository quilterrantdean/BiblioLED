#ifndef METRICS_H
#define METRICS_H

#include <stddef.h>
#include "biblio_types.h"

int metrics_get_dashboard(const app_config_t *cfg, dashboard_metrics_t *out, char *err, size_t err_sz);

#endif
