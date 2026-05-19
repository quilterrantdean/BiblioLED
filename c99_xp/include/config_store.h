#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <stddef.h>
#include "biblio_types.h"

void config_set_defaults(app_config_t *cfg);
int config_load_user_settings(const char *path, app_config_t *cfg, char *err, size_t err_sz);
int config_save_user_settings(const char *path, const app_config_t *cfg, char *err, size_t err_sz);

#endif
