#include <string.h>
#include "metrics.h"

int metrics_get_dashboard(const app_config_t *cfg, dashboard_metrics_t *out, char *err, size_t err_sz) {
    (void)cfg;
    if (!out) return 1;
    memset(out, 0, sizeof(*out));
    if (err && err_sz) err[0] = '\0';
    return 0;
}
