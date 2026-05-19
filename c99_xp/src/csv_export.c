#include <stdio.h>
#include "csv_export.h"

int csv_export_history(const app_config_t *cfg, const loan_record_t *items, int count, const char *out_path, char *err, size_t err_sz) {
    FILE *f;
    int i;
    char delim = (cfg && cfg->csv_delimiter) ? cfg->csv_delimiter : ';';
    if (!out_path) return 1;
    f = fopen(out_path, "wb");
    if (!f) {
        if (err && err_sz) snprintf(err, err_sz, "Erro ao exportar CSV");
        return 1;
    }
    fprintf(f, "Title%cLoanDate%cType\n", delim, delim);
    for (i = 0; i < count; ++i) {
        fprintf(f, "%s%c%s%c%s\n", items[i].title, delim, items[i].loan_date, delim, items[i].type);
    }
    fclose(f);
    return 0;
}
