#include <stdio.h>
#include <string.h>
#include "config_store.h"
#include "loans_repo.h"
#include "metrics.h"

int main(int argc, char **argv) {
    app_config_t cfg;
    char err[256];
    dashboard_metrics_t m;

    config_set_defaults(&cfg);

    if (argc < 2) {
        puts("uso: biblioled_cli <init-db|metrics>");
        return 1;
    }

    if (strcmp(argv[1], "init-db") == 0) {
        if (loans_init_db(&cfg, err, sizeof(err)) != 0) {
            fprintf(stderr, "erro: %s\n", err);
            return 2;
        }
        puts("ok: init-db");
        return 0;
    }

    if (strcmp(argv[1], "metrics") == 0) {
        if (metrics_get_dashboard(&cfg, &m, err, sizeof(err)) != 0) {
            fprintf(stderr, "erro: %s\n", err);
            return 3;
        }
        printf("daily_available=%d\nmonthly_available=%d\n", m.daily_available, m.monthly_available);
        return 0;
    }

    puts("comando não suportado nesta versão inicial");
    return 1;
}
