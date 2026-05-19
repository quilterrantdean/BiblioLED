#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config_store.h"
#include "loans_repo.h"
#include "metrics.h"
#include "csv_export.h"

static const char *arg_value(int argc, char **argv, const char *name) {
    int i;
    for (i = 2; i < argc - 1; ++i) {
        if (strcmp(argv[i], name) == 0) return argv[i + 1];
    }
    return NULL;
}

int main(int argc, char **argv) {
    app_config_t cfg;
    char err[256] = {0};
    config_load_user_settings("user_settings.json", &cfg, err, sizeof(err));

    if (argc < 2) {
        puts("uso: biblioled_cli <init-db|add|list|delete|edit|metrics|export>");
        return 1;
    }

    if (strcmp(argv[1], "init-db") == 0) {
        return loans_init_db(&cfg, err, sizeof(err)) == 0 ? (puts("ok: init-db"), 0) : (fprintf(stderr, "erro: %s\n", err), 2);
    } else if (strcmp(argv[1], "add") == 0) {
        const char *title = arg_value(argc, argv, "--title");
        const char *date = arg_value(argc, argv, "--date");
        const char *type = arg_value(argc, argv, "--type");
        if (!title || !date || !type) { puts("faltam argumentos"); return 1; }
        return loans_add(&cfg, title, date, type, err, sizeof(err)) == 0 ? (puts("ok: add"), 0) : (fprintf(stderr, "erro: %s\n", err), 2);
    } else if (strcmp(argv[1], "list") == 0) {
        loan_record_t rows[4096]; int n = 0; int i;
        if (loans_read_all(&cfg, rows, 4096, &n, err, sizeof(err)) != 0) { fprintf(stderr, "erro: %s\n", err); return 2; }
        for (i = 0; i < n && i < 4096; ++i) printf("%d | %s | %s | %s\n", rows[i].id, rows[i].loan_date, rows[i].type, rows[i].title);
        return 0;
    } else if (strcmp(argv[1], "delete") == 0) {
        const char *id_s = arg_value(argc, argv, "--id");
        if (!id_s) return 1;
        return loans_delete(&cfg, atoi(id_s), err, sizeof(err)) == 0 ? (puts("ok: delete"), 0) : (fprintf(stderr, "erro: %s\n", err), 2);
    } else if (strcmp(argv[1], "edit") == 0) {
        const char *id_s = arg_value(argc, argv, "--id");
        const char *title = arg_value(argc, argv, "--title");
        const char *date = arg_value(argc, argv, "--date");
        const char *type = arg_value(argc, argv, "--type");
        if (!id_s || !title || !date || !type) return 1;
        return loans_edit(&cfg, atoi(id_s), title, date, type, err, sizeof(err)) == 0 ? (puts("ok: edit"), 0) : (fprintf(stderr, "erro: %s\n", err), 2);
    } else if (strcmp(argv[1], "metrics") == 0) {
        dashboard_metrics_t m; int i;
        if (metrics_get_dashboard(&cfg, &m, err, sizeof(err)) != 0) { fprintf(stderr, "erro: %s\n", err); return 3; }
        printf("daily_available=%d\nmonthly_available=%d\nexpiries=%d\n", m.daily_available, m.monthly_available, m.expiries_count);
        for (i = 0; i < m.expiries_count; ++i) {
            printf("EXP | %s | %d | %s | %s\n", m.expiries[i].expiry_date, m.expiries[i].days_left, m.expiries[i].type, m.expiries[i].title);
        }
        return 0;
    } else if (strcmp(argv[1], "export") == 0) {
        const char *out = arg_value(argc, argv, "--out");
        loan_record_t rows[4096]; int n = 0;
        if (!out) return 1;
        if (loans_read_all(&cfg, rows, 4096, &n, err, sizeof(err)) != 0) { fprintf(stderr, "erro: %s\n", err); return 2; }
        return csv_export_history(&cfg, rows, n > 4096 ? 4096 : n, out, err, sizeof(err)) == 0 ? (puts("ok: export"), 0) : (fprintf(stderr, "erro: %s\n", err), 2);
    }

    puts("comando não suportado");
    return 1;
}
